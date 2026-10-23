/********************************** (C) COPYRIGHT *******************************
* File Name          : descriptor.c
* Author             : WCH
* Version            : V1.0.0
* Date               : 2026/10/20
* Description        : USB descriptor for CDC-ECM example
*********************************************************************************
* Copyright (c) 2026 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

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
    .idProduct = 0xFE32,
    .bcdDevice = USB_DRIVER_VERSION_NUMBER,
    .iManufacturer = 1,
    .iProduct = 2,
    .iSerialNumber = 3,
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
    0x09,                                // bLength
    USB_DESC_CONFIGURATION,              // bDescriptorType (Configuration)
    USB_U16_TO_U8_LSB(80),               // wTotalLength 80
    0x02,                                // bNumInterfaces 2
    0x01,                                // bConfigurationValue
    0x00,                                // iConfiguration (String Index)
    0x80,                                // bmAttributes
    0x32,                                // bMaxPower 100mA

    0x09,                                // bLength
    USB_DESC_INTERFACE,                  // bDescriptorType (Interface)
    0x00,                                // bInterfaceNumber
    0x00,                                // bAlternateSetting
    0x01,                                // bNumEndpoints
    USB_CLASS_CDC,                       // bInterfaceClass
    CDC_SUBCLASS_ETHERNET_CONTROL_MODEL, // bInterfaceSubClass
    CDC_PROTOCOL_NONE,                   // bInterfaceProtocol
    0x00,                                // iInterface

    0x05,                                // bFunctionLength
    USB_DESC_CS_INTERFACE,               // bDescriptorType
    CDC_DESC_HEADER,                     // bDescriptorSubtype
    USB_U16_TO_U8_LSB(0x0120),           // bcdCDC

    0x05,                                // bFunctionLength
    USB_DESC_CS_INTERFACE,               // bDescriptorType
    CDC_DESC_UNION,                      // bDescriptorSubtype
    0x00,                                // bControlInterface
    0x01,                                // bSubordinateInterface0

    0x0D,                                // bFunctionLength
    USB_DESC_CS_INTERFACE,               // bDescriptorType
    CDC_DESC_ETHERNET_NETWORKING,        // bDescriptorSubtype
    0x04,                                // iMACAddress
    USB_U32_TO_U8_LSB(0),                // bmEthernetStatistics
    USB_U16_TO_U8_LSB(1514),             // wMaxSegmentSize
    USB_U16_TO_U8_LSB(0),                // wNumberMCFilters
    0x00,                                // bNumberPowerFilters

    0x07,                                // bLength
    USB_DESC_ENDPOINT,                   // bDescriptorType
    0x81,                                // bEndpointAddress
    0x03,                                // bmAttributes
    USB_U16_TO_U8_LSB(16),               // wMaxPacketSize
    0x0A,                                // bInterval

    0x09,                                // bLength
    USB_DESC_INTERFACE,                  // bDescriptorType
    0x01,                                // bInterfaceNumber
    0x00,                                // bAlternateSetting
    0x00,                                // bNumEndpoints
    USB_CLASS_CDC_DATA,                  // bInterfaceClass
    0x00,                                // bInterfaceSubClass
    0x00,                                // bInterfaceProtocol
    0x00,                                // iInterface

    0x09,                                // bLength
    USB_DESC_INTERFACE,                  // bDescriptorType
    0x01,                                // bInterfaceNumber
    0x01,                                // bAlternateSetting
    0x02,                                // bNumEndpoints
    USB_CLASS_CDC_DATA,                  // bInterfaceClass
    0x00,                                // bInterfaceSubClass
    0x00,                                // bInterfaceProtocol
    0x00,                                // iInterface

    0x07,                                // bLength
    USB_DESC_ENDPOINT,                   // bDescriptorType
    0x82,                                // bEndpointAddress
    0x02,                                // bmAttributes
    USB_U16_TO_U8_LSB(512),              // wMaxPacketSize
    0x00,                                // bInterval

    0x07,                                // bLength
    USB_DESC_ENDPOINT,                   // bDescriptorType
    0x02,                                // bEndpointAddress
    0x02,                                // bmAttributes
    USB_U16_TO_U8_LSB(512),              // wMaxPacketSize
    0x00,                                // bInterval
};

const uint8_t config_desc_fs[] = {
    0x09,                                // bLength
    USB_DESC_CONFIGURATION,              // bDescriptorType (Configuration)
    USB_U16_TO_U8_LSB(80),               // wTotalLength 80
    0x02,                                // bNumInterfaces 2
    0x01,                                // bConfigurationValue
    0x00,                                // iConfiguration (String Index)
    0x80,                                // bmAttributes
    0x32,                                // bMaxPower 100mA

    0x09,                                // bLength
    USB_DESC_INTERFACE,                  // bDescriptorType (Interface)
    0x00,                                // bInterfaceNumber
    0x00,                                // bAlternateSetting
    0x01,                                // bNumEndpoints
    USB_CLASS_CDC,                       // bInterfaceClass
    CDC_SUBCLASS_ETHERNET_CONTROL_MODEL, // bInterfaceSubClass
    CDC_PROTOCOL_NONE,                   // bInterfaceProtocol
    0x00,                                // iInterface

    0x05,                                // bFunctionLength
    USB_DESC_CS_INTERFACE,               // bDescriptorType
    CDC_DESC_HEADER,                     // bDescriptorSubtype
    USB_U16_TO_U8_LSB(0x0120),           // bcdCDC

    0x05,                                // bFunctionLength
    USB_DESC_CS_INTERFACE,               // bDescriptorType
    CDC_DESC_UNION,                      // bDescriptorSubtype
    0x00,                                // bControlInterface
    0x01,                                // bSubordinateInterface0

    0x0D,                                // bFunctionLength
    USB_DESC_CS_INTERFACE,               // bDescriptorType
    CDC_DESC_ETHERNET_NETWORKING,        // bDescriptorSubtype
    0x04,                                // iMACAddress
    USB_U32_TO_U8_LSB(0),                // bmEthernetStatistics
    USB_U16_TO_U8_LSB(1514),             // wMaxSegmentSize
    USB_U16_TO_U8_LSB(0),                // wNumberMCFilters
    0x00,                                // bNumberPowerFilters

    0x07,                                // bLength
    USB_DESC_ENDPOINT,                   // bDescriptorType
    0x81,                                // bEndpointAddress
    0x03,                                // bmAttributes
    USB_U16_TO_U8_LSB(16),               // wMaxPacketSize
    0x0A,                                // bInterval

    0x09,                                // bLength
    USB_DESC_INTERFACE,                  // bDescriptorType
    0x01,                                // bInterfaceNumber
    0x00,                                // bAlternateSetting
    0x00,                                // bNumEndpoints
    USB_CLASS_CDC_DATA,                  // bInterfaceClass
    0x00,                                // bInterfaceSubClass
    0x00,                                // bInterfaceProtocol
    0x00,                                // iInterface

    0x09,                                // bLength
    USB_DESC_INTERFACE,                  // bDescriptorType
    0x01,                                // bInterfaceNumber
    0x01,                                // bAlternateSetting
    0x02,                                // bNumEndpoints
    USB_CLASS_CDC_DATA,                  // bInterfaceClass
    0x00,                                // bInterfaceSubClass
    0x00,                                // bInterfaceProtocol
    0x00,                                // iInterface

    0x07,                                // bLength
    USB_DESC_ENDPOINT,                   // bDescriptorType
    0x82,                                // bEndpointAddress
    0x02,                                // bmAttributes
    USB_U16_TO_U8_LSB(64),               // wMaxPacketSize
    0x00,                                // bInterval

    0x07,                                // bLength
    USB_DESC_ENDPOINT,                   // bDescriptorType
    0x02,                                // bEndpointAddress
    0x02,                                // bmAttributes
    USB_U16_TO_U8_LSB(64),               // wMaxPacketSize
    0x00,                                // bInterval
};
