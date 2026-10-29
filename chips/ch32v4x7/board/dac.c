/********************************** (C) COPYRIGHT *******************************
* File Name          : dac.c
* Author             : WCH
* Version            : V1.0.0
* Date               : 2026/10/20
* Description        : DAC driver board support package
*********************************************************************************
* Copyright (c) 2026 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

/* @include */
#include <stdint.h>
#include <string.h>

#include "ch32v4x7.h"
#include "dac.h"

/* @define */
#define MAX_CHANNELS_COUNT     2
#define MAX_CHANNELS_BUF_COUNT 8
#define MAX_CHANNELS_BUF_SIZE  64

#define DMA_ENABLE             0x3593
#define DMA_DISABLE            0x3592

/* @struct */
typedef struct
{
    uint32_t load;
    uint32_t deal;
    uint32_t count;
} xfer_manage_t;

/* @global */
static xfer_manage_t xfer_manage[MAX_CHANNELS_COUNT];
static size_t buf_size[MAX_CHANNELS_COUNT][MAX_CHANNELS_BUF_COUNT];
static uint16_t dma_buf[MAX_CHANNELS_COUNT][MAX_CHANNELS_BUF_COUNT][MAX_CHANNELS_BUF_SIZE];

void dac_init(void)
{
    memset(buf_size, 0, sizeof(buf_size));
    memset(xfer_manage, 0, sizeof(xfer_manage));
    memset(dma_buf, 0, sizeof(dma_buf));

    RCC_PB1PeriphClockCmd(RCC_PB1Periph_DAC | RCC_PB1Periph_TIM7, ENABLE);
    RCC_PB2PeriphClockCmd(RCC_PB2Periph_GPIOA, ENABLE);
    RCC_HBPeriphClockCmd(RCC_HBPeriph_DMA2, ENABLE);

    GPIO_InitTypeDef GPIO_InitStructure = {0};
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4 | GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_High;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_SetBits(GPIOA, GPIO_Pin_4 | GPIO_Pin_5);

    DAC_InitTypeDef DAC_InitType = {0};
    DAC_DeInit();
    DAC_InitType.DAC_Trigger = DAC_Trigger_T7_TRGO;
    DAC_InitType.DAC_WaveGeneration = DAC_WaveGeneration_None;
    DAC_InitType.DAC_OutputBuffer = DAC_OutputBuffer_Disable;
    DAC_Init(DAC_Channel_1, &DAC_InitType);
    DAC_DMACmd(DAC_Channel_1, ENABLE);
    DAC_Cmd(DAC_Channel_1, ENABLE);

    DAC_InitType.DAC_Trigger = DAC_Trigger_T7_TRGO;
    DAC_InitType.DAC_WaveGeneration = DAC_WaveGeneration_None;
    DAC_InitType.DAC_OutputBuffer = DAC_OutputBuffer_Disable;
    DAC_Init(DAC_Channel_2, &DAC_InitType);
    DAC_DMACmd(DAC_Channel_2, ENABLE);
    DAC_Cmd(DAC_Channel_2, ENABLE);

    DMA_InitTypeDef DMA_InitStructure = {0};
    DMA_DeInit(DMA2_Channel3);
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&DAC->R12BDHR1;
    DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)NULL;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST;
    DMA_InitStructure.DMA_BufferSize = 0;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
    DMA_InitStructure.DMA_Priority = DMA_Priority_VeryHigh;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA2_Channel3, &DMA_InitStructure);
    DMA_ITConfig(DMA2_Channel3, DMA_IT_TC, ENABLE);
    NVIC_EnableIRQ(DMA2_Channel3_IRQn);

    DMA_DeInit(DMA2_Channel4);
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&DAC->R12BDHR2;
    DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)NULL;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST;
    DMA_InitStructure.DMA_BufferSize = 0;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
    DMA_InitStructure.DMA_Priority = DMA_Priority_VeryHigh;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA2_Channel4, &DMA_InitStructure);
    DMA_ITConfig(DMA2_Channel4, DMA_IT_TC, ENABLE);
    NVIC_EnableIRQ(DMA2_Channel4_IRQn);

    TIM_DeInit(TIM7);
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure = {0};
    TIM_TimeBaseInitStructure.TIM_Period = 208 - 1;
    TIM_TimeBaseInitStructure.TIM_Prescaler = SystemCoreClock / 10000000 - 1;
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 0x00;
    TIM_TimeBaseInit(TIM7, &TIM_TimeBaseInitStructure);
    TIM_SelectOutputTrigger(TIM7, TIM_TRGOSource_Update);
    TIM_Cmd(TIM7, ENABLE);
}

