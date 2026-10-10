/**
 * @file usbh_driver_private.h
 * @author Links (lhd@wch.cn)
 * @brief USB host core private header file
 *
 * @copyright Copyright (c) 2026
 *
 */

#ifndef USBH_DRIVER_PRIVATE_H
#define USBH_DRIVER_PRIVATE_H

/* @include */
#include "usb_define.h"
#include "host/usbh_driver_public.h"

#ifdef __cplusplus
extern "C" {
#endif

/* @struct */
typedef struct usbh_handle
{
    /* Port context */
    void *port_ctx;

    /* USB host operations */
    bool (*open)(usbh_handle_t *h);
    bool (*close)(usbh_handle_t *h);
} usbh_handle_t;

#ifdef __cplusplus
}
#endif

#endif // USBH_DRIVER_PRIVATE_H
