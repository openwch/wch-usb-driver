/**
 * @file button.c
 * @author Links (lhd@wch.cn)
 * @brief Button driver board support package
 *
 * @copyright Copyright (c) 2026
 *
 */

/* @include */
#include <stdint.h>

#include "ch32v30x.h"

void button_init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_5 |
                                  GPIO_Pin_6 | GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
}

uint8_t button_read(void)
{
    return ~(GPIOA->INDR & 0xFF);
}
