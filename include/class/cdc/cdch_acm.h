/**
 * @file cdch_acm.h
 * @author Links (lhd@wch.cn)
 * @brief USB CDC-ACM host class driver
 *
 * @copyright Copyright (c) 2026
 *
 */

#ifndef CDCH_ACM_H
#define CDCH_ACM_H

/* @include */
#include "usb_define.h"
#include "class/cdc/cdc.h"
#include "host/usbh_driver_public.h"

#ifdef __cplusplus
extern "C" {
#endif

/* @typedef */
typedef struct acm_handle acmh_handle_t;

/* @function pointer */
typedef void (*cdch_acm_open_cb)(acmh_handle_t *acm, bool rst);
typedef void (*cdch_acm_read_cb)(acmh_handle_t *acm, const void *buf, size_t length);
typedef void (*cdch_acm_write_cb)(acmh_handle_t *acm, const void *buf, size_t length);

/* @function declaration */
void cdch_acm_drv_task(void);
bool cdch_acm_drv_open(usbh_handle_t *h);
bool cdch_acm_drv_close(usbh_handle_t *h);

size_t cdch_acm_get_count(void);
acmh_handle_t *cdch_acm_get_handle(size_t index);
void cdch_acm_open(acmh_handle_t *acm, const cdc_line_coding_t *line_coding, cdch_acm_open_cb cb);
void cdch_acm_close(acmh_handle_t *acm);
bool cdch_acm_is_open(acmh_handle_t *acm);
bool cdch_acm_read(acmh_handle_t *acm, void *buf, size_t length, uint32_t timeout, cdch_acm_read_cb cb);
bool cdch_acm_write(acmh_handle_t *acm, const void *buf, size_t length, uint32_t timeout, cdch_acm_write_cb cb);
void cdch_acm_set_dtr(acmh_handle_t *acm, bool dtr);
void cdch_acm_set_rts(acmh_handle_t *acm, bool rts);
void cdch_acm_set_break(acmh_handle_t *acm, bool brk);
void cdch_acm_send_break(acmh_handle_t *acm, uint16_t duration);
bool cdch_acm_get_dtr(acmh_handle_t *acm);
bool cdch_acm_get_rts(acmh_handle_t *acm);

#ifdef __cplusplus
}
#endif

#endif // CDCH_ACM_H
