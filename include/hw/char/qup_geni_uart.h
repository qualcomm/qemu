/*
 * Qualcomm GENI UART (QUP UART)
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_CHAR_QCOM_GENI_UART_H
#define HW_CHAR_QCOM_GENI_UART_H

#include "qemu/typedefs.h"

#define TYPE_QUP_GENI_UART "qup-geni-uart"

/* QUP GENI UART creation function (implemented in Rust) */
DeviceState *qup_geni_uart_create(uint64_t addr, qemu_irq irq, Chardev *chr);

#endif /* HW_CHAR_QCOM_GENI_UART_H */
