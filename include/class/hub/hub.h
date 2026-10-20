/********************************** (C) COPYRIGHT *******************************
* File Name          : hub.h
* Author             : WCH
* Version            : V1.0.0
* Date               : 2026/10/20
* Description        : USB HUB class definition header file
*********************************************************************************
* Copyright (c) 2026 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

#ifndef HUB_H
#define HUB_H

/* @include */
#include "usb_define.h"

#ifdef __cplusplus
extern "C" {
#endif

/* @enum */
typedef enum
{
    HUB_PROTOCOL_FULL_SPEED = 0x00,
    HUB_PROTOCOL_HIGH_SPEED_STT = 0x01,
    HUB_PROTOCOL_HIGH_SPEED_MTT = 0x02,
} hub_protocol_t;

typedef enum
{
    HUB_DESC_HUB = 0x29,
} hub_desc_t;

typedef enum
{
    HUB_REQ_GET_STATUS = 0x00,
    HUB_REQ_CLEAR_FEATURE = 0x01,
    HUB_REQ_SET_FEATURE = 0x03,
    HUB_REQ_GET_DESCRIPTOR = 0x06,
    HUB_REQ_SET_DESCRIPTOR = 0x07,
    HUB_REQ_CLEAR_TT_BUFFER = 0x08,
    HUB_REQ_RESET_TT = 0x09,
    HUB_REQ_GET_TT_STATE = 0x0A,
    HUB_REQ_STOP_TT = 0x0B,
} hub_request_t;

typedef enum
{
    HUB_FEATURE_C_HUB_LOCAL_POWER = 0x00,
    HUB_FEATURE_C_HUB_OVER_CURRENT = 0x01,

    HUB_FEATURE_PORT_CONNECTION = 0x00,
    HUB_FEATURE_PORT_ENABLE = 0x01,
    HUB_FEATURE_PORT_SUSPEND = 0x02,
    HUB_FEATURE_PORT_OVER_CURRENT = 0x03,
    HUB_FEATURE_PORT_RESET = 0x04,
    HUB_FEATURE_PORT_POWER = 0x08,
    HUB_FEATURE_PORT_LOW_SPEED = 0x09,
    HUB_FEATURE_C_PORT_CONNECTION = 0x10,
    HUB_FEATURE_C_PORT_ENABLE = 0x11,
    HUB_FEATURE_C_PORT_SUSPEND = 0x12,
    HUB_FEATURE_C_PORT_OVER_CURRENT = 0x13,
    HUB_FEATURE_C_PORT_RESET = 0x14,
    HUB_FEATURE_PORT_TEST = 0x15,
    HUB_FEATURE_PORT_INDICATOR = 0x16,
} hub_feature_t;

/* @struct */
typedef struct
{
    union
    {
        uint16_t status;

        struct
        {
            uint16_t local_power : 1;
            uint16_t over_current : 1;
            uint16_t reserved : 14;
        } status_bits;
    };

    union
    {
        uint16_t change;

        struct
        {
            uint16_t local_power : 1;
            uint16_t over_current : 1;
            uint16_t reserved : 14;
        } change_bits;
    };
} hub_status_t;

typedef struct
{
    union
    {
        uint16_t status;

        struct
        {
            uint16_t connect : 1;
            uint16_t enable : 1;
            uint16_t suspend : 1;
            uint16_t over_current : 1;
            uint16_t reset : 1;
            uint16_t reserved1 : 3;
            uint16_t power : 1;
            uint16_t low_speed : 1;
            uint16_t high_speed : 1;
            uint16_t test_mode : 1;
            uint16_t port_indicator : 1;
            uint16_t reserved2 : 3;
        } status_bits;
    };

    union
    {
        uint16_t change;

        struct
        {
            uint16_t connect : 1;
            uint16_t enable : 1;
            uint16_t suspend : 1;
            uint16_t over_current : 1;
            uint16_t reset : 1;
        } change_bits;
    };
} hub_port_status_t;

/* @struct */
typedef struct __attribute__((packed))
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bNbrPorts;
    uint16_t wHubCharacteristics;
    uint8_t bPwrOn2PwrGood;
    uint8_t bHubContrCurrent;
    uint8_t DeviceRemovable;
    uint8_t PortPwrCtrlMask[];
} usb_desc_hub_t;

#ifdef __cplusplus
}
#endif

#endif // HUB_H
