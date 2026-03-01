/*
 * Qualcomm QCT QTimer
 *
 * Copyright(c) 2019-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */


#include "qemu/osdep.h"
#include "hw/core/irq.h"
#include "hw/core/qdev-properties.h"
#include "hw/timer/qct-qtimer.h"
#include "migration/vmstate.h"
#include "qapi/error.h"
#include "qemu/bitops.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "qemu/timer.h"

/* Common timer implementation.  */

#define QTIMER_MEM_SIZE_BYTES 0x1000
#define QTIMER_MEM_REGION_SIZE_BYTES 0x1000
#define QTIMER_DEFAULT_FREQ_HZ 19200000ULL
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

/*
 * qct_qtimer_read/write:
 * if offset < 0x1000 read restricted registers:
 * QCT_QTIMER_AC_CNTFREQ/CNTSR/CNTTID/CNTACR/CNTOFF_(LO/HI)/QCT_QTIMER_VERSION
 */
static uint64_t qct_qtimer_read(void *opaque, hwaddr offset, unsigned size)
{
    QCTQtimerState *s = (QCTQtimerState *)opaque;
    uint32_t frame = 0;

    switch (offset) {
    case QCT_QTIMER_AC_CNTFRQ:
        return s->freq;
    case QCT_QTIMER_AC_CNTSR:
        return s->secure;
    case QCT_QTIMER_AC_CNTTID:
        return s->cnttid;
    case QCT_QTIMER_AC_CNTACR_START ... QCT_QTIMER_AC_CNTACR_END:
        frame = (offset - 0x40) / 0x4;
        if (frame >= s->nr_frames) {
            qemu_log_mask(LOG_GUEST_ERROR,
                          "%s: QCT_QTIMER_AC_CNT: Bad offset %x\n", __func__,
                          (int)offset);
            return 0x0;
        }
        return s->timer[frame].cnt_ctrl;
    case QCT_QTIMER_VERSION:
        return TIMER_VERSION;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: QCT_QTIMER_AC_CNT: Bad offset %" PRIx32 "\n",
                      __func__, (int)offset);
        return 0x0;
    }

    qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad offset 0x%" PRIx32 "\n", __func__,
                  (int)offset);
    return 0;
}

static void qct_qtimer_write(void *opaque, hwaddr offset, uint64_t value,
                             unsigned size)
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
                qemu_log_mask(LOG_GUEST_ERROR,
                              "%s: QCT_QTIMER_AC_CNTSR: Bad value %x\n",
                              __func__, (int)value);
            else
                s->secure = value;
            return;
        case QCT_QTIMER_AC_CNTACR_START ... QCT_QTIMER_AC_CNTACR_END:
            frame = (offset - QCT_QTIMER_AC_CNTACR_START) / 0x4;
            if (frame >= s->nr_frames) {
                qemu_log_mask(LOG_GUEST_ERROR,
                              "%s: QCT_QTIMER_AC_CNT: Bad offset %x\n",
                              __func__, (int)offset);
                return;
            }
            s->timer[frame].cnt_ctrl = value;
            return;
        default:
            qemu_log_mask(LOG_GUEST_ERROR,
                          "%s: QCT_QTIMER_AC_CNT: Bad offset %x\n", __func__,
                          (int)offset);
            return;
        }
    } else
        qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad offset %" PRIx32 "\n", __func__,
                      (int)offset);
}

static const MemoryRegionOps qct_qtimer_ops = {
    .read = qct_qtimer_read,
    .write = qct_qtimer_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static const VMStateDescription vmstate_qct_qtimer = {
    .name = "qct-qtimer",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]){ VMSTATE_END_OF_LIST() }
};

static void qct_qtimer_init(Object *obj)
{
    QCTQtimerState *s = QCT_QTIMER(obj);

    object_property_add_uint32_ptr(obj, "secure", &s->secure,
                                   OBJ_PROP_FLAG_READ);
    object_property_add_uint32_ptr(obj, "frame_id", &s->frame_id,
                                   OBJ_PROP_FLAG_READ);
}

/*
 * Derive the physical counter value from QEMU's virtual clock.
 * This gives a monotonically increasing counter at the timer frequency.
 */
static uint64_t get_cntpct(QCTHextimerState *s)
{
    return muldiv64(qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL),
                    s->freq, NANOSECONDS_PER_SECOND);
}

static void hex_timer_update(QCTHextimerState *s)
{
    /* Update interrupts.  */
    int level = s->int_level && (s->control & QCT_QTIMER_CNTP_CTL_ENABLE);
    qemu_set_irq(s->irq, level);
}

/*
 * Recompute timer alarm after CVAL or CTL changes.
 * Schedules a one-shot alarm for when cntpct reaches cntval.
 */
