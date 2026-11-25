/*
 * DWMAC network controller model.
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "hw/core/sysbus.h"
#include "hw/core/irq.h"
#include "hw/core/qdev-properties.h"
#include "migration/vmstate.h"
#include "system/dma.h"
#include "net/net.h"
#include "qom/object.h"
#include "qapi/error.h"

/* Device type. */
#define TYPE_DWMAC_STMMAC "dwmac-stmmac"
OBJECT_DECLARE_SIMPLE_TYPE(DwmacState, DWMAC_STMMAC)

typedef enum { DWMAC_VAR_XGMAC2 = 0, DWMAC_VAR_DW25GMAC = 1 } DwmacVariant;

/* Register definitions. */

/* MMIO windows. */
#define DWMAC_MAC_SIZE          0x20000
#define DWMAC_RGMII_SIZE        0x1000
#define DWMAC_XPCS_SIZE         0x10000

/* MAC and DMA registers. */
#define R_GMAC4_VERSION         0x0110
#define R_GMAC_INT_STATUS       0x00b0
#define R_GMAC_INT_EN           0x00b4

#define GMAC_VERSION_XGMAC2     0x7621
#define GMAC_VERSION_DW25GMAC   0x5542

#define R_DMA_MODE              0x3000
#define   DMA_MODE_SWR          (1u << 0)
#define   DMA_MODE_DSCB         (1u << 16)
#define   DMA_MODE_HMIC         (1u << 17)
#define R_DMA_SYSBUS_MODE       0x3004
#define R_DMA_STATUS            0x3008

#define R_IND_CTRL              0x3080
#define   IND_OB                (1u << 0)
#define R_IND_DATA              0x3084

/* Queue count. */
#define R_HW_FEATURE2           0x0124
#define HW_FEATURE2_4Q  ((3u << 18) | (3u << 12) | (3u << 6) | (3u << 0))

/* Channel layouts. */
#define XGMAC2_CHAN_BASE        0x3100
#define XGMAC2_CHAN_STRIDE      0x80

#define DW25GMAC_EVEN_CHAN_BASE        0x8500
#define DW25GMAC_ODD_CHAN_BASE         0x8580
#define DW25GMAC_CHAN_STRIDE       0x1000

/* Per-channel register offsets. */
#define CH_CONTROL              0x00
#define CH_TX_CONTROL           0x04
#define   CH_TX_CONTROL_ST      (1u << 0)
#define   CH_TX_CONTROL_EDSE    (1u << 28)
#define CH_RX_CONTROL           0x08
#define   CH_RX_CONTROL_SR      (1u << 0)
#define CH_TXDESC_HI            0x10
#define CH_TXDESC_LO            0x14
#define CH_RXDESC_HI            0x18
#define CH_RXDESC_LO            0x1c
#define CH_TXDESC_TAIL          0x24
#define CH_RXDESC_TAIL          0x2c
#define CH_TX_RING_LEN          0x30
#define CH_RX_RING_LEN          0x34
#define CH_INTR_ENA             0x38
#define CH_CUR_TXDESC           0x44
#define CH_CUR_RXDESC           0x4c
#define CH_STATUS               0x60
#define   CH_STATUS_TI          (1u << 0)
#define   CH_STATUS_TPS         (1u << 1)
#define   CH_STATUS_TBU         (1u << 2)
#define   CH_STATUS_RI          (1u << 6)
#define   CH_STATUS_AIS         (1u << 14)
#define   CH_STATUS_NIS         (1u << 15)

/* Descriptor fields. */
#define TDES3_OWN               (1u << 31)
#define TDES3_CTXT              (1u << 30)
#define TDES3_FD                (1u << 29)
#define TDES3_LD                (1u << 28)
#define TDES2_IOC               (1u << 31)
#define TDES2_B1L_MASK          0x3fff
#define RDES3_OWN               (1u << 31)
#define RDES3_FD                (1u << 29)
#define RDES3_LD                (1u << 28)
#define RDES3_PL_MASK           0x3fff

/* Hardware feature registers. */
#define R_HW_FEATURE0           0x011c
#define R_HW_FEATURE1           0x0120

#define R_HW_FEATURE3           0x0128
#define R_HW_FEATURE4           0x012c

#define HW_FEATURE0_DW25GMAC \
    ((1u << 16) | (1u << 14) | (1u << 12) | (1u << 11) | \
     (1u <<  8) | (1u <<  5) | (1u <<  1))

