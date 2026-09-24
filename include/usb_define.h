/**
 * @file usb_define.h
 * @author Links (lhd@wch.cn)
 * @brief USB driver define header file
 *
 * @copyright Copyright (c) 2026
 *
 */

#ifndef USB_DEFINE_H
#define USB_DEFINE_H

/* @include */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* @define */
#define USB_MIN(a, b)                   ((a) < (b) ? (a) : (b))
#define USB_MAX(a, b)                   ((a) > (b) ? (a) : (b))

#define USB_ENDP_DIR(ep)                ((ep) & 0x80)
#define USB_ENDP_NUM(ep)                ((ep) & 0x0F)
#define USB_MAX_ENDP_NUM                (16)

#define USB_ARRAY_SIZE(arr)             (sizeof(arr) / sizeof((arr)[0]))

#define USB_GET_REQ_DIR(bmRequestType)  (((bmRequestType) & 0x80) >> 7)
#define USB_GET_REQ_TYPE(bmRequestType) (((bmRequestType) & 0x60) >> 5)
#define USB_GET_REQ_RCPT(bmRequestType) (((bmRequestType) & 0x1F) >> 0)

/* @enum */
typedef enum
{
    USB_SPEED_UNKNOWN = 0,
    USB_SPEED_FULL = 1,
    USB_SPEED_LOW = 2,
    USB_SPEED_HIGH = 3,
    USB_SPEED_SUPER = 4,
} usb_speed_t;

typedef enum
{
    USB_ENDP_TYPE_CTRL = 0,
    USB_ENDP_TYPE_ISOC = 1,
    USB_ENDP_TYPE_BULK = 2,
    USB_ENDP_TYPE_INTR = 3,
} usb_endp_type_t;

typedef enum
{
    USB_TEST_SELECT_RESERVED = 0,
    USB_TEST_SELECT_J = 1,
    USB_TEST_SELECT_K = 2,
    USB_TEST_SELECT_SE0_NAK = 3,
    USB_TEST_SELECT_PACKET = 4,
    USB_TEST_SELECT_FORCE_ENABLE = 5,
} usb_test_select_t;

typedef enum
{
    USB_CTRL_STAGE_SETUP = 0,
    USB_CTRL_STAGE_DATA = 1,
    USB_CTRL_STAGE_STATUS = 2,
} usb_ctrl_stage_t;

typedef enum
{
    USB_DIR_OUT = 0,
    USB_DIR_IN = 1,
} usb_dir_t;

typedef enum
{
    USB_TYPE_STANDARD = 0,
    USB_TYPE_CLASS = 1,
    USB_TYPE_VENDOR = 2,
    USB_TYPE_RESERVED = 3,
} usb_type_t;

typedef enum
{
    USB_RCPT_DEVICE = 0,
    USB_RCPT_INTERFACE = 1,
    USB_RCPT_ENDPOINT = 2,
    USB_RCPT_OTHER = 3,
} usb_rcpt_t;

typedef enum
{
    USB_REQ_GET_STATUS = 0,
    USB_REQ_CLEAR_FEATURE = 1,
    USB_REQ_RESERVED = 2,
    USB_REQ_SET_FEATURE = 3,
    USB_REQ_RESERVED2 = 4,
    USB_REQ_SET_ADDRESS = 5,
    USB_REQ_GET_DESCRIPTOR = 6,
    USB_REQ_SET_DESCRIPTOR = 7,
    USB_REQ_GET_CONFIGURATION = 8,
    USB_REQ_SET_CONFIGURATION = 9,
    USB_REQ_GET_INTERFACE = 10,
    USB_REQ_SET_INTERFACE = 11,
    USB_REQ_SYNCH_FRAME = 12,
} usb_request_code_t;

typedef enum
{
    // Standard USB Descriptor Types
    USB_DESC_DEVICE = 0x01,
    USB_DESC_CONFIGURATION = 0x02,
    USB_DESC_STRING = 0x03,
    USB_DESC_INTERFACE = 0x04,
    USB_DESC_ENDPOINT = 0x05,
    USB_DESC_DEVICE_QUALIFIER = 0x06,
    USB_DESC_OTHER_SPEED_CONFIG = 0x07,
    USB_DESC_INTERFACE_POWER = 0x08,
    USB_DESC_OTG = 0x09,
    USB_DESC_DEBUG = 0x0A,
    USB_DESC_INTERFACE_ASSOCIATION = 0x0B,
    USB_DESC_BOS = 0x0F,
    USB_DESC_DEVICE_CAPABILITY = 0x10,
    USB_DESC_FUNCTIONAL = 0x21,

    // Class Specific Descriptor
    USB_DESC_CS_DEVICE = 0x21,
    USB_DESC_CS_CONFIGURATION = 0x22,
    USB_DESC_CS_STRING = 0x23,
    USB_DESC_CS_INTERFACE = 0x24,
    USB_DESC_CS_ENDPOINT = 0x25,

    // SuperSpeed USB Descriptors
    USB_DESC_SUPERSPEED_ENDPOINT_COMPANION = 0x30,
    USB_DESC_SUPERSPEED_ISO_ENDPOINT_COMPANION = 0x31,
} usb_desc_type_t;

typedef enum
{
    USB_FEATURE_EDPT_HALT = 0,
    USB_FEATURE_REMOTE_WAKEUP = 1,
    USB_FEATURE_TEST_MODE = 2
} tusb_feature_selector_t;

/* @typedef */
typedef uint8_t usb_endp_t;

typedef struct __attribute__((packed))
{
    uint8_t bmRequestType;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} usb_setup_t;

/* USB Descriptor */
typedef struct __attribute__((packed))
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t bcdUSB;
    uint8_t bDeviceClass;
    uint8_t bDeviceSubClass;
    uint8_t bDeviceProtocol;
    uint8_t bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t iManufacturer;
    uint8_t iProduct;
    uint8_t iSerialNumber;
    uint8_t bNumConfigurations;
} usb_desc_device_t;

typedef struct __attribute__((packed))
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t wTotalLength;
    uint8_t bNumInterfaces;
    uint8_t bConfigurationValu;
    uint8_t iConfiguration;
    uint8_t bmAttributes;
    uint8_t bMaxPower;
} usb_desc_config_t;

typedef struct __attribute__((packed))
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bInterfaceNumber;
    uint8_t bAlternateSetting;
    uint8_t bNumEndpoints;
    uint8_t bInterfaceClass;
    uint8_t bInterfaceSubClass;
    uint8_t bInterfaceProtocol;
    uint8_t iInterface;
} usb_desc_interface_t;

typedef struct __attribute__((packed))
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bEndpointAddress;
    uint8_t bmAttributes;
    uint16_t wMaxPacketSize;
    uint8_t bInterval;
} usb_desc_endpoint_t;

typedef struct __attribute__((packed))
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t bcdUSB;
    uint8_t bDeviceClass;
    uint8_t bDeviceSubClass;
    uint8_t bDeviceProtocol;
    uint8_t bMaxPacketSize0;
    uint8_t bNumConfigurations;
    uint8_t bReserved;
} usb_desc_qualifier_t;

#ifdef __cplusplus
}
#endif

#endif // USB_DEFINE_H