static void hex_timer_recompute(QCTHextimerState *s)
{
    uint64_t now, diff;
    int64_t now_ns, target_ns;

    timer_del(&s->alarm);

    if (!(s->control & QCT_QTIMER_CNTP_CTL_ENABLE)) {
        s->int_level = 0;
        hex_timer_update(s);
        return;
    }

    /*
     * Always deassert the interrupt and schedule an alarm rather than
     * firing immediately.  This ensures the L2VIC sees a proper 0→1
     * edge transition when the alarm fires, even if the compare value
     * is already in the past.
     */
    s->int_level = 0;
    hex_timer_update(s);

    now_ns = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
    now = get_cntpct(s);

    if (now >= s->cntval) {
        /*
         * Compare value already in the past — schedule alarm to fire
         * as soon as possible (next main loop iteration).
         */
        timer_mod(&s->alarm, now_ns + 1);
        return;
    }

    /* Schedule alarm for when counter reaches compare value */
    diff = s->cntval - now;
    target_ns = now_ns + muldiv64(diff, NANOSECONDS_PER_SECOND, s->freq);

    if (target_ns <= now_ns) {
        /* Overflow — set as far in the future as possible */
        timer_mod(&s->alarm, INT64_MAX);
    } else {
        timer_mod(&s->alarm, target_ns);
    }
}

/* Timer alarm callback — fires when cntpct reaches cntval */
static void hex_timer_alarm(void *opaque)
{
    QCTHextimerState *s = (QCTHextimerState *)opaque;

    s->int_level = 1;
    hex_timer_update(s);
}

static MemTxResult hex_timer_read(void *opaque, hwaddr offset, uint64_t *data,
                                  unsigned size, MemTxAttrs attrs)
{
    QCTQtimerState *qct_s = (QCTQtimerState *)opaque;
    uint32_t slot_nr = (offset & 0xF000) >> 12;
    uint32_t reg_offset = offset & 0xFFF;
    uint32_t view = slot_nr % qct_s->nr_views;
    uint32_t frame = slot_nr / qct_s->nr_views;

    if (frame >= qct_s->nr_frames) {
        *data = 0;
        return MEMTX_ACCESS_ERROR;
    }
    QCTHextimerState *s = &qct_s->timer[frame];

    /*
     * This is the case where we have 2 views, but the second one is not
     * implemented.
     */
    if (view && !(qct_s->cnttid & (0x4 << (frame * 4)))) {
        *data = 0;
        return MEMTX_OK;
    }

    switch (reg_offset) {
    case QCT_QTIMER_CNT_FREQ: /* Ticks/Second */
        if (!(s->cnt_ctrl & QCT_QTIMER_AC_CNTACR_RFRQ)) {
            return MEMTX_ACCESS_ERROR;
        }

        if (view && !((s->cntpl0acr & QCT_QTIMER_CNTPL0ACR_PL0PCTEN) ||
                      (s->cntpl0acr & QCT_QTIMER_CNTPL0ACR_PL0VCTEN))) {
            return MEMTX_ACCESS_ERROR;
        }

        *data = s->freq;
        return MEMTX_OK;
    case QCT_QTIMER_CNTP_CVAL_LO:
        if (!(s->cnt_ctrl & QCT_QTIMER_AC_CNTACR_RWPT)) {
            return MEMTX_ACCESS_ERROR;
        }

        if (view && !(s->cntpl0acr & QCT_QTIMER_CNTPL0ACR_PL0CTEN)) {
            return MEMTX_ACCESS_ERROR;
        }

        *data = extract64(s->cntval, 0, 32);
        return MEMTX_OK;
    case QCT_QTIMER_CNTP_CVAL_HI:
        if (!(s->cnt_ctrl & QCT_QTIMER_AC_CNTACR_RWPT)) {
            return MEMTX_ACCESS_ERROR;
        }

        if (view && !(s->cntpl0acr & QCT_QTIMER_CNTPL0ACR_PL0CTEN)) {
            return MEMTX_ACCESS_ERROR;
        }

        *data = extract64(s->cntval, 32, 32);
        return MEMTX_OK;
    case QCT_QTIMER_CNTPCT_LO:
        if (!(s->cnt_ctrl & QCT_QTIMER_AC_CNTACR_RPCT)) {
            return MEMTX_ACCESS_ERROR;
        }

        if (view && !(s->cntpl0acr & QCT_QTIMER_CNTPL0ACR_PL0PCTEN)) {
            return MEMTX_ACCESS_ERROR;
        }

        *data = extract64(get_cntpct(s), 0, 32);
        return MEMTX_OK;
    case QCT_QTIMER_CNTPCT_HI:
        if (!(s->cnt_ctrl & QCT_QTIMER_AC_CNTACR_RPCT)) {
            return MEMTX_ACCESS_ERROR;
        }

        if (view && !(s->cntpl0acr & QCT_QTIMER_CNTPL0ACR_PL0PCTEN)) {
            return MEMTX_ACCESS_ERROR;
        }

        *data = extract64(get_cntpct(s), 32, 32);
        return MEMTX_OK;
    case QCT_QTIMER_CNTP_TVAL:
        if (!(s->cnt_ctrl & QCT_QTIMER_AC_CNTACR_RWPT)) {
            return MEMTX_ACCESS_ERROR;
        }

        if (view && !(s->cntpl0acr & QCT_QTIMER_CNTPL0ACR_PL0CTEN)) {
            return MEMTX_ACCESS_ERROR;
        }

        *data = (uint32_t)(s->cntval - get_cntpct(s));
        return MEMTX_OK;
    case QCT_QTIMER_CNTP_CTL:
        if (!(s->cnt_ctrl & QCT_QTIMER_AC_CNTACR_RWPT)) {
            return MEMTX_ACCESS_ERROR;
        }

        if (view && !(s->cntpl0acr & QCT_QTIMER_CNTPL0ACR_PL0CTEN)) {
            return MEMTX_ACCESS_ERROR;
        }

        {
            uint32_t ctl = s->control & 0x3;
            if (get_cntpct(s) >= s->cntval) {
                ctl |= QCT_QTIMER_CNTP_CTL_ISTAT;
            }
            *data = ctl;
        }
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
        qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad offset %" PRIx32 "\n", __func__,
                      (int)offset);
        *data = 0;
        return MEMTX_ACCESS_ERROR;
    }
}

