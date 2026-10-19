/********************************** (C) COPYRIGHT *******************************
* File Name          : hidh_private.h
* Author             : WCH
* Version            : V1.0.0
* Date               : 2026/10/15
* Description        : USB HID host class driver private header
*********************************************************************************
* Copyright (c) 2026 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

#ifndef HIDH_PRIVATE_H
#define HIDH_PRIVATE_H

/* @include */
#include "usb_define.h"
#include "host/usbh_driver_private.h"
#include "class/hid/hid.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef USB_CLASS_HIDH_DRIVER_EN

/* @typedef */
typedef struct hidh_handle hidh_handle_t;
typedef struct hidh_instance hidh_instance_t;

/* @function pointer */
typedef void (*hidh_instance_delete_cb)(hidh_instance_t *instance);
typedef void (*hidh_instance_start_parse_cb)(hidh_instance_t *instance);
typedef void (*hidh_instance_get_report_cb)(hidh_instance_t *instance, uint16_t page, uint16_t usage, int32_t value);
typedef uint32_t (*hidh_instance_set_report_cb)(hidh_instance_t *instance, uint16_t page, uint16_t usage);

/* @struct */
typedef struct hidh_instance
{
    uint8_t report_id;
    void *handle;
    void *input_items;
    void *output_items;
    void *feature_items;
    hidh_instance_delete_cb delete_cb;
    hidh_instance_start_parse_cb start_parse_cb;
    hidh_instance_get_report_cb get_report_cb;
    hidh_instance_set_report_cb set_report_cb;
} hidh_instance_t;

/* @function declarations */

/**
 * @brief Set the report for the specified HID instance
 * 
 * @param hid The HID host handle
 * @param instance The HID instance for which to set the report
 * @return true if the report was successfully set
 * @return false if setting the report failed
 */
bool hidh_set_report(hidh_handle_t *hid, hidh_instance_t *instance);

/**
 * @brief Perform the keyboard task for the HID host
 * 
 */
void hidh_keyboard_task(void);

/**
 * @brief Create a new keyboard instance for the specified HID host
 * 
 * @param hid The HID host handle
 * @return hidh_instance_t* The newly created keyboard instance, or NULL if creation failed
 */
hidh_instance_t *hidh_keyboard_create_instance(hidh_handle_t *hid);

/**
 * @brief Perform the mouse task for the HID host
 * 
 */
void hidh_mouse_task(void);

/**
 * @brief Create a new mouse instance for the specified HID host
 * 
 * @param hid The HID host handle
 * @return hidh_instance_t* The newly created mouse instance, or NULL if creation failed
 */
hidh_instance_t *hidh_mouse_create_instance(hidh_handle_t *hid);

#endif // USB_CLASS_HIDH_DRIVER_EN

#ifdef __cplusplus
}
#endif

#endif // HIDH_PRIVATE_H
