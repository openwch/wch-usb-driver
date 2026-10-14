/**
 * @file hidh.h
 * @author Links (lhd@wch.cn)
 * @brief USB HID host class driver
 *
 * @copyright Copyright (c) 2026
 *
 */

#ifndef HIDH_H
#define HIDH_H

/* @include */
#include "usb_define.h"
#include "class/hid/hid.h"
#include "host/usbh_driver_public.h"

#ifdef __cplusplus
extern "C" {
#endif

/* @struct */
typedef struct
{
    uint32_t button_status_bitmap[256 / 8 / 4];
} hidh_keyboard_t;

typedef struct
{
    uint8_t button;
    int32_t dx;
    int32_t dy;
    int32_t wheel;
} hidh_mouse_t;

/* extern */
extern hidh_keyboard_t hidh_keyboard;
extern hidh_mouse_t hidh_mouse;

/* @function pointer */
typedef void (*hidh_keyboard_changed_cb)(hidh_keyboard_t *keyboard);
typedef void (*hidh_mouse_changed_cb)(hidh_mouse_t *mouse);

/* @function declarations */

/**
 * @brief Handle the USB HID host driver tasks
 * 
 */
void hidh_drv_task(void);

/**
 * @brief Open the USB HID host driver
 * 
 * @param h USB host handle
 * 
 * @return Operation result
 */
bool hidh_drv_open(usbh_handle_t *h);

/**
 * @brief Close the USB HID host driver
 * 
 * @param h USB host handle
 * 
 * @return Operation result
 */
bool hidh_drv_close(usbh_handle_t *h);

/**
 * @brief Register a callback function for keyboard state changes
 * 
 * @param cb Callback function to be called when the keyboard state changes
 */
void hidh_drv_register_keyboard_changed_cb(hidh_keyboard_changed_cb cb);

/**
 * @brief Register a callback function for mouse state changes
 * 
 * @param cb Callback function to be called when the mouse state changes
 */
void hidh_drv_register_mouse_changed_cb(hidh_mouse_changed_cb cb);

#ifdef __cplusplus
}
#endif

#endif // HIDH_H