static MemTxResult hex_timer_write(void *opaque, hwaddr offset, uint64_t value,
                                   unsigned size, MemTxAttrs attrs)
{
    QCTQtimerState *qct_s = (QCTQtimerState *)opaque;
    uint32_t slot_nr = (offset & 0xF000) >> 12;
    uint32_t reg_offset = offset & 0xFFF;
    uint32_t view = slot_nr % qct_s->nr_views;
    uint32_t frame = slot_nr / qct_s->nr_views;

    if (frame >= qct_s->nr_frames) {
        return MEMTX_ACCESS_ERROR;
    }
    QCTHextimerState *s = &qct_s->timer[frame];

    /*
     * This is the case where we have 2 views, but the second one is not
     * implemented.
     */
    if (view && !(qct_s->cnttid & (0x4 << (frame * 4)))) {
        return MEMTX_OK;
    }

    switch (reg_offset) {
    case QCT_QTIMER_CNTP_CVAL_LO:
        if (!(s->cnt_ctrl & QCT_QTIMER_AC_CNTACR_RWPT)) {
            return MEMTX_ACCESS_ERROR;
        }

        if (view && !(s->cntpl0acr & QCT_QTIMER_CNTPL0ACR_PL0CTEN)) {
            return MEMTX_ACCESS_ERROR;
        }

        s->cntval = deposit64(s->cntval, 0, 32, value);
        s->int_level = 0;
        hex_timer_recompute(s);
        break;
    case QCT_QTIMER_CNTP_CVAL_HI:
        if (!(s->cnt_ctrl & QCT_QTIMER_AC_CNTACR_RWPT)) {
            return MEMTX_ACCESS_ERROR;
        }

        if (view && !(s->cntpl0acr & QCT_QTIMER_CNTPL0ACR_PL0CTEN)) {
            return MEMTX_ACCESS_ERROR;
        }

        s->cntval = deposit64(s->cntval, 32, 32, value);
        hex_timer_recompute(s);
        break;
    case QCT_QTIMER_CNTP_CTL:
        if (!(s->cnt_ctrl & QCT_QTIMER_AC_CNTACR_RWPT)) {
            return MEMTX_ACCESS_ERROR;
        }

        if (view && !(s->cntpl0acr & QCT_QTIMER_CNTPL0ACR_PL0CTEN)) {
            return MEMTX_ACCESS_ERROR;
        }

        s->control = value;
        hex_timer_recompute(s);
        break;
    case QCT_QTIMER_CNTP_TVAL:
        if (!(s->cnt_ctrl & QCT_QTIMER_AC_CNTACR_RWPT)) {
            return MEMTX_ACCESS_ERROR;
        }

        if (view && !(s->cntpl0acr & QCT_QTIMER_CNTPL0ACR_PL0CTEN)) {
            return MEMTX_ACCESS_ERROR;
        }

        s->cntval = get_cntpct(s) + (int64_t)(int32_t)value;
        s->int_level = 0;
        hex_timer_recompute(s);
        break;
    case QCT_QTIMER_CNTPL0ACR:
        if (view) {
            break;
        }

        s->cntpl0acr = value;
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad offset %" PRIx32 "\n", __func__,
                      (int)offset);
        return MEMTX_ACCESS_ERROR;
    }
    hex_timer_update(s);
    return MEMTX_OK;
}

