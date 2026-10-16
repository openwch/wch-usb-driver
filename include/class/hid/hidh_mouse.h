/********************************** (C) COPYRIGHT *******************************
* File Name          : hidh_mouse.h
* Author             : WCH
* Version            : V1.0.0
* Date               : 2026/10/15
* Description        : USB HID host mouse class driver
*********************************************************************************
* Copyright (c) 2026 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

#ifndef HIDH_MOUSE_H
#define HIDH_MOUSE_H

/* @include */
#include "usb_define.h"
#include "class/hid/hid.h"

#ifdef __cplusplus
extern "C" {
#endif

/* @struct */
typedef struct
{
    uint8_t buttons;
    uint32_t x;
    uint32_t y;
} hidh_mouse_status_t;

typedef struct
{
    int32_t dx;
    int32_t dy;
    int32_t wheel;
} hidh_mouse_change_t;

/* extern */
extern hidh_mouse_status_t hidh_mouse_status;

/* @function pointer */
typedef void (*hidh_mouse_changed_cb)(hidh_mouse_status_t *status, hidh_mouse_change_t *change);

/* @function declarations */

/**
 * @brief Register a callback function for mouse state changes
 * 
 * @param cb Callback function to be called when the mouse state changes
 */
void hidh_register_mouse_changed_cb(hidh_mouse_changed_cb cb);

/**
 * @brief Unregister the callback function for mouse state changes
 * 
 */
void hidh_unregister_mouse_changed_cb(void);

#ifdef __cplusplus
}
#endif

#endif // HIDH_MOUSE_H
