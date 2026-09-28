/**
 * @file hidd.h
 * @author Links (lhd@wch.cn)
 * @brief USB HID device class driver
 *
 * @copyright Copyright (c) 2026
 *
 */

#ifndef HIDD_H
#define HIDD_H

/* @include */
#include "usb_define.h"

#ifdef __cplusplus
extern "C" {
#endif

/* @struct */
typedef struct hidd_handle hidd_handle_t;

typedef struct
{
    uint8_t itf_num;

    const void *hid_desc;
    const void *report_desc;
    const void *phy_desc;

    size_t hid_desc_size;
    size_t report_desc_size;
    size_t phy_desc_size;

    const usb_desc_endpoint_t *in_ep;
    const usb_desc_endpoint_t *out_ep;

    bool (*get_report_prev_cb)(hidd_handle_t *hidd, uint8_t type, uint8_t id, size_t xfer_len, void **buf, size_t *len);
    bool (*set_report_prev_cb)(hidd_handle_t *hidd, uint8_t type, uint8_t id, size_t xfer_len, void **buf, size_t *len);
    void (*get_report_comp_cb)(hidd_handle_t *hidd, uint8_t type, uint8_t id, void *buf, size_t len);
    void (*set_report_comp_cb)(hidd_handle_t *hidd, uint8_t type, uint8_t id, void *buf, size_t len);
    void (*set_idle_cb)(hidd_handle_t *hidd, uint8_t report_id, uint8_t idle_rate);
    void (*set_protocol_cb)(hidd_handle_t *hidd, uint8_t protocol);
} hidd_info_t;

typedef struct hidd_handle
{
    usbd_handle_t *usbd_handle;
    const hidd_info_t *info;

    uint32_t ctrl_req_buf;

    uint8_t protocol;
    uint8_t idle_rate[256];
} hidd_handle_t;

/* @function declaration */
bool hidd_drv_open(usbd_handle_t *usbd, hidd_handle_t *hidd, const hidd_info_t *info);

#ifdef __cplusplus
}
#endif

#endif // HIDD_H
