/********************************** (C) COPYRIGHT *******************************
* File Name          : usart.c
* Author             : WCH
* Version            : V1.0.0
* Date               : 2026/10/20
* Description        : USART driver board support package
*********************************************************************************
* Copyright (c) 2026 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

/* @include */
#include <stdint.h>
#include <string.h>

#include "ch32v30x.h"
#include "usart.h"

/* @global */
static size_t dma_offset;
static uint8_t dma_buffer[2048];

void usart_init(void)
{
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    GPIO_InitTypeDef GPIO_InitStructure = {0};
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    DMA_InitTypeDef DMA_InitStructure = {0};
    DMA_DeInit(DMA1_Channel2);
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&USART3->DATAR;
    DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)NULL;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST;
    DMA_InitStructure.DMA_BufferSize = 0;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
    DMA_InitStructure.DMA_Priority = DMA_Priority_VeryHigh;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel2, &DMA_InitStructure);
    DMA_Cmd(DMA1_Channel2, ENABLE);

    DMA_DeInit(DMA1_Channel3);
    memset(&DMA_InitStructure, 0, sizeof(DMA_InitStructure));
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&USART3->DATAR;
    DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)dma_buffer;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
    DMA_InitStructure.DMA_BufferSize = sizeof(dma_buffer);
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
    DMA_InitStructure.DMA_Priority = DMA_Priority_VeryHigh;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel3, &DMA_InitStructure);
    DMA_Cmd(DMA1_Channel3, ENABLE);
}

bool usart_config(uint32_t baudrate, usart_stop_bits_t stop_bits, usart_parity_t parity)
{
    if (!baudrate || baudrate > 9000000) return false;

    USART_InitTypeDef USART_InitStructure = {0};
    USART_DeInit(USART3);
    USART_InitStructure.USART_BaudRate = baudrate;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;

    switch (stop_bits)
    {
    case USART_STOP_BITS_1:
        USART_InitStructure.USART_StopBits = USART_StopBits_1;
        break;

    case USART_STOP_BITS_2:
        USART_InitStructure.USART_StopBits = USART_StopBits_2;
        break;

    case USART_STOP_BITS_1_5:
        USART_InitStructure.USART_StopBits = USART_StopBits_1_5;
        break;

    default:
        return false;
    }

    switch (parity)
    {
    case USART_PARITY_NONE:
        USART_InitStructure.USART_Parity = USART_Parity_No;
        break;

    case USART_PARITY_EVEN:
        USART_InitStructure.USART_Parity = USART_Parity_Even;
        break;

    case USART_PARITY_ODD:
        USART_InitStructure.USART_Parity = USART_Parity_Odd;
        break;

    default:
        return false;
    }

    USART_Init(USART3, &USART_InitStructure);
    USART_DMACmd(USART3, USART_DMAReq_Tx | USART_DMAReq_Rx, ENABLE);
    USART_Cmd(USART3, ENABLE);
    return true;
}

size_t usart_read(uint8_t *buffer, size_t length)
{
    if (buffer == NULL || length == 0) return 0;

    const size_t write_pos = sizeof(dma_buffer) - DMA_GetCurrDataCounter(DMA1_Channel3);
    size_t read_pos = dma_offset;
    if (write_pos == read_pos) return 0;

    const size_t avail = write_pos > read_pos ? write_pos - read_pos : sizeof(dma_buffer) - read_pos + write_pos;
    const size_t to_copy = avail < length ? avail : length;
    const size_t first_len = to_copy < sizeof(dma_buffer) - read_pos ? to_copy : sizeof(dma_buffer) - read_pos;
    memcpy(buffer, &dma_buffer[read_pos], first_len);

    if (to_copy > first_len)
    {
        memcpy(buffer + first_len, dma_buffer, to_copy - first_len);
    }

    read_pos = read_pos + to_copy > sizeof(dma_buffer) ? read_pos + to_copy - sizeof(dma_buffer) : read_pos + to_copy;
    dma_offset = read_pos;
    return to_copy;
}

bool usart_write(const uint8_t *buffer, size_t length)
{
    if (!buffer || !length) return true;

    if (!DMA_GetCurrDataCounter(DMA1_Channel2))
    {
        DMA_Cmd(DMA1_Channel2, DISABLE);
        DMA1_Channel2->MADDR = (uint32_t)buffer;
        DMA1_Channel2->CNTR = length;
        DMA_Cmd(DMA1_Channel2, ENABLE);
        return true;
    }
    return false;
}
