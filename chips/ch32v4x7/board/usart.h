/********************************** (C) COPYRIGHT *******************************
* File Name          : usart.h
* Author             : WCH
* Version            : V1.0.0
* Date               : 2026/10/20
* Description        : USART driver board support package
*********************************************************************************
* Copyright (c) 2026 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

#ifndef USART_H
#define USART_H

/* @include */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* @enum */
typedef enum
{
    USART_STOP_BITS_1 = 0x01,
    USART_STOP_BITS_2 = 0x02,
    USART_STOP_BITS_1_5 = 0x03,
} usart_stop_bits_t;

typedef enum
{
    USART_PARITY_NONE = 0x00,
    USART_PARITY_EVEN = 0x01,
    USART_PARITY_ODD = 0x02,
} usart_parity_t;

/* @function declarations */
void usart_init(void);
bool usart_config(uint32_t baudrate, usart_stop_bits_t stop_bits, usart_parity_t parity);
size_t usart_read(uint8_t *buffer, size_t length);
bool usart_write(const uint8_t *buffer, size_t length);

#ifdef __cplusplus
}
#endif

#endif // USART_H
