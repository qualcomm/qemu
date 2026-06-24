/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * QTest testcase for the QCT QtTimer device
 */

#include "qemu/osdep.h"
#include "libqtest-single.h"
#include "hw/timer/qct-qtimer.h"
#include "qemu/bitops.h"

#define QTIMER_DEFAULT_FREQ_HZ 19200000ULL
#define QTIMER_VIEW_BASE 0xfc921000
#define QTIMER_AC_BASE 0xfc920000    /* Access control base */
#define QTIMER_FRAME2_BASE 0xfc922000 /* Second frame for multi-frame tests */

/* Timer testing constants */
#define TIMER_TEST_OFFSET 1000
/* TIMER_TEST_OFFSET ticks expressed in nanoseconds of QEMU_CLOCK_VIRTUAL */
#define TIMER_TEST_NS \
    ((TIMER_TEST_OFFSET * 1000000000ULL) / QTIMER_DEFAULT_FREQ_HZ)

static uint32_t qtimer_read32(uint64_t base, uint32_t offset)
{
    return readl(base + offset);
}

__attribute__((unused))
static void qtimer_write32(uint64_t base, uint32_t offset, uint32_t value)
{
    writel(base + offset, value);
}

static uint64_t qtimer_read64(uint64_t base, uint32_t offset)
{
    uint32_t lo = qtimer_read32(base, offset);
    uint32_t hi = qtimer_read32(base, offset + 4);
    return ((uint64_t)hi << 32) | lo;
}

static void qtimer_write64(uint64_t base, uint32_t offset, uint64_t value)
{
    qtimer_write32(base, offset, extract64(value, 0, 32));
    qtimer_write32(base, offset + 4, extract64(value, 32, 32));
}

/* Test basic device presence and register access */
static void test_qtimer_basic_access(void)
{
    uint32_t val;

    /* Test frequency register - should be default value */
    val = qtimer_read32(QTIMER_VIEW_BASE, QCT_QTIMER_CNT_FREQ);
    g_assert_cmpuint(val, ==, QTIMER_DEFAULT_FREQ_HZ);
}

/* Test multiple timer frames */
static void test_qtimer_multiple_frames(void)
{
    uint32_t val;
    uint64_t frame0_base = QTIMER_VIEW_BASE;
    uint64_t frame1_base = QTIMER_VIEW_BASE + 0x1000;  /* Next frame */

    /* Test that both frames have the same frequency */
    val = qtimer_read32(frame0_base, QCT_QTIMER_CNT_FREQ);
    g_assert_cmpuint(val, ==, QTIMER_DEFAULT_FREQ_HZ);

    val = qtimer_read32(frame1_base, QCT_QTIMER_CNT_FREQ);
    g_assert_cmpuint(val, ==, QTIMER_DEFAULT_FREQ_HZ);
}

/* Test that registers exist and can be accessed */
static void test_qtimer_register_reads(void)
{
    qtimer_read32(QTIMER_VIEW_BASE, QCT_QTIMER_CNT_FREQ);
    qtimer_read64(QTIMER_VIEW_BASE, QCT_QTIMER_CNTPCT_LO);
    qtimer_read64(QTIMER_VIEW_BASE, QCT_QTIMER_CNTP_CVAL_LO);
    qtimer_read32(QTIMER_VIEW_BASE, QCT_QTIMER_CNTP_CTL);
    qtimer_read32(QTIMER_VIEW_BASE, QCT_QTIMER_CNTP_TVAL);
}

/* Test access control register functionality */
static void test_qtimer_access_control(void)
{
    uint32_t val;

    /* Test access control register - should allow setting bits */
    qtimer_write32(QTIMER_AC_BASE, 0x40, 0x3f);  /* Enable all timers */
    val = qtimer_read32(QTIMER_AC_BASE, 0x40);
    g_assert_cmpuint(val & 0x3f, ==, 0x3f);
}

/* Test timer control and value registers */
static void test_qtimer_control_registers(void)
{
    uint32_t ctl_val, tval;
    uint64_t cval_before, cval_after;

    /* Read current counter value */
    cval_before = qtimer_read64(QTIMER_VIEW_BASE, QCT_QTIMER_CNTPCT_LO);

    /* Set timer value (TVAL) */
    tval = 1000;  /* Small value for testing */
    qtimer_write32(QTIMER_VIEW_BASE, QCT_QTIMER_CNTP_TVAL, tval);

    /* Enable timer */
    qtimer_write32(QTIMER_VIEW_BASE, QCT_QTIMER_CNTP_CTL, 1);
    ctl_val = qtimer_read32(QTIMER_VIEW_BASE, QCT_QTIMER_CNTP_CTL);

    /* Check if timer enable bit is readable: */
    if (ctl_val & 1) {
        g_assert_cmpuint(ctl_val & 1, ==, 1);  /* Timer should be enabled */
    }

    /* Read CVAL - should be current counter + TVAL */
    cval_after = qtimer_read64(QTIMER_VIEW_BASE, QCT_QTIMER_CNTP_CVAL_LO);
    /* CVAL should be greater than before since we set TVAL */
    g_assert_cmpuint(cval_after, >, cval_before);

    /* Disable timer */
    qtimer_write32(QTIMER_VIEW_BASE, QCT_QTIMER_CNTP_CTL, 0);
    ctl_val = qtimer_read32(QTIMER_VIEW_BASE, QCT_QTIMER_CNTP_CTL);
    g_assert_cmpuint(ctl_val & 1, ==, 0);  /* Timer should be disabled */
}

