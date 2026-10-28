/********************************** (C) COPYRIGHT *******************************
* File Name          : uacd.h
* Author             : WCH
* Version            : V1.0.0
* Date               : 2026/10/20
* Description        : USB UAC device class driver
*********************************************************************************
* Copyright (c) 2026 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

#ifndef UACD_H
#define UACD_H

/* @include */
#include "usb_define.h"
#include "class/uac/uac.h"
#include "device/usbd_driver_public.h"

#ifdef __cplusplus
extern "C" {
#endif

/* @struct */

/**
 * @brief USB UAC device class driver handle structure
 * 
 */
typedef struct uacd_handle
{
    /* USB device handle associated with this UAC device (must) */
    usbd_handle_t *usbd_handle;

    /* Control interface number for the UAC device (must) */
    uint8_t ctrl_itf_num;

    /* Temporary parameter block for UAC device operations (optional) */
    __attribute__((aligned(4))) uint8_t temp_buf[64];

    /* Callback functions for the UAC device (optional) */
    void (*set_itf_cb)(struct uacd_handle *uacd, uint8_t itf_num, uint8_t alt_setting);
    void (*get_itf_cb)(struct uacd_handle *uacd, uint8_t itf_num, uint8_t *alt_setting);
    void (*set_cur_cb)(struct uacd_handle *uacd, uint8_t rcpt, uint8_t id_ep, uint16_t value, void *buf, size_t len);
    void (*set_min_cb)(struct uacd_handle *uacd, uint8_t rcpt, uint8_t id_ep, uint16_t value, void *buf, size_t len);
    void (*set_max_cb)(struct uacd_handle *uacd, uint8_t rcpt, uint8_t id_ep, uint16_t value, void *buf, size_t len);
    void (*set_res_cb)(struct uacd_handle *uacd, uint8_t rcpt, uint8_t id_ep, uint16_t value, void *buf, size_t len);
    void (*get_cur_cb)(struct uacd_handle *uacd, uint8_t rcpt, uint8_t id_ep, uint16_t value, void **buf, size_t *len);
    void (*get_min_cb)(struct uacd_handle *uacd, uint8_t rcpt, uint8_t id_ep, uint16_t value, void **buf, size_t *len);
    void (*get_max_cb)(struct uacd_handle *uacd, uint8_t rcpt, uint8_t id_ep, uint16_t value, void **buf, size_t *len);
    void (*get_res_cb)(struct uacd_handle *uacd, uint8_t rcpt, uint8_t id_ep, uint16_t value, void **buf, size_t *len);
    void (*read_comp_cb)(struct uacd_handle *uacd, usb_endp_t endp, void *buf, size_t len);
    void (*write_comp_cb)(struct uacd_handle *uacd, usb_endp_t endp, const void *buf, size_t len);
} uacd_handle_t;

/* @function declarations */

/**
 * @brief Open the USB UAC device class driver instance
 * 
 * @param uacd UAC device class driver instance handle
 * 
 * @return Operation result
 */
bool uacd_drv_open(uacd_handle_t *uacd);

/**
 * @brief Append a UAC stream interface to the UAC device class driver instance
 * 
 * @param uacd UAC device class driver instance handle
 * @param itf_num Interface number of the UAC stream to append
 * 
 * @return Operation result
 */
bool uacd_drv_stream_interface_append(uacd_handle_t *uacd, uint8_t itf_num);

/**
 * @brief Append a UAC stream endpoint to the UAC device class driver instance
 * 
 * @param uacd UAC device class driver instance handle
 * @param desc Descriptor of the UAC stream endpoint to open
 * 
 * @return Operation result
 */
bool uacd_drv_stream_endpoint_append(uacd_handle_t *uacd, const uac_desc_audio_endpoint_t *desc);

/**
 * @brief Open a UAC stream endpoint for the UAC device class driver instance
 * 
 * @param uacd UAC device class driver instance handle
 * @param desc Descriptor of the UAC stream endpoint to open
 * 
 * @return Operation result
 */
bool uacd_drv_open_stream_endpoint(uacd_handle_t *uacd, const uac_desc_audio_endpoint_t *desc);

/**
 * @brief Close a UAC stream endpoint for the UAC device class driver instance
 * 
 * @param uacd UAC device class driver instance handle
 * @param desc Descriptor of the UAC stream endpoint to close
 * 
 * @return Operation result
 */
bool uacd_drv_close_stream_endpoint(uacd_handle_t *uacd, const uac_desc_audio_endpoint_t *desc);

/**
 * @brief Read data from the USB UAC device class driver instance
 * 
 * @param uacd UAC device class driver instance handle
 * @param desc Descriptor of the UAC stream endpoint to read from
 * @param buf Buffer to store the read data
 * @param len Length of the data to read
 * 
 * @return Operation result
 */
bool uacd_drv_read(uacd_handle_t *uacd, const uac_desc_audio_endpoint_t *desc, void *buf, size_t len);

/**
 * @brief Write data to the USB UAC device class driver instance
 * 
 * @param uacd UAC device class driver instance handle
 * @param desc Descriptor of the UAC stream endpoint to write to
 * @param buf Buffer containing the data to write
 * @param len Length of the data to write
 * 
 * @return Operation result
 */
bool uacd_drv_write(uacd_handle_t *uacd, const uac_desc_audio_endpoint_t *desc, const void *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif // UACD_H
