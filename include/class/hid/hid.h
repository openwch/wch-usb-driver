/**
 * @file hid.h
 * @author Links (lhd@wch.cn)
 * @brief USB HID class definition header file
 *
 * @copyright Copyright (c) 2026
 *
 */

#ifndef HID_H
#define HID_H

/* @include */
#include "usb_define.h"

#ifdef __cplusplus
extern "C" {
#endif

/* @enum */
typedef enum
{
    HID_DESC_HID = 0x21,
    HID_DESC_REPORT = 0x22,
    HID_DESC_PHYSICAL = 0x23,
} hid_desc_type_t;

typedef enum
{
    HID_REQ_GET_REPORT = 0x01,
    HID_REQ_GET_IDLE = 0x02,
    HID_REQ_GET_PROTOCOL = 0x03,
    HID_REQ_SET_REPORT = 0x09,
    HID_REQ_SET_IDLE = 0x0A,
    HID_REQ_SET_PROTOCOL = 0x0B,
} hid_request_code_t;

typedef enum
{
    HID_REPORT_TYPE_INPUT = 0x01,
    HID_REPORT_TYPE_OUTPUT = 0x02,
    HID_REPORT_TYPE_FEATURE = 0x03,
} hid_report_type_t;

typedef enum
{
    HID_PROTOCOL_BOOT = 0,
    HID_PROTOCOL_REPORT = 1,
} hid_protocol_t;

/* @struct */
typedef struct __attribute__((packed))
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t bcdHID;
    uint8_t bCountryCode;
    uint8_t bNumDescriptors;
    uint8_t bReportType;
    uint16_t wReportLength;
} hid_desc_t;

#ifdef __cplusplus
}
#endif

#endif // HID_H
