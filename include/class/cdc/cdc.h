/********************************** (C) COPYRIGHT *******************************
* File Name          : cdc.h
* Author             : WCH
* Version            : V1.0.0
* Date               : 2026/10/20
* Description        : USB CDC class definition header file
*********************************************************************************
* Copyright (c) 2026 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

#ifndef CDC_H
#define CDC_H

/* @include */
#include "usb_define.h"

#ifdef __cplusplus
extern "C" {
#endif

/* @define */
#define CDC_LINE_STATE_DTR_MASK             0x01
#define CDC_LINE_STATE_RTS_MASK             0x02

#define CDC_ACM_CAPBIT_COMM_FEATURE         0x01
#define CDC_ACM_CAPBIT_LINE_CODING          0x02
#define CDC_ACM_CAPBIT_SEND_BREAK           0x04

#define CDC_ECM_STAT_XMIT_OK                0x00000001
#define CDC_ECM_STAT_RVC_OK                 0x00000002
#define CDC_ECM_STAT_XMIT_ERROR             0x00000004
#define CDC_ECM_STAT_RCV_ERROR              0x00000008
#define CDC_ECM_STAT_RCV_NO_BUFFER          0x00000010
#define CDC_ECM_STAT_DIRECTED_BYTES_XMIT    0x00000020
#define CDC_ECM_STAT_DIRECTED_FRAMES_XMIT   0x00000040
#define CDC_ECM_STAT_MULTICAST_BYTES_XMIT   0x00000080
#define CDC_ECM_STAT_MULTICAST_FRAMES_XMIT  0x00000100
#define CDC_ECM_STAT_BROADCAST_BYTES_XMIT   0x00000200
#define CDC_ECM_STAT_BROADCAST_FRAMES_XMIT  0x00000400
#define CDC_ECM_STAT_DIRECTED_BYTES_RCV     0x00000800
#define CDC_ECM_STAT_DIRECTED_FRAMES_RCV    0x00001000
#define CDC_ECM_STAT_MULTICAST_BYTES_RCV    0x00002000
#define CDC_ECM_STAT_MULTICAST_FRAMES_RCV   0x00004000
#define CDC_ECM_STAT_BROADCAST_BYTES_RCV    0x00008000
#define CDC_ECM_STAT_BROADCAST_FRAMES_RCV   0x00010000
#define CDC_ECM_STAT_RCV_CRC_ERROR          0x00020000
#define CDC_ECM_STAT_TRANSMIT_QUEUE_LENGTH  0x00040000
#define CDC_ECM_STAT_RCV_ERROR_ALIGNMENT    0x00080000
#define CDC_ECM_STAT_XMIT_ONE_COLLISION     0x00100000
#define CDC_ECM_STAT_XMIT_MORE_COLLISIONS   0x00200000
#define CDC_ECM_STAT_XMIT_DEFERRED          0x00400000
#define CDC_ECM_STAT_XMIT_MAX_COLLISIONS    0x00800000
#define CDC_ECM_STAT_RCV_OVERRUN            0x01000000
#define CDC_ECM_STAT_XMIT_UNDERRUN          0x02000000
#define CDC_ECM_STAT_XMIT_HEARTBEAT_FAILURE 0x04000000
#define CDC_ECM_STAT_XMIT_TIMES_CRS_LOST    0x08000000
#define CDC_ECM_STAT_XMIT_LATE_COLLISIONS   0x10000000

/* @enum */
typedef enum
{
    CDC_SUBCLASS_DIRECT_LINE_CONTROL_MODEL = 0x01,
    CDC_SUBCLASS_ABSTRACT_CONTROL_MODEL = 0x02,
    CDC_SUBCLASS_TELEPHONE_CONTROL_MODEL = 0x03,
    CDC_SUBCLASS_MULTICHANNEL_CONTROL_MODEL = 0x04,
    CDC_SUBCLASS_CAPI_CONTROL_MODEL = 0x05,
    CDC_SUBCLASS_ETHERNET_CONTROL_MODEL = 0x06,
    CDC_SUBCLASS_ATM_NETWORKING_CONTROL_MODEL = 0x07,
    CDC_SUBCLASS_WIRELESS_HANDSET_CONTROL_MODEL = 0x08,
    CDC_SUBCLASS_DEVICE_MANAGEMENT = 0x09,
    CDC_SUBCLASS_MOBILE_DIRECT_LINE_MODEL = 0x0A,
    CDC_SUBCLASS_OBEX = 0x0B,
    CDC_SUBCLASS_ETHERNET_EMULATION_MODEL = 0x0C,
    CDC_SUBCLASS_NETWORK_CONTROL_MODEL = 0x0D,
} cdc_subclass_t;

typedef enum
{
    CDC_PROTOCOL_NONE = 0x00,
    CDC_PROTOCOL_ATCOMMAND = 0x01,
    CDC_PROTOCOL_ATCOMMAND_PCCA_101 = 0x02,
    CDC_PROTOCOL_ATCOMMAND_PCCA_101_AND_ANNEXO = 0x03,
    CDC_PROTOCOL_ATCOMMAND_GSM_707 = 0x04,
    CDC_PROTOCOL_ATCOMMAND_3GPP_27007 = 0x05,
    CDC_PROTOCOL_ATCOMMAND_CDMA = 0x06,
    CDC_PROTOCOL_ETHERNET_EMULATION_MODEL = 0x07,
} cdc_protocol_t;

typedef enum
{
    CDC_DESC_HEADER = 0x00,
    CDC_DESC_CALL_MANAGEMENT = 0x01,
    CDC_DESC_ABSTRACT_CONTROL_MANAGEMENT = 0x02,
    CDC_DESC_DIRECT_LINE_MANAGEMENT = 0x03,
    CDC_DESC_TELEPHONE_RINGER = 0x04,
    CDC_DESC_TELEPHONE_CALL_AND_LINE_STATE_REPORTING_CAPACITY = 0x05,
    CDC_DESC_UNION = 0x06,
    CDC_DESC_COUNTRY_SELECTION = 0x07,
    CDC_DESC_TELEPHONE_OPERATIONAL_MODES = 0x08,
    CDC_DESC_USB_TERMINAL = 0x09,
    CDC_DESC_NETWORK_CHANNEL_TERMINAL = 0x0A,
    CDC_DESC_PROTOCOL_UNIT = 0x0B,
    CDC_DESC_EXTENSION_UNIT = 0x0C,
    CDC_DESC_MULTICHANNEL_MANAGEMENT = 0x0D,
    CDC_DESC_CAPI_CONTROL_MANAGEMENT = 0x0E,
    CDC_DESC_ETHERNET_NETWORKING = 0x0F,
    CDC_DESC_ATM_NETWORKING = 0x10,
    CDC_DESC_WIRELESS_HANDSET_CONTROL_MODEL = 0x11,
    CDC_DESC_MOBILE_DIRECT_LINE_MODEL = 0x12,
    CDC_DESC_MOBILE_DIRECT_LINE_MODEL_DETAIL = 0x13,
    CDC_DESC_DEVICE_MANAGEMENT_MODEL = 0x14,
    CDC_DESC_OBEX = 0x15,
    CDC_DESC_COMMAND_SET = 0x16,
    CDC_DESC_COMMAND_SET_DETAIL = 0x17,
    CDC_DESC_TELEPHONE_CONTROL_MODEL = 0x18,
    CDC_DESC_OBEX_SERVICE_IDENTIFIER = 0x19,
    CDC_DESC_NCM = 0x1A,
} cdc_desc_t;

typedef enum
{
    CDC_REQ_SEND_ENCAPSULATED_COMMAND = 0x00,
    CDC_REQ_GET_ENCAPSULATED_RESPONSE = 0x01,
    CDC_REQ_SET_COMM_FEATURE = 0x02,
    CDC_REQ_GET_COMM_FEATURE = 0x03,
    CDC_REQ_CLEAR_COMM_FEATURE = 0x04,

    CDC_REQ_SET_AUX_LINE_STATE = 0x10,
    CDC_REQ_SET_HOOK_STATE = 0x11,
    CDC_REQ_PULSE_SETUP = 0x12,
    CDC_REQ_SEND_PULSE = 0x13,
    CDC_REQ_SET_PULSE_TIME = 0x14,
    CDC_REQ_RING_AUX_JACK = 0x15,

    CDC_REQ_SET_LINE_CODING = 0x20,
    CDC_REQ_GET_LINE_CODING = 0x21,
    CDC_REQ_SET_CONTROL_LINE_STATE = 0x22,
    CDC_REQ_SEND_BREAK = 0x23,

    CDC_REQ_SET_RINGER_PARMS = 0x30,
    CDC_REQ_GET_RINGER_PARMS = 0x31,
    CDC_REQ_SET_OPERATION_PARMS = 0x32,
    CDC_REQ_GET_OPERATION_PARMS = 0x33,
    CDC_REQ_SET_LINE_PARMS = 0x34,
    CDC_REQ_GET_LINE_PARMS = 0x35,
    CDC_REQ_DIAL_DIGITS = 0x36,
    CDC_REQ_SET_UNIT_PARAMETER = 0x37,
    CDC_REQ_GET_UNIT_PARAMETER = 0x38,
    CDC_REQ_CLEAR_UNIT_PARAMETER = 0x39,
    CDC_REQ_GET_PROFILE = 0x3A,

    CDC_REQ_SET_ETHERNET_MULTICAST_FILTERS = 0x40,
    CDC_REQ_SET_ETHERNET_POWER_MANAGEMENT_PATTERN_FILTER = 0x41,
    CDC_REQ_GET_ETHERNET_POWER_MANAGEMENT_PATTERN_FILTER = 0x42,
    CDC_REQ_SET_ETHERNET_PACKET_FILTER = 0x43,
    CDC_REQ_GET_ETHERNET_STATISTIC = 0x44,

    CDC_REQ_SET_ATM_DATA_FORMAT = 0x50,
    CDC_REQ_GET_ATM_DEVICE_STATISTICS = 0x51,
    CDC_REQ_SET_ATM_DEFAULT_VC = 0x52,
    CDC_REQ_GET_ATM_VC_STATISTICS = 0x53,

    CDC_REQ_MDLM_SEMANTIC_MODEL = 0x60,
} cdc_request_t;

typedef enum
{
    CDC_NOTIF_NETWORK_CONNECTION = 0x00,
    CDC_NOTIF_RESPONSE_AVAILABLE = 0x01,
    CDC_NOTIF_AUX_JACK_HOOK_STATE = 0x08,
    CDC_NOTIF_RING_DETECT = 0x09,
    CDC_NOTIF_SERIAL_STATE = 0x20,
    CDC_NOTIF_CALL_STATE_CHANGE = 0x28,
    CDC_NOTIF_LINE_STATE_CHANGE = 0x29,
    CDC_NOTIF_CONNECTION_SPEED_CHANGE = 0x2A,
} cdc_notif_t;

/* @struct */
typedef struct __attribute__((packed))
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubType;
    uint16_t bcdCDC;
} cdc_desc_header_t;

