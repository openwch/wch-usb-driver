/**
 * @file main.c
 * @author Links (lhd@wch.cn)
 * @brief Main program for CDC-ACM example
 *
 * @copyright Copyright (c) 2026
 *
 */

/* @include*/
#include <assert.h>
#include <string.h>

#include "board.h"

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

int main(void)
{
    board_init();

    usbh_handle = (usbh_handle_t *)board_usb_init(USBH_INDEX, USB_MODE_HOST);
    assert(usbh_handle != NULL);

    assert(usbh_drv_open(usbh_handle));

    while (1)
    {
        usbh_drv_task(usbh_handle);
    }

    return 0;
}
