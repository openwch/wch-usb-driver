/**
 * @file usb_driver.h
 * @author Links (lhd@wch.cn)
 * @brief USB driver header file
 *
 * @copyright Copyright (c) 2026
 *
 */

#ifndef USB_DRIVER_H
#define USB_DRIVER_H

/* @include */
/* Standard library includes */
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

/* USB driver includes */
#include "usb_define.h"
#include "usb_config.h"

#include "device/usbd_driver_public.h"
#include "host/usbh_driver_public.h"

#ifdef __cplusplus
extern "C" {
#endif

/* @define */
#ifndef USB_LOG_TAG
#define USB_LOG_TAG "?"
#endif

#ifdef USB_DRIVER_LOG_INFO_EN
#define USB_LOGI(format, ...) USB_LOG_OUTPUT("[" USB_LOG_TAG "][I]: " format "\r\n", ##__VA_ARGS__)
#else
#define USB_LOGI(format, ...)
#endif

#ifdef USB_DRIVER_LOG_WARNING_EN
#define USB_LOGW(format, ...) USB_LOG_OUTPUT("[" USB_LOG_TAG "][W]: " format "\r\n", ##__VA_ARGS__)
#else
#define USB_LOGW(format, ...)
#endif

#ifdef USB_DRIVER_LOG_ERROR_EN
#define USB_LOGE(format, ...) USB_LOG_OUTPUT("[" USB_LOG_TAG "][E]: " format "\r\n", ##__VA_ARGS__)
#else
#define USB_LOGE(format, ...)
#endif

#ifdef __cplusplus
}
#endif

#endif // USB_DRIVER_H