#define HW_FEATURE1_DW25GMAC \
    ((11u << 27) | (3u << 21) | (1u << 18) | (1u << 17) | \
     (2u  << 14) | (7u <<  6) | (7u <<  0))

#define HW_FEATURE3_DW25GMAC \
    ((1u << 11) | (1u << 9) | (1u << 3))

/* XPCS registers. */
#define XPCS_SR_MII_PCS_CTRL1   0x0000
#define XPCS_VR_MII_DIG_CTRL1   0x5000
#define XPCS_VR_MII_AN_CTRL     0x5004
#define XPCS_VR_MII_AN_INTR_STS 0x5008
#define XPCS_SR_MII_MMD_CTRL    0x4000
#define XPCS_VR_MII_PCS_CTRL1   0x2000
#define XPCS_VR_XS_DIG_CTRL1    0x2000
#define XPCS_VR_XS_DIG_STS      0x2040

#define XPCS_SW_RST_MASK        (1u << 15)
#define XPCS_USXG_RST_MASK      (1u << 10)
#define XPCS_SOFT_RST           (1u << 15)

#define XPCS_REGS               (0x6000 / 4)

#define DWMAC_MAC_REGS           128

#define DWMAC_NUM_CHANS         8
#define DWMAC_TX_RING_MAX       1024
#define DWMAC_FRAME_MAX         16384

#define DWMAC_LOOP_QLEN         64

/* Channel state. */
typedef struct DwmacChan {
    uint32_t control;
    uint32_t tx_control;
    uint32_t rx_control;
    uint32_t txdesc_hi, txdesc_lo;
    uint32_t rxdesc_hi, rxdesc_lo;
    uint32_t txdesc_tail;
    uint32_t rxdesc_tail;
    uint32_t tx_ring_len;
    uint32_t rx_ring_len;
    uint32_t intr_ena;
    uint32_t cur_txdesc;
    uint32_t cur_rxdesc;
    uint32_t status;
} DwmacChan;

/* Device state. */
struct DwmacState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
    MemoryRegion rgmii_iomem;
    MemoryRegion xpcs_iomem;
    qemu_irq irq;
    NICState *nic;
    NICConf conf;

    char *variant_str;
    DwmacVariant variant;

    uint32_t dma_mode;
    uint32_t dma_sysbus_mode;
    uint32_t gmac_int_en;

    uint32_t mac_regs[DWMAC_MAC_REGS];

    uint32_t ind_ctrl;
    uint32_t ind_data[DWMAC_NUM_CHANS][8];

    DwmacChan chan[DWMAC_NUM_CHANS];

    uint32_t xpcs_regs[XPCS_REGS];

    struct {
        uint8_t  data[DWMAC_FRAME_MAX];
        uint32_t len;
    } loop_q[DWMAC_LOOP_QLEN];
    uint32_t loop_head;
    uint32_t loop_tail;
    uint32_t loop_count;

    bool link_up;
    bool xpcs_loopback;
};

/* Channel address helpers. */
static int chan_decode(const DwmacState *s, hwaddr off, hwaddr *sub)
{
    if (s->variant == DWMAC_VAR_DW25GMAC) {

        const hwaddr CHAN_REG_SPAN = 0x80;
        for (int idx = 0; idx < DWMAC_NUM_CHANS; idx++) {
            hwaddr base = ((idx % 2) ? DW25GMAC_ODD_CHAN_BASE : DW25GMAC_EVEN_CHAN_BASE)
                          + idx * DW25GMAC_CHAN_STRIDE;
            if (off >= base && off < base + CHAN_REG_SPAN) {
                *sub = off - base;
                return idx;
            }
        }
        return -1;
    } else {

        if (off >= XGMAC2_CHAN_BASE &&
            off < XGMAC2_CHAN_BASE + DWMAC_NUM_CHANS * XGMAC2_CHAN_STRIDE) {
            int idx = (off - XGMAC2_CHAN_BASE) / XGMAC2_CHAN_STRIDE;
            *sub = off - (XGMAC2_CHAN_BASE + idx * XGMAC2_CHAN_STRIDE);
            return idx;
        }
    }
    return -1;
}

/* Forward declarations. */
static void dwmac_loop_enqueue(DwmacState *s, const uint8_t *buf, uint32_t len);
static void dwmac_loop_flush(DwmacState *s);

