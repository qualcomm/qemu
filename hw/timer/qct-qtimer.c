/*
 * Qualcomm QCT QTimer
 *
 *  Copyright(c) 2019-2020 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */


#include "qemu/osdep.h"
#include "hw/core/irq.h"
#include "hw/core/qdev-properties.h"
#include "hw/timer/qct-qtimer.h"
#include "migration/vmstate.h"
#include "qemu/bitops.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "qemu/timer.h"
#include "system/runstate.h"
#include "qapi/error.h"
#include "trace.h"

#define HEX_TIMER_DEBUG 0
#define HEX_TIMER_LOG(...) \
    do { \
        if (HEX_TIMER_DEBUG) { \
            rcu_read_lock(); \
            fprintf(stderr, __VA_ARGS__); \
            rcu_read_unlock(); \
        } \
    } while (0)

/* Common timer implementation.  */

#define QTIMER_MEM_SIZE_BYTES 0x1000
#define QTIMER_MEM_REGION_SIZE_BYTES 0x1000
#define QTIMER_DEFAULT_FREQ_HZ   19200000ULL
#define QTMR_TIMER_INDEX_MASK (0xf000)

/*
 * QTimer version reg:
 *
 *    3                   2                   1
 *  1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * | Major |         Minor         |           Step                |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
static unsigned int TIMER_VERSION = 0x20020000;

/* Return current counter value derived from QEMU_CLOCK_VIRTUAL. */
static uint64_t hex_timer_now(QCTHextimerState *s)
{
    int64_t now = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
    if (now <= s->offset_ns) {
        return 0;
    }
    uint32_t scaler = MAX(s->qtimer->freq_scale, 1u);
    uint64_t scaled_elapsed = (uint64_t)(now - s->offset_ns) / scaler;
    return muldiv64(scaled_elapsed, s->freq, NANOSECONDS_PER_SECOND);
}

/* Arm (or disarm) the one-shot deadline timer. */
static void hex_timer_rearm(QCTHextimerState *s)
{
    int64_t deadline_ns;
    if (!(s->control & QCT_QTIMER_CNTP_CTL_ENABLE)) {
        timer_del(s->timer);
        return;
    }
    /*
     * muldiv64 truncates, so the computed deadline can be up to 1 ns
     * earlier than the true cntval crossing. Add 1 ns so hex_timer_now()
     * is guaranteed to be >= cntval when the QEMUTimer fires; without
     * this, hex_timer_tick would re-arm at the same deadline and loop.
     */
    uint32_t scaler = MAX(s->qtimer->freq_scale, 1u);
    uint64_t base_ns = muldiv64(s->cntval, NANOSECONDS_PER_SECOND, s->freq);
    if (base_ns > ((uint64_t)INT64_MAX - (uint64_t)s->offset_ns - 1) / scaler) {
        timer_del(s->timer);
        return;
    }
    deadline_ns = s->offset_ns + (int64_t)(base_ns * scaler) + 1;
    timer_mod(s->timer, deadline_ns);
}

/* qct_qtimer_read/write:
 * if offset < 0x1000 read restricted registers:
 * QCT_QTIMER_AC_CNTFREQ/CNTSR/CNTTID/CNTACR/CNTOFF_(LO/HI)/QCT_QTIMER_VERSION
 */
static uint64_t qct_qtimer_read(void *opaque, hwaddr offset,
                           unsigned size)
{
    QCTQtimerState *s = (QCTQtimerState *)opaque;
    uint32_t frame = 0;

    switch (offset) {
    case QCT_QTIMER_AC_CNTFRQ:
       return s->freq;
    case QCT_QTIMER_AC_CNTSR:
       return s->secure;
    case QCT_QTIMER_AC_CNTTID_0:
       return s->cnttid_0;
    case QCT_QTIMER_AC_CNTTID_1:
       return s->cnttid_1;
    case QCT_QTIMER_AC_CNTACR_START ... QCT_QTIMER_AC_CNTACR_END:
        frame = (offset - 0x40) / 0x4;
        if (frame >= s->nr_frames) {
            qemu_log_mask(LOG_GUEST_ERROR,
                        "%s: QCT_QTIMER_AC_CNT: Bad offset %x\n",
                        __func__, (int) offset);
            return 0x0;
        }
        return s->timer[frame].cnt_ctrl;
    case QCT_QTIMER_VERSION:
        return TIMER_VERSION;
    default:
            qemu_log_mask(LOG_GUEST_ERROR,
                          "%s: QCT_QTIMER_AC_CNT: Bad offset %x\n",
                          __func__, (int) offset);
        return 0x0;
    }

    qemu_log_mask(LOG_GUEST_ERROR,
                  "%s: Bad offset 0x%x\n", __func__, (int)offset);
    return 0;
}

