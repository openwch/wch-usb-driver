/********************************** (C) COPYRIGHT *******************************
* File Name          : board.h
* Author             : WCH
* Version            : V1.0.0
* Date               : 2026/10/15
* Description        : Board support package
*********************************************************************************
* Copyright (c) 2026 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

#ifndef BOARD_H
#define BOARD_H

/* @include */
#include "usb_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

/* @define */
#ifndef USB_COUNT
#define USB_COUNT 1
#endif

/* @enum */
typedef enum
{
    USB_MODE_IDLE,
    USB_MODE_DEVICE,
    USB_MODE_HOST,
} usb_mode_t;

/* @function declarations */
void board_init(void);
void *board_usb_init(uint8_t index, usb_mode_t mode);
void board_usb_deinit(uint8_t index);

#ifdef __cplusplus
}
#endif

#endif
