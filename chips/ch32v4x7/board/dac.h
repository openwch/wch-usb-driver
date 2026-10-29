/********************************** (C) COPYRIGHT *******************************
* File Name          : dac.h
* Author             : WCH
* Version            : V1.0.0
* Date               : 2026/10/20
* Description        : DAC driver board support package
*********************************************************************************
* Copyright (c) 2026 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

#ifndef DAC_H
#define DAC_H

/* @include */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* @function declarations */
void dac_init(void);
bool dac_write(const void *ch1_buf, const void *ch2_buf, size_t length);

#ifdef __cplusplus
}
#endif

#endif // DAC_H
