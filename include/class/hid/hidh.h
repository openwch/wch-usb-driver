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

#ifdef __cplusplus
}
#endif

#endif // HIDH_H