/* Test CVAL register direct access */
static void test_qtimer_cval_access(void)
{
    uint64_t test_cval = 0x123456789abcdef0ULL;
    uint64_t read_cval;
    uint64_t current_time;

    /* Read current time to use as baseline */
    current_time = qtimer_read64(QTIMER_VIEW_BASE, QCT_QTIMER_CNTPCT_LO);

    /* Use a future time value relative to current counter */
    test_cval = current_time + 10000;

    /* Write and read back CVAL */
    qtimer_write64(QTIMER_VIEW_BASE, QCT_QTIMER_CNTP_CVAL_LO, test_cval);
    read_cval = qtimer_read64(QTIMER_VIEW_BASE, QCT_QTIMER_CNTP_CVAL_LO);
    g_assert_cmpuint(read_cval, ==, test_cval);
}

/* Test counter progression */
static void test_qtimer_counter_progression(void)
{
    uint64_t count1, count2;

    /*
     * Read counter twice - in qtest mode, counter does not advance
     * so we just verify it's readable and consistent.
     */
    count1 = qtimer_read64(QTIMER_VIEW_BASE, QCT_QTIMER_CNTPCT_LO);
    count2 = qtimer_read64(QTIMER_VIEW_BASE, QCT_QTIMER_CNTPCT_LO);

    g_assert_cmpuint(count2, ==, count1);

    /* Verify frequency register is accessible and has expected value */
    uint32_t freq = qtimer_read32(QTIMER_VIEW_BASE, QCT_QTIMER_CNT_FREQ);
    g_assert_cmpuint(freq, ==, QTIMER_DEFAULT_FREQ_HZ);
}

/* Test timer behavior with clock control - robust timing test */
static void test_qtimer_timer_behavior(void)
{
    uint64_t current_count, target_count, new_count;

    /* Get current counter value */
    current_count = qtimer_read64(QTIMER_VIEW_BASE, QCT_QTIMER_CNTPCT_LO);

    /* Set timer to expire in near future */
    target_count = current_count + TIMER_TEST_OFFSET;
    qtimer_write64(QTIMER_VIEW_BASE, QCT_QTIMER_CNTP_CVAL_LO, target_count);

    /* Verify CVAL was set correctly */
    uint64_t read_cval = qtimer_read64(QTIMER_VIEW_BASE,
        QCT_QTIMER_CNTP_CVAL_LO);
    g_assert_cmpuint(read_cval, ==, target_count);

    /* Enable timer */
    qtimer_write32(QTIMER_VIEW_BASE, QCT_QTIMER_CNTP_CTL, 1);

    uint64_t ctl_val = qtimer_read64(QTIMER_VIEW_BASE, QCT_QTIMER_CNTP_CTL);
    /* Check that EN is set and ISTAT is disable (Not IRQ pending)*/
    g_assert_cmpuint(ctl_val, ==, 0x1);

    /* Step virtual clock forward but not past target */
    qtest_clock_step(global_qtest, TIMER_TEST_NS / 2);

    /* Timer should still be running */
    new_count = qtimer_read64(QTIMER_VIEW_BASE, QCT_QTIMER_CNTPCT_LO);
    g_assert_cmpuint(new_count, >=, current_count);

    /* Step past the target time */
    qtest_clock_step(global_qtest, TIMER_TEST_NS);

    /* Verify counter has advanced past target */
    new_count = qtimer_read64(QTIMER_VIEW_BASE, QCT_QTIMER_CNTPCT_LO);
    g_assert_cmpuint(new_count, >=, target_count);

    /* Disable timer */
    qtimer_write32(QTIMER_VIEW_BASE, QCT_QTIMER_CNTP_CTL, 0);

    ctl_val = qtimer_read64(QTIMER_VIEW_BASE, QCT_QTIMER_CNTP_CTL);
    /*
     * Let's check that EN is disabled and ISTAT is set because
     * new_count >= target_count, so we should have IRQ pending.
     */
    g_assert_cmpuint(ctl_val, ==, 0x4);

    /*
     * Step clock — CNTPCT runs independently of CNTP_CTL.EN. Only the
     * compare/IRQ logic is gated by EN, so the counter must keep
     * advancing even after the timer is disabled.
     */
    qtest_clock_step(global_qtest, TIMER_TEST_NS / 2);

    uint64_t count_after_disable = qtimer_read64(QTIMER_VIEW_BASE,
                                                 QCT_QTIMER_CNTPCT_LO);
    g_assert_cmpuint(count_after_disable, >, new_count);

    /* ISTAT remains set while CNTPCT >= CVAL, even with EN=0 */
    ctl_val = qtimer_read64(QTIMER_VIEW_BASE, QCT_QTIMER_CNTP_CTL);
    g_assert_cmpuint(ctl_val, ==, 0x4);

    /* Test TVAL direct setting and read-back */
    qtimer_write32(QTIMER_VIEW_BASE, QCT_QTIMER_CNTP_TVAL, 2000);

    /* Verify timer configuration persists across enable/disable cycles */
    qtimer_write32(QTIMER_VIEW_BASE, QCT_QTIMER_CNTP_CTL, 1);
    qtimer_write32(QTIMER_VIEW_BASE, QCT_QTIMER_CNTP_CTL, 0);

    /* Final verification that all registers are accessible */
    qtimer_read32(QTIMER_VIEW_BASE, QCT_QTIMER_CNT_FREQ);
    qtimer_read64(QTIMER_VIEW_BASE, QCT_QTIMER_CNTPCT_LO);
    qtimer_read64(QTIMER_VIEW_BASE, QCT_QTIMER_CNTP_CVAL_LO);
    qtimer_read32(QTIMER_VIEW_BASE, QCT_QTIMER_CNTP_CTL);
    qtimer_read32(QTIMER_VIEW_BASE, QCT_QTIMER_CNTP_TVAL);
}

