/**
 * @file usbd_driver_public.h
 * @author Links (lhd@wch.cn)
 * @brief USB device core public header file
 *
 * @copyright Copyright (c) 2026
 *
 */

#ifndef USBD_DRIVER_CORE_PUBLIC_H
#define USBD_DRIVER_CORE_PUBLIC_H

/* @include */
#include "usb_define.h"

#ifdef __cplusplus
extern "C" {
#endif

/* @struct */
typedef struct usbd_handle usbd_handle_t;

/* @function declaration */
bool usbd_drv_open(usbd_handle_t *h, usb_speed_t speed, bool sof_en);

#ifdef __cplusplus
}
#endif

#endif // USBD_DRIVER_CORE_PUBLIC_H
