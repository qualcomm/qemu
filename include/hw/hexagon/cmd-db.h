/*
 * Hexagon CMD_DB (Command Database) loader
 *
 * Copyright (c) 2025 Qualcomm Technologies, Inc. All Rights Reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_HEXAGON_CMD_DB_H
#define HW_HEXAGON_CMD_DB_H

#include "hw/core/sysbus.h"

#define CMD_DB_HEADER_SIZE 1024

void hexagon_load_cmd_db(hwaddr header_addr, hwaddr bin_addr);

#endif /* HW_HEXAGON_CMD_DB_H */
