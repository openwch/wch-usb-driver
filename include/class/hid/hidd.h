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

typedef struct hidd_handle
{
    usbd_handle_t *usbd_handle;

    uint8_t itf_num;

    const usb_desc_endpoint_t *in_ep;
    const usb_desc_endpoint_t *out_ep;

    uint32_t ctrl_req_buf;
    uint8_t protocol;
    uint8_t idle_rate[256];

    void *report_buf;
    size_t report_buf_size;

    bool (*get_desc_cb)(hidd_handle_t *hidd, uint8_t desc_type, uint8_t desc_index, void **desc, size_t *len);
    bool (*set_desc_cb)(hidd_handle_t *hidd, uint8_t desc_type, uint8_t desc_index, void **desc, size_t *len);
    bool (*get_report_prev_cb)(hidd_handle_t *hidd, uint8_t type, uint8_t id, size_t xfer_len, size_t *len);
    bool (*set_report_prev_cb)(hidd_handle_t *hidd, uint8_t type, uint8_t id, size_t xfer_len, size_t *len);
    void (*get_report_comp_cb)(hidd_handle_t *hidd, uint8_t type, uint8_t id, void *buf, size_t len);
    void (*set_report_comp_cb)(hidd_handle_t *hidd, uint8_t type, uint8_t id, void *buf, size_t len);
    void (*set_idle_cb)(hidd_handle_t *hidd, uint8_t report_id, uint8_t idle_rate);
    void (*set_protocol_cb)(hidd_handle_t *hidd, uint8_t protocol);
    void (*read_comp_cb)(hidd_handle_t *hidd, void *buf, size_t len);
    void (*write_comp_cb)(hidd_handle_t *hidd, const void *buf, size_t len);
} hidd_handle_t;

/* @function declaration */
bool hidd_drv_open(hidd_handle_t *hidd);
bool hidd_drv_read(hidd_handle_t *hidd, void *buf, size_t len);
bool hidd_drv_write(hidd_handle_t *hidd, const void *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif // HIDD_H
