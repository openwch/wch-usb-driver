/********************************** (C) COPYRIGHT *******************************
* File Name          : cdch_ecm.h
* Author             : WCH
* Version            : V1.0.0
* Date               : 2026/10/20
* Description        : USB CDC-ECM host class driver
*********************************************************************************
* Copyright (c) 2026 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

#ifndef CDCH_ECM_H
#define CDCH_ECM_H

/* @include */
#include "usb_define.h"
#include "class/cdc/cdc.h"
#include "host/usbh_driver_public.h"

#ifdef __cplusplus
extern "C" {
#endif

/* @typedef */
typedef struct ecmh_handle ecmh_handle_t;

/* @function pointer */
typedef void (*cdch_ecm_open_cb)(ecmh_handle_t *ecm, bool rst);
typedef void (*cdch_ecm_connect_change_cb)(ecmh_handle_t *ecm, bool connected);
typedef void (*cdch_ecm_speed_change_cb)(ecmh_handle_t *ecm, uint32_t upload_speed, uint32_t download_speed);
typedef void (*cdch_ecm_read_cb)(ecmh_handle_t *ecm, const void *buf, size_t length);
typedef void (*cdch_ecm_write_cb)(ecmh_handle_t *ecm, const void *buf, size_t length);

/* @function declarations */

/**
 * @brief Handle the USB CDC-ECM host driver tasks
 * 
 */
void cdch_ecm_drv_task(void);

/**
 * @brief Open the USB CDC-ECM host driver
 * 
 * @param h USB host handle
 * 
 * @return Operation result
 */
bool cdch_ecm_drv_open(usbh_handle_t *h);

/**
 * @brief Close the USB CDC-ECM host driver
 * 
 * @param h USB host handle
 * 
 * @return Operation result
 */
bool cdch_ecm_drv_close(usbh_handle_t *h);

/**
 * @brief Get the number of USB CDC-ECM host driver instances
 * 
 * @return The number of USB CDC-ECM host driver instances
 */
size_t cdch_ecm_get_count(void);

/**
 * @brief Get the USB CDC-ECM host driver handle by index
 * 
 * @param index The index of the USB CDC-ECM host driver instance
 * 
 * @return CDC-ECM instance handle, or NULL if the index is invalid
 */
ecmh_handle_t *cdch_ecm_get_handle(size_t index);

/**
 * @brief Open the USB CDC-ECM host driver instance
 * 
 * @param ecm CDC-ECM instance handle
 * @param open_cb The callback function to be called when the open operation is complete
 * @param connect_cb The callback function to be called when the connection status changes
 * @param speed_cb The callback function to be called when the connection speed changes
 */
void cdch_ecm_open(ecmh_handle_t *ecm, cdch_ecm_open_cb open_cb, cdch_ecm_connect_change_cb connect_cb,
                   cdch_ecm_speed_change_cb speed_cb);

/**
 * @brief Close the USB CDC-ECM host driver instance
 * 
 * @param ecm CDC-ECM instance handle
 */
void cdch_ecm_close(ecmh_handle_t *ecm);

/**
 * @brief Get the MAC address of the USB CDC-ECM host driver instance
 * 
 * @param ecm CDC-ECM instance handle
 * 
 * @return Pointer to the MAC address
 */
const uint8_t *cdch_ecm_get_mac_addr(ecmh_handle_t *ecm);

/**
 * @brief Check if the USB CDC-ECM host driver instance is open
 * 
 * @param ecm CDC-ECM instance handle
 * 
 * @return true if the instance is open
 */
bool cdch_ecm_is_open(ecmh_handle_t *ecm);

/**
 * @brief Check if the USB CDC-ECM host driver instance is connected
 * 
 * @param ecm CDC-ECM instance handle
 * 
 * @return true if the instance is connected
 */
bool cdch_ecm_is_connected(ecmh_handle_t *ecm);

/**
 * @brief Get the connection speed of the USB CDC-ECM host driver instance
 * 
 * @param ecm CDC-ECM instance handle
 * @param upload_speed Pointer to store the upload speed
 * @param download_speed Pointer to store the download speed
 * 
 * @return Operation result
 */
bool cdch_ecm_get_speed(ecmh_handle_t *ecm, uint32_t *upload_speed, uint32_t *download_speed);

/**
 * @brief Read data from the USB CDC-ECM host driver instance
 * 
 * @param ecm CDC-ECM instance handle
 * @param buf The buffer to store the read data
 * @param length The length of the data to read
 * @param timeout The timeout for the read operation in milliseconds
 * @param cb The callback function to be called when the read operation is complete
 * 
 * @return Operation result
 */
bool cdch_ecm_read(ecmh_handle_t *ecm, void *buf, size_t length, uint32_t timeout, cdch_ecm_read_cb cb);

/**
 * @brief Write data to the USB CDC-ECM host driver instance
 * 
 * @param ecm CDC-ECM instance handle
 * @param buf The buffer containing the data to write
 * @param length The length of the data to write
 * @param timeout The timeout for the write operation in milliseconds
 * @param cb The callback function to be called when the write operation is complete
 * 
 * @return Operation result
 */
bool cdch_ecm_write(ecmh_handle_t *ecm, const void *buf, size_t length, uint32_t timeout, cdch_ecm_write_cb cb);

#ifdef __cplusplus
}
#endif

#endif // CDCH_ECM_H
