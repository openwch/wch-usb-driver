/********************************** (C) COPYRIGHT *******************************
* File Name          : led.c
* Author             : WCH
* Version            : V1.0.0
* Date               : 2026/10/15
* Description        : LED driver board support package
*********************************************************************************
* Copyright (c) 2026 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

/* @include */
#include <stdint.h>

#include "ch32v205.h"

void led_init(void)
{
    RCC_PB2PeriphClockCmd(RCC_PB2Periph_GPIOB, ENABLE);
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_High;
    GPIO_SetBits(GPIOB, GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2);
    GPIO_Init(GPIOB, &GPIO_InitStructure);
}

void led_write(uint8_t data)
{
    GPIOB->OUTDR = (GPIOB->OUTDR & ~0x07) | (~data & 0x07);
}
