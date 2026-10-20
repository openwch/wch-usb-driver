/********************************** (C) COPYRIGHT *******************************
* File Name          : hubh.h
* Author             : WCH
* Version            : V1.0.0
* Date               : 2026/10/20
* Description        : USB hub host class driver
*********************************************************************************
* Copyright (c) 2026 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

#ifndef HUBH_H
#define HUBH_H

/* @include */
#include "usb_define.h"
#include "host/usbh_driver_public.h"

#ifdef __cplusplus
extern "C" {
#endif

/* @function declarations */

/**
 * @brief USB hub host class driver task function
 * 
 */
void hubh_drv_task(void);

/**
 * @brief Open the USB hub host class driver
 * 
 * @param h USB host driver handle
 * 
 * @return Operation result
 */
bool hubh_drv_open(usbh_handle_t *h);

/**
 * @brief Close the USB hub host class driver
 * 
 * @param h USB host driver handle
 * 
 * @return Operation result
 */
bool hubh_drv_close(usbh_handle_t *h);

#ifdef __cplusplus
}
#endif

#endif // HUBH_H