/* IRQ. */
static void dwmac_update_irq(DwmacState *s)
{
    int level = 0;
    for (int i = 0; i < DWMAC_NUM_CHANS; i++) {
        DwmacChan *c = &s->chan[i];
        if ((c->status & c->intr_ena) &
            (CH_STATUS_TI | CH_STATUS_RI | CH_STATUS_TBU |
             CH_STATUS_NIS | CH_STATUS_AIS)) {
            level = 1;
            break;
        }
    }
    qemu_set_irq(s->irq, level);
}

/* TX. */
static void dwmac_tx_run(DwmacState *s, int ci)
{
    DwmacChan *c = &s->chan[ci];
    uint64_t ring_base = ((uint64_t)c->txdesc_hi << 32) | c->txdesc_lo;
    uint32_t ring_len = (c->tx_ring_len & (DWMAC_TX_RING_MAX - 1)) + 1;
    uint8_t frame[DWMAC_FRAME_MAX];
    uint32_t frame_len = 0;
    int processed = 0;

    if (!(c->tx_control & CH_TX_CONTROL_ST) || !ring_base || !ring_len) {
        return;
    }

    bool edse = (c->tx_control & CH_TX_CONTROL_EDSE) != 0;
    uint32_t desc_stride = edse ? 32 : 16;
    uint32_t basic_off   = edse ? 16 : 0;

    if (!c->cur_txdesc ||
        c->cur_txdesc < ring_base ||
        c->cur_txdesc >= ring_base + ring_len * desc_stride) {
        c->cur_txdesc = ring_base;
    }

    while (processed < (int)ring_len) {
        uint32_t des[4];
        hwaddr slot = c->cur_txdesc;
        hwaddr dptr = slot + basic_off;

        if (dma_memory_read(&address_space_memory, dptr, des, sizeof(des),
                            MEMTXATTRS_UNSPECIFIED) != MEMTX_OK) {
            break;
        }
        uint32_t tdes3 = le32_to_cpu(des[3]);
        if (!(tdes3 & TDES3_OWN)) {
            c->status |= CH_STATUS_TBU | CH_STATUS_NIS;
            break;
        }
        if (tdes3 & TDES3_CTXT) {
            des[3] = cpu_to_le32(tdes3 & ~TDES3_OWN);
            dma_memory_write(&address_space_memory, dptr, des, sizeof(des),
                             MEMTXATTRS_UNSPECIFIED);
            goto advance;
        }

        uint32_t tdes2 = le32_to_cpu(des[2]);
        uint64_t buf = ((uint64_t)le32_to_cpu(des[1]) << 32) | le32_to_cpu(des[0]);
        uint32_t blen = tdes2 & TDES2_B1L_MASK;

        if (tdes3 & TDES3_FD) { frame_len = 0; }
        if (blen && frame_len + blen <= DWMAC_FRAME_MAX) {
            dma_memory_read(&address_space_memory, buf, frame + frame_len, blen,
                            MEMTXATTRS_UNSPECIFIED);
            frame_len += blen;
        }

        if (tdes3 & TDES3_LD) {
            NetClientState *nc = qemu_get_queue(s->nic);
            qemu_send_packet(nc, frame, frame_len);

            dwmac_loop_enqueue(s, frame, frame_len);
            frame_len = 0;
        }

        des[3] = cpu_to_le32(tdes3 & ~TDES3_OWN);
        dma_memory_write(&address_space_memory, dptr, des, sizeof(des),
                         MEMTXATTRS_UNSPECIFIED);
        if (tdes2 & TDES2_IOC) {
            c->status |= CH_STATUS_TI | CH_STATUS_NIS;
        }
advance:
        c->cur_txdesc += desc_stride;
        if (c->cur_txdesc >= ring_base + ring_len * desc_stride) {
            c->cur_txdesc = ring_base;
        }
        processed++;
    }
    dwmac_update_irq(s);
}

