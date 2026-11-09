/********************************** (C) COPYRIGHT *******************************
* File Name          : uac.h
* Author             : WCH
* Version            : V1.0.0
* Date               : 2026/10/20
* Description        : USB UAC class definition header file
*********************************************************************************
* Copyright (c) 2026 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

#ifndef UAC_H
#define UAC_H

/* @include */
#include "usb_define.h"

#ifdef __cplusplus
extern "C" {
#endif

/* @define */
#define UAC_CHANNEL_CONFIG_LEFT_FRONT      0x0001
#define UAC_CHANNEL_CONFIG_RIGHT_FRONT     0x0002
#define UAC_CHANNEL_CONFIG_CENTER_FRONT    0x0004
#define UAC_CHANNEL_CONFIG_LOW_FREQUENCY   0x0008
#define UAC_CHANNEL_CONFIG_LEFT_SURROUND   0x0010
#define UAC_CHANNEL_CONFIG_RIGHT_SURROUND  0x0020
#define UAC_CHANNEL_CONFIG_LEFT_CENTER     0x0040
#define UAC_CHANNEL_CONFIG_RIGHT_CENTER    0x0080
#define UAC_CHANNEL_CONFIG_SURROUND        0x0100
#define UAC_CHANNEL_CONFIG_SIDE_LEFT       0x0200
#define UAC_CHANNEL_CONFIG_SIDE_RIGHT      0x0400
#define UAC_CHANNEL_CONFIG_TOP_CENTER      0x0800
#define UAC_CHANNEL_CONFIG_TOP_FRONT_LEFT  0x1000
#define UAC_CHANNEL_CONFIG_TOP_FRONT_RIGHT 0x2000
#define UAC_CHANNEL_CONFIG_TOP_REAR_LEFT   0x4000
#define UAC_CHANNEL_CONFIG_TOP_REAR_RIGHT  0x8000

/* Class-specific AS isochronous audio data endpoint bmAttributes */
#define UAC_AS_ENDP_ATTR_SAMPLING_FREQ     0x01
#define UAC_AS_ENDP_ATTR_PITCH             0x02
#define UAC_AS_ENDP_ATTR_MAX_PACKETS_ONLY  0x80

/* Standard AS isochronous audio data endpoint bmAttributes */
#define UAC_ENDP_ATTR_SYNC_TYPE_MASK       0x0C
#define UAC_ENDP_ATTR_SYNC_TYPE_NONE       0x00
#define UAC_ENDP_ATTR_SYNC_TYPE_ASYNC      0x04
#define UAC_ENDP_ATTR_SYNC_TYPE_ADAPTIVE   0x08
#define UAC_ENDP_ATTR_SYNC_TYPE_SYNC       0x0C

/* @enum */
typedef enum
{
    UAC_SUBCLASS_AUDIO_CONTROL = 0x01,
    UAC_SUBCLASS_AUDIO_STREAMING = 0x02,
    UAC_SUBCLASS_MIDI_STREAMING = 0x03,
} uac_subclass_t;

typedef enum
{
    UAC_PROTOCOL_UNDEFINED = 0x00,
} uac_protocol_t;

typedef enum
{
    UAC_AC_DESC_HEADER = 0x01,
    UAC_AC_DESC_INPUT_TERMINAL = 0x02,
    UAC_AC_DESC_OUTPUT_TERMINAL = 0x03,
    UAC_AC_DESC_MIXER_UNIT = 0x04,
    UAC_AC_DESC_SELECTOR_UNIT = 0x05,
    UAC_AC_DESC_FEATURE_UNIT = 0x06,
    UAC_AC_DESC_PROCESSING_UNIT = 0x07,
    UAC_AC_DESC_EXTENSION_UNIT = 0x08,
} uac_ac_desc_t;

typedef enum
{
    UAC_AS_DESC_GENERAL = 0x01,
    UAC_AS_DESC_FORMAT_TYPE = 0x02,
    UAC_AS_DESC_FORMAT_SPECIFIC = 0x03,
} uac_as_desc_t;

typedef enum
{
    UAC_ENDP_SUBTYPE_GENERAL = 0x01,
} uac_endp_subtype_t;

typedef enum
{
    UAC_FORMAT_TYPE_UNDEFINED = 0x00,
    UAC_FORMAT_TYPE_I = 0x01,
    UAC_FORMAT_TYPE_II = 0x02,
    UAC_FORMAT_TYPE_III = 0x03,
} uac_format_type_t;

typedef enum
{
    UAC_FORMAT_TAG_TYPE_I_UNDEFINED = 0x0000,
    UAC_FORMAT_TAG_TYPE_I_PCM = 0x0001,
    UAC_FORMAT_TAG_TYPE_I_PCM8 = 0x0002,
    UAC_FORMAT_TAG_TYPE_I_IEEE_FLOAT = 0x0003,
    UAC_FORMAT_TAG_TYPE_I_ALAW = 0x0004,
    UAC_FORMAT_TAG_TYPE_I_MULAW = 0x0005,
} uac_format_tag_t;

typedef enum
{
    UAC_REQ_SET_CUR = 0x01,
    UAC_REQ_GET_CUR = 0x81,
    UAC_REQ_SET_MIN = 0x02,
    UAC_REQ_GET_MIN = 0x82,
    UAC_REQ_SET_MAX = 0x03,
    UAC_REQ_GET_MAX = 0x83,
    UAC_REQ_SET_RES = 0x04,
    UAC_REQ_GET_RES = 0x84,
    UAC_REQ_SET_MEM = 0x05,
    UAC_REQ_GET_MEM = 0x85,
    UAC_REQ_GET_STAT = 0xFF,
} uac_request_t;

typedef enum
{
    UAC_TERMINAL_USB_TYPE_UNDEFINED = 0x0100,
    UAC_TERMINAL_USB_TYPE_STREAMING = 0x0101,
    UAC_TERMINAL_USB_TYPE_VENDOR_SPEC = 0x01FF,
} uac_terminal_usb_type_t;

typedef enum
{
    UAC_TERMINAL_INPUT_UNDEFINED = 0x0200,
    UAC_TERMINAL_INPUT_GENERIC_MICROPHONE = 0x0201,
    UAC_TERMINAL_INPUT_DESKTOP_MICROPHONE = 0x0202,
    UAC_TERMINAL_INPUT_PERSONAL_MICROPHONE = 0x0203,
    UAC_TERMINAL_INPUT_OMNI_DIRECTIONAL_MICROPHONE = 0x0204,
    UAC_TERMINAL_INPUT_MICROPHONE_ARRAY = 0x0205,
    UAC_TERMINAL_INPUT_PROCESSING_MICROPHONE_ARRAY = 0x0206,
} uac_terminal_input_type_t;

typedef enum
{
    UAC_TERMINAL_OUTPUT_UNDEFINED = 0x0300,
    UAC_TERMINAL_OUTPUT_GENERIC_SPEAKER = 0x0301,
    UAC_TERMINAL_OUTPUT_HEADPHONES = 0x0302,
    UAC_TERMINAL_OUTPUT_HEAD_MOUNTED_DISPLAY_AUDIO = 0x0303,
    UAC_TERMINAL_OUTPUT_DESKTOP_SPEAKER = 0x0304,
    UAC_TERMINAL_OUTPUT_ROOM_SPEAKER = 0x0305,
    UAC_TERMINAL_OUTPUT_COMMUNICATION_SPEAKER = 0x0306,
    UAC_TERMINAL_OUTPUT_LOW_FREQUENCY_EFFECTS_SPEAKER = 0x0307,
} uac_terminal_output_type_t;

typedef enum
{
    UAC_TERMINAL_SELECT_COPY_PROTECT = 0x01,
} uac_terminal_selector_t;

typedef enum
{
    UAC_FEATURE_UNIT_SELECT_MUTE = 0x01,
    UAC_FEATURE_UNIT_SELECT_VOLUME = 0x02,
    UAC_FEATURE_UNIT_SELECT_BASS = 0x03,
    UAC_FEATURE_UNIT_SELECT_MID = 0x04,
    UAC_FEATURE_UNIT_SELECT_TREBLE = 0x05,
    UAC_FEATURE_UNIT_SELECT_GRAPHIC_EQUALIZER = 0x06,
    UAC_FEATURE_UNIT_SELECT_AUTOMATIC_GAIN = 0x07,
    UAC_FEATURE_UNIT_SELECT_DELAY = 0x08,
    UAC_FEATURE_UNIT_SELECT_BASS_BOOST = 0x09,
    UAC_FEATURE_UNIT_SELECT_LOUDNESS = 0x0A,
} uac_feature_unit_selector_t;

typedef enum
{
    UAC_ENDP_SELECT_SAMPLING_FREQ = 0x01,
    UAC_ENDP_SELECT_PITCH = 0x02,
} uac_endp_selector_t;

/* @struct */
typedef struct __attribute__((packed))
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubType;
    uint16_t bcdADC;
    uint16_t wTotalLength;
    uint8_t bInCollection;
    uint8_t baInterfaceNr[];
} uac_desc_header_t;