static void qct_qtimer_write(void *opaque, hwaddr offset,
                        uint64_t value, unsigned size)
{
    QCTQtimerState *s = (QCTQtimerState *)opaque;
    uint32_t frame = 0;

    if (offset < 0x1000) {
        switch (offset) {
        case QCT_QTIMER_AC_CNTFRQ:
            s->freq = value;
            return;
        case QCT_QTIMER_AC_CNTSR:
            if (value > 0xFF)
                qemu_log_mask(LOG_GUEST_ERROR, "%s: QCT_QTIMER_AC_CNTSR: Bad value %x\n",
                              __func__, (int) value);
            else
                s->secure = value;
            return;
        case QCT_QTIMER_AC_CNTACR_START ... QCT_QTIMER_AC_CNTACR_END:
            frame = (offset - QCT_QTIMER_AC_CNTACR_START) / 0x4;
            if (frame >= s->nr_frames) {
                qemu_log_mask(LOG_GUEST_ERROR,
                            "%s: QCT_QTIMER_AC_CNT: Bad offset %x\n",
                            __func__, (int) offset);
                return;
            }
            s->timer[frame].cnt_ctrl = value;
            return;
        default:
            qemu_log_mask(LOG_GUEST_ERROR, "%s: QCT_QTIMER_AC_CNT: Bad offset %x\n",
                          __func__, (int) offset);
            return;
        }
    }
    else
        qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad offset %x\n", __func__, (int)offset);
}

static const MemoryRegionOps qct_qtimer_ops = {
    .read = qct_qtimer_read,
    .write = qct_qtimer_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
        .unaligned = false,
    },
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static const VMStateDescription vmstate_qct_qtimer = {
    .name = "qct-qtimer",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_END_OF_LIST()
    }
};

static void qct_qtimer_init(Object *obj)
{
    QCTQtimerState *s = QCT_QTIMER(obj);

    object_property_add_uint32_ptr(obj, "secure", &s->secure, OBJ_PROP_FLAG_READ);
    object_property_add_uint32_ptr(obj, "frame_id", &s->frame_id, OBJ_PROP_FLAG_READ);
}

static void hex_timer_update(QCTHextimerState *s)
{
    int level = s->int_level
                && (s->control & QCT_QTIMER_CNTP_CTL_ENABLE)
                && !(s->control & QCT_QTIMER_CNTP_CTL_INTEN);
    trace_qtimer_interrupt();

    qemu_set_irq(s->irq, level);
}