/* MMIO. */
static uint64_t dwmac_read(void *opaque, hwaddr off, unsigned size)
{
    DwmacState *s = opaque;
    hwaddr sub;
    int ci;

    switch (off) {
    case R_GMAC4_VERSION:
        return (s->variant == DWMAC_VAR_DW25GMAC) ?
               GMAC_VERSION_DW25GMAC : GMAC_VERSION_XGMAC2;
    case R_GMAC_INT_STATUS: return 0;
    case R_GMAC_INT_EN:     return s->gmac_int_en;

    case R_HW_FEATURE0:
        return (s->variant == DWMAC_VAR_DW25GMAC) ? HW_FEATURE0_DW25GMAC : 0;
    case R_HW_FEATURE1:
        return (s->variant == DWMAC_VAR_DW25GMAC) ? HW_FEATURE1_DW25GMAC : 0;
    case R_HW_FEATURE2:
        return (s->variant == DWMAC_VAR_DW25GMAC) ? HW_FEATURE2_4Q : 0;
    case R_HW_FEATURE3:
        return (s->variant == DWMAC_VAR_DW25GMAC) ? HW_FEATURE3_DW25GMAC : 0;
    case R_HW_FEATURE4:
        return 0;
    case R_DMA_MODE:

        if (s->variant == DWMAC_VAR_DW25GMAC) {

            return (s->dma_mode & ~(DMA_MODE_SWR | DMA_MODE_DSCB)) | DMA_MODE_HMIC;
        }
        return s->dma_mode & ~DMA_MODE_SWR;
    case R_DMA_SYSBUS_MODE: return s->dma_sysbus_mode;
    case R_DMA_STATUS: {
        uint32_t v = 0;
        for (int i = 0; i < DWMAC_NUM_CHANS; i++) {
            if (s->chan[i].status) v |= (1u << i);
        }
        return v;
    }

    case R_IND_CTRL:

        return s->ind_ctrl & ~IND_OB;
    case R_IND_DATA: {

        uint32_t mode = (s->ind_ctrl >> 24) & 0xf;
        uint32_t chan = (s->ind_ctrl >> 8) & 0x7f;
        if (chan < DWMAC_NUM_CHANS && mode < 8) {
            return s->ind_data[chan][mode];
        }
        return 0;
    }
    }

    ci = chan_decode(s, off, &sub);
    if (ci >= 0) {
        DwmacChan *c = &s->chan[ci];
        switch (sub) {
        case CH_CONTROL:      return c->control;
        case CH_TX_CONTROL:   return c->tx_control;
        case CH_RX_CONTROL:   return c->rx_control;
        case CH_TXDESC_HI:    return c->txdesc_hi;
        case CH_TXDESC_LO:    return c->txdesc_lo;
        case CH_RXDESC_HI:    return c->rxdesc_hi;
        case CH_RXDESC_LO:    return c->rxdesc_lo;
        case CH_TXDESC_TAIL:  return c->txdesc_tail;
        case CH_RXDESC_TAIL:  return c->rxdesc_tail;
        case CH_TX_RING_LEN:  return c->tx_ring_len;
        case CH_RX_RING_LEN:  return c->rx_ring_len;
        case CH_INTR_ENA:     return c->intr_ena;
        case CH_CUR_TXDESC:   return c->cur_txdesc;
        case CH_CUR_RXDESC:   return c->cur_rxdesc;
        case CH_STATUS:       return c->status;
        }
    }

    if (off < DWMAC_MAC_REGS * 4) {
        return s->mac_regs[off >> 2];
    }

    qemu_log_mask(LOG_UNIMP, "dwmac: unhandled read @0x%" HWADDR_PRIx "\n", off);
    return 0;
}

