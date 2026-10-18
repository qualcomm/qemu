/*
 * QTest testcase for the QUP GENI UART
 *
 * The expectations here are taken from the Linux qcom_geni_serial driver,
 * which is the primary consumer of this device: register semantics that the
 * driver depends on are asserted directly.
 *
 * Copyright (c) 2026 Qualcomm Technologies, Inc. All Rights Reserved.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "libqtest.h"
#include "qemu/module.h"
#include "qemu/sockets.h"

/* VIRT_QUP_UART0 on the hexagon virt machine; wired to serial_hd(1) */
#define QUP_BASE                    0x10004000

#define SE_GENI_STATUS              0x040
#define SE_GENI_DMA_MODE_EN         0x258
#define SE_UART_LOOPBACK_CFG        0x22c
#define SE_UART_TX_TRANS_LEN        0x270
#define SE_GENI_M_CMD0              0x600
#define SE_GENI_M_IRQ_STATUS        0x610
#define SE_GENI_M_IRQ_EN            0x614
#define SE_GENI_M_IRQ_CLEAR         0x618
#define SE_GENI_S_CMD0              0x630
#define SE_GENI_S_CMD_CTRL          0x634
#define SE_GENI_S_IRQ_STATUS        0x640
#define SE_GENI_S_IRQ_EN            0x644
#define SE_GENI_S_IRQ_CLEAR         0x648
#define SE_GENI_TX_FIFOn            0x700
#define SE_GENI_RX_FIFOn            0x780
#define SE_GENI_RX_FIFO_STATUS      0x804
#define SE_GENI_TX_WATERMARK_REG    0x80c
#define SE_GENI_RX_WATERMARK_REG    0x810

#define SE_DMA_TX_PTR_L             0xc30
#define SE_DMA_TX_PTR_H             0xc34
#define SE_DMA_TX_LEN               0xc3c
#define SE_DMA_TX_IRQ_STAT          0xc40
#define SE_DMA_TX_IRQ_CLR           0xc44
#define SE_DMA_TX_IRQ_EN_SET        0xc4c
#define SE_DMA_TX_FSM_RST           0xc58
#define SE_DMA_RX_PTR_L             0xd30
#define SE_DMA_RX_PTR_H             0xd34
#define SE_DMA_RX_LEN               0xd3c
#define SE_DMA_RX_IRQ_STAT          0xd40
#define SE_DMA_RX_IRQ_CLR           0xd44
#define SE_DMA_RX_IRQ_EN_SET        0xd4c
#define SE_DMA_RX_LEN_IN            0xd54
#define SE_DMA_RX_FSM_RST           0xd58
#define DMA_DONE                    (1U << 0)
#define DMA_EOT                     (1U << 1)
#define DMA_ERROR                   (1U << 2)
#define DMA_RESET_DONE              (1U << 3)
#define TEST_RAM                    0x00010000

#define M_GENI_CMD_ACTIVE           (1U << 0)
#define S_GENI_CMD_ACTIVE           (1U << 12)

#define M_CMD_DONE_EN               (1U << 0)
#define M_TX_FIFO_WATERMARK_EN      (1U << 30)
#define M_RX_FIFO_WATERMARK_EN      (1U << 26)

#define S_RX_FIFO_WATERMARK_EN      (1U << 26)
#define S_RX_FIFO_LAST_EN           (1U << 27)

#define RX_LAST                     (1U << 31)
#define RX_LAST_BYTE_VALID_MSK      (0x7U << 28)
#define RX_LAST_BYTE_VALID_SHFT     28
#define RX_FIFO_WC_MSK              0x1ffffffU

#define M_OPCODE_SHFT               27
#define UART_START_TX               0x1
#define UART_START_READ             0x1

#define DEF_TX_WM                   2
#define UART_RX_WM                  2

static uint32_t qup_read(QTestState *qts, uint32_t off)
{
    return qtest_readl(qts, QUP_BASE + off);
}

static void qup_write(QTestState *qts, uint32_t off, uint32_t val)
{
    qtest_writel(qts, QUP_BASE + off, val);
}

/*
 * Bring up a virt machine with the QUP UART (serial_hd(1)) attached to a unix
 * socket we hold the other end of, so the test can observe transmitted bytes
 * and inject received ones.
 */