static MemTxResult hex_timer_read(void *opaque,
                                hwaddr offset,
                                uint64_t *data,
                                unsigned size,
                                MemTxAttrs attrs)
{
    QCTQtimerState *qct_s = (QCTQtimerState*)opaque;
    uint32_t stride = qct_s->frame_stride;
    uint32_t stride_shift = ctz32(stride);
    uint32_t slot_nr = (offset & (0xF * stride)) >> stride_shift;
    uint32_t reg_offset = offset & (stride - 1);
    uint32_t view = slot_nr % qct_s->nr_views;
    uint32_t frame = slot_nr / qct_s->nr_views;

    if (frame >= qct_s->nr_frames) {
        *data = 0;
        return MEMTX_ACCESS_ERROR;
    }
    QCTHextimerState *s = &qct_s->timer[frame];

    trace_qtimer_read(offset);

    // This is the case where we have 2 views, but the second one is not implemented
    {
        uint32_t cnttid = (frame < 8) ? qct_s->cnttid_0 : qct_s->cnttid_1;
        uint32_t frame_idx = (frame < 8) ? frame : frame - 8;
        if (view && !(cnttid & (0x4 << (frame_idx * 4)))) {
            *data = 0;
            return MEMTX_OK;
        }
    }

    switch (reg_offset) {
        case (QCT_QTIMER_CNT_FREQ): /* Ticks/Second */
            if(!(s->cnt_ctrl & QCT_QTIMER_AC_CNTACR_RFRQ)) {
                return MEMTX_ACCESS_ERROR;
            }

            if (view && !((s->cntpl0acr & QCT_QTIMER_CNTPL0ACR_PL0PCTEN)
                   || (s->cntpl0acr & QCT_QTIMER_CNTPL0ACR_PL0VCTEN))) {
                return MEMTX_ACCESS_ERROR;
            }

            *data = s->freq;
            return MEMTX_OK;
        case (QCT_QTIMER_CNTP_CVAL_LO): /* TimerLoad */
            if(!(s->cnt_ctrl & QCT_QTIMER_AC_CNTACR_RWPT)) {
                return MEMTX_ACCESS_ERROR;
            }

            if (view && !(s->cntpl0acr & QCT_QTIMER_CNTPL0ACR_PL0CTEN)) {
                return MEMTX_ACCESS_ERROR;
            }

            *data = extract64(s->cntval, 0, 32);
            return MEMTX_OK;
        case (QCT_QTIMER_CNTP_CVAL_HI): /* TimerLoad */
            if(!(s->cnt_ctrl & QCT_QTIMER_AC_CNTACR_RWPT)) {
                return MEMTX_ACCESS_ERROR;
            }

            if (view && !(s->cntpl0acr & QCT_QTIMER_CNTPL0ACR_PL0CTEN)) {
                return MEMTX_ACCESS_ERROR;
            }

            *data = extract64(s->cntval, 32, 32);
            return MEMTX_OK;
        case QCT_QTIMER_CNTPCT_LO:
            if(!(s->cnt_ctrl & QCT_QTIMER_AC_CNTACR_RPCT)) {
                return MEMTX_ACCESS_ERROR;
            }

            if (view && !(s->cntpl0acr & QCT_QTIMER_CNTPL0ACR_PL0PCTEN)) {
                return MEMTX_ACCESS_ERROR;
            }

            *data = extract64(hex_timer_now(s), 0, 32);
            return MEMTX_OK;
        case QCT_QTIMER_CNTPCT_HI:
            if(!(s->cnt_ctrl & QCT_QTIMER_AC_CNTACR_RPCT)) {
                return MEMTX_ACCESS_ERROR;
            }

            if (view && !(s->cntpl0acr & QCT_QTIMER_CNTPL0ACR_PL0PCTEN)) {
                return MEMTX_ACCESS_ERROR;
            }

            *data = extract64(hex_timer_now(s), 32, 32);
            return MEMTX_OK;
        case (QCT_QTIMER_CNTP_TVAL): /* CVAL - CNTP */
            if(!(s->cnt_ctrl & QCT_QTIMER_AC_CNTACR_RWPT)) {
                return MEMTX_ACCESS_ERROR;
            }

            if (view && !(s->cntpl0acr & QCT_QTIMER_CNTPL0ACR_PL0CTEN)) {
                return MEMTX_ACCESS_ERROR;
            }

            *data = (uint32_t)(int32_t)(int64_t)(s->cntval - hex_timer_now(s));
            return MEMTX_OK;
        case (QCT_QTIMER_CNTP_CTL): /* TimerMIS */
            if(!(s->cnt_ctrl & QCT_QTIMER_AC_CNTACR_RWPT)) {
                return MEMTX_ACCESS_ERROR;
            }

            if (view && !(s->cntpl0acr & QCT_QTIMER_CNTPL0ACR_PL0CTEN)) {
                return MEMTX_ACCESS_ERROR;
            }
            /*
             * The register CTNP_CTL has 3 bits [0:2]
             * Bit 2: ISTAT (Interrupt is pending or not)
             * Bit 1: IMSK (Mask Interrupt or UnMask)
             * Bit 0: EN (Enable or Disable the timer)
             */
            *data = s->control | ((s->int_level & 0x1) << 2);
            return MEMTX_OK;
        case QCT_QTIMER_CNTPL0ACR:
            if (view) {
                *data = 0;
            } else {
                *data = s->cntpl0acr;
            }
            return MEMTX_OK;

        case QCT_QTIMER_VERSION:
            *data = TIMER_VERSION;
            return MEMTX_OK;

        default:
            qemu_log_mask(LOG_GUEST_ERROR,
                          "%s: Bad offset %x\n", __func__, (int)offset);
            *data = 0;
            return MEMTX_ACCESS_ERROR;
    }
}

