/********************************** (C) COPYRIGHT *******************************
* File Name          : msch.h
* Author             : WCH
* Version            : V1.0.0
* Date               : 2026/10/20
* Description        : USB MSC host class driver
*********************************************************************************
* Copyright (c) 2026 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

#ifndef MSCH_H
#define MSCH_H

/* @include */
#include "usb_define.h"
#include "class/msc/msc.h"
#include "host/usbh_driver_public.h"

#ifdef __cplusplus
extern "C" {
#endif

/* @typedef */
typedef struct msch_handle msch_handle_t;

/* @function pointer */

/**
 * @brief MSC host read/write completion callback function
 * 
 * @param msc MSC instance handle
 * @param lun Logical unit number
 * @param rst Transfer result
 * @param buf Data buffer passed to msch_read() or msch_write()
 * @param count Number of sectors requested
 */
typedef void (*msch_xfer_cb)(msch_handle_t *msc, uint8_t lun, bool rst, void *buf, uint32_t count);

/* @function declarations */

/**
 * @brief Handle the USB MSC host driver tasks
 * 
 */
void msch_drv_task(void);

/**
 * @brief Open the USB MSC host driver
 * 
 * @param h USB host handle
 * 
 * @return Operation result
 */
bool msch_drv_open(usbh_handle_t *h);

/**
 * @brief Close the USB MSC host driver
 * 
 * @param h USB host handle
 * 
 * @return Operation result
 */
bool msch_drv_close(usbh_handle_t *h);

/**
 * @brief Get the number of USB MSC host driver instances
 * 
 * @return The number of enumerated USB MSC instances
 */
size_t msch_get_count(void);

/**
 * @brief Get the USB MSC host driver handle by index
 * 
 * @param index The index of the USB MSC instance
 * 
 * @return MSC instance handle, or NULL if the index is invalid
 */
msch_handle_t *msch_get_handle(size_t index);

/**
 * @brief Get the number of logical units of the MSC instance
 * 
 * @param msc MSC instance handle
 * 
 * @return The number of logical units
 */
uint8_t msch_get_lun_count(msch_handle_t *msc);

/**
 * @brief Check if a logical unit is ready for sector read and write
 * 
 * @param msc MSC instance handle
 * @param lun Logical unit number
 * 
 * @return true if the logical unit is ready, false otherwise
 */
bool msch_lun_is_ready(msch_handle_t *msc, uint8_t lun);

/**
 * @brief Get the sector size of a logical unit in bytes
 * 
 * @param msc MSC instance handle
 * @param lun Logical unit number
 * 
 * @return Sector size in bytes, 0 if the logical unit is not ready
 */
uint32_t msch_get_sector_size(msch_handle_t *msc, uint8_t lun);

/**
 * @brief Get the sector count of a logical unit
 * 
 * @param msc MSC instance handle
 * @param lun Logical unit number
 * 
 * @return Sector count, 0 if the logical unit is not ready
 */
uint32_t msch_get_sector_count(msch_handle_t *msc, uint8_t lun);

/**
 * @brief Get the SCSI vendor identification string of a logical unit
 * 
 * @param msc MSC instance handle
 * @param lun Logical unit number
 * 
 * @return Null-terminated string, or NULL if the parameters are invalid
 */
const char *msch_get_vendor_id(msch_handle_t *msc, uint8_t lun);

/**
 * @brief Get the SCSI product identification string of a logical unit
 * 
 * @param msc MSC instance handle
 * @param lun Logical unit number
 * 
 * @return Null-terminated string, or NULL if the parameters are invalid
 */
const char *msch_get_product_id(msch_handle_t *msc, uint8_t lun);

/**
 * @brief Read sectors from a logical unit
 * 
 * @param msc MSC instance handle
 * @param lun Logical unit number
 * @param sector Start sector (LBA)
 * @param count Number of sectors to read (1 ~ 65535)
 * @param buf Buffer to store the data, 4-byte aligned, at least count * sector size bytes
 * @param cb The callback function to be called when the read operation is complete
 * 
 * @return true if the request is accepted, false otherwise
 */
bool msch_read(msch_handle_t *msc, uint8_t lun, uint32_t sector, uint32_t count, void *buf, msch_xfer_cb cb);

/**
 * @brief Write sectors to a logical unit
 * 
 * @param msc MSC instance handle
 * @param lun Logical unit number
 * @param sector Start sector (LBA)
 * @param count Number of sectors to write (1 ~ 65535)
 * @param buf Buffer containing the data, 4-byte aligned, at least count * sector size bytes
 * @param cb The callback function to be called when the write operation is complete
 * 
 * @return true if the request is accepted, false otherwise
 */
bool msch_write(msch_handle_t *msc, uint8_t lun, uint32_t sector, uint32_t count, const void *buf, msch_xfer_cb cb);

#ifdef __cplusplus
}
#endif

#endif // MSCH_H