bool dac_write(const void *ch1_buf, const void *ch2_buf, size_t length)
{
    if (!ch1_buf || !ch2_buf || !length || length > MAX_CHANNELS_BUF_SIZE) return false;
    if (xfer_manage[0].count >= MAX_CHANNELS_BUF_COUNT || xfer_manage[1].count >= MAX_CHANNELS_BUF_COUNT) return false;

    for (uint8_t ch = 0; ch < MAX_CHANNELS_COUNT; ch++)
    {
        xfer_manage_t *manage = &xfer_manage[ch];
        if (ch == 0)
        {
            memcpy(dma_buf[ch][manage->load], ch1_buf, length << 1);
        }
        else
        {
            memcpy(dma_buf[ch][manage->load], ch2_buf, length << 1);
        }
        buf_size[ch][manage->load] = length;
        manage->load = (manage->load + 1) % MAX_CHANNELS_BUF_COUNT;
        manage->count++;
    }

    if (!(DMA2_Channel3->CFGR & DMA_CFGR1_EN))
    {
        DMA2_Channel3->MADDR = (uint32_t)dma_buf[0][xfer_manage[0].deal];
        DMA2_Channel3->CNTR = buf_size[0][xfer_manage[0].deal];
        DMA2_Channel3->CFGR |= DMA_CFGR1_EN;
    }

    if (!(DMA2_Channel4->CFGR & DMA_CFGR1_EN))
    {
        DMA2_Channel4->MADDR = (uint32_t)dma_buf[1][xfer_manage[1].deal];
        DMA2_Channel4->CNTR = buf_size[1][xfer_manage[1].deal];
        DMA2_Channel4->CFGR |= DMA_CFGR1_EN;
    }

    static uint32_t counter = 0;
    if (counter++ >= 1000)
    {
        counter = 0;
        if ((xfer_manage[0].count + xfer_manage[1].count) + 4 > MAX_CHANNELS_BUF_COUNT * 2)
        {
            TIM7->ATRLR--;
        }
        else if (xfer_manage[0].count + xfer_manage[1].count < 4)
        {
            TIM7->ATRLR++;
        }
    }

    return true;
}

__attribute__((interrupt("WCH-Interrupt-fast"))) void DMA2_Channel3_IRQHandler(void)
{
    if (DMA_GetITStatus(DMA2_IT_TC3) != RESET)
    {
        DMA_ClearITPendingBit(DMA2_IT_TC3);
        xfer_manage_t *manage = &xfer_manage[0];
        manage->deal = (manage->deal + 1) % MAX_CHANNELS_BUF_COUNT;
        manage->count--;
        if (manage->count)
        {
            DMA2_Channel3->CFGR = DMA_DISABLE;
            DMA2_Channel3->MADDR = (uint32_t)dma_buf[0][manage->deal];
            DMA2_Channel3->CNTR = buf_size[0][manage->deal];
            DMA2_Channel3->CFGR = DMA_ENABLE;
        }
        else
        {
            DMA2_Channel3->CFGR = DMA_DISABLE;
        }
    }
}

__attribute__((interrupt("WCH-Interrupt-fast"))) void DMA2_Channel4_IRQHandler(void)
{
    if (DMA_GetITStatus(DMA2_IT_TC4) != RESET)
    {
        DMA_ClearITPendingBit(DMA2_IT_TC4);
        xfer_manage_t *manage = &xfer_manage[1];
        manage->deal = (manage->deal + 1) % MAX_CHANNELS_BUF_COUNT;
        manage->count--;
        if (manage->count)
        {
            DMA2_Channel4->CFGR = DMA_DISABLE;
            DMA2_Channel4->MADDR = (uint32_t)dma_buf[1][manage->deal];
            DMA2_Channel4->CNTR = buf_size[1][manage->deal];
            DMA2_Channel4->CFGR = DMA_ENABLE;
        }
        else
        {
            DMA2_Channel4->CFGR = DMA_DISABLE;
        }
    }
}
