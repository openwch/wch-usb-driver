/**
 * @file usbh_driver_public.h
 * @author Links (lhd@wch.cn)
 * @brief USB host core public header file
 *
 * @copyright Copyright (c) 2026
 *
 */

#ifndef USBH_DRIVER_PUBLIC_H
#define USBH_DRIVER_PUBLIC_H

/* @include */
#include "usb_define.h"

#ifdef __cplusplus
extern "C" {
#endif

/* @typedef */
typedef struct usbh_handle usbh_handle_t;

/* @function declaration */
bool usbh_drv_open(usbh_handle_t *h);
bool usbh_drv_close(usbh_handle_t *h);

#ifdef __cplusplus
}
#endif

#endif // USBH_DRIVER_PUBLIC_H
