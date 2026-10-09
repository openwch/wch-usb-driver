/**
 * @file descriptor.c
 * @author Links (lhd@wch.cn)
 * @brief USB descriptor for HID keyboard and mouse example
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
    .bcdUSB = 0x0110,
    .bDeviceClass = 0x00,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,
    .bMaxPacketSize0 = 0x40,
    .idVendor = 0x1A86,
    .idProduct = 0xFE30,
    .bcdDevice = USB_DRIVER_VERSION_NUMBER,
    .iManufacturer = 1,
    .iProduct = 2,
    .iSerialNumber = 3,
    .bNumConfigurations = 1,
};

const usb_desc_qualifier_t qua_desc = {
    .bLength = 0x0A,
    .bDescriptorType = 0x06,
    .bcdUSB = 0x0110,
    .bDeviceClass = 0x00,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,
    .bMaxPacketSize0 = 0x40,
    .bNumConfigurations = 0x01,
    .bReserved = 0x00,
};

const uint8_t config_desc[] = {
    0x09,                  // bLength
    0x02,                  // bDescriptorType (Configuration)
    USB_U16_TO_U8_LSB(59), // wTotalLength 59
    0x02,                  // bNumInterfaces 2
    0x01,                  // bConfigurationValue
    0x00,                  // iConfiguration (String Index)
    0xA0,                  // bmAttributes Remote Wakeup
    0x32,                  // bMaxPower 100mA

    0x09,                  // bLength
    0x04,                  // bDescriptorType (Interface)
    0x00,                  // bInterfaceNumber 0
    0x00,                  // bAlternateSetting
    0x01,                  // bNumEndpoints 1
    0x03,                  // bInterfaceClass
    0x01,                  // bInterfaceSubClass
    0x01,                  // bInterfaceProtocol
    0x00,                  // iInterface (String Index)

    0x09,                  // bLength
    0x21,                  // bDescriptorType (HID)
    0x11,
    0x01,                  // bcdHID 1.11
    0x00,                  // bCountryCode
    0x01,                  // bNumDescriptors
    0x22,                  // bDescriptorType[0] (HID)
    USB_U16_TO_U8_LSB(62), // wDescriptorLength[0]

    0x07,                  // bLength
    0x05,                  // bDescriptorType (Endpoint)
    0x81,                  // bEndpointAddress (IN/D2H)
    0x03,                  // bmAttributes (Interrupt)
    USB_U16_TO_U8_LSB(8),  // wMaxPacketSize
    0x01,                  // bInterval 1 (unit depends on device speed)

    0x09,                  // bLength
    0x04,                  // bDescriptorType (Interface)
    0x01,                  // bInterfaceNumber 1
    0x00,                  // bAlternateSetting
    0x01,                  // bNumEndpoints 1
    0x03,                  // bInterfaceClass
    0x01,                  // bInterfaceSubClass
    0x02,                  // bInterfaceProtocol
    0x00,                  // iInterface (String Index)

    0x09,                  // bLength
    0x21,                  // bDescriptorType (HID)
    0x11,
    0x01,                  // bcdHID 1.11
    0x00,                  // bCountryCode
    0x01,                  // bNumDescriptors
    0x22,                  // bDescriptorType[0] (HID)
    USB_U16_TO_U8_LSB(52), // wDescriptorLength[0]

    0x07,                  // bLength
    0x05,                  // bDescriptorType (Endpoint)
    0x82,                  // bEndpointAddress (IN/D2H)
    0x03,                  // bmAttributes (Interrupt)
    USB_U16_TO_U8_LSB(8),  // wMaxPacketSize
    0x01,                  // bInterval 1 (unit depends on device speed)
};

const uint8_t keyboard_report_desc[] = {
    0x05, 0x01,       // Usage Page (Generic Desktop Ctrls)
    0x09, 0x06,       // Usage (Keyboard)
    0xA1, 0x01,       // Collection (Application)
    0x05, 0x07,       //   Usage Page (Kbrd/Keypad)
    0x19, 0xE0,       //   Usage Minimum (0xE0)
    0x29, 0xE7,       //   Usage Maximum (0xE7)
    0x15, 0x00,       //   Logical Minimum (0)
    0x25, 0x01,       //   Logical Maximum (1)
    0x75, 0x01,       //   Report Size (1)
    0x95, 0x08,       //   Report Count (8)
    0x81, 0x02,       //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x95, 0x01,       //   Report Count (1)
    0x75, 0x08,       //   Report Size (8)
    0x81, 0x01,       //   Input (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x95, 0x03,       //   Report Count (3)
    0x75, 0x01,       //   Report Size (1)
    0x05, 0x08,       //   Usage Page (LEDs)
    0x19, 0x01,       //   Usage Minimum (Num Lock)
    0x29, 0x03,       //   Usage Maximum (Scroll Lock)
    0x91, 0x02,       //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x95, 0x05,       //   Report Count (5)
    0x75, 0x01,       //   Report Size (1)
    0x91, 0x01,       //   Output (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x95, 0x06,       //   Report Count (6)
    0x75, 0x08,       //   Report Size (8)
    0x26, 0xFF, 0x00, //   Logical Maximum (255)
    0x05, 0x07,       //   Usage Page (Kbrd/Keypad)
    0x19, 0x00,       //   Usage Minimum (0x00)
    0x29, 0x91,       //   Usage Maximum (0x91)
    0x81, 0x00,       //   Input (Data,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,             // End Collection
};

const uint8_t mouse_report_desc[] = {
    0x05, 0x01, // Usage Page (Generic Desktop Ctrls)
    0x09, 0x02, // Usage (Mouse)
    0xA1, 0x01, // Collection (Application)
    0x09, 0x01, //   Usage (Pointer)
    0xA1, 0x00, //   Collection (Physical)
    0x05, 0x09, //     Usage Page (Button)
    0x19, 0x01, //     Usage Minimum (0x01)
    0x29, 0x03, //     Usage Maximum (0x03)
    0x15, 0x00, //     Logical Minimum (0)
    0x25, 0x01, //     Logical Maximum (1)
    0x75, 0x01, //     Report Size (1)
    0x95, 0x03, //     Report Count (3)
    0x81, 0x02, //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x75, 0x05, //     Report Size (5)
    0x95, 0x01, //     Report Count (1)
    0x81, 0x01, //     Input (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x01, //     Usage Page (Generic Desktop Ctrls)
    0x09, 0x30, //     Usage (X)
    0x09, 0x31, //     Usage (Y)
    0x09, 0x38, //     Usage (Wheel)
    0x15, 0x81, //     Logical Minimum (-127)
    0x25, 0x7F, //     Logical Maximum (127)
    0x75, 0x08, //     Report Size (8)
    0x95, 0x03, //     Report Count (3)
    0x81, 0x06, //     Input (Data,Var,Rel,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,       //   End Collection
    0xC0,       // End Collection
};