typedef struct __attribute__((packed))
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubType;
    uint8_t bTerminalID;
    uint16_t wTerminalType;
    uint8_t bAssocTerminal;
    uint8_t bNrChannels;
    uint16_t wChannelConfig;
    uint8_t iChannelNames;
    uint8_t iTerminal;
} uac_desc_input_terminal_t;

typedef struct __attribute__((packed))
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubType;
    uint8_t bTerminalID;
    uint16_t wTerminalType;
    uint8_t bAssocTerminal;
    uint8_t bSourceID;
    uint8_t iTerminal;
} uac_desc_output_terminal_t;

typedef struct __attribute__((packed))
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubType;
    uint8_t bUnitID;
    uint8_t bNrInPins;
    // uint8_t baSourceID[];
    uint8_t bNrChannels;
    uint16_t wChannelConfig;
    uint8_t iChannelNames;
    // uint8_t bmControls[];
    uint8_t iMixer;
} uac_desc_mixer_unit_t;

typedef struct __attribute__((packed))
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubType;
    uint8_t bUnitID;
    uint8_t bNrInPins;
    // uint8_t baSourceID[];
    uint8_t iSelector;
} uac_desc_select_unit_t;

typedef struct __attribute__((packed))
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubType;
    uint8_t bUnitID;
    uint8_t bSourceID;
    uint8_t bControlSize;
    // uint8_t bmaControls[];
    uint8_t iFeature;
} uac_desc_feature_unit_t;