static QTestState *qup_init(int *sock_fd)
{
    g_autofree char *sock_dir = NULL;
    g_autofree char *sock_path = NULL;
    int listen_fd;
    QTestState *qts;

    sock_dir = g_dir_make_tmp("qtest-qup-XXXXXX", NULL);
    g_assert_nonnull(sock_dir);
    sock_path = g_strdup_printf("%s/sock", sock_dir);

    listen_fd = qtest_socket_server(sock_path);

    qts = qtest_initf("-machine virt "
                      "-chardev socket,id=qup0,path=%s "
                      "-serial null -serial chardev:qup0", sock_path);

    *sock_fd = accept(listen_fd, NULL, NULL);
    g_assert_cmpint(*sock_fd, >=, 0);
    close(listen_fd);

    unlink(sock_path);
    rmdir(sock_dir);

    return qts;
}

/*
 * Read exactly @len bytes of transmitted data. Each qtest register access
 * round-trips through QEMU's main loop, which is what lets the chardev make
 * progress, so poll the socket rather than assuming the data is already here.
 */
static bool qup_recv(QTestState *qts, int fd, uint8_t *buf, size_t len)
{
    size_t got = 0;

    for (int i = 0; i < 100 && got < len; i++) {
        GPollFD pfd = { .fd = fd, .events = G_IO_IN };
        ssize_t n;

        if (g_poll(&pfd, 1, 50) > 0) {
            n = read(fd, buf + got, len - got);
            if (n > 0) {
                got += n;
                continue;
            }
        }
        /* Nudge the main loop so pending chardev writes are flushed. */
        qup_read(qts, SE_GENI_STATUS);
    }

    return got == len;
}

/* Poll @off until all bits in @mask are set, letting the main loop run. */
static uint32_t qup_wait_bits(QTestState *qts, uint32_t off, uint32_t mask)
{
    uint32_t val = 0;

    for (int i = 0; i < 100; i++) {
        val = qup_read(qts, off);
        if ((val & mask) == mask) {
            break;
        }
        g_usleep(1000);
    }

    return val;
}

static void qup_start_tx(QTestState *qts, uint32_t len)
{
    qup_write(qts, SE_UART_TX_TRANS_LEN, len);
    qup_write(qts, SE_GENI_M_CMD0, UART_START_TX << M_OPCODE_SHFT);
}

static void qup_start_rx(QTestState *qts)
{
    qup_write(qts, SE_GENI_RX_WATERMARK_REG, UART_RX_WM);
    qup_write(qts, SE_GENI_S_IRQ_EN, S_RX_FIFO_WATERMARK_EN | S_RX_FIFO_LAST_EN);
    qup_write(qts, SE_GENI_S_CMD0, UART_START_READ << M_OPCODE_SHFT);
    g_assert_cmphex(qup_read(qts, SE_GENI_STATUS) & S_GENI_CMD_ACTIVE, ==,
                    S_GENI_CMD_ACTIVE);
}

/*
 * qcom_geni_serial_start_tx_fifo() arms a transfer by enabling the TX
 * watermark interrupt and waiting for it to fire; it only issues M_CMD0 from
 * within the resulting interrupt. The watermark is therefore a level
 * condition on the FIFO occupancy and must assert with no command active,
 * otherwise the driver waits forever for an interrupt that never arrives.
 */
static void test_tx_watermark_is_level(void)
{
    QTestState *qts;
    int fd;

    qts = qup_init(&fd);

    qup_write(qts, SE_GENI_TX_WATERMARK_REG, DEF_TX_WM);

    g_assert_cmphex(qup_read(qts, SE_GENI_STATUS) & M_GENI_CMD_ACTIVE, ==, 0);
    g_assert_cmphex(qup_read(qts, SE_GENI_M_IRQ_STATUS) & M_TX_FIFO_WATERMARK_EN,
                    ==, M_TX_FIFO_WATERMARK_EN);

    /* Arming the interrupt with the level already high must raise the line. */
    qup_write(qts, SE_GENI_M_IRQ_EN, M_TX_FIFO_WATERMARK_EN);
    g_assert_cmphex(qup_read(qts, SE_GENI_M_IRQ_STATUS) & M_TX_FIFO_WATERMARK_EN,
                    ==, M_TX_FIFO_WATERMARK_EN);

    /* A zero threshold is how the driver disables the watermark. */
    qup_write(qts, SE_GENI_TX_WATERMARK_REG, 0);
    g_assert_cmphex(qup_read(qts, SE_GENI_M_IRQ_STATUS) & M_TX_FIFO_WATERMARK_EN,
                    ==, 0);

    close(fd);
    qtest_quit(qts);
}

