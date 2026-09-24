/**
 * @file main.c
 * @author Links (lhd@wch.cn)
 * @brief Main program for HID keyboard and mouse example
 *
 * @copyright Copyright (c) 2026
 *
 */

/* @include*/
#include <assert.h>

#include "usb_driver.h"
#include "board.h"

int main(void)
{
    board_init();

    usbd_handle_t *h = board_usbd_init(0);
    assert(h != NULL);

    usbd_drv_open(h, USB_SPEED_HIGH, false);

    while (1);

    return 0;
}
