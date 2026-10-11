/**
 * @file hubh.h
 * @author Links (lhd@wch.cn)
 * @brief USB hub host class driver
 *
 * @copyright Copyright (c) 2026
 *
 */

#ifndef HUBH_H
#define HUBH_H

/* @include */
#include "usb_define.h"
#include "class/hub/hubh.h"
#include "host/usbh_driver_public.h"

#ifdef __cplusplus
extern "C" {
#endif

/* @struct */
typedef struct
{
} hubh_handle_t;

/* @function declaration */
void hubh_drv_task(void);
bool hubh_drv_open(usbh_handle_t *h);
bool hubh_drv_close(usbh_handle_t *h);

#ifdef __cplusplus
}
#endif

#endif // HUBH_H