/*
 * SE_UART_TX_TRANS_LEN declares the exact byte count of the transfer, so a
 * trailing NUL inside the final FIFO word is real payload and must be
 * transmitted rather than treated as padding.
 */
static void test_tx_honours_trans_len(void)
{
    /* "@AB\0" packed little-endian into one FIFO word */
    const uint8_t expect[4] = { '@', 'A', 'B', 0x00 };
    uint8_t buf[4] = { 0xff, 0xff, 0xff, 0xff };
    QTestState *qts;
    int fd;

    qts = qup_init(&fd);

    qup_write(qts, SE_GENI_TX_WATERMARK_REG, DEF_TX_WM);
    qup_start_tx(qts, sizeof(expect));
    qup_write(qts, SE_GENI_TX_FIFOn, 0x00424140);

    g_assert_true(qup_recv(qts, fd, buf, sizeof(buf)));
    g_assert_cmpmem(buf, sizeof(buf), expect, sizeof(expect));

    close(fd);
    qtest_quit(qts);
}

/*
 * Once TX_TRANS_LEN bytes have been consumed the command is finished:
 * M_CMD_DONE_EN latches and M_GENI_CMD_ACTIVE drops. The driver reads
 * SE_GENI_STATUS to decide whether to continue the current transfer or start
 * a new one, so a command that never retires wedges TX after the first write.
 */
static void test_tx_command_retires(void)
{
    uint8_t buf[2];
    QTestState *qts;
    int fd;

    qts = qup_init(&fd);

    qup_write(qts, SE_GENI_TX_WATERMARK_REG, DEF_TX_WM);
    qup_start_tx(qts, 2);

    g_assert_cmphex(qup_read(qts, SE_GENI_STATUS) & M_GENI_CMD_ACTIVE, ==,
                    M_GENI_CMD_ACTIVE);

    qup_write(qts, SE_GENI_TX_FIFOn, 0x00004241); /* "AB" */
    g_assert_true(qup_recv(qts, fd, buf, sizeof(buf)));

    g_assert_cmphex(qup_read(qts, SE_GENI_M_IRQ_STATUS) & M_CMD_DONE_EN, ==,
                    M_CMD_DONE_EN);
    g_assert_cmphex(qup_read(qts, SE_GENI_STATUS) & M_GENI_CMD_ACTIVE, ==, 0);

    /* A second transfer must be able to start once the first has retired. */
    qup_start_tx(qts, 2);
    g_assert_cmphex(qup_read(qts, SE_GENI_STATUS) & M_GENI_CMD_ACTIVE, ==,
                    M_GENI_CMD_ACTIVE);
    qup_write(qts, SE_GENI_TX_FIFOn, 0x00004443); /* "CD" */
    g_assert_true(qup_recv(qts, fd, buf, sizeof(buf)));
    g_assert_cmpmem(buf, sizeof(buf), "CD", 2);

    close(fd);
    qtest_quit(qts);
}

/*
 * qcom_geni_serial_isr() dispatches RX solely from s_irq_status; RX events
 * reported only on the primary interrupt register are never serviced and the
 * FIFO is never drained.
 */
static void test_rx_raises_secondary_irq(void)
{
    /* Two whole FIFO words, so the level reaches UART_RX_WM */
    const uint8_t in[8] = { 'w', 'x', 'y', 'z', 'W', 'X', 'Y', 'Z' };
    QTestState *qts;
    uint32_t s_irq;
    int fd;

    qts = qup_init(&fd);
    qup_start_rx(qts);

    g_assert_cmpint(write(fd, in, sizeof(in)), ==, sizeof(in));

    s_irq = qup_wait_bits(qts, SE_GENI_S_IRQ_STATUS, S_RX_FIFO_WATERMARK_EN);
    g_assert_cmphex(s_irq & S_RX_FIFO_WATERMARK_EN, ==, S_RX_FIFO_WATERMARK_EN);

    g_assert_cmphex(qup_read(qts, SE_GENI_RX_FIFO_STATUS) & RX_FIFO_WC_MSK,
                    ==, 2);
    g_assert_cmphex(qup_read(qts, SE_GENI_RX_FIFOn), ==, 0x7a797877);
    g_assert_cmphex(qup_read(qts, SE_GENI_RX_FIFOn), ==, 0x5a595857);

    /* Clearing is write-1-to-clear against the secondary status register. */
    qup_write(qts, SE_GENI_S_IRQ_CLEAR, s_irq);
    g_assert_cmphex(qup_read(qts, SE_GENI_S_IRQ_STATUS) &
                    S_RX_FIFO_WATERMARK_EN, ==, 0);

    close(fd);
    qtest_quit(qts);
}

