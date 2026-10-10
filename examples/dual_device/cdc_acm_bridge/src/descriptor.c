/**
 * @file descriptor.c
 * @author Links (lhd@wch.cn)
 * @brief USB descriptor for CDC-ACM Bridge example
 *
 * @copyright Copyright (c) 2026
 *
 */

/* @include*/
#include "usb_driver.h"

/* @global */
const usb_desc_device_t device_desc = {
    .bLength = 0x12,
    .bDescriptorType = 0x01,
    .bcdUSB = 0x0200,
    .bDeviceClass = USB_CLASS_CDC,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,
    .bMaxPacketSize0 = 0x40,
    .idVendor = 0x1A86,
    .idProduct = 0xFE31,
    .bcdDevice = USB_DRIVER_VERSION_NUMBER,
    .iManufacturer = 1,
    .iProduct = 2,
    .iSerialNumber = 0,
    .bNumConfigurations = 1,
};

const usb_desc_qualifier_t qua_desc = {
    .bLength = 0x0A,
    .bDescriptorType = 0x06,
    .bcdUSB = 0x0200,
    .bDeviceClass = USB_CLASS_CDC,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,
    .bMaxPacketSize0 = 0x40,
    .bNumConfigurations = 0x01,
    .bReserved = 0x00,
};

const uint8_t config_desc_hs[] = {
    0x09,                                                     // bLength
    USB_DESC_CONFIGURATION,                                   // bDescriptorType (Configuration)
    USB_U16_TO_U8_LSB(67),                                    // wTotalLength 67
    0x02,                                                     // bNumInterfaces 2
    0x01,                                                     // bConfigurationValue
    0x00,                                                     // iConfiguration (String Index)
    0x80,                                                     // bmAttributes
    0x32,                                                     // bMaxPower 100mA

    0x09,                                                     // bLength
    USB_DESC_INTERFACE,                                       // bDescriptorType (Interface)
    0x00,                                                     // bInterfaceNumber
    0x00,                                                     // bAlternateSetting
    0x01,                                                     // bNumEndpoints
    USB_CLASS_CDC,                                            // bInterfaceClass
    CDC_SUBCLASS_ABSTRACT_CONTROL_MODEL,                      // bInterfaceSubClass
    CDC_PROTOCOL_NONE,                                        // bInterfaceProtocol
    0x00,                                                     // iInterface

    0x05,                                                     // bFunctionLength
    USB_DESC_CS_INTERFACE,                                    // bDescriptorType
    CDC_DESC_HEADER,                                          // bDescriptorSubtype
    USB_U16_TO_U8_LSB(0x0120),                                // bcdCDC

    0x05,                                                     // bFunctionLength
    USB_DESC_CS_INTERFACE,                                    // bDescriptorType
    CDC_DESC_CALL_MANAGEMENT,                                 // bDescriptorSubtype
    0x00,                                                     // bmCapabilities
    0x01,                                                     // bDataInterface

    0x04,                                                     // bFunctionLength
    USB_DESC_CS_INTERFACE,                                    // bDescriptorType
    CDC_DESC_ABSTRACT_CONTROL_MANAGEMENT,                     // bDescriptorSubtype
    (CDC_ACM_CAPBIT_LINE_CODING | CDC_ACM_CAPBIT_SEND_BREAK), // bmCapabilities

    0x05,                                                     // bFunctionLength
    USB_DESC_CS_INTERFACE,                                    // bDescriptorType
    CDC_DESC_UNION,                                           // bDescriptorSubtype
    0x00,                                                     // bControlInterface
    0x01,                                                     // bSubordinateInterface0

    0x07,                                                     // bLength
    USB_DESC_ENDPOINT,                                        // bDescriptorType
    0x81,                                                     // bEndpointAddress
    0x03,                                                     // bmAttributes
    USB_U16_TO_U8_LSB(8),                                     // wMaxPacketSize
    0x0A,                                                     // bInterval

    0x09,                                                     // bLength
    USB_DESC_INTERFACE,                                       // bDescriptorType
    0x01,                                                     // bInterfaceNumber
    0x00,                                                     // bAlternateSetting
    0x02,                                                     // bNumEndpoints
    USB_CLASS_CDC_DATA,                                       // bInterfaceClass
    0x00,                                                     // bInterfaceSubClass
    0x00,                                                     // bInterfaceProtocol
    0x00,                                                     // iInterface

    0x07,                                                     // bLength
    USB_DESC_ENDPOINT,                                        // bDescriptorType
    0x82,                                                     // bEndpointAddress
    0x02,                                                     // bmAttributes
    USB_U16_TO_U8_LSB(512),                                   // wMaxPacketSize
    0x00,                                                     // bInterval

    0x07,                                                     // bLength
    USB_DESC_ENDPOINT,                                        // bDescriptorType
    0x02,                                                     // bEndpointAddress
    0x02,                                                     // bmAttributes
    USB_U16_TO_U8_LSB(512),                                   // wMaxPacketSize
    0x00,                                                     // bInterval
};

