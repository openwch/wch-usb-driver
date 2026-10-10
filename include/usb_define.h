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

#define USB_ENDP_DIR(endp)              ((endp) & 0x80)
#define USB_ENDP_NUM(endp)              ((endp) & 0x0F)
#define USB_ENDP_GET_TYPE(attributes)   ((attributes) & 0x03)
#define USB_ENDP_GET_MPS(mps)           ((mps) & 0x07FF)
#define USB_MAX_ENDP_NUM                (16)

#define USB_ARRAY_SIZE(arr)             (sizeof(arr) / sizeof((arr)[0]))

#define USB_SET_REQ(dir, type, rcpt)    (((dir) << 7) | ((type) << 5) | (rcpt))
#define USB_GET_REQ_DIR(bmRequestType)  (((bmRequestType) & 0x80) >> 7)
#define USB_GET_REQ_TYPE(bmRequestType) (((bmRequestType) & 0x60) >> 5)
#define USB_GET_REQ_RCPT(bmRequestType) (((bmRequestType) & 0x1F) >> 0)

#define USB_SELF_POWERED_MASK           (0x40)
#define USB_REMOTE_WAKEUP_MASK          (0x20)

#define USB_U16_HIGH(data)              ((uint8_t)(((data) >> 8) & 0x00FF))
#define USB_U16_LOW(data)               ((uint8_t)(((data) >> 0) & 0x00FF))
#define USB_U16_TO_U8_LSB(data)         USB_U16_LOW(data), USB_U16_HIGH(data)
#define USB_U16_TO_U8_MSB(data)         USB_U16_HIGH(data), USB_U16_LOW(data)

#define USB_U32_BYTE0(data)             ((uint8_t)(((data) >> 0) & 0x000000FF))
#define USB_U32_BYTE1(data)             ((uint8_t)(((data) >> 8) & 0x000000FF))
#define USB_U32_BYTE2(data)             ((uint8_t)(((data) >> 16) & 0x000000FF))
#define USB_U32_BYTE3(data)             ((uint8_t)(((data) >> 24) & 0x000000FF))

#ifndef USB_LOG_TAG
#define USB_LOG_TAG "?"
#endif

#ifdef USB_DRIVER_LOG_INFO_EN
#define USB_LOGI(format, ...) USB_LOG_OUTPUT("[" USB_LOG_TAG "][I]: " format "\r\n", ##__VA_ARGS__)
#else
#define USB_LOGI(format, ...)
#endif

#ifdef USB_DRIVER_LOG_WARNING_EN
#define USB_LOGW(format, ...) USB_LOG_OUTPUT("[" USB_LOG_TAG "][W]: " format "\r\n", ##__VA_ARGS__)
#else
#define USB_LOGW(format, ...)
#endif

#ifdef USB_DRIVER_LOG_ERROR_EN
#define USB_LOGE(format, ...) USB_LOG_OUTPUT("[" USB_LOG_TAG "][E]: " format "\r\n", ##__VA_ARGS__)
#else
#define USB_LOGE(format, ...)
#endif

/* @typedef */
typedef uint8_t usb_endp_t;

/* @enum */
typedef enum
{
    USB_SPEED_UNKNOWN,
    USB_SPEED_FULL,
    USB_SPEED_LOW,
    USB_SPEED_HIGH,
    USB_SPEED_SUPER,
} usb_speed_t;

typedef enum
{
    USB_ENDP_TYPE_CTRL,
    USB_ENDP_TYPE_ISOC,
    USB_ENDP_TYPE_BULK,
    USB_ENDP_TYPE_INTR,
} usb_endp_type_t;

typedef enum
{
    USB_CTRL_STAGE_SETUP,
    USB_CTRL_STAGE_DATA,
    USB_CTRL_STAGE_STATUS,
} usb_ctrl_stage_t;

typedef enum
{
    USB_DIR_OUT,
    USB_DIR_IN,
} usb_dir_t;

typedef enum
{
    USB_REQ_TYPE_STANDARD,
    USB_REQ_TYPE_CLASS,
    USB_REQ_TYPE_VENDOR,
    USB_REQ_TYPE_RESERVED,
} usb_req_type_t;

typedef enum
{
    USB_REQ_RCPT_DEVICE,
    USB_REQ_RCPT_INTERFACE,
    USB_REQ_RCPT_ENDPOINT,
    USB_REQ_RCPT_OTHER,
} usb_req_rcpt_t;

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
} usb_request_t;

typedef enum
{
    USB_FEATURE_EDPT_HALT = 0,
    USB_FEATURE_REMOTE_WAKEUP = 1,
    USB_FEATURE_TEST_MODE = 2,
} usb_feature_t;

typedef enum
{
    USB_CLASS_UNSPECIFIED = 0,
    USB_CLASS_AUDIO = 1,
    USB_CLASS_CDC = 2,
    USB_CLASS_HID = 3,
    USB_CLASS_RESERVED_4 = 4,
    USB_CLASS_PHYSICAL = 5,
    USB_CLASS_IMAGE = 6,
    USB_CLASS_PRINTER = 7,
    USB_CLASS_MSC = 8,
    USB_CLASS_HUB = 9,
    USB_CLASS_CDC_DATA = 10,
    USB_CLASS_SMART_CARD = 11,
    USB_CLASS_RESERVED_12 = 12,
    USB_CLASS_CONTENT_SECURITY = 13,
    USB_CLASS_VIDEO = 14,
    USB_CLASS_PERSONAL_HEALTHCARE = 15,
    USB_CLASS_AUDIO_VIDEO = 16,

    USB_CLASS_DIAGNOSTIC = 0xDC,
    USB_CLASS_WIRELESS_CONTROLLER = 0xE0,
    USB_CLASS_MISC = 0xEF,
    USB_CLASS_APPLICATION_SPECIFIC = 0xFE,
    USB_CLASS_VENDOR_SPECIFIC = 0xFF,
} usb_class_code_t;

typedef enum
{
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

    USB_DESC_CS_DEVICE = 0x21,
    USB_DESC_CS_CONFIGURATION = 0x22,
    USB_DESC_CS_STRING = 0x23,
    USB_DESC_CS_INTERFACE = 0x24,
    USB_DESC_CS_ENDPOINT = 0x25,

    USB_DESC_SUPERSPEED_ENDPOINT_COMPANION = 0x30,
    USB_DESC_SUPERSPEED_ISO_ENDPOINT_COMPANION = 0x31,
} usb_desc_type_t;

typedef enum
{
    USB_TEST_SELECT_RESERVED,
    USB_TEST_SELECT_J,
    USB_TEST_SELECT_K,
    USB_TEST_SELECT_SE0_NAK,
    USB_TEST_SELECT_PACKET,
    USB_TEST_SELECT_FORCE_ENABLE,
} usb_test_select_t;

/* @struct */
typedef struct __attribute__((packed))
{
    uint8_t bmRequestType;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} usb_setup_t;

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
    uint16_t bcdUSB;
    uint8_t bDeviceClass;
    uint8_t bDeviceSubClass;
    uint8_t bDeviceProtocol;
    uint8_t bMaxPacketSize0;
    uint8_t bNumConfigurations;
    uint8_t bReserved;
} usb_desc_qualifier_t;

typedef struct __attribute__((packed))
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t wTotalLength;
    uint8_t bNumInterfaces;
    uint8_t bConfigurationValue;
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

#ifdef __cplusplus
}
#endif

#endif // USB_DEFINE_H