static void dwmac_write(void *opaque, hwaddr off, uint64_t val, unsigned size)
{
    DwmacState *s = opaque;
    hwaddr sub;
    int ci;

    switch (off) {
    case R_GMAC_INT_EN:      s->gmac_int_en = val; return;
    case R_DMA_MODE:         s->dma_mode = val; return;
    case R_DMA_SYSBUS_MODE:  s->dma_sysbus_mode = val; return;
    case R_IND_CTRL:

        s->ind_ctrl = val & ~IND_OB;
        return;
    case R_IND_DATA: {

        uint32_t mode = (s->ind_ctrl >> 24) & 0xf;
        uint32_t chan = (s->ind_ctrl >> 8) & 0x7f;
        if (chan < DWMAC_NUM_CHANS && mode < 8) {
            s->ind_data[chan][mode] = val;
        }
        return;
    }
    }

    ci = chan_decode(s, off, &sub);
    if (ci >= 0) {
        DwmacChan *c = &s->chan[ci];
        switch (sub) {
        case CH_CONTROL:     c->control = val; return;
        case CH_TX_CONTROL:
            c->tx_control = val;
            if (val & CH_TX_CONTROL_ST) {
                c->cur_txdesc = ((uint64_t)c->txdesc_hi << 32) | c->txdesc_lo;
            }
            return;
        case CH_RX_CONTROL:
            c->rx_control = val;
            if (val & CH_RX_CONTROL_SR) {
                c->cur_rxdesc = ((uint64_t)c->rxdesc_hi << 32) | c->rxdesc_lo;
            }
            return;
        case CH_TXDESC_HI:   c->txdesc_hi = val; return;
        case CH_TXDESC_LO:   c->txdesc_lo = val; return;
        case CH_RXDESC_HI:   c->rxdesc_hi = val; return;
        case CH_RXDESC_LO:   c->rxdesc_lo = val; return;
        case CH_TXDESC_TAIL:
            c->txdesc_tail = val;
            dwmac_tx_run(s, ci);
            return;
        case CH_RXDESC_TAIL:
            c->rxdesc_tail = val;

            dwmac_loop_flush(s);
            return;
        case CH_TX_RING_LEN: c->tx_ring_len = val; return;
        case CH_RX_RING_LEN: c->rx_ring_len = val; return;
        case CH_INTR_ENA:    c->intr_ena = val; return;
        case CH_STATUS:
            c->status &= ~val;
            dwmac_update_irq(s);
            return;
        }
    }

    if (off < DWMAC_MAC_REGS * 4) {
        s->mac_regs[off >> 2] = val;
        return;
    }

    qemu_log_mask(LOG_UNIMP,
                  "dwmac: unhandled write @0x%" HWADDR_PRIx " = 0x%" PRIx64 "\n",
                  off, val);
}

static const MemoryRegionOps dwmac_ops = {
    .read  = dwmac_read,
    .write = dwmac_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid.min_access_size = 4,
    .valid.max_access_size = 4,
};

/* RGMII. */
static uint64_t rgmii_read(void *opaque, hwaddr off, unsigned size)
{
    return 0;
}
static void rgmii_write(void *opaque, hwaddr off, uint64_t val, unsigned size)
{

}
static const MemoryRegionOps rgmii_ops = {
    .read = rgmii_read, .write = rgmii_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid.min_access_size = 4, .valid.max_access_size = 4,
};

/* XPCS. */
static inline uint32_t xpcs_idx(hwaddr off)
{
    return (off < DWMAC_XPCS_SIZE) ? (off >> 2) : 0;
}

static uint64_t xpcs_read(void *opaque, hwaddr off, unsigned size)
{
    DwmacState *s = opaque;
    uint32_t idx = xpcs_idx(off);
    uint32_t val = s->xpcs_regs[idx];

    switch (off) {
    case XPCS_SR_MII_PCS_CTRL1:
    case XPCS_VR_MII_DIG_CTRL1:
        val &= ~XPCS_SW_RST_MASK;
        s->xpcs_regs[idx] = val;
        break;
    case XPCS_VR_MII_PCS_CTRL1:

        val &= ~(XPCS_SW_RST_MASK | XPCS_USXG_RST_MASK);
        s->xpcs_regs[idx] = val;
        break;
    case 0x4004:
    case 0x0004:
        val |= (1u << 2);

        s->link_up = true;
        s->xpcs_loopback = true;
        break;
    case XPCS_VR_MII_AN_INTR_STS:

        val |= (1u << 0);
        break;
    default:
        break;
    }
    return val;
}

static void xpcs_write(void *opaque, hwaddr off, uint64_t val, unsigned size)
{
    DwmacState *s = opaque;
    uint32_t idx = xpcs_idx(off);
    s->xpcs_regs[idx] = (uint32_t)val;

    if (off == XPCS_VR_MII_AN_INTR_STS) {
        s->xpcs_regs[idx] &= ~(uint32_t)val;
    }
}

static const MemoryRegionOps xpcs_ops = {
    .read = xpcs_read, .write = xpcs_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid.min_access_size = 4, .valid.max_access_size = 4,
};

