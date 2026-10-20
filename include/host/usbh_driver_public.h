/********************************** (C) COPYRIGHT *******************************
* File Name          : usbh_driver_public.h
* Author             : WCH
* Version            : V1.0.0
* Date               : 2026/10/20
* Description        : USB host core public header file
*********************************************************************************
* Copyright (c) 2026 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

#ifndef USBH_DRIVER_PUBLIC_H
#define USBH_DRIVER_PUBLIC_H

/* @include */
#include "usb_define.h"

#ifdef __cplusplus
extern "C" {
#endif

/* @typedef */
typedef struct usbh_device usbh_device_t;
typedef struct usbh_handle usbh_handle_t;

/* @function declarations */

/**
 * @brief USB host driver tasks process
 * 
 * @param h USB host driver handle
 */
void usbh_drv_task(usbh_handle_t *h);

/**
 * @brief Open the USB host driver
 * 
 * @param h USB host driver handle
 * 
 * @return Operation result
 */
bool usbh_drv_open(usbh_handle_t *h);

/**
 * @brief Close the USB host driver
 * 
 * @param h USB host driver handle
 * 
 * @return Operation result
 */
bool usbh_drv_close(usbh_handle_t *h);

#ifdef __cplusplus
}
#endif

#endif // USBH_DRIVER_PUBLIC_H
