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

#ifdef __cplusplus
extern "C" {
#endif

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

/* @function declarations */

/**
 * @brief Parse the variable usage for a given HID report item
 * 
 * @param item The HID report item to parse
 * @return uint16_t* A pointer to the parsed variable usage list
 */
uint16_t *hidh_variable_usage_parse(const report_item_t *item);

/**
 * @brief Parse the array usage for a given HID report item
 * 
 * @param item The HID report item to parse
 * @param usage_min Pointer to store the minimum usage value
 * @param usage_max Pointer to store the maximum usage value
 */
bool hidh_array_usage_parse(const report_item_t *item, uint16_t *usage_min, uint16_t *usage_max);

/**
 * @brief Get the value of a field from the input data of a HID report item
 * 
 * @param data The input data buffer of the HID report item
 * @param offset The bit offset of the field within the input data
 * @param size The size of the field in bits
 * @param is_signed Whether the field is signed
 * @return int32_t The value of the field
 */
int32_t hidh_input_get_field(const uint8_t *data, uint32_t offset, uint8_t size, bool is_signed);

/**
 * @brief Process the HID keyboard tasks
 * 
 */
void hidh_keyboard_task(void);

/**
 * @brief Create a new HID keyboard instance
 * 
 * @return void* A handle to the newly created HID keyboard instance
 */
void *hidh_keyboard_create(void);

/**
 * @brief Delete an existing HID keyboard instance
 * 
 * @param handle A handle to the HID keyboard instance to delete
 */
void hidh_keyboard_delete(void *handle);

/**
 * @brief Parse the input data for a given HID keyboard instance
 * 
 * @param handle A handle to the HID keyboard instance
 * @param item The HID report item to parse
 * @param data The input data buffer of the HID report item
 * @param len The length of the input data buffer
 */
void hidh_keyboard_input_parse(void *handle, const report_item_t *item, const uint8_t *data, size_t len);

/**
 * @brief Process the HID mouse tasks
 * 
 */
void hidh_mouse_task(void);

/**
 * @brief Create a new HID mouse instance
 * 
 * @return void* A handle to the newly created HID mouse instance
 */
void *hidh_mouse_create(void);

/**
 * @brief Delete an existing HID mouse instance
 * 
 * @param handle A handle to the HID mouse instance to delete
 */
void hidh_mouse_delete(void *handle);

/**
 * @brief Parse the input data for a given HID mouse instance
 * 
 * @param handle A handle to the HID mouse instance
 * @param item The HID report item to parse
 * @param data The input data buffer of the HID report item
 * @param len The length of the input data buffer
 */
void hidh_mouse_input_parse(void *handle, const report_item_t *item, const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif // HIDH_PRIVATE_H