/*
 * qcom_geni_serial_handle_rx_fifo() sizes the tail of the burst from
 * RX_LAST/RX_LAST_BYTE_VALID; without them a short final word is read as a
 * full one and the guest consumes padding bytes as if they were input.
 */
static void test_rx_last_partial_word(void)
{
    QTestState *qts;
    uint32_t status;
    int fd;

    qts = qup_init(&fd);
    qup_start_rx(qts);

    g_assert_cmpint(write(fd, "q", 1), ==, 1);

    status = qup_wait_bits(qts, SE_GENI_RX_FIFO_STATUS, RX_LAST);

    g_assert_cmphex(status & RX_FIFO_WC_MSK, ==, 1);
    g_assert_cmphex(status & RX_LAST, ==, RX_LAST);
    g_assert_cmpuint((status & RX_LAST_BYTE_VALID_MSK) >>
                     RX_LAST_BYTE_VALID_SHFT, ==, 1);

    /* A short burst also ends the transfer, reported as S_RX_FIFO_LAST_EN. */
    g_assert_cmphex(qup_read(qts, SE_GENI_S_IRQ_STATUS) & S_RX_FIFO_LAST_EN,
                    ==, S_RX_FIFO_LAST_EN);

    g_assert_cmphex(qup_read(qts, SE_GENI_RX_FIFOn) & 0xff, ==, 'q');

    /* Draining the short word clears both the count and the LAST report. */
    status = qup_read(qts, SE_GENI_RX_FIFO_STATUS);
    g_assert_cmphex(status & RX_FIFO_WC_MSK, ==, 0);
    g_assert_cmphex(status & RX_LAST, ==, 0);

    close(fd);
    qtest_quit(qts);
}

static void test_dma_tx(void)
{
    const uint8_t data[] = { 0x00, 0x41, 0xff, 0x42, 0x00 };
    uint8_t out[sizeof(data)];
    int fd;
    QTestState *qts = qup_init(&fd);

    qtest_memwrite(qts, TEST_RAM, data, sizeof(data));
    qup_write(qts, SE_GENI_DMA_MODE_EN, 1);
    qup_write(qts, SE_DMA_TX_IRQ_EN_SET, DMA_DONE | DMA_ERROR);
    qup_start_tx(qts, sizeof(data));
    qup_write(qts, SE_DMA_TX_PTR_L, TEST_RAM);
    qup_write(qts, SE_DMA_TX_PTR_H, 0);
    qup_write(qts, SE_DMA_TX_LEN, sizeof(data));
    g_assert_true(qup_recv(qts, fd, out, sizeof(out)));
    g_assert_cmpmem(out, sizeof(out), data, sizeof(data));
    g_assert_cmphex(qup_read(qts, SE_DMA_TX_IRQ_STAT), ==, DMA_DONE);
    g_assert_cmphex(qup_read(qts, SE_GENI_STATUS) & M_GENI_CMD_ACTIVE, ==, 0);
    qup_write(qts, SE_DMA_TX_IRQ_CLR, DMA_DONE);
    g_assert_cmphex(qup_read(qts, SE_DMA_TX_IRQ_STAT), ==, 0);
    qup_write(qts, SE_DMA_TX_FSM_RST, 1);
    g_assert_cmphex(qup_read(qts, SE_DMA_TX_IRQ_STAT), ==, DMA_RESET_DONE);
    close(fd);
    qtest_quit(qts);
}