static MemTxResult hex_timer_write(void *opaque,
                                    hwaddr offset,
                                    uint64_t value,
                                    unsigned size,
                                    MemTxAttrs attrs)
{
    QCTQtimerState *qct_s = (QCTQtimerState*)opaque;
    uint32_t stride = qct_s->frame_stride;
    uint32_t stride_shift = ctz32(stride);
    uint32_t slot_nr = (offset & (0xF * stride)) >> stride_shift;
    uint32_t reg_offset = offset & (stride - 1);
    uint32_t view = slot_nr % qct_s->nr_views;
    uint32_t frame = slot_nr / qct_s->nr_views;

    if (frame >= qct_s->nr_frames) {
        return MEMTX_ACCESS_ERROR;
    }
    QCTHextimerState *s = &qct_s->timer[frame];

    trace_qtimer_write(offset, value);

    // This is the case where we have 2 views, but the second one is not implemented
    {
        uint32_t cnttid = (frame < 8) ? qct_s->cnttid_0 : qct_s->cnttid_1;
        uint32_t frame_idx = (frame < 8) ? frame : frame - 8;
        if (view && !(cnttid & (0x4 << (frame_idx * 4)))) {
            return MEMTX_OK;
        }
    }

    switch (reg_offset) {
        case (QCT_QTIMER_CNTP_CVAL_LO): /* TimerLoad */
            HEX_TIMER_LOG("CVAL_LO write: 0x%" PRIx64 "\n", value);

            if(!(s->cnt_ctrl & QCT_QTIMER_AC_CNTACR_RWPT)) {
                return MEMTX_ACCESS_ERROR;
            }

            if (view && !(s->cntpl0acr & QCT_QTIMER_CNTPL0ACR_PL0CTEN)) {
                return MEMTX_ACCESS_ERROR;
            }

            s->int_level = 0;
            s->cntval = deposit64(s->cntval, 0, 32, value);
            hex_timer_rearm(s);
            break;
        case (QCT_QTIMER_CNTP_CVAL_HI):
            if(!(s->cnt_ctrl & QCT_QTIMER_AC_CNTACR_RWPT)) {
                return MEMTX_ACCESS_ERROR;
            }

            if (view && !(s->cntpl0acr & QCT_QTIMER_CNTPL0ACR_PL0CTEN)) {
                return MEMTX_ACCESS_ERROR;
            }

            s->int_level = 0;
            s->cntval = deposit64(s->cntval, 32, 32, value);
            hex_timer_rearm(s);
            break;
        case (QCT_QTIMER_CNTP_CTL): /* Timer control register */
            HEX_TIMER_LOG("\tctl write: %" PRIu64 "\n", value);

            if(!(s->cnt_ctrl & QCT_QTIMER_AC_CNTACR_RWPT)) {
                return MEMTX_ACCESS_ERROR;
            }

            if (view && !(s->cntpl0acr & QCT_QTIMER_CNTPL0ACR_PL0CTEN)) {
                return MEMTX_ACCESS_ERROR;
            }

            /*
             * ISTAT (bit 2) is read-only; keep SW writes from polluting it.
             */
            s->control = value & ~QCT_QTIMER_CNTP_CTL_ISTAT;
            hex_timer_rearm(s);
            break;
        case (QCT_QTIMER_CNTP_TVAL): /* CVAL - CNTP */
            if(!(s->cnt_ctrl & QCT_QTIMER_AC_CNTACR_RWPT)) {
                return MEMTX_ACCESS_ERROR;
            }

            if (view && !(s->cntpl0acr & QCT_QTIMER_CNTPL0ACR_PL0CTEN)) {
                return MEMTX_ACCESS_ERROR;
            }

            /* TVAL write: CVAL = CNTPCT + TVAL (TVAL is signed 32-bit) */
            s->int_level = 0;
            s->cntval = hex_timer_now(s) + (int64_t)(int32_t)value;
            hex_timer_rearm(s);
            break;
        case QCT_QTIMER_CNTPL0ACR:
            if (view) {
                break;
            }

            s->cntpl0acr = value;
            break;
        default:
            qemu_log_mask(LOG_GUEST_ERROR,
                          "%s: Bad offset %x\n", __func__, (int)offset);
            return MEMTX_ACCESS_ERROR;
    }
    hex_timer_update(s);
    return MEMTX_OK;

}

