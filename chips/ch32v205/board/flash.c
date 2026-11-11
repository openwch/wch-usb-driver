/********************************** (C) COPYRIGHT *******************************
* File Name          : flash.c
* Author             : WCH
* Version            : V1.0.0
* Date               : 2026/10/20
* Description        : FLASH driver board support package
*********************************************************************************
* Copyright (c) 2026 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

/* @include */
#include <stdint.h>
#include <string.h>

#include "ch32v205.h"
#include "flash.h"

/* @define */
#define FLASH_START_SECTOR 64

void flash_init()
{
    FLASH_Unlock();
}

bool flash_erase(uint32_t sector)
{
    if (sector >= FLASH_SECTOR_COUNT) return false;
    return FLASH_ROM_ERASE(FLASH_BASE + (FLASH_START_SECTOR + sector) * FLASH_SECTOR_SIZE, FLASH_SECTOR_SIZE) ==
           FLASH_COMPLETE;
}

bool flash_read(uint32_t sector, void *buf)
{
    if (sector >= FLASH_SECTOR_COUNT || !buf) return false;
    memcpy(buf, (const void *)(FLASH_BASE + (FLASH_START_SECTOR + sector) * FLASH_SECTOR_SIZE), FLASH_SECTOR_SIZE);
    return true;
}

bool flash_write(uint32_t sector, const void *buf)
{
    if (sector >= FLASH_SECTOR_COUNT || !buf) return false;
    return FLASH_ROM_WRITE(FLASH_BASE + (FLASH_START_SECTOR + sector) * FLASH_SECTOR_SIZE, (uint32_t *)buf,
                           FLASH_SECTOR_SIZE) == FLASH_COMPLETE;
}