static void test_dma_rx(void)
{
    const uint8_t data[] = { 0x41, 0x00, 0xff, 0x42, 0x43, 0x44, 0x45 };
    uint8_t out[sizeof(data)];
    int fd;
    QTestState *qts = qup_init(&fd);

    qup_write(qts, SE_GENI_DMA_MODE_EN, 1);
    qup_write(qts, SE_DMA_RX_IRQ_EN_SET, DMA_DONE | DMA_EOT | DMA_ERROR);
    qup_write(qts, SE_GENI_S_CMD0, UART_START_READ << M_OPCODE_SHFT);
    qup_write(qts, SE_DMA_RX_PTR_L, TEST_RAM);
    qup_write(qts, SE_DMA_RX_PTR_H, 0);
    qup_write(qts, SE_DMA_RX_LEN, 4);
    g_assert_cmpint(write(fd, data, sizeof(data)), ==, sizeof(data));
    g_assert_cmphex(qup_wait_bits(qts, SE_DMA_RX_IRQ_STAT, DMA_DONE) & DMA_DONE,
                    ==, DMA_DONE);
    g_assert_cmpuint(qup_read(qts, SE_DMA_RX_LEN_IN), ==, 4);
    qtest_memread(qts, TEST_RAM, out, 4);
    g_assert_cmpmem(out, 4, data, 4);
    qup_write(qts, SE_DMA_RX_IRQ_CLR, DMA_DONE);
    qup_write(qts, SE_DMA_RX_PTR_L, TEST_RAM + 4);
    qup_write(qts, SE_DMA_RX_LEN, 4);
    g_assert_cmphex(qup_wait_bits(qts, SE_DMA_RX_IRQ_STAT, DMA_DONE) & DMA_DONE,
                    ==, DMA_DONE);
    g_assert_cmpuint(qup_read(qts, SE_DMA_RX_LEN_IN), ==, 3);
    qtest_memread(qts, TEST_RAM + 4, out + 4, 3);
    g_assert_cmpmem(out, sizeof(out), data, sizeof(data));
    qup_write(qts, SE_DMA_RX_IRQ_CLR, DMA_DONE);
    qup_write(qts, SE_GENI_S_CMD_CTRL, 4);
    g_assert_cmphex(qup_read(qts, SE_DMA_RX_IRQ_STAT) & DMA_EOT, ==, DMA_EOT);
    qup_write(qts, SE_DMA_RX_IRQ_CLR, DMA_EOT);
    qup_write(qts, SE_DMA_RX_FSM_RST, 1);
    g_assert_cmphex(qup_read(qts, SE_DMA_RX_IRQ_STAT), ==, DMA_RESET_DONE);
    close(fd);
    qtest_quit(qts);
}

static void test_dma_tx_backpressure(void)
{
    const size_t len = 1024 * 1024;
    g_autofree uint8_t *data = g_malloc(len);
    g_autofree uint8_t *out = g_malloc(len);
    size_t got = 0;
    int fd;
    QTestState *qts = qup_init(&fd);

    for (size_t i = 0; i < len; i++) {
        data[i] = i;
    }
    qtest_memwrite(qts, TEST_RAM, data, len);
    qup_write(qts, SE_GENI_DMA_MODE_EN, 1);
    qup_write(qts, SE_DMA_TX_IRQ_EN_SET, DMA_DONE);
    qup_start_tx(qts, len);
    qup_write(qts, SE_DMA_TX_PTR_L, TEST_RAM);
    qup_write(qts, SE_DMA_TX_PTR_H, 0);
    qup_write(qts, SE_DMA_TX_LEN, len);

    /* QEMU must remain responsive while the backend is not being read. */
    g_assert_cmphex(qup_read(qts, SE_DMA_TX_IRQ_STAT) & DMA_DONE, ==, 0);
    g_assert_cmphex(qup_read(qts, SE_GENI_STATUS) & M_GENI_CMD_ACTIVE, !=, 0);
    for (int i = 0; i < 10000 && got < len; i++) {
        GPollFD pfd = { .fd = fd, .events = G_IO_IN };
        if (g_poll(&pfd, 1, 10) > 0) {
            ssize_t n = read(fd, out + got, len - got);
            if (n > 0) {
                got += n;
            }
        }
        qup_read(qts, SE_GENI_STATUS);
    }
    g_assert_cmpuint(got, ==, len);
    g_assert_cmpmem(out, len, data, len);
    g_assert_cmphex(qup_wait_bits(qts, SE_DMA_TX_IRQ_STAT, DMA_DONE) & DMA_DONE,
                    ==, DMA_DONE);
    close(fd);
    qtest_quit(qts);
}

static void test_dma_fault(void)
{
    int fd;
    QTestState *qts = qup_init(&fd);

    qup_write(qts, SE_GENI_DMA_MODE_EN, 1);
    qup_start_tx(qts, 4);
    qup_write(qts, SE_DMA_TX_PTR_L, 0);
    qup_write(qts, SE_DMA_TX_PTR_H, 1);
    qup_write(qts, SE_DMA_TX_LEN, 4);
    g_assert_cmphex(qup_read(qts, SE_DMA_TX_IRQ_STAT), ==, DMA_ERROR);
    close(fd);
    qtest_quit(qts);
}

