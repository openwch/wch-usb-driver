/**
 * @file led.c
 * @author Links (lhd@wch.cn)
 * @brief LED driver board support package
 *
 * @copyright Copyright (c) 2026
 *
 */

/* @include */
#include <stdint.h>

#include "ch32v30x.h"

void led_init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_SetBits(GPIOB, GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2);
    GPIO_Init(GPIOB, &GPIO_InitStructure);
}

void led_write(uint8_t data)
{
    GPIOB->OUTDR = (GPIOB->OUTDR & ~0x07) | (~data & 0x07);
}
