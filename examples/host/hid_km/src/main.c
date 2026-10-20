/********************************** (C) COPYRIGHT *******************************
* File Name          : main.c
* Author             : WCH
* Version            : V1.0.0
* Date               : 2026/10/20
* Description        : Main program for HID keyboard and mouse example
*********************************************************************************
* Copyright (c) 2026 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

/* @include*/
#include <assert.h>
#include <string.h>

#include "board.h"
#include "led.h"

#include "usb_driver.h"

/* @define */
#ifndef USBH_INDEX
#define USBH_INDEX 0
#endif

#if USBH_INDEX >= USB_COUNT
#error "Not supported: USBH_INDEX exceeds USB_COUNT"
#endif

/* @global */
static usbh_handle_t *usbh_handle;

static void keyboard_changed_cb(hidh_keyboard_t *keyboard)
{
    static const char *key_name_string[] = {
        [HID_KEY_CODE_A] = "A", [HID_KEY_CODE_B] = "B", [HID_KEY_CODE_C] = "C", [HID_KEY_CODE_D] = "D",
        [HID_KEY_CODE_E] = "E", [HID_KEY_CODE_F] = "F", [HID_KEY_CODE_G] = "G", [HID_KEY_CODE_H] = "H",
        [HID_KEY_CODE_I] = "I", [HID_KEY_CODE_J] = "J", [HID_KEY_CODE_K] = "K", [HID_KEY_CODE_L] = "L",
        [HID_KEY_CODE_M] = "M", [HID_KEY_CODE_N] = "N", [HID_KEY_CODE_O] = "O", [HID_KEY_CODE_P] = "P",
        [HID_KEY_CODE_Q] = "Q", [HID_KEY_CODE_R] = "R", [HID_KEY_CODE_S] = "S", [HID_KEY_CODE_T] = "T",
        [HID_KEY_CODE_U] = "U", [HID_KEY_CODE_V] = "V", [HID_KEY_CODE_W] = "W", [HID_KEY_CODE_X] = "X",
        [HID_KEY_CODE_Y] = "Y", [HID_KEY_CODE_Z] = "Z", [HID_KEY_CODE_1] = "1", [HID_KEY_CODE_2] = "2",
        [HID_KEY_CODE_3] = "3", [HID_KEY_CODE_4] = "4", [HID_KEY_CODE_5] = "5", [HID_KEY_CODE_6] = "6",
        [HID_KEY_CODE_7] = "7", [HID_KEY_CODE_8] = "8", [HID_KEY_CODE_9] = "9", [HID_KEY_CODE_0] = "0",
    };

    printf("Keyboard led status %02X key status: [ ", keyboard->led_status);

    for (size_t i = 0; i < USB_ARRAY_SIZE(keyboard->button_status_bitmap) * 32; i++)
    {
        if (i < USB_ARRAY_SIZE(key_name_string) && key_name_string[i] &&
            (keyboard->button_status_bitmap[i >> 5] & (1 << (i & 0x1F))))
        {
            printf("%s ", key_name_string[i]);
        }
    }
    printf("]\n");

    led_write(keyboard->led_status);
}

static void mouse_changed_cb(hidh_mouse_status_t *status, hidh_mouse_change_t *change)
{
    printf("Mouse status: buttons=%02X x=%5d y=%5d changed: dx=%+5d dy=%+5d wheel=%d\n", status->buttons, status->x,
           status->y, change->dx, change->dy, change->wheel);
}

int main(void)
{
    board_init();
    led_init();

    usbh_handle = (usbh_handle_t *)board_usb_init(USBH_INDEX, USB_MODE_HOST);
    assert(usbh_handle != NULL);

    assert(usbh_drv_open(usbh_handle));
    assert(hubh_drv_open(usbh_handle));
    assert(hidh_drv_open(usbh_handle));

    hidh_register_keyboard_changed_cb(keyboard_changed_cb);
    hidh_register_mouse_changed_cb(mouse_changed_cb);

    while (1)
    {
        usbh_drv_task(usbh_handle);
        hubh_drv_task();
        hidh_drv_task();
    }

    return 0;
}
