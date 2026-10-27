/********************************** (C) COPYRIGHT *******************************
* File Name          : hidd.h
* Author             : WCH
* Version            : V1.0.0
* Date               : 2026/10/20
* Description        : USB HID device class driver
*********************************************************************************
* Copyright (c) 2026 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

#ifndef HIDD_H
#define HIDD_H

/* @include */
#include "usb_define.h"
#include "class/hid/hid.h"
#include "device/usbd_driver_public.h"

#ifdef __cplusplus
extern "C" {
#endif

/* @struct */

/**
 * @brief USB HID device class driver handle structure
 * 
 */
typedef struct hidd_handle
{
    /* USB device handle associated with this HID device (must) */
    usbd_handle_t *usbd_handle;

    /* Interface number of the HID device (must) */
    uint8_t itf_num;

    /* Endpoint descriptors for the HID device (optional) */
    const usb_desc_endpoint_t *in_ep;
    const usb_desc_endpoint_t *out_ep;

    /* HID protocol and idle rate (optional) */
    uint8_t protocol;
    uint8_t idle_rate[256];
    uint32_t ctrl_req_buf;

    /* Report buffer for HID reports (optional) */
    void *report_buf;
    size_t report_buf_size;

    /* Callback functions for getting HID descriptors (must) */
    bool (*get_desc_cb)(struct hidd_handle *hidd, uint8_t desc_type, uint8_t desc_index, void **desc, size_t *len);

    /* Callback functions for handling HID class requests and events (optional) */
    bool (*set_desc_cb)(struct hidd_handle *hidd, uint8_t desc_type, uint8_t desc_index, void **desc, size_t *len);
    bool (*get_report_prev_cb)(struct hidd_handle *hidd, uint8_t type, uint8_t id, size_t xfer_len, size_t *len);
    bool (*set_report_prev_cb)(struct hidd_handle *hidd, uint8_t type, uint8_t id, size_t xfer_len, size_t *len);
    void (*get_report_comp_cb)(struct hidd_handle *hidd, uint8_t type, uint8_t id, void *buf, size_t len);
    void (*set_report_comp_cb)(struct hidd_handle *hidd, uint8_t type, uint8_t id, void *buf, size_t len);
    void (*set_idle_cb)(struct hidd_handle *hidd, uint8_t report_id, uint8_t idle_rate);
    void (*set_protocol_cb)(struct hidd_handle *hidd, uint8_t protocol);
    void (*read_comp_cb)(struct hidd_handle *hidd, void *buf, size_t len);
    void (*write_comp_cb)(struct hidd_handle *hidd, const void *buf, size_t len);
} hidd_handle_t;

/* @function declarations */

/**
 * @brief Open the USB HID device class driver
 * 
 * @param hidd HID class driver handle
 * 
 * @return Operation result
 */
bool hidd_drv_open(hidd_handle_t *hidd);

/**
 * @brief Read data from the USB HID interface
 * 
 * @param hidd HID class driver handle
 * @param buf Buffer to store the read data
 * @param len Length of the data to read
 * 
 * @return Operation result
 */
bool hidd_drv_read(hidd_handle_t *hidd, void *buf, size_t len);

/**
 * @brief Write data to the USB HID device
 * 
 * @param hidd HID class driver handle
 * @param buf Buffer containing the data to write
 * @param len Length of the data to write
 * 
 * @return Operation result
 */
bool hidd_drv_write(hidd_handle_t *hidd, const void *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif // HIDD_H
