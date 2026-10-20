/********************************** (C) COPYRIGHT *******************************
* File Name          : led.h
* Author             : WCH
* Version            : V1.0.0
* Date               : 2026/10/20
* Description        : LED driver board support package
*********************************************************************************
* Copyright (c) 2026 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

#ifndef LED_H
#define LED_H

/* @include */
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* @define */
#define LED_0 (1 << 0)
#define LED_1 (1 << 1)
#define LED_2 (1 << 2)

/* @function declarations */
void led_init(void);
void led_write(uint8_t data);

#ifdef __cplusplus
}
#endif

#endif // LED_H