static void hex_timer_tick(void *opaque)
{
    QCTHextimerState *s = (QCTHextimerState *)opaque;
    HEX_TIMER_LOG("\nFIRE!!! cntval=%" PRId64 " now=%" PRId64 "\n",
                  s->cntval, hex_timer_now(s));
    if (hex_timer_now(s) >= s->cntval) {
        s->int_level = 1;
        hex_timer_update(s);
    } else {
        hex_timer_rearm(s);
    }
}

static const MemoryRegionOps hex_timer_ops = {
        .read_with_attrs = hex_timer_read,
        .write_with_attrs = hex_timer_write,
        .endianness = DEVICE_LITTLE_ENDIAN,
        .valid = {
            .min_access_size = 4,
            .max_access_size = 4,
            .unaligned = false,
        },
        .impl = {
            .min_access_size = 4,
            .max_access_size = 4,
        },
};

static const VMStateDescription vmstate_hex_timer = {
    .name = "hex_timer",
    .version_id = 2,
    .minimum_version_id = 2,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32(control, QCTHextimerState),
        VMSTATE_UINT32(cnt_ctrl, QCTHextimerState),
        VMSTATE_INT64(offset_ns, QCTHextimerState),
        VMSTATE_UINT64(cntval, QCTHextimerState),
        VMSTATE_UINT32(cntpl0acr, QCTHextimerState),
        VMSTATE_UINT32(int_level, QCTHextimerState),
        VMSTATE_TIMER_PTR(timer, QCTHextimerState),
        VMSTATE_END_OF_LIST()
    }
};

static void qct_qtimer_realize(DeviceState *dev, Error **errp)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);
    QCTQtimerState *s = QCT_QTIMER(dev);
    unsigned int i;

    if (s->nr_frames > QCT_QTIMER_TIMER_FRAME_ELTS) {
        error_setg(errp, "nr_frames too high");
        return;
    }

    if (s->nr_views > QCT_QTIMER_TIMER_VIEW_ELTS) {
        error_setg(errp, "nr_views too high");
        return;
    }

    memory_region_init_io(&s->iomem, OBJECT(sbd), &qct_qtimer_ops, s,
                          "qutimer", QTIMER_MEM_SIZE_BYTES);
    sysbus_init_mmio(sbd, &s->iomem);

    memory_region_init_io(&s->view_iomem, OBJECT(sbd), &hex_timer_ops, s,
                          "qutimer_views", s->frame_stride * s->nr_frames * s->nr_views);
    sysbus_init_mmio(sbd, &s->view_iomem);

    for (i = 0; i < s->nr_frames; i++) {
        QCTHextimerState *t = &s->timer[i];

        t->qtimer = s;
        t->freq = QTIMER_DEFAULT_FREQ_HZ;

        s->secure |= (1 << i);

        sysbus_init_irq(sbd, &t->irq);

        t->timer = timer_new_ns(QEMU_CLOCK_VIRTUAL, hex_timer_tick, t);
        vmstate_register(NULL, VMSTATE_INSTANCE_ID_ANY, &vmstate_hex_timer, t);
    }
}

