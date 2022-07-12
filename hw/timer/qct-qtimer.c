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
#include "hw/irq.h"
#include "hw/qdev-properties.h"
#include "hw/timer/qct-qtimer.h"
#include "migration/vmstate.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "qemu/timer.h"
#include "sysemu/runstate.h"
#include "qapi/error.h"

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
#define HIGH_32(val) (0x0ffffffffULL & (val >> 32))
#define LOW_32(val) (0x0ffffffffULL & val)

/* Merge the IRQs from the component devices.  */
static void qutimer_set_irq(QCTQtimerState *s, int irq, int level)
{
    s->level[irq] = level;
    /*
     * FIXME: Do we really want to do this ?
     * s->timer[0].int_level = level;
     * s->timer[1].int_level = level;
     * s->timer[2].int_level = level;
     */
    qemu_set_irq(s->irq, s->level[0] || s->level[1]);
}

/* qct_qtimer_read/write:
 * if offset < 0x1000 read restricted registers:
 * QCT_QTIMER_AC_CNTFREQ/CNTSR/CNTTID/CNTACR/CNTOFF_(LO/HI)/QCT_QTIMER_VERSION
 */
static uint64_t qct_qtimer_read(void *opaque, hwaddr offset,
                           unsigned size)
{
    QCTQtimerState *s = (QCTQtimerState *)opaque;

    switch (offset) {
    case QCT_QTIMER_AC_CNTFRQ:
       return s->freq;
    case QCT_QTIMER_AC_CNTSR:
       return s->secure;
    case QCT_QTIMER_AC_CNTTID:
       return 0x11; /* Only frame 0 and frame 1 are implemented. */
    case QCT_QTIMER_AC_CNTACR_0:
        return s->timer[0].cnt_ctrl;
    case QCT_QTIMER_AC_CNTACR_1:
        return s->timer[1].cnt_ctrl;
    case QCT_QTIMER_AC_CNTACR_2:
        return s->timer[2].cnt_ctrl;
    case QCT_QTIMER_VERSION:
        return 0x10000000;
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

    if (offset < 0x1000) {
        switch (offset) {
        case QCT_QTIMER_AC_CNTFRQ:
            s->freq = value;
            return;
        case QCT_QTIMER_AC_CNTSR:
            if (value > 3)
                qemu_log_mask(LOG_GUEST_ERROR, "%s: QCT_QTIMER_AC_CNTSR: Bad value %x\n",
                              __func__, (int) value);
            else
                s->secure = value;
            return;
        case QCT_QTIMER_AC_CNTACR_0:
            s->timer[0].cnt_ctrl = value;
            return;
        case QCT_QTIMER_AC_CNTACR_1:
            s->timer[1].cnt_ctrl = value;
            return;
        case QCT_QTIMER_AC_CNTACR_2:
            s->timer[2].cnt_ctrl = value;
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
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static const VMStateDescription vmstate_qct_qtimer = {
    .name = "qct-qtimer",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_INT32_ARRAY(level, QCTQtimerState, QCT_QTIMER_TIMER_ELTS),
        VMSTATE_END_OF_LIST()
    }
};

static void qct_qtimer_init(Object *obj)
{
    QCTQtimerState *s = QCT_QTIMER(obj);

    object_property_add_uint32_ptr(obj, "secure", &s->secure, OBJ_PROP_FLAG_READ);
    object_property_add_uint32_ptr(obj, "frame_id", &s->frame_id, OBJ_PROP_FLAG_READ);
    s->secure = QCT_QTIMER_AC_CNTSR_NSN_1 | QCT_QTIMER_AC_CNTSR_NSN_2
        | QCT_QTIMER_AC_CNTSR_NSN_3;
}

static void hex_timer_update(QCTHextimerState *s)
{
    /* Update interrupts.  */
    int level = s->int_level && (s->control & QCT_QTIMER_CNTP_CTL_ENABLE);
    if (level) qemu_irq_pulse(s->irq);
    else qemu_set_irq(s->irq, level);
}

static uint64_t hex_timer_read(void *opaque, hwaddr offset, unsigned size)
{
    QCTQtimerState *qct_s = (QCTQtimerState*)opaque;
    uint32_t slot_nr = (offset & 0xF000) >> 12;
    uint32_t frame = slot_nr / qct_s->nr_views;
    QCTHextimerState *s = &qct_s->timer[frame];
    uint32_t reg_offset = offset & 0xFFF;

    switch (reg_offset) {
        case (QCT_QTIMER_CNT_FREQ): /* Ticks/Second */
            return s->freq;
        case (QCT_QTIMER_CNTP_CVAL_LO): /* TimerLoad */
            return LOW_32((s->cntval));
        case (QCT_QTIMER_CNTP_CVAL_HI): /* TimerLoad */
            return HIGH_32((s->cntval));
        case QCT_QTIMER_CNTPCT_LO:
            return LOW_32((s->cntpct + (ptimer_get_count(s->timer))));
        case QCT_QTIMER_CNTPCT_HI:
            return HIGH_32((s->cntpct + (ptimer_get_count(s->timer))));
        case (QCT_QTIMER_CNTP_TVAL): /* CVAL - CNTP */
            return (s->cntval -
                    (HIGH_32((s->cntpct + (ptimer_get_count(s->timer)))) +
                     LOW_32((s->cntpct + (ptimer_get_count(s->timer))))));

        case (QCT_QTIMER_CNTP_CTL): /* TimerMIS */
            return s->int_level;
        default:
            qemu_log_mask(LOG_GUEST_ERROR,
                          "%s: Bad offset %x\n", __func__, (int)offset);
            return 0;
    }
}

/*
 * Reset the timer limit after settings have changed.
 * May only be called from inside a ptimer transaction block.
 */
static void hex_timer_recalibrate(QCTHextimerState *s, int reload)
{
    uint64_t limit;
    /* Periodic.  */
    limit = s->limit;
    ptimer_set_limit(s->timer, limit, reload);
}

static void hex_timer_write(void *opaque, hwaddr offset,
                            uint64_t value, unsigned size)
{
    QCTQtimerState *qct_s = (QCTQtimerState*)opaque;
    uint32_t slot_nr = (offset & 0xF000) >> 12;
    uint32_t frame = slot_nr / qct_s->nr_views;
    QCTHextimerState *s = &qct_s->timer[frame];
    uint32_t reg_offset = offset & 0xFFF;

    HEX_TIMER_LOG("\ta timer write: %" PRIu64 ", %" PRIu64 "\n", offset, value);

    switch (reg_offset) {
        case (QCT_QTIMER_CNTP_CVAL_LO): /* TimerLoad */
            /*HEX_TIMER_LOG ("A s->limit        = %d\n", s->limit);
              HEX_TIMER_LOG ("B s->limit        = %d\n", s->limit);
              HEX_TIMER_LOG ("value             = %d\n",value);
              HEX_TIMER_LOG ("s->cntpct         = %d\n", s->cntpct);
              HEX_TIMER_LOG ("s->cntcval        = %d\n", s->cntval);
              HEX_TIMER_LOG ("value - cntcval   = %d\n", value - s->cntval);
             */
            HEX_TIMER_LOG("value(%" PRId64 ") - cntcval(%" PRId64 ") = %" PRId64
                          "\n",
                          value, s->cntval, value - s->cntval);
            s->int_level = 0;
            s->cntval = value;
            ptimer_transaction_begin(s->timer);
            if (s->control & QCT_QTIMER_CNTP_CTL_ENABLE) {
                /* Pause the timer if it is running.  This may cause some
                   inaccuracy due to rounding, but avoids other issues. */
                ptimer_stop(s->timer);
            }
            hex_timer_recalibrate(s, 1);
            if (s->control & QCT_QTIMER_CNTP_CTL_ENABLE) {
                ptimer_run(s->timer, 0);
            }
            ptimer_transaction_commit(s->timer);
            break;
        case (QCT_QTIMER_CNTP_CVAL_HI):
            qemu_log_mask(LOG_GUEST_ERROR,
                          "%s: QCT_QTIMER_CNTP_CVAL_HI is read-only\n", __func__);
            break;
        case (QCT_QTIMER_CNTP_CTL): /* Timer control register */
            HEX_TIMER_LOG("\tctl write: %" PRIu64 "\n", value);
            ptimer_transaction_begin(s->timer);
            if (s->control & QCT_QTIMER_CNTP_CTL_ENABLE) {
                /* Pause the timer if it is running.  This may cause some
                   inaccuracy due to rounding, but avoids other issues. */
                ptimer_stop(s->timer);
            }
            s->control = value;
            hex_timer_recalibrate(s, s->control & QCT_QTIMER_CNTP_CTL_ENABLE);
            ptimer_set_freq(s->timer, s->freq);
            ptimer_set_period(s->timer, 1);
            if (s->control & QCT_QTIMER_CNTP_CTL_ENABLE) {
                ptimer_run(s->timer, 0);
            }
            ptimer_transaction_commit(s->timer);
            break;
        case (QCT_QTIMER_CNTP_TVAL): /* CVAL - CNTP */
            ptimer_transaction_begin(s->timer);
            if (s->control & QCT_QTIMER_CNTP_CTL_ENABLE) {
                /* Pause the timer if it is running.  This may cause some
                   inaccuracy due to rounding, but avoids other issues. */
                ptimer_stop(s->timer);
            }
            s->cntval = s->cntpct + value;
            ptimer_set_freq(s->timer, s->freq);
            ptimer_set_period(s->timer, 1);
            if (s->control & QCT_QTIMER_CNTP_CTL_ENABLE) {
                ptimer_run(s->timer, 0);
            }
            ptimer_transaction_commit(s->timer);
            break;
        default:
            qemu_log_mask(LOG_GUEST_ERROR,
                          "%s: Bad offset %x\n", __func__, (int)offset);
            break;
    }
    hex_timer_update(s);
}

static void hex_timer_tick(void *opaque)
{
    QCTHextimerState *s = (QCTHextimerState *)opaque;
    if ((s->cntpct >= s->cntval) && (s->int_level != 1)) {
        s->int_level = 1;
        HEX_TIMER_LOG("\nFIRE!!! %" PRId64 "\n", s->cntpct);
        hex_timer_update(s);
        return;
    }
    s->cntpct += s->limit;
}

static const MemoryRegionOps hex_timer_ops = {
        .read = hex_timer_read,
        .write = hex_timer_write,
        .endianness = DEVICE_NATIVE_ENDIAN,
};

static const VMStateDescription vmstate_hex_timer = {
    .name = "hex_timer",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32(control, QCTHextimerState),
        VMSTATE_UINT32(cnt_ctrl, QCTHextimerState),
        VMSTATE_UINT64(cntpct, QCTHextimerState),
        VMSTATE_UINT64(cntval, QCTHextimerState),
        VMSTATE_UINT64(limit, QCTHextimerState),
        VMSTATE_UINT32(int_level, QCTHextimerState),
        VMSTATE_PTIMER(timer, QCTHextimerState),
        VMSTATE_END_OF_LIST()
    }
};

static void qct_qtimer_realize(DeviceState *dev, Error **errp)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);
    QCTQtimerState *s = QCT_QTIMER(dev);
    unsigned int i;

    s->nr_frames = QCT_QTIMER_TIMER_ELTS;
    s->nr_views = 1;

    memory_region_init_io(&s->iomem, OBJECT(sbd), &qct_qtimer_ops, s,
                          "qutimer", QTIMER_MEM_SIZE_BYTES);
    sysbus_init_mmio(sbd, &s->iomem);
    sysbus_init_irq(sbd, &s->irq);

    memory_region_init_io(&s->view_iomem, OBJECT(sbd), &hex_timer_ops, s,
                          "qutimer_views", QTIMER_MEM_SIZE_BYTES * s->nr_frames * s->nr_views);
    sysbus_init_mmio(sbd, &s->view_iomem);

    for (i = 0; i < QCT_QTIMER_TIMER_ELTS; i++) {
        s->timer[i].limit = 1;
        s->timer[i].control = QCT_QTIMER_CNTP_CTL_ENABLE;
        s->timer[i].cnt_ctrl = (QCT_QTIMER_AC_CNTACR_RWPT | QCT_QTIMER_AC_CNTACR_RWVT |
                    QCT_QTIMER_AC_CNTACR_RVOFF | QCT_QTIMER_AC_CNTACR_RFRQ |
                    QCT_QTIMER_AC_CNTACR_RPVCT | QCT_QTIMER_AC_CNTACR_RPCT);
        s->timer[i].devid = i;
        s->timer[i].qtimer = s;
        s->timer[i].freq = QTIMER_DEFAULT_FREQ_HZ;

        sysbus_init_irq(sbd, &(s->timer[i].irq));

        (s->timer[i]).timer = ptimer_init(hex_timer_tick, &s->timer[i], PTIMER_POLICY_LEGACY);
        vmstate_register(NULL, VMSTATE_INSTANCE_ID_ANY, &vmstate_hex_timer, &s->timer[i]);
    }
}

static Property qct_qtimer_properties[] = {
    DEFINE_PROP_UINT32("freq", QCTQtimerState, freq, QTIMER_DEFAULT_FREQ_HZ),
    DEFINE_PROP_END_OF_LIST(),
};

static void qct_qtimer_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *k = DEVICE_CLASS(klass);

    device_class_set_props(k, qct_qtimer_properties);
    k->realize = qct_qtimer_realize;
    k->vmsd = &vmstate_qct_qtimer;
}

static const TypeInfo qct_qtimer_info = {
    .name          = TYPE_QCT_QTIMER,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(QCTQtimerState),
    .instance_init = qct_qtimer_init,
    .class_init    = qct_qtimer_class_init,
};

static void qct_qtimer_register_types(void)
{
    type_register_static(&qct_qtimer_info);
}

type_init(qct_qtimer_register_types)
