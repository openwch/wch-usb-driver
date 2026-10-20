/********************************** (C) COPYRIGHT *******************************
* File Name          : descriptor.h
* Author             : WCH
* Version            : V1.0.0
* Date               : 2026/10/20
* Description        : USB descriptor for CDC-ACM example
*********************************************************************************
* Copyright (c) 2026 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

#ifndef DESCRIPTOR_H
#define DESCRIPTOR_H

/* @include */
#include "usb_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

/* @extern */
extern const usb_desc_device_t device_desc;
extern const usb_desc_qualifier_t qua_desc;
extern const uint8_t config_desc_hs[67];
extern const uint8_t config_desc_fs[67];

#ifdef __cplusplus
}
#endif

#endif // DESCRIPTOR_H
