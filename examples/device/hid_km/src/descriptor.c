/**
 * @file descriptor.c
 * @author Links (lhd@wch.cn)
 * @brief USB descriptor for HID keyboard and mouse example
 *
 * @copyright Copyright (c) 2026
 *
 */

/* @include */
#include "usb_driver.h"

/* @global */
static usb_desc_device_t device_desc = {
    .bLength = sizeof(usb_desc_device_t),
    .bDescriptorType = USB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0x00,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,
    .bMaxPacketSize0 = 64,
    .idVendor = 0x1A86,
    .idProduct = 0xFE64,
    .bcdDevice = 0x0100,
    .iManufacturer = 1,
    .iProduct = 2,
    .iSerialNumber = 3,
    .bNumConfigurations = 1,
};
