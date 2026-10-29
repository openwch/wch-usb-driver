/********************************** (C) COPYRIGHT *******************************
* File Name          : adc.h
* Author             : WCH
* Version            : V1.0.0
* Date               : 2026/10/20
* Description        : ADC driver board support package
*********************************************************************************
* Copyright (c) 2026 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

#ifndef ADC_H
#define ADC_H

/* @include */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* @function declarations */
void adc_init(void);
size_t adc_read(uint16_t *buffer, size_t length);

#ifdef __cplusplus
}
#endif

#endif // ADC_H
