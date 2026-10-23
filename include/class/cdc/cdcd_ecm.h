/********************************** (C) COPYRIGHT *******************************
* File Name          : cdcd_ecm.h
* Author             : WCH
* Version            : V1.0.0
* Date               : 2026/10/20
* Description        : USB CDC-ECM device class driver
*********************************************************************************
* Copyright (c) 2026 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

#ifndef CDCD_ECM_H
#define CDCD_ECM_H

/* @include */
#include "usb_define.h"
#include "class/cdc/cdc.h"
#include "device/usbd_driver_public.h"

#ifdef __cplusplus
extern "C" {
#endif

/* @struct */
typedef struct cdcd_ecm_handle cdcd_ecm_handle_t;

/**
 * @brief USB CDC-ECM device class driver handle structure
 * 
 */
typedef struct cdcd_ecm_handle
{
    /* USB device handle associated with this CDC-ECM device (must) */
    usbd_handle_t *usbd_handle;

    /* Control and data interface number for the CDC-ECM device (must) */
    uint8_t ctrl_itf_num;
    uint8_t data_itf_num;

    /* Endpoint descriptors for the CDC-ECM device (must) */
    const usb_desc_endpoint_t *notify_ep;
    const usb_desc_endpoint_t *in_ep;
    const usb_desc_endpoint_t *out_ep;

    /* Current alternate setting for the data interface (optional) */
    __attribute__((aligned(4))) uint8_t alt_setting;

    /* Notification buffer (optional) */
    __attribute__((aligned(4))) uint8_t notify_buf[16];

    /* Callback functions for the CDC-ECM device (optional) */
    void (*set_interface_cb)(cdcd_ecm_handle_t *cdcd_ecm, uint16_t alt_setting);
    void (*set_ethernet_packet_filter_cb)(cdcd_ecm_handle_t *cdcd_ecm, uint16_t bitmap);
    void (*read_comp_cb)(cdcd_ecm_handle_t *cdcd_ecm, void *buf, size_t len);
    void (*write_comp_cb)(cdcd_ecm_handle_t *cdcd_ecm, const void *buf, size_t len);
} cdcd_ecm_handle_t;

/* @function declarations */

/**
 * @brief Open the USB CDC-ECM device class driver instance
 * 
 * @param cdcd_ecm CDC-ECM device class driver instance handle
 * 
 * @return Operation result
 */
bool cdcd_ecm_drv_open(cdcd_ecm_handle_t *cdcd_ecm);

/**
 * @brief Control the data endpoint of the USB CDC-ECM device class driver instance
 * 
 * @param cdcd_ecm CDC-ECM device class driver instance handle
 * @param enable Enable or disable the data endpoint (true to enable, false to disable)
 * 
 * @return Operation result
 */
bool cdcd_ecm_drv_ctrl_data_endpoint(cdcd_ecm_handle_t *cdcd_ecm, bool enable);

/**
 * @brief Notify the USB CDC-ECM device class driver of a change in connection status
 * 
 * @param cdcd_ecm CDC-ECM device class driver instance handle
 * @param connected Connection status (true for connected, false for disconnected)
 * 
 * @return Operation result
 */
bool cdcd_ecm_drv_connect_change_notify(cdcd_ecm_handle_t *cdcd_ecm, bool connected);

/**
 * @brief Notify the USB CDC-ECM device class driver of a change in connection speed
 * 
 * @param cdcd_ecm CDC-ECM device class driver instance handle
 * @param ul_bit_rate Uplink bit rate
 * @param dl_bit_rate Downlink bit rate
 * 
 * @return Operation result
 */
bool cdcd_ecm_drv_speed_change_notify(cdcd_ecm_handle_t *cdcd_ecm, uint32_t ul_bit_rate, uint32_t dl_bit_rate);

/**
 * @brief Read data from the USB CDC-ECM device class driver instance
 * 
 * @param cdcd_ecm CDC-ECM device class driver instance handle
 * @param buf The buffer to store the read data
 * @param len The length of the data to read
 * 
 * @return Operation result
 */
bool cdcd_ecm_drv_read(cdcd_ecm_handle_t *cdcd_ecm, void *buf, size_t len);

/**
 * @brief Write data to the USB CDC-ECM device class driver instance
 * 
 * @param cdcd_ecm CDC-ECM device class driver instance handle
 * @param buf The buffer containing the data to write
 * @param len The length of the data to write
 * 
 * @return Operation result
 */
bool cdcd_ecm_drv_write(cdcd_ecm_handle_t *cdcd_ecm, const void *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif // CDCD_ECM_H
