/********************************** (C) COPYRIGHT *******************************
* File Name          : mscd.h
* Author             : WCH
* Version            : V1.0.0
* Date               : 2026/10/20
* Description        : USB MSC device class driver
*********************************************************************************
* Copyright (c) 2026 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

#ifndef MSCD_H
#define MSCD_H

/* @include */
#include "usb_define.h"
#include "class/msc/msc.h"
#include "device/usbd_driver_public.h"

#ifdef __cplusplus
extern "C" {
#endif

/* @struct */

/**
 * @brief USB MSC device class driver handle structure
 * 
 */
typedef struct mscd_handle
{
    /* USB device handle associated with this MSC device (must) */
    usbd_handle_t *usbd_handle;

    /* Control and data interface number for the MSC device (must) */
    uint8_t itf_num;
    uint8_t subclass;
    uint8_t protocol;
    scsi_device_type_t device_type;
    __attribute__((aligned(4))) uint8_t max_lun;

    /* Vendor and product identification strings (optional) */
    char vendor_id[8];
    char product_id[16];

    /* Storage sector information (must) */
    void *sector_buf;
    uint32_t sector_size;
    uint32_t sector_count;

    /* Endpoint descriptors for the MSC device (must) */
    const usb_desc_endpoint_t *in_ep;
    const usb_desc_endpoint_t *out_ep;

    /* Callback functions for the MSC device (optional) */
    void (*bulk_only_reset_cb)(struct mscd_handle *msc);
    bool (*read_sector)(struct mscd_handle *msc, uint32_t sector);
    bool (*write_sector)(struct mscd_handle *msc, uint32_t sector);

    /* Bulk-only run-time context (ignore) */
    uint8_t bulk_only_stage;
    uint16_t sector_offset;
    uint16_t sector_length;
    uint32_t sector_start;
    uint32_t actual_xfer_len;
    __attribute__((aligned(4))) msc_cbw_t cbw;
    __attribute__((aligned(4))) msc_csw_t csw;
    __attribute__((aligned(4))) uint8_t temp_data[128];
    __attribute__((aligned(4))) scsi_sense_data_t sense;
} mscd_handle_t;

/* @function declarations */

/**
 * @brief Open the USB MSC device class driver instance
 * 
 * @param msc MSC device class driver instance handle
 * 
 * @return Operation result
 */
bool mscd_drv_open(mscd_handle_t *msc);

#ifdef __cplusplus
}
#endif

#endif // MSCD_H
