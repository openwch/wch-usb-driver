/********************************** (C) COPYRIGHT *******************************
* File Name          : flash.h
* Author             : WCH
* Version            : V1.0.0
* Date               : 2026/10/20
* Description        : FLASH driver board support package
*********************************************************************************
* Copyright (c) 2026 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

#ifndef FLASH_H
#define FLASH_H

/* @include */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* @define */
#define FLASH_SECTOR_SIZE  512
#define FLASH_SECTOR_COUNT 448

/* @function declarations */
void flash_init();
bool flash_erase(uint32_t sector);
bool flash_read(uint32_t sector, void *buf);
bool flash_write(uint32_t sector, const void *buf);

#ifdef __cplusplus
}
#endif

#endif // FLASH_H