typedef struct __attribute__((packed))
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubType;
    uint8_t bUnitID;
    uint16_t wProcessType;
    uint8_t bNrInPins;
    // uint8_t baSourceID[];
    uint8_t bNrChannels;
    uint16_t wChannelConfig;
    uint8_t iChannelNames;
    uint8_t bControlSize;
    // uint8_t bmControls[];
    uint8_t iProcessing;
} uac_desc_processing_unit_t;

typedef struct __attribute__((packed))
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubType;
    uint8_t bUnitID;
    uint16_t wExtensionCode;
    uint8_t bNrInPins;
    // uint8_t baSourceID[];
    uint8_t bNrChannels;
    uint16_t wChannelConfig;
    uint8_t iChannelNames;
    uint8_t bControlSize;
    // uint8_t bmControls[];
    uint8_t iExtension;
} uac_desc_extension_unit_t;

typedef struct __attribute__((packed))
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubType;
    uint8_t bTerminalLink;
    uint8_t bDelay;
    uint16_t wFormatTag;
} uac_desc_as_itf_t;

typedef struct __attribute__((packed))
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubType;
    uint8_t bFormatType;
    uint8_t bNrChannels;
    uint8_t bSubframeSize;
    uint8_t bBitResolution;
    uint8_t bSamFreqType;
    uint8_t tSamFreq[];
} uac_desc_as_format_t;

typedef struct __attribute__((packed))
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bEndpointAddress;
    uint8_t bmAttributes;
    uint16_t wMaxPacketSize;
    uint8_t bInterval;
    uint8_t bRefresh;
    uint8_t bSynchAddress;
} uac_desc_audio_endpoint_t;

typedef struct __attribute__((packed))
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubType;
    uint8_t bmAttributes;
    uint8_t bLockDelayUnits;
    uint16_t wLockDelay;
} uac_desc_audio_as_endpoint_t;

#ifdef __cplusplus
}
#endif

#endif // UAC_H
