/********************************** (C) COPYRIGHT *******************************
* File Name          : descriptor.c
* Author             : WCH
* Version            : V1.0.0
* Date               : 2026/10/20
* Description        : USB descriptor for UAC Headset example
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
    .bcdUSB = 0x0110,
    .bDeviceClass = USB_CLASS_AUDIO,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,
    .bMaxPacketSize0 = 0x40,
    .idVendor = 0x1A86,
    .idProduct = 0xFE33,
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
    .bDeviceClass = USB_CLASS_AUDIO,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,
    .bMaxPacketSize0 = 0x40,
    .bNumConfigurations = 0x01,
    .bReserved = 0x00,
};

const uint8_t config_desc[] = {
    0x09,                                                     // bLength
    USB_DESC_CONFIGURATION,                                   // bDescriptorType (Configuration)
    USB_U16_TO_U8_LSB(174),                                   // wTotalLength 174
    0x03,                                                     // bNumInterfaces 3
    0x01,                                                     // bConfigurationValue
    0x00,                                                     // iConfiguration (String Index)
    0x80,                                                     // bmAttributes
    0x32,                                                     // bMaxPower 100mA

    0x09,                                                     // bLength
    USB_DESC_INTERFACE,                                       // bDescriptorType (Interface)
    0x00,                                                     // bInterfaceNumber 0
    0x00,                                                     // bAlternateSetting
    0x00,                                                     // bNumEndpoints 0
    USB_CLASS_AUDIO,                                          // bInterfaceClass:(Audio  )
    UAC_SUBCLASS_AUDIO_CONTROL,                               // bInterfaceSubClass (Audio Control)
    UAC_PROTOCOL_UNDEFINED,                                   // bInterfaceProtocol
    0x00,                                                     // iInterface (String Index)

    0x0A,                                                     // bLength
    USB_DESC_CS_INTERFACE,                                    // bDescriptorType (See Next Line)
    UAC_AC_DESC_HEADER,                                       // bDescriptorSubtype (CS_INTERFACE -> HEADER)
    USB_U16_TO_U8_LSB(0x0100),                                // bcdADC 1.00
    USB_U16_TO_U8_LSB(52),                                    // wTotalLength 52
    0x02,                                                     // bInCollection 0x02
    0x01,                                                     // baInterfaceNr 1
    0x02,                                                     // baInterfaceNr 2

    0x0C,                                                     // bLength
    USB_DESC_CS_INTERFACE,                                    // bDescriptorType (See Next Line)
    UAC_AC_DESC_INPUT_TERMINAL,                               // bDescriptorSubtype (CS_INTERFACE -> INPUT_TERMINAL)
    0x01,                                                     // bTerminalID
    USB_U16_TO_U8_LSB(UAC_TERMINAL_USB_TYPE_STREAMING),       // wTerminalType (USB Streaming)
    0x00,                                                     // bAssocTerminal
    0x02,                                                     // bNrChannels 2
    USB_U16_TO_U8_LSB(0x0000),                                // wChannelConfig
    0x00,                                                     // iChannelNames
    0x00,                                                     // iTerminal

    0x09,                                                     // bLength
    USB_DESC_CS_INTERFACE,                                    // bDescriptorType (See Next Line)
    UAC_AC_DESC_OUTPUT_TERMINAL,                              // bDescriptorSubtype (CS_INTERFACE -> OUTPUT_TERMINAL)
    0x02,                                                     // bTerminalID
    USB_U16_TO_U8_LSB(UAC_TERMINAL_OUTPUT_GENERIC_SPEAKER),   // wTerminalType (Speaker)
    0x00,                                                     // bAssocTerminal
    0x01,                                                     // bSourceID
    0x00,                                                     // iTerminal

    0x0C,                                                     // bLength
    USB_DESC_CS_INTERFACE,                                    // bDescriptorType (See Next Line)
    UAC_AC_DESC_INPUT_TERMINAL,                               // bDescriptorSubtype (CS_INTERFACE -> INPUT_TERMINAL)
    0x03,                                                     // bTerminalID
    USB_U16_TO_U8_LSB(UAC_TERMINAL_INPUT_GENERIC_MICROPHONE), // wTerminalType (Microphone)
    0x00,                                                     // bAssocTerminal
    0x01,                                                     // bNrChannels 1
    USB_U16_TO_U8_LSB(0x0000),                                // wChannelConfig
    0x00,                                                     // iChannelNames
    0x00,                                                     // iTerminal

    0x09,                                                     // bLength
    USB_DESC_CS_INTERFACE,                                    // bDescriptorType (See Next Line)
    UAC_AC_DESC_OUTPUT_TERMINAL,                              // bDescriptorSubtype (CS_INTERFACE -> OUTPUT_TERMINAL)
    0x04,                                                     // bTerminalID
    USB_U16_TO_U8_LSB(UAC_TERMINAL_USB_TYPE_STREAMING),       // wTerminalType (USB Streaming)
    0x00,                                                     // bAssocTerminal
    0x03,                                                     // bSourceID
    0x00,                                                     // iTerminal

    0x09,                                                     // bLength
    USB_DESC_INTERFACE,                                       // bDescriptorType (Interface)
    0x01,                                                     // bInterfaceNumber 1
    0x00,                                                     // bAlternateSetting
    0x00,                                                     // bNumEndpoints 0
    USB_CLASS_AUDIO,                                          // bInterfaceClass:(Audio  )
    UAC_SUBCLASS_AUDIO_STREAMING,                             // bInterfaceSubClass (Audio Streaming)
    UAC_PROTOCOL_UNDEFINED,                                   // bInterfaceProtocol
    0x00,                                                     // iInterface (String Index)

    0x09,                                                     // bLength
    USB_DESC_INTERFACE,                                       // bDescriptorType (Interface)
    0x01,                                                     // bInterfaceNumber 1
    0x01,                                                     // bAlternateSetting
    0x01,                                                     // bNumEndpoints 1
    USB_CLASS_AUDIO,                                          // bInterfaceClass:(Audio  )
    UAC_SUBCLASS_AUDIO_STREAMING,                             // bInterfaceSubClass (Audio Streaming)
    UAC_PROTOCOL_UNDEFINED,                                   // bInterfaceProtocol
    0x00,                                                     // iInterface (String Index)

    0x07,                                                     // bLength
    USB_DESC_CS_INTERFACE,                                    // bDescriptorType (See Next Line)
    UAC_AS_DESC_GENERAL,                                      // bDescriptorSubtype (CS_INTERFACE -> AS_GENERAL)
    0x01,                                                     // bTerminalLink
    0x01,                                                     // bDelay 1
    USB_U16_TO_U8_LSB(0x0001),                                // wFormatTag (PCM)

    0x0B,                                                     // bLength
    USB_DESC_CS_INTERFACE,                                    // bDescriptorType (See Next Line)
    UAC_AS_DESC_FORMAT_TYPE,                                  // bDescriptorSubtype (CS_INTERFACE -> FORMAT_TYPE)
    0x01,                                                     // bFormatType 1
    0x02,                                                     // bNrChannels (Stereo)
    0x02,                                                     // bSubFrameSize 2
    0x10,                                                     // bBitResolution 16
    0x01,                                                     // bSamFreqType 1
    USB_U24_TO_U8_LSB(48000),                                 // tSamFreq[1] 48000 Hz

    0x09,                                                     // bLength
    USB_DESC_ENDPOINT,                                        // bDescriptorType (See Next Line)
    0x01,                                                     // bEndpointAddress (OUT/H2D)
    0x09,                                                     // bmAttributes (Isochronous, Adaptive, Data EP)
    USB_U16_TO_U8_LSB(200),                                   // wMaxPacketSize 200
    0x01,                                                     // bInterval 1 (unit depends on device speed)
    0x00,                                                     // bRefresh
    0x00,                                                     // bSyncAddress

    0x07,                                                     // bLength
    USB_DESC_CS_ENDPOINT,                                     // bDescriptorType (See Next Line)
    UAC_ENDP_SUBTYPE_GENERAL,                                 // bDescriptorSubtype (CS_ENDPOINT -> EP_GENERAL)
    0x00,                                                     // bmAttributes (None)
    0x01,                                                     // bLockDelayUnits
    USB_U16_TO_U8_LSB(1),                                     // wLockDelay 1

    0x09,                                                     // bLength
    USB_DESC_INTERFACE,                                       // bDescriptorType (Interface)
    0x02,                                                     // bInterfaceNumber 2
    0x00,                                                     // bAlternateSetting
    0x00,                                                     // bNumEndpoints 0
    USB_CLASS_AUDIO,                                          // bInterfaceClass:(Audio  )
    UAC_SUBCLASS_AUDIO_STREAMING,                             // bInterfaceSubClass (Audio Streaming)
    UAC_PROTOCOL_UNDEFINED,                                   // bInterfaceProtocol
    0x00,                                                     // iInterface (String Index)

    0x09,                                                     // bLength
    USB_DESC_INTERFACE,                                       // bDescriptorType (Interface)
    0x02,                                                     // bInterfaceNumber 2
    0x01,                                                     // bAlternateSetting
    0x01,                                                     // bNumEndpoints 1
    USB_CLASS_AUDIO,                                          // bInterfaceClass:(Audio  )
    UAC_SUBCLASS_AUDIO_STREAMING,                             // bInterfaceSubClass (Audio Streaming)
    UAC_PROTOCOL_UNDEFINED,                                   // bInterfaceProtocol
    0x00,                                                     // iInterface (String Index)

    0x07,                                                     // bLength
    USB_DESC_CS_INTERFACE,                                    // bDescriptorType (See Next Line)
    UAC_AS_DESC_GENERAL,                                      // bDescriptorSubtype (CS_INTERFACE -> AS_GENERAL)
    0x04,                                                     // bTerminalLink
    0x01,                                                     // bDelay 1
    USB_U16_TO_U8_LSB(0x0001),                                // wFormatTag (PCM)

    0x0B,                                                     // bLength
    USB_DESC_CS_INTERFACE,                                    // bDescriptorType (See Next Line)
    UAC_AS_DESC_FORMAT_TYPE,                                  // bDescriptorSubtype (CS_INTERFACE -> FORMAT_TYPE)
    0x01,                                                     // bFormatType 1
    0x01,                                                     // bNrChannels (Mono)
    0x02,                                                     // bSubFrameSize 2
    0x10,                                                     // bBitResolution 16
    0x01,                                                     // bSamFreqType 1
    USB_U24_TO_U8_LSB(48000),                                 // tSamFreq[1] 48000 Hz

    0x09,                                                     // bLength
    USB_DESC_ENDPOINT,                                        // bDescriptorType (See Next Line)
    0x82,                                                     // bEndpointAddress (IN/D2H)
    0x09,                                                     // bmAttributes (Isochronous, Adaptive, Data EP)
    USB_U16_TO_U8_LSB(200),                                   // wMaxPacketSize 200
    0x01,                                                     // bInterval 1 (unit depends on device speed)
    0x00,                                                     // bRefresh
    0x00,                                                     // bSyncAddress

    0x07,                                                     // bLength
    USB_DESC_CS_ENDPOINT,                                     // bDescriptorType (See Next Line)
    UAC_ENDP_SUBTYPE_GENERAL,                                 // bDescriptorSubtype (CS_ENDPOINT -> EP_GENERAL)
    0x00,                                                     // bmAttributes (None)
    0x00,                                                     // bLockDelayUnits
    USB_U16_TO_U8_LSB(0),                                     // wLockDelay 0
};
