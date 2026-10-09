/**
 * @file usb_config.h
 * @author Links (lhd@wch.cn)
 * @brief USB driver configuration header file
 *
 * @copyright Copyright (c) 2026
 *
 */

#ifndef USB_CONFIG_H
#define USB_CONFIG_H

/* @include */
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

// USB device and host drivers enable macros
// #define USB_DEVICE_DRIVER_EN
#define USB_HOST_DRIVER_EN

// USB class device drivers enable macros
#ifdef USB_DEVICE_DRIVER_EN
// #define USB_CLASS_HIDD_DRIVER_EN
// #define USB_CLASS_CDCD_ACM_DRIVER_EN
#endif

// USB class host drivers enable macros
#ifdef USB_HOST_DRIVER_EN
// #define USB_CLASS_HIDH_DRIVER_EN
#define USB_CLASS_CDCH_ACM_DRIVER_EN
#endif

// USB driver log output enable macros
#define USB_DRIVER_LOG_INFO_EN
#define USB_DRIVER_LOG_WARNING_EN
#define USB_DRIVER_LOG_ERROR_EN

// USB driver log output interface macro definitions.
#ifndef USB_LOG_OUTPUT
#define USB_LOG_OUTPUT(format, ...) printf(format, ##__VA_ARGS__)
#endif

// USB device driver parameters
#define USBD_REQUEST_CB_COUNT   12
#define USBD_INTERFACE_CB_COUNT 2

// USB host driver parameters

#ifdef __cplusplus
}
#endif

#endif // USB_CONFIG_H
