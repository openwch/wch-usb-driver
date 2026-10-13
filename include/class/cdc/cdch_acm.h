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

/* @function declarations */

/**
 * @brief Handle the USB CDC-ACM host driver tasks
 * 
 */
void cdch_acm_drv_task(void);

/**
 * @brief Open the USB CDC-ACM host driver
 * 
 * @param h USB host handle
 * 
 * @return Operation result
 */
bool cdch_acm_drv_open(usbh_handle_t *h);

/**
 * @brief Close the USB CDC-ACM host driver
 * 
 * @param h USB host handle
 * 
 * @return Operation result
 */
bool cdch_acm_drv_close(usbh_handle_t *h);

/**
 * @brief Get the number of USB CDC-ACM host driver instances
 * 
 * @return The number of USB CDC-ACM host driver instances
 */
size_t cdch_acm_get_count(void);

/**
 * @brief Get the USB CDC-ACM host driver handle by index
 * 
 * @param index The index of the USB CDC-ACM host driver instance
 * 
 * @return CDC-ACM instance handle, or NULL if the index is invalid
 */
acmh_handle_t *cdch_acm_get_handle(size_t index);

/**
 * @brief Open the USB CDC-ACM host driver instance
 * 
 * @param acm CDC-ACM instance handle
 * @param line_coding The line coding configuration for the USB CDC-ACM host driver instance
 * @param cb The callback function to be called when the open operation is complete
 */
void cdch_acm_open(acmh_handle_t *acm, const cdc_line_coding_t *line_coding, cdch_acm_open_cb cb);

/**
 * @brief Close the USB CDC-ACM host driver instance
 * 
 * @param acm CDC-ACM instance handle
 */
void cdch_acm_close(acmh_handle_t *acm);

/**
 * @brief Check if the USB CDC-ACM host driver instance is open
 * 
 * @param acm CDC-ACM instance handle
 * 
 * @return Operation result
 */
bool cdch_acm_is_open(acmh_handle_t *acm);

/**
 * @brief Read data from the USB CDC-ACM host driver instance
 * 
 * @param acm CDC-ACM instance handle
 * @param buf The buffer to store the read data
 * @param length The length of the data to read
 * @param timeout The timeout for the read operation in milliseconds
 * @param cb The callback function to be called when the read operation is complete
 * 
 * @return Operation result
 */
bool cdch_acm_read(acmh_handle_t *acm, void *buf, size_t length, uint32_t timeout, cdch_acm_read_cb cb);

/**
 * @brief Write data to the USB CDC-ACM host driver instance
 * 
 * @param acm CDC-ACM instance handle
 * @param buf The buffer containing the data to write
 * @param length The length of the data to write
 * @param timeout The timeout for the write operation in milliseconds
 * @param cb The callback function to be called when the write operation is complete
 * 
 * @return Operation result
 */
bool cdch_acm_write(acmh_handle_t *acm, const void *buf, size_t length, uint32_t timeout, cdch_acm_write_cb cb);

/**
 * @brief Set the Data Terminal Ready (DTR) signal for the USB CDC-ACM host driver instance
 * 
 * @param acm CDC-ACM instance handle
 * @param dtr The DTR signal state to set
 */
void cdch_acm_set_dtr(acmh_handle_t *acm, bool dtr);

/**
 * @brief Set the Request To Send (RTS) signal for the USB CDC-ACM host driver instance
 * 
 * @param acm CDC-ACM instance handle
 * @param rts The RTS signal state to set
 */
void cdch_acm_set_rts(acmh_handle_t *acm, bool rts);

/**
 * @brief Set the break condition for the USB CDC-ACM host driver instance
 * 
 * @param acm CDC-ACM instance handle
 * @param brk The break signal state to set
 */
void cdch_acm_set_break(acmh_handle_t *acm, bool brk);

/**
 * @brief Send a break signal for the specified duration on the USB CDC-ACM host driver instance
 * 
 * @param acm CDC-ACM instance handle
 * @param duration The duration of the break signal in milliseconds
 */
void cdch_acm_send_break(acmh_handle_t *acm, uint16_t duration);

/**
 * @brief Get the current state of the Data Terminal Ready (DTR) signal for the USB CDC-ACM host driver instance
 * 
 * @param acm CDC-ACM instance handle
 * @return true if the DTR signal is set, false otherwise
 */
bool cdch_acm_get_dtr(acmh_handle_t *acm);

/**
 * @brief Get the current state of the Request To Send (RTS) signal for the USB CDC-ACM host driver instance
 * 
 * @param acm CDC-ACM instance handle
 * @return true if the RTS signal is set, false otherwise
 */
bool cdch_acm_get_rts(acmh_handle_t *acm);

#ifdef __cplusplus
}
#endif

#endif // CDCH_ACM_H
