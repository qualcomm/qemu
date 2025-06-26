/*
 * QEMU CXL Mailbox
 *
 * This work is licensed under the terms of the GNU GPL, version 2. See the
 * COPYING file in the top-level directory.
 */

#ifndef CXL_MAILBOX_H
#define CXL_MAILBOX_H

#define CXL_MBOX_CONFIG_CHANGE_COLD_RESET (1)
#define CXL_MBOX_IMMEDIATE_CONFIG_CHANGE (1 << 1)
#define CXL_MBOX_IMMEDIATE_DATA_CHANGE (1 << 2)
#define CXL_MBOX_IMMEDIATE_POLICY_CHANGE (1 << 3)
#define CXL_MBOX_IMMEDIATE_LOG_CHANGE (1 << 4)
#define CXL_MBOX_SECURITY_STATE_CHANGE (1 << 5)
#define CXL_MBOX_BACKGROUND_OPERATION (1 << 6)
#define CXL_MBOX_BACKGROUND_OPERATION_ABORT (1 << 7)
#define CXL_MBOX_SECONDARY_MBOX_SUPPORTED (1 << 8)
#define CXL_MBOX_REQUEST_ABORT_BACKGROUND_OP_SUPPORTED (1 << 9)
#define CXL_MBOX_CEL_10_TO_11_VALID (1 << 10)
#define CXL_MBOX_CONFIG_CHANGE_CONV_RESET (1 << 11)
#define CXL_MBOX_CONFIG_CHANGE_CXL_RESET (1 << 12)

#define CXL_LOG_CAP_CLEAR_SUPPORTED (1 << 0)
#define CXL_LOG_CAP_POPULATE_SUPPORTED (1 << 1)
#define CXL_LOG_CAP_AUTO_POPULATE_SUPPORTED (1 << 2)
#define CXL_LOG_CAP_PERSISTENT_COLD_RESET_SUPPORTED (1 << 3)

enum {
    INFOSTAT    = 0x00,
        #define IS_IDENTIFY   0x1
        #define BACKGROUND_OPERATION_STATUS    0x2
        #define GET_RESPONSE_MSG_LIMIT         0x3
        #define SET_RESPONSE_MSG_LIMIT         0x4
        #define BACKGROUND_OPERATION_ABORT     0x5
    EVENTS      = 0x01,
        #define GET_RECORDS   0x0
        #define CLEAR_RECORDS   0x1
        #define GET_INTERRUPT_POLICY   0x2
        #define SET_INTERRUPT_POLICY   0x3
    FIRMWARE_UPDATE = 0x02,
        #define GET_INFO      0x0
        #define TRANSFER      0x1
        #define ACTIVATE      0x2
    TIMESTAMP   = 0x03,
        #define GET           0x0
        #define SET           0x1
    LOGS        = 0x04,
        #define GET_SUPPORTED 0x0
        #define GET_LOG       0x1
        #define GET_LOG_CAPABILITIES   0x2
        #define CLEAR_LOG     0x3
        #define POPULATE_LOG  0x4
    FEATURES    = 0x05,
        #define GET_SUPPORTED 0x0
        #define GET_FEATURE   0x1
        #define SET_FEATURE   0x2
    MAINTENANCE = 0x06,
        #define PERFORM 0x0
    IDENTIFY    = 0x40,
        #define MEMORY_DEVICE 0x0
    CCLS        = 0x41,
        #define GET_PARTITION_INFO     0x0
        #define GET_LSA       0x2
        #define SET_LSA       0x3
    HEALTH_INFO_ALERTS = 0x42,
        #define GET_ALERT_CONFIG 0x1
        #define SET_ALERT_CONFIG 0x2
    SANITIZE    = 0x44,
        #define OVERWRITE     0x0
        #define SECURE_ERASE  0x1
        #define MEDIA_OPERATIONS 0x2
    PERSISTENT_MEM = 0x45,
        #define GET_SECURITY_STATE     0x0
    MEDIA_AND_POISON = 0x43,
        #define GET_POISON_LIST        0x0
        #define INJECT_POISON          0x1
        #define CLEAR_POISON           0x2
        #define GET_SCAN_MEDIA_CAPABILITIES 0x3
        #define SCAN_MEDIA             0x4
        #define GET_SCAN_MEDIA_RESULTS 0x5
    DCD_CONFIG  = 0x48,
        #define GET_DC_CONFIG          0x0
        #define GET_DYN_CAP_EXT_LIST   0x1
        #define ADD_DYN_CAP_RSP        0x2
        #define RELEASE_DYN_CAP        0x3
    PHYSICAL_SWITCH = 0x51,
        #define IDENTIFY_SWITCH_DEVICE      0x0
        #define GET_PHYSICAL_PORT_STATE     0x1
        #define PHYSICAL_PORT_CONTROL       0x2
    TUNNEL = 0x53,
        #define MANAGEMENT_COMMAND     0x0
    FMAPI_DCD_MGMT = 0x56,
        #define GET_DCD_INFO    0x0
        #define GET_HOST_DC_REGION_CONFIG   0x1
        #define SET_DC_REGION_CONFIG        0x2
        #define GET_DC_REGION_EXTENT_LIST   0x3
        #define INITIATE_DC_ADD             0x4
        #define INITIATE_DC_RELEASE         0x5
    GLOBAL_MEM_ACCESS_EP_MGMT = 0X59,
};

#endif
