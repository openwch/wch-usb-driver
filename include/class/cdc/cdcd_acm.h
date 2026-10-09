/**
 * @file cdcd_acm.h
 * @author Links (lhd@wch.cn)
 * @brief USB CDC-ACM device class driver
 *
 * @copyright Copyright (c) 2026
 *
 */

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

typedef struct cdcd_acm_handle
{
    usbd_handle_t *usbd_handle;

    uint8_t ctrl_itf_num;
    uint8_t data_itf_num;

    const usb_desc_endpoint_t *notify_ep;
    const usb_desc_endpoint_t *in_ep;
    const usb_desc_endpoint_t *out_ep;

    cdc_line_coding_t line_coding;

    bool (*set_line_coding_cb)(cdcd_acm_handle_t *cdcd_acm, cdc_line_coding_t *line_coding);
    void (*set_control_line_state_cb)(cdcd_acm_handle_t *cdcd_acm, uint16_t bitmap);
    void (*send_break_cb)(cdcd_acm_handle_t *cdcd_acm, uint16_t duration);
    void (*read_comp_cb)(cdcd_acm_handle_t *cdcd_acm, void *buf, size_t len);
    void (*write_comp_cb)(cdcd_acm_handle_t *cdcd_acm, const void *buf, size_t len);
} cdcd_acm_handle_t;

/* @function declaration */
bool cdcd_acm_drv_open(cdcd_acm_handle_t *cdcd_acm);
bool cdcd_acm_drv_read(cdcd_acm_handle_t *cdcd_acm, void *buf, size_t len);
bool cdcd_acm_drv_write(cdcd_acm_handle_t *cdcd_acm, const void *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif // CDCD_ACM_H
