/********************************** (C) COPYRIGHT *******************************
* File Name          : adc.c
* Author             : WCH
* Version            : V1.0.0
* Date               : 2026/10/20
* Description        : ADC driver board support package
*********************************************************************************
* Copyright (c) 2026 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

/* @include */
#include <stdint.h>
#include <string.h>

#include "ch32v4x7.h"
#include "adc.h"

/* @global */
static uint16_t dma_buf[64];

void adc_init(void)
{
    RCC_PB2PeriphClockCmd(RCC_PB2Periph_GPIOA | RCC_PB2Periph_ADC1 | RCC_PB2Periph_TIM8 | RCC_PB2Periph_AFIO, ENABLE);
    RCC_HBPeriphClockCmd(RCC_HBPeriph_DMA1, ENABLE);
    RCC_ADCCLKConfig(RCC_PCLK2_Div8);

    GPIO_PinRemapConfig(GPIO_Remap_ADC1_ETRGREG, ENABLE);

    GPIO_InitTypeDef GPIO_InitStructure = {0};
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_High;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    ADC_DeInit(ADC1);
    ADC_InitTypeDef ADC_InitStructure = {0};
    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode = ENABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_Ext_IT11_T8_TRGO;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel = 1;
    ADC_Init(ADC1, &ADC_InitStructure);
    ADC_RegularChannelConfig(ADC1, ADC_Channel_0, 1, ADC_SampleTime_CyclesMode7);
    ADC_ExternalTrigConvCmd(ADC1, ENABLE);
    ADC_DMACmd(ADC1, ENABLE);
    ADC_Cmd(ADC1, ENABLE);

    DMA_DeInit(DMA1_Channel12);
    DMA_InitTypeDef DMA_InitStructure = {0};
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&ADC1->RDATAR;
    DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)dma_buf;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
    DMA_InitStructure.DMA_BufferSize = sizeof(dma_buf) / sizeof(dma_buf[0]);
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
    DMA_InitStructure.DMA_Priority = DMA_Priority_High;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel12, &DMA_InitStructure);
    DMA_ITConfig(DMA1_Channel12, DMA_IT_TC, ENABLE);
    NVIC_EnableIRQ(DMA1_Channel12_IRQn);

    TIM_DeInit(TIM8);
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure = {0};
    TIM_TimeBaseInitStructure.TIM_Period = 208 - 1;
    TIM_TimeBaseInitStructure.TIM_Prescaler = SystemCoreClock / 10000000 - 1;
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 0x00;
    TIM_TimeBaseInit(TIM8, &TIM_TimeBaseInitStructure);
    TIM_SelectOutputTrigger(TIM8, TIM_TRGOSource_Update);
    TIM_Cmd(TIM8, ENABLE);
}

size_t adc_read(uint16_t *buffer, size_t length)
{
    if (!buffer || !length) return 0;

    DMA_Cmd(DMA1_Channel12, DISABLE);
    size_t xfer_len = sizeof(dma_buf) / sizeof(dma_buf[0]) - DMA1_Channel12->CNTR;
    size_t copy_len = xfer_len > length ? length : xfer_len;
    memcpy(buffer, dma_buf, copy_len << 1);
    DMA1_Channel12->MADDR = (uint32_t)dma_buf;
    DMA1_Channel12->CNTR = sizeof(dma_buf) / sizeof(dma_buf[0]);
    DMA_Cmd(DMA1_Channel12, ENABLE);
    return copy_len;
}

__attribute__((interrupt("WCH-Interrupt-fast"))) void DMA1_Channel12_IRQHandler(void)
{
    if (DMA_GetITStatus(DMA1_IT_TC12) != RESET)
    {
        DMA_ClearITPendingBit(DMA1_IT_TC12);
    }
}