typedef struct __attribute__((packed))
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubType;
    uint8_t bmCapabilities;
    uint8_t bDataInterface;
} cdc_desc_call_management_t;

typedef struct __attribute__((packed))
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubType;
    uint8_t bControlInterface;
    uint8_t bSubordinateInterface[];
} cdc_desc_union_t;

typedef struct __attribute__((packed))
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubType;
    uint8_t iCountryCodeRelDate;
    uint16_t wCountryCode[];
} cdc_desc_country_selection_t;

typedef struct __attribute__((packed))
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubType;
    uint8_t bmCapabilities;
} cdc_desc_abstract_control_management_t;

typedef struct __attribute__((packed))
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubType;
    uint8_t bmCapabilities;
} cdc_desc_ethernet_networking_t;

typedef struct __attribute__((packed))
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubType;
    uint8_t iMACAddress;
    uint32_t bmEthernetStatistics;
    uint16_t wMaxSegmentSize;
    uint16_t wNumberMCFilters;
    uint8_t bNumberPowerFilters;
} cdc_desc_direct_line_management_t;

typedef struct __attribute__((packed))
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubType;
    uint8_t bRingerVolSteps;
    uint8_t bNumRingerPatterns;
} cdc_desc_telephone_ringer_t;

typedef struct __attribute__((packed))
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubType;
    uint8_t bmCapabilities;
} cdc_desc_telephone_operational_modes_t;

typedef struct __attribute__((packed))
{
    uint32_t bit_rate;
    uint8_t stop_bits;
    uint8_t parity;
    uint8_t data_bits;
} cdc_line_coding_t;

typedef struct __attribute__((packed))
{
    uint32_t DLBitRate;
    uint32_t ULBitRate;
} cdc_connect_speed_change_notify_t;

#ifdef __cplusplus
}
#endif

#endif // CDC_H