/* RX. */
static int dwmac_rx_deliver_chan(DwmacState *s, int ci,
                                 const uint8_t *buf, size_t len)
{
    DwmacChan *c = &s->chan[ci];
    uint64_t ring_base = ((uint64_t)c->rxdesc_hi << 32) | c->rxdesc_lo;
    uint32_t ring_len = (c->rx_ring_len & (DWMAC_TX_RING_MAX - 1)) + 1;
    uint32_t des[4];
    hwaddr dptr;

    if (!(c->rx_control & CH_RX_CONTROL_SR) || !ring_base) {
        return 0;
    }
    if (!c->cur_rxdesc || c->cur_rxdesc < ring_base) {
        c->cur_rxdesc = ring_base;
    }
    if (len > DWMAC_FRAME_MAX) {
        len = DWMAC_FRAME_MAX;
    }

    dptr = c->cur_rxdesc;
    if (dma_memory_read(&address_space_memory, dptr, des, sizeof(des),
                        MEMTXATTRS_UNSPECIFIED) != MEMTX_OK) {
        return -1;
    }
    if (!(le32_to_cpu(des[3]) & RDES3_OWN)) {
        return 0;
    }

    uint64_t buf_addr = ((uint64_t)le32_to_cpu(des[1]) << 32) | le32_to_cpu(des[0]);
    dma_memory_write(&address_space_memory, buf_addr, buf, len,
                     MEMTXATTRS_UNSPECIFIED);

    des[0] = des[1] = des[2] = 0;
    des[3] = cpu_to_le32(RDES3_FD | RDES3_LD | ((uint32_t)len & RDES3_PL_MASK));
    dma_memory_write(&address_space_memory, dptr, des, sizeof(des),
                     MEMTXATTRS_UNSPECIFIED);

    c->cur_rxdesc += 16;
    if (c->cur_rxdesc >= ring_base + ring_len * 16) {
        c->cur_rxdesc = ring_base;
    }
    c->status |= CH_STATUS_RI | CH_STATUS_NIS;
    return 1;
}

static int dwmac_rx_deliver_any(DwmacState *s, const uint8_t *buf, size_t len)
{
    for (int ci = 0; ci < DWMAC_NUM_CHANS; ci++) {
        int r = dwmac_rx_deliver_chan(s, ci, buf, len);
        if (r == 1) {
            return ci;
        }
    }
    return -1;
}

static bool dwmac_can_receive(NetClientState *nc)
{
    DwmacState *s = qemu_get_nic_opaque(nc);

    for (int ci = 0; ci < DWMAC_NUM_CHANS; ci++) {
        DwmacChan *c = &s->chan[ci];
        uint64_t ring_base = ((uint64_t)c->rxdesc_hi << 32) | c->rxdesc_lo;
        uint32_t des[4];

        if (!(c->rx_control & CH_RX_CONTROL_SR) || !ring_base) {
            continue;
        }
        if (!c->cur_rxdesc || c->cur_rxdesc < ring_base) {
            c->cur_rxdesc = ring_base;
        }
        if (dma_memory_read(&address_space_memory, c->cur_rxdesc, des,
                            sizeof(des), MEMTXATTRS_UNSPECIFIED) != MEMTX_OK) {
            continue;
        }
        if (le32_to_cpu(des[3]) & RDES3_OWN) {
            return true;
        }
    }
    return false;
}

static ssize_t dwmac_receive(NetClientState *nc, const uint8_t *buf, size_t len)
{
    DwmacState *s = qemu_get_nic_opaque(nc);
    int ci = dwmac_rx_deliver_any(s, buf, len);

    if (ci < 0) {
        return 0;
    }
    dwmac_update_irq(s);
    return len;
}

/* Loopback. */
static void dwmac_loop_flush(DwmacState *s)
{
    bool delivered = false;

    while (s->loop_count) {
        uint32_t idx = s->loop_head;
        int ci = dwmac_rx_deliver_any(s, s->loop_q[idx].data,
                                      s->loop_q[idx].len);
        if (ci < 0) {
            break;
        }
        s->loop_head = (s->loop_head + 1) % DWMAC_LOOP_QLEN;
        s->loop_count--;
        delivered = true;
    }
    if (delivered) {
        dwmac_update_irq(s);
    }
}

static void dwmac_loop_enqueue(DwmacState *s, const uint8_t *buf, uint32_t len)
{
    if (!s->xpcs_loopback) {

        return;
    }
    if (len > DWMAC_FRAME_MAX) {
        len = DWMAC_FRAME_MAX;
    }
    if (s->loop_count >= DWMAC_LOOP_QLEN) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "dwmac: loopback queue full, dropping frame\n");
        return;
    }
    memcpy(s->loop_q[s->loop_tail].data, buf, len);
    s->loop_q[s->loop_tail].len = len;
    s->loop_tail = (s->loop_tail + 1) % DWMAC_LOOP_QLEN;
    s->loop_count++;
    dwmac_loop_flush(s);
}

