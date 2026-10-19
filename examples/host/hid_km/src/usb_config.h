/********************************** (C) COPYRIGHT *******************************
* File Name          : usb_config.h
* Author             : WCH
* Version            : V1.0.0
* Date               : 2026/10/15
* Description        : USB driver configuration header file
*********************************************************************************
* Copyright (c) 2026 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

#ifndef USB_CONFIG_H
#define USB_CONFIG_H

/* @include */
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* USB device and host drivers enable macros */
// #define USB_DEVICE_DRIVER_EN
#define USB_HOST_DRIVER_EN

/* USB class device drivers enable macros */
#ifdef USB_DEVICE_DRIVER_EN
// #define USB_CLASS_HIDD_DRIVER_EN
// #define USB_CLASS_CDCD_ACM_DRIVER_EN
#endif

/* USB class host drivers enable macros */
#ifdef USB_HOST_DRIVER_EN
#define USB_CLASS_HUBH_DRIVER_EN
#define USB_CLASS_HIDH_DRIVER_EN
// #define USB_CLASS_CDCH_ACM_DRIVER_EN
#endif

/* USB driver log output enable macros */
#define USB_DRIVER_LOG_INFO_EN
#define USB_DRIVER_LOG_WARNING_EN
#define USB_DRIVER_LOG_ERROR_EN

/* USB driver log output interface macros */
#ifndef USB_LOG_OUTPUT
#define USB_LOG_OUTPUT(format, ...) printf(format, ##__VA_ARGS__)
#endif

/* USB device driver parameter macros */
#ifdef USB_DEVICE_DRIVER_EN
#define USBD_REQUEST_CB_COUNT   12
#define USBD_INTERFACE_CB_COUNT 2
#endif

/* USB host driver parameters macros */
#ifdef USB_HOST_DRIVER_EN
#define USBH_DRIVER_MAX_COUNT   4
#define USBH_DEVICE_POOL_SIZE   4
#define USBH_ENDPOINT_POOL_SIZE 12
#define USBH_DESC_BUF_SIZE      512
#endif

/* USB HUB host class driver configuration macros */
#ifdef USB_CLASS_HUBH_DRIVER_EN
#define HUBH_HANDLE_POOL_SIZE 4
#endif

/* USB HID host class driver configuration macros */
#ifdef USB_CLASS_HIDH_DRIVER_EN
#define USB_CLASS_HIDH_KEYBOARD_EN
#define USB_CLASS_HIDH_MOUSE_EN

#define USB_CLASS_HIDH_KEYBOARD_MAX_COUNT 4
#define USB_CLASS_HIDH_MOUSE_MAX_COUNT    4

#define HIDH_HANDLE_POOL_SIZE             16
#define HIDH_REPORT_ITEM_POOL_SIZE        64
#define HIDH_MAX_INSTANCE_COUNT           8
#define HIDH_REPORT_DESC_BUF_SIZE         512
#define HIDH_GET_REPORT_BUF_SIZE          128
#define HIDH_SET_REPORT_BUF_SIZE          128
#endif

/* USB CDC-ACM host class driver configuration macros */
#ifdef USB_CLASS_CDCH_ACM_DRIVER_EN
#define CDCH_ACM_HANDLE_POOL_SIZE 2
#endif

#ifdef __cplusplus
}
#endif

#endif // USB_CONFIG_H
