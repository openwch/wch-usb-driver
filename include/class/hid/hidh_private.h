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

/* @enum */
typedef enum
{
    REPORT_TYPE_INPUT,
    REPORT_TYPE_OUTPUT,
    REPORT_TYPE_FEATURE,
    REPORT_TYPE_COUNT,
} report_type_t;

/* @function pointer */
typedef void (*input_start_parse_cb)(void *handle);
typedef void (*input_usage_parse_cb)(void *handle, uint16_t usage_page, uint16_t usage, int32_t value);

/* @struct */
typedef struct
{
    uint8_t report_id;
    uint16_t usage_page;

    uint32_t size;
    uint32_t count;

    int32_t logical_min;
    int32_t logical_max;
    int32_t physical_min;
    int32_t physical_max;
    int32_t unit_exponent;
    uint32_t unit;
} report_global_info_t;

typedef struct hid_report_item
{
    struct hid_report_item *next;
    bool used;
    uint16_t flags;
    uint16_t local_report_size;
    const uint8_t *local_report_buf;
    report_global_info_t global_info;
} report_item_t;

typedef struct
{
    uint8_t report_id;
    uint16_t top_usage;
    uint16_t top_usage_page;
    void *handle;
    input_start_parse_cb start_parse_cb;
    input_usage_parse_cb usage_parse_cb;
    report_item_t *type_item_list[REPORT_TYPE_COUNT];
} item_list_t;

typedef struct hidh_handle
{
    struct hidh_handle *next;
    usbh_device_t *dev;

    /* HID interface information */
    const usb_desc_interface_t *itf_desc;
    const usb_desc_hid_t *hid_desc;
    const usb_desc_endpoint_t *in_ep;
    const usb_desc_endpoint_t *out_ep;
    __attribute__((aligned(4))) uint8_t report_desc_buf[HIDH_REPORT_DESC_BUF_SIZE];

    /* Run-time context */
    bool has_report_id;
    uint8_t enum_stage;
    uint16_t list_count;
    item_list_t report_item_list[HIDH_MAX_REPORT_ID_COUNT];
    __attribute__((aligned(4))) usb_setup_t ctrl_setup;
    __attribute__((aligned(4))) uint8_t read_buf[HIDH_IN_DATA_BUF_SIZE];
    __attribute__((aligned(4))) uint8_t write_buf[HIDH_OUT_DATA_BUF_SIZE];
} hidh_handle_t;

/* @function declarations */

/**
 * @brief Process the HID keyboard tasks
 * 
 */
void hidh_keyboard_task(void);

/**
 * @brief Create a new HID keyboard instance
 * 
 * @param parent A handle to the parent HID device
 * @return void* A handle to the newly created HID keyboard instance
 */
void *hidh_keyboard_create(hidh_handle_t *parent);

/**
 * @brief Delete an existing HID keyboard instance
 * 
 * @param handle A handle to the HID keyboard instance to delete
 */
void hidh_keyboard_delete(void *handle);

/**
 * @brief Start parsing the HID keyboard input report
 * 
 * @param handle A handle to the HID keyboard instance
 */
void keyboard_input_start_parse(void *handle);

/**
 * @brief Parse the HID keyboard input report usage
 * 
 * @param handle A handle to the HID keyboard instance
 * @param usage_page The usage page of the input report
 * @param usage The usage of the input report
 * @param value The value of the input report
 */
void keyboard_input_usage_parse(void *handle, uint16_t usage_page, uint16_t usage, int32_t value);

/**
 * @brief Process the HID mouse tasks
 * 
 */
void hidh_mouse_task(void);

/**
 * @brief Create a new HID mouse instance
 * 
 * @param parent A handle to the parent HID device
 * @return void* A handle to the newly created HID mouse instance
 */
void *hidh_mouse_create(hidh_handle_t *parent);

/**
 * @brief Delete an existing HID mouse instance
 * 
 * @param handle A handle to the HID mouse instance to delete
 */
void hidh_mouse_delete(void *handle);

/**
 * @brief Start parsing the HID mouse input report
 * 
 * @param handle A handle to the HID mouse instance
 */
void mouse_input_start_parse(void *handle);

/**
 * @brief Parse the HID mouse input report usage
 * 
 * @param handle A handle to the HID mouse instance
 * @param usage_page The usage page of the input report
 * @param usage The usage of the input report
 * @param value The value of the input report
 */
void mouse_input_usage_parse(void *handle, uint16_t usage_page, uint16_t usage, int32_t value);

#endif // USB_CLASS_HIDH_DRIVER_EN

#ifdef __cplusplus
}
#endif

#endif // HIDH_PRIVATE_H