static NetClientInfo net_dwmac_info = {
    .type = NET_CLIENT_DRIVER_NIC,
    .size = sizeof(NICState),
    .can_receive = dwmac_can_receive,
    .receive = dwmac_receive,
};

/* Device lifecycle. */
static void dwmac_reset_hold(Object *obj, ResetType type)
{
    DwmacState *s = DWMAC_STMMAC(obj);
    s->dma_mode = 0;
    s->dma_sysbus_mode = 0;
    s->gmac_int_en = 0;
    s->ind_ctrl = 0;
    memset(s->mac_regs, 0, sizeof(s->mac_regs));
    memset(s->chan, 0, sizeof(s->chan));
    memset(s->xpcs_regs, 0, sizeof(s->xpcs_regs));

    s->loop_head = s->loop_tail = s->loop_count = 0;
    s->link_up = false;
    s->xpcs_loopback = false;

    s->xpcs_regs[0x0008 >> 2] = 0x7996;
    s->xpcs_regs[0x000c >> 2] = 0xced0;

    s->xpcs_regs[0x0004 >> 2] = (1u << 2);
    s->xpcs_regs[0x4004 >> 2] = (1u << 2);

    s->xpcs_regs[0x5008 >> 2] = (1u << 14) | (1u << 13) | (0x3u << 10);
}

static void dwmac_realize(DeviceState *dev, Error **errp)
{
    DwmacState *s = DWMAC_STMMAC(dev);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);

    if (!s->variant_str || strcmp(s->variant_str, "xgmac2") == 0) {
        s->variant = DWMAC_VAR_XGMAC2;
    } else if (strcmp(s->variant_str, "dw25gmac") == 0) {
        s->variant = DWMAC_VAR_DW25GMAC;
    } else {
        error_setg(errp, "invalid variant '%s'; expected 'xgmac2' or 'dw25gmac'",
                   s->variant_str);
        return;
    }

    memory_region_init_io(&s->iomem, OBJECT(dev), &dwmac_ops, s,
                          TYPE_DWMAC_STMMAC, DWMAC_MAC_SIZE);
    sysbus_init_mmio(sbd, &s->iomem);

    memory_region_init_io(&s->rgmii_iomem, OBJECT(dev), &rgmii_ops, s,
                          "dwmac-rgmii", DWMAC_RGMII_SIZE);
    sysbus_init_mmio(sbd, &s->rgmii_iomem);

    memory_region_init_io(&s->xpcs_iomem, OBJECT(dev), &xpcs_ops, s,
                          "dwmac-xpcs", DWMAC_XPCS_SIZE);
    sysbus_init_mmio(sbd, &s->xpcs_iomem);

    sysbus_init_irq(sbd, &s->irq);

    qemu_macaddr_default_if_unset(&s->conf.macaddr);
    s->nic = qemu_new_nic(&net_dwmac_info, &s->conf,
                          object_get_typename(OBJECT(dev)), dev->id,
                          &dev->mem_reentrancy_guard, s);
    qemu_format_nic_info_str(qemu_get_queue(s->nic), s->conf.macaddr.a);
}

static const VMStateDescription vmstate_dwmac = {
    .name = TYPE_DWMAC_STMMAC,
    .version_id = 2,
    .minimum_version_id = 2,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT32(dma_mode, DwmacState),
        VMSTATE_UINT32(dma_sysbus_mode, DwmacState),
        VMSTATE_UINT32(gmac_int_en, DwmacState),
        VMSTATE_END_OF_LIST()
    },
};

static const Property dwmac_properties[] = {
    DEFINE_NIC_PROPERTIES(DwmacState, conf),
    DEFINE_PROP_STRING("variant", DwmacState, variant_str),
};

static void dwmac_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    ResettableClass *rc = RESETTABLE_CLASS(klass);

    dc->realize = dwmac_realize;
    dc->vmsd = &vmstate_dwmac;
    rc->phases.hold = dwmac_reset_hold;
    device_class_set_props(dc, dwmac_properties);
    set_bit(DEVICE_CATEGORY_NETWORK, dc->categories);
}

static const TypeInfo dwmac_types[] = {
    {
        .name          = TYPE_DWMAC_STMMAC,
        .parent        = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(DwmacState),
        .class_init    = dwmac_class_init,
    },
};

DEFINE_TYPES(dwmac_types)
