/********************************** (C) COPYRIGHT *******************************
* File Name          : hidh_keyboard.h
* Author             : WCH
* Version            : V1.0.0
* Date               : 2026/10/15
* Description        : USB HID host keyboard class driver
*********************************************************************************
* Copyright (c) 2026 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

#ifndef HIDH_KEYBOARD_H
#define HIDH_KEYBOARD_H

/* @include */
#include "usb_define.h"
#include "class/hid/hid.h"

#ifdef __cplusplus
extern "C" {
#endif

/* @struct */
typedef struct
{
    uint8_t led_status;
    uint32_t button_status_bitmap[256 / 32];
} hidh_keyboard_t;

/* extern */
extern hidh_keyboard_t hidh_keyboard;

/* @function pointer */
typedef void (*hidh_keyboard_changed_cb)(hidh_keyboard_t *keyboard);

/* @function declarations */

/**
 * @brief Register a callback function for keyboard state changes
 * 
 * @param cb Callback function to be called when the keyboard state changes
 */
void hidh_register_keyboard_changed_cb(hidh_keyboard_changed_cb cb);

/**
 * @brief Unregister the callback function for keyboard state changes
 * 
 */
void hidh_unregister_keyboard_changed_cb(void);

/**
 * @brief Get the status of a specific key on the keyboard
 * 
 * @param key_code The key code of the key to check
 */
bool hidh_keyboard_get_key_status(uint8_t key_code);

#ifdef __cplusplus
}
#endif

#endif // HIDH_KEYBOARD_H