static void test_dma_loopback(void)
{
    const uint8_t data[] = { 0, 1, 2, 3, 4, 5, 6 };
    uint8_t received[sizeof(data)];
    GPollFD pfd;
    int fd;
    QTestState *qts = qup_init(&fd);

    qtest_memwrite(qts, TEST_RAM, data, sizeof(data));
    qup_write(qts, SE_GENI_DMA_MODE_EN, 1);
    qup_write(qts, SE_UART_LOOPBACK_CFG, 1);
    qup_write(qts, SE_GENI_S_CMD0, UART_START_READ << M_OPCODE_SHFT);
    qup_write(qts, SE_DMA_RX_PTR_L, TEST_RAM + 0x1000);
    qup_write(qts, SE_DMA_RX_LEN, 4);
    qup_start_tx(qts, sizeof(data));
    qup_write(qts, SE_DMA_TX_PTR_L, TEST_RAM);
    qup_write(qts, SE_DMA_TX_LEN, sizeof(data));
    g_assert_cmphex(qup_read(qts, SE_DMA_RX_IRQ_STAT) & DMA_DONE, ==, DMA_DONE);
    g_assert_cmphex(qup_read(qts, SE_DMA_TX_IRQ_STAT) & DMA_DONE, ==, 0);
    g_assert_cmpuint(qup_read(qts, SE_DMA_RX_LEN_IN), ==, 4);
    qup_write(qts, SE_DMA_RX_IRQ_CLR, DMA_DONE);
    qup_write(qts, SE_DMA_RX_PTR_L, TEST_RAM + 0x1004);
    qup_write(qts, SE_DMA_RX_LEN, 4);
    g_assert_cmphex(qup_read(qts, SE_DMA_RX_IRQ_STAT) & DMA_DONE, ==, DMA_DONE);
    g_assert_cmphex(qup_read(qts, SE_DMA_TX_IRQ_STAT) & DMA_DONE, ==, DMA_DONE);
    g_assert_cmpuint(qup_read(qts, SE_DMA_RX_LEN_IN), ==, 3);
    qtest_memread(qts, TEST_RAM + 0x1000, received, sizeof(received));
    g_assert_cmpmem(received, sizeof(received), data, sizeof(data));
    pfd = (GPollFD) { .fd = fd, .events = G_IO_IN };
    g_assert_cmpint(g_poll(&pfd, 1, 20), ==, 0);
    close(fd);
    qtest_quit(qts);
}

static void test_dma_self_access(void)
{
    int fd;
    QTestState *qts = qup_init(&fd);

    qup_write(qts, SE_GENI_DMA_MODE_EN, 1);
    qup_start_tx(qts, 4);
    qup_write(qts, SE_DMA_TX_PTR_L, QUP_BASE + SE_GENI_STATUS);
    qup_write(qts, SE_DMA_TX_PTR_H, 0);
    qup_write(qts, SE_DMA_TX_LEN, 4);
    g_assert_cmphex(qup_read(qts, SE_DMA_TX_IRQ_STAT) & DMA_ERROR, ==, DMA_ERROR);
    close(fd);
    qtest_quit(qts);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);

    qtest_add_func("/qup-geni-uart/tx-watermark-is-level",
                   test_tx_watermark_is_level);
    qtest_add_func("/qup-geni-uart/tx-honours-trans-len",
                   test_tx_honours_trans_len);
    qtest_add_func("/qup-geni-uart/tx-command-retires",
                   test_tx_command_retires);
    qtest_add_func("/qup-geni-uart/rx-raises-secondary-irq",
                   test_rx_raises_secondary_irq);
    qtest_add_func("/qup-geni-uart/rx-last-partial-word",
                   test_rx_last_partial_word);

    qtest_add_func("/qup-geni-uart/dma-tx", test_dma_tx);
    qtest_add_func("/qup-geni-uart/dma-rx", test_dma_rx);
    qtest_add_func("/qup-geni-uart/dma-fault", test_dma_fault);
    qtest_add_func("/qup-geni-uart/dma-loopback", test_dma_loopback);
    qtest_add_func("/qup-geni-uart/dma-self-access", test_dma_self_access);
    qtest_add_func("/qup-geni-uart/dma-tx-backpressure",
                   test_dma_tx_backpressure);

    return g_test_run();
}