const uint8_t config_desc_fs[] = {
    0x09,                                                     // bLength
    USB_DESC_CONFIGURATION,                                   // bDescriptorType (Configuration)
    USB_U16_TO_U8_LSB(67),                                    // wTotalLength 67
    0x02,                                                     // bNumInterfaces 2
    0x01,                                                     // bConfigurationValue
    0x00,                                                     // iConfiguration (String Index)
    0x80,                                                     // bmAttributes
    0x32,                                                     // bMaxPower 100mA

    0x09,                                                     // bLength
    USB_DESC_INTERFACE,                                       // bDescriptorType (Interface)
    0x00,                                                     // bInterfaceNumber
    0x00,                                                     // bAlternateSetting
    0x01,                                                     // bNumEndpoints
    USB_CLASS_CDC,                                            // bInterfaceClass
    CDC_SUBCLASS_ABSTRACT_CONTROL_MODEL,                      // bInterfaceSubClass
    CDC_PROTOCOL_NONE,                                        // bInterfaceProtocol
    0x00,                                                     // iInterface

    0x05,                                                     // bFunctionLength
    USB_DESC_CS_INTERFACE,                                    // bDescriptorType
    CDC_DESC_HEADER,                                          // bDescriptorSubtype
    USB_U16_TO_U8_LSB(0x0120),                                // bcdCDC

    0x05,                                                     // bFunctionLength
    USB_DESC_CS_INTERFACE,                                    // bDescriptorType
    CDC_DESC_CALL_MANAGEMENT,                                 // bDescriptorSubtype
    0x00,                                                     // bmCapabilities
    0x01,                                                     // bDataInterface

    0x04,                                                     // bFunctionLength
    USB_DESC_CS_INTERFACE,                                    // bDescriptorType
    CDC_DESC_ABSTRACT_CONTROL_MANAGEMENT,                     // bDescriptorSubtype
    (CDC_ACM_CAPBIT_LINE_CODING | CDC_ACM_CAPBIT_SEND_BREAK), // bmCapabilities

    0x05,                                                     // bFunctionLength
    USB_DESC_CS_INTERFACE,                                    // bDescriptorType
    CDC_DESC_UNION,                                           // bDescriptorSubtype
    0x00,                                                     // bControlInterface
    0x01,                                                     // bSubordinateInterface0

    0x07,                                                     // bLength
    USB_DESC_ENDPOINT,                                        // bDescriptorType
    0x81,                                                     // bEndpointAddress
    0x03,                                                     // bmAttributes
    USB_U16_TO_U8_LSB(8),                                     // wMaxPacketSize
    0x0A,                                                     // bInterval

    0x09,                                                     // bLength
    USB_DESC_INTERFACE,                                       // bDescriptorType
    0x01,                                                     // bInterfaceNumber
    0x00,                                                     // bAlternateSetting
    0x02,                                                     // bNumEndpoints
    USB_CLASS_CDC_DATA,                                       // bInterfaceClass
    0x00,                                                     // bInterfaceSubClass
    0x00,                                                     // bInterfaceProtocol
    0x00,                                                     // iInterface

    0x07,                                                     // bLength
    USB_DESC_ENDPOINT,                                        // bDescriptorType
    0x82,                                                     // bEndpointAddress
    0x02,                                                     // bmAttributes
    USB_U16_TO_U8_LSB(64),                                    // wMaxPacketSize
    0x00,                                                     // bInterval

    0x07,                                                     // bLength
    USB_DESC_ENDPOINT,                                        // bDescriptorType
    0x02,                                                     // bEndpointAddress
    0x02,                                                     // bmAttributes
    USB_U16_TO_U8_LSB(64),                                    // wMaxPacketSize
    0x00,                                                     // bInterval
};
