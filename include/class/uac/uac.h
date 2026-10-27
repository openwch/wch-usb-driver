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

/* @enum */
typedef enum
{
    UAC_SUBCLASS_AUDIO_CONTROL = 0x01,
    UAC_SUBCLASS_AUDIO_STREAMING = 0x02,
    UAC_SUBCLASS_MIDI_STREAMING = 0x03,
} uac_subclass_t;

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

#ifdef __cplusplus
}
#endif

#endif // UAC_H