static const MemoryRegionOps hex_timer_ops = {
    .read_with_attrs = hex_timer_read,
    .write_with_attrs = hex_timer_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static const VMStateDescription vmstate_hex_timer = {
    .name = "hex_timer",
    .version_id = 2,
    .minimum_version_id = 2,
    .fields = (VMStateField[]){ VMSTATE_UINT32(control, QCTHextimerState),
                                VMSTATE_UINT32(cnt_ctrl, QCTHextimerState),
                                VMSTATE_UINT64(cntval, QCTHextimerState),
                                VMSTATE_UINT32(int_level, QCTHextimerState),
                                VMSTATE_TIMER(alarm, QCTHextimerState),
                                VMSTATE_END_OF_LIST() }
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

    memory_region_init_io(&s->iomem, OBJECT(sbd), &qct_qtimer_ops, s, "qutimer",
                          QTIMER_MEM_SIZE_BYTES);
    sysbus_init_mmio(sbd, &s->iomem);

    memory_region_init_io(&s->view_iomem, OBJECT(sbd), &hex_timer_ops, s,
                          "qutimer_views",
                          QTIMER_MEM_SIZE_BYTES * s->nr_frames * s->nr_views);
    sysbus_init_mmio(sbd, &s->view_iomem);

    for (i = 0; i < s->nr_frames; i++) {
        s->timer[i].control = QCT_QTIMER_CNTP_CTL_ENABLE;
        s->timer[i].cntval = UINT64_MAX;
        s->timer[i].cnt_ctrl =
            (QCT_QTIMER_AC_CNTACR_RWPT | QCT_QTIMER_AC_CNTACR_RWVT |
             QCT_QTIMER_AC_CNTACR_RVOFF | QCT_QTIMER_AC_CNTACR_RFRQ |
             QCT_QTIMER_AC_CNTACR_RPVCT | QCT_QTIMER_AC_CNTACR_RPCT);
        s->timer[i].qtimer = s;
        s->timer[i].freq = QTIMER_DEFAULT_FREQ_HZ;

        s->secure |= (1 << i);

        sysbus_init_irq(sbd, &(s->timer[i].irq));

        timer_init_ns(&s->timer[i].alarm, QEMU_CLOCK_VIRTUAL,
                      hex_timer_alarm, &s->timer[i]);
        vmstate_register(NULL, VMSTATE_INSTANCE_ID_ANY, &vmstate_hex_timer,
                         &s->timer[i]);
    }
}

static const Property qct_qtimer_properties[] = {
    DEFINE_PROP_UINT32("freq", QCTQtimerState, freq, QTIMER_DEFAULT_FREQ_HZ),
    DEFINE_PROP_UINT32("nr_frames", QCTQtimerState, nr_frames, 2),
    DEFINE_PROP_UINT32("nr_views", QCTQtimerState, nr_views, 1),
    DEFINE_PROP_UINT32("cnttid", QCTQtimerState, cnttid, 0x11),
};

/* Forward declarations */
static uint32_t qct_qtimer_get_timer_lo(QTimerInterface *obj);
static uint32_t qct_qtimer_get_timer_hi(QTimerInterface *obj);

static void qct_qtimer_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *k = DEVICE_CLASS(klass);
    QTimerInterfaceClass *qic = QTIMER_INTERFACE_CLASS(klass);

    device_class_set_props(k, qct_qtimer_properties);
    k->realize = qct_qtimer_realize;
    k->vmsd = &vmstate_qct_qtimer;

    /* Implement QTimerInterface methods */
    qic->get_timer_lo = qct_qtimer_get_timer_lo;
    qic->get_timer_hi = qct_qtimer_get_timer_hi;
}

static const TypeInfo qct_qtimer_info = {
    .name = TYPE_QCT_QTIMER,
    .parent = TYPE_SYS_BUS_DEVICE,
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
    /* Use frame 0 for timer access */
    if (s->nr_frames > 0) {
        uint64_t count = get_cntpct(&s->timer[0]);
        return extract64(count, 0, 32);
    }
    return 0;
}

static uint32_t qct_qtimer_get_timer_hi(QTimerInterface *obj)
{
    QCTQtimerState *s = QCT_QTIMER(obj);
    /* Use frame 0 for timer access */
    if (s->nr_frames > 0) {
        uint64_t count = get_cntpct(&s->timer[0]);
        return extract64(count, 32, 32);
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
