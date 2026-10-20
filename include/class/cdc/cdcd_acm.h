/********************************** (C) COPYRIGHT *******************************
* File Name          : cdcd_acm.h
* Author             : WCH
* Version            : V1.0.0
* Date               : 2026/10/20
* Description        : USB CDC-ACM device class driver
*********************************************************************************
* Copyright (c) 2026 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

#ifndef CDCD_ACM_H
#define CDCD_ACM_H

/* @include */
#include "usb_define.h"
#include "class/cdc/cdc.h"
#include "device/usbd_driver_public.h"

#ifdef __cplusplus
extern "C" {
#endif

/* @struct */
typedef struct cdcd_acm_handle cdcd_acm_handle_t;

/**
 * @brief USB CDC-ACM device class driver handle structure
 * 
 */
typedef struct cdcd_acm_handle
{
    /* USB device handle associated with this CDC-ACM device (must) */
    usbd_handle_t *usbd_handle;

    /* Control and data interface number for the CDC-ACM device (must) */
    uint8_t ctrl_itf_num;
    uint8_t data_itf_num;

    /* Endpoint descriptors for the CDC-ACM device (must) */
    const usb_desc_endpoint_t *notify_ep;
    const usb_desc_endpoint_t *in_ep;
    const usb_desc_endpoint_t *out_ep;

    /* Line coding configuration for the CDC-ACM device (optional) */
    cdc_line_coding_t line_coding;

    /* Callback functions for the CDC-ACM device (optional) */
    bool (*set_line_coding_cb)(cdcd_acm_handle_t *cdcd_acm, cdc_line_coding_t *line_coding);
    void (*set_control_line_state_cb)(cdcd_acm_handle_t *cdcd_acm, uint16_t bitmap);
    void (*send_break_cb)(cdcd_acm_handle_t *cdcd_acm, uint16_t duration);
    void (*read_comp_cb)(cdcd_acm_handle_t *cdcd_acm, void *buf, size_t len);
    void (*write_comp_cb)(cdcd_acm_handle_t *cdcd_acm, const void *buf, size_t len);
} cdcd_acm_handle_t;

/* @function declarations */

/**
 * @brief Open the USB CDC-ACM device class driver instance
 * 
 * @param cdcd_acm CDC-ACM device class driver instance handle
 * 
 * @return Operation result
 */
bool cdcd_acm_drv_open(cdcd_acm_handle_t *cdcd_acm);

/**
 * @brief Read data from the USB CDC-ACM device class driver instance
 * 
 * @param cdcd_acm CDC-ACM device class driver instance handle
 * @param buf The buffer to store the read data
 * @param len The length of the data to read
 * 
 * @return Operation result
 */
bool cdcd_acm_drv_read(cdcd_acm_handle_t *cdcd_acm, void *buf, size_t len);

/**
 * @brief Write data to the USB CDC-ACM device class driver instance
 * 
 * @param cdcd_acm CDC-ACM device class driver instance handle
 * @param buf The buffer containing the data to write
 * @param len The length of the data to write
 * 
 * @return Operation result
 */
bool cdcd_acm_drv_write(cdcd_acm_handle_t *cdcd_acm, const void *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif // CDCD_ACM_H