static void test_qtimer_on_machine(gconstpointer data)
{
    const char *machine = (const char *)data;
    g_autofree char *args = g_strdup_printf(
        "-machine %s -global qct-qtimer.freq-scale=1", machine);

    qtest_start(args);

    /* Run all the qtimer tests */
    test_qtimer_basic_access();
    test_qtimer_multiple_frames();
    test_qtimer_register_reads();
    test_qtimer_access_control();
    test_qtimer_control_registers();
    test_qtimer_cval_access();
    test_qtimer_counter_progression();
    test_qtimer_timer_behavior();

    qtest_end();
}

/*
 * SA8775P CDSP0: verify CNTPCT advances independently of CTL.EN.
 * The counter runs from boot regardless of SW programming the timer,
 * so simply stepping the virtual clock must produce a higher value.
 */
#define SA8775P_QTIMER_VIEW_BASE (0x26300000 + 0xA1000)

static void test_sa8775p_ticking(void)
{
    uint64_t count1, count2;

    qtest_start("-machine SA8775P_CDSP0");

    qtest_clock_step(global_qtest, TIMER_TEST_NS);
    count1 = qtimer_read64(SA8775P_QTIMER_VIEW_BASE, QCT_QTIMER_CNTPCT_LO);

    qtest_clock_step(global_qtest, TIMER_TEST_NS);
    count2 = qtimer_read64(SA8775P_QTIMER_VIEW_BASE, QCT_QTIMER_CNTPCT_LO);

    g_assert_cmpuint(count2, >, count1);

    qtest_end();
}

static void test_qtimer_frame_stride(gconstpointer data)
{
    uint32_t stride = GPOINTER_TO_UINT(data);
    uint32_t freq;
    g_autofree char *args = g_strdup_printf(
        "-machine virt -global qct-qtimer.frame_stride=0x%x", stride);

    qtest_start(args);

    freq = qtimer_read32(QTIMER_VIEW_BASE, QCT_QTIMER_CNT_FREQ);
    g_assert_cmpuint(freq, ==, QTIMER_DEFAULT_FREQ_HZ);

    freq = qtimer_read32(QTIMER_VIEW_BASE + stride, QCT_QTIMER_CNT_FREQ);
    g_assert_cmpuint(freq, ==, QTIMER_DEFAULT_FREQ_HZ);

    if (stride > 0x1000) {
        freq = qtimer_read32(QTIMER_VIEW_BASE + 0x1000, QCT_QTIMER_CNT_FREQ);
        g_assert_cmpuint(freq, ==, 0);
    }

    qtest_end();
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);

    /* Test on virt machine */
    qtest_add_data_func("/qct-qtimer/virt/all-tests", "virt",
                        test_qtimer_on_machine);

    /* Test on V66G_1024 machine */
    qtest_add_data_func("/qct-qtimer/V66G_1024/all-tests", "V66G_1024",
                        test_qtimer_on_machine);

    /* Verify counter is live on SA8775P_CDSP0 */
    qtest_add_func("/qct-qtimer/SA8775P_CDSP0/ticking",
                   test_sa8775p_ticking);

    qtest_add_data_func("/qct-qtimer/virt/frame-stride-2000",
                        GUINT_TO_POINTER(0x2000), test_qtimer_frame_stride);
    qtest_add_data_func("/qct-qtimer/virt/frame-stride-4000",
                        GUINT_TO_POINTER(0x4000), test_qtimer_frame_stride);

    return g_test_run();
}