static void qct_qtimer_reset_hold(Object *obj, ResetType type)
{
    QCTQtimerState *s = QCT_QTIMER(obj);
    unsigned int i;

    for (i = 0; i < s->nr_frames; i++) {
        QCTHextimerState *t = &s->timer[i];

        /*
         * CTL reset value per TRM is 0 (EN=0, IMASK=0, ISTAT=0); SW must
         * explicitly enable the compare/IRQ logic. cntval defaults to the
         * largest 64-bit value so the timer never fires before SW arms it.
         */
        t->control = 0;
        t->cnt_ctrl = (QCT_QTIMER_AC_CNTACR_RWPT | QCT_QTIMER_AC_CNTACR_RWVT |
                       QCT_QTIMER_AC_CNTACR_RVOFF | QCT_QTIMER_AC_CNTACR_RFRQ |
                       QCT_QTIMER_AC_CNTACR_RPVCT | QCT_QTIMER_AC_CNTACR_RPCT);
        t->cntval = UINT64_MAX;
        t->int_level = 0;
        t->offset_ns = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
        timer_del(t->timer);
    }
}

static const Property qct_qtimer_properties[] = {
    DEFINE_PROP_UINT32("freq", QCTQtimerState, freq, QTIMER_DEFAULT_FREQ_HZ),
    DEFINE_PROP_UINT32("freq-scale", QCTQtimerState, freq_scale, 1),
    DEFINE_PROP_UINT32("nr_frames", QCTQtimerState, nr_frames, 2),
    DEFINE_PROP_UINT32("nr_views", QCTQtimerState, nr_views, 1),
    DEFINE_PROP_UINT32("frame_stride", QCTQtimerState, frame_stride, 0x1000),
    DEFINE_PROP_UINT32("cnttid_0", QCTQtimerState, cnttid_0, 0x11),
    DEFINE_PROP_UINT32("cnttid_1", QCTQtimerState, cnttid_1, 0x0),
};

static void qct_qtimer_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *k = DEVICE_CLASS(klass);
    ResettableClass *rc = RESETTABLE_CLASS(klass);

    device_class_set_props(k, qct_qtimer_properties);
    k->realize = qct_qtimer_realize;
    k->vmsd = &vmstate_qct_qtimer;
    rc->phases.hold = qct_qtimer_reset_hold;
}

static const TypeInfo qct_qtimer_info = {
    .name          = TYPE_QCT_QTIMER,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(QCTQtimerState),
    .instance_init = qct_qtimer_init,
    .class_init    = qct_qtimer_class_init,
    .interfaces    = (InterfaceInfo[]) {
        { TYPE_QTIMER_INTERFACE },
        { }
    },
};

/* QTimer Interface Implementation */
static uint32_t qct_qtimer_get_timer_lo(QTimerInterface *obj)
{
    QCTQtimerState *s = QCT_QTIMER(obj);
    if (s->nr_frames > 0) {
        return extract64(hex_timer_now(&s->timer[0]), 0, 32);
    }
    return 0;
}

static uint32_t qct_qtimer_get_timer_hi(QTimerInterface *obj)
{
    QCTQtimerState *s = QCT_QTIMER(obj);
    if (s->nr_frames > 0) {
        return extract64(hex_timer_now(&s->timer[0]), 32, 32);
    }
    return 0;
}

/* Helper functions for external access */
uint32_t qtimer_get_timer_lo(QTimerInterface *qtimer)
{
    QTimerInterfaceClass *klass = QTIMER_INTERFACE_GET_CLASS(qtimer);
    return klass->get_timer_lo(qtimer);
}

uint32_t qtimer_get_timer_hi(QTimerInterface *qtimer)
{
    QTimerInterfaceClass *klass = QTIMER_INTERFACE_GET_CLASS(qtimer);
    return klass->get_timer_hi(qtimer);
}

static void qtimer_interface_class_init(ObjectClass *klass, const void *data)
{
    QTimerInterfaceClass *qic = QTIMER_INTERFACE_CLASS(klass);

    qic->get_timer_lo = qct_qtimer_get_timer_lo;
    qic->get_timer_hi = qct_qtimer_get_timer_hi;
}

static const TypeInfo qtimer_interface_info = {
    .name = TYPE_QTIMER_INTERFACE,
    .parent = TYPE_INTERFACE,
    .class_size = sizeof(QTimerInterfaceClass),
    .class_init = qtimer_interface_class_init,
};

static void qct_qtimer_register_types(void)
{
    type_register_static(&qct_qtimer_info);
    type_register_static(&qtimer_interface_info);
}

type_init(qct_qtimer_register_types)
