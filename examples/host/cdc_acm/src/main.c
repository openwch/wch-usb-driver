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
#include "button.h"
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
static uint8_t baud_index = 0;
static __attribute__((aligned(4))) uint8_t read_buf[2048];
static const uint32_t baud_rate_list[] = {9600, 115200, 460800, 921600};
static cdc_line_coding_t line_coding = {
    .bit_rate = 0,
    .stop_bits = 1,
    .parity = 0,
    .data_bits = 8,
};

static void acm_open_cb(acmh_handle_t *acm, bool rst)
{
    printf("CDC-ACM0: Set baud rate to %u %s\n", line_coding.bit_rate, rst ? "success" : "fail");
}

static void acm_read_cb(acmh_handle_t *acm, const void *buf, size_t length)
{
    printf("CDC-ACM0: Read %u bytes\n", length);
}

static void acm_write_cb(acmh_handle_t *acm, const void *buf, size_t length)
{
    printf("CDC-ACM0: Wrote %u bytes\n", length);
}

int main(void)
{
    board_init();
    button_init();
    led_init();

    usbh_handle = (usbh_handle_t *)board_usb_init(USBH_INDEX, USB_MODE_HOST);
    assert(usbh_handle != NULL);

    assert(usbh_drv_open(usbh_handle));
    assert(hubh_drv_open(usbh_handle));
    assert(cdch_acm_drv_open(usbh_handle));

    while (1)
    {
        usbh_drv_task(usbh_handle);
        hubh_drv_task();
        cdch_acm_drv_task();

        /* Check for changes in the number of connected CDC-ACM devices */
        static size_t last_acm_count = 0;
        size_t curr_acm_count = cdch_acm_get_count();
        if (curr_acm_count != last_acm_count)
        {
            printf("CDC-ACM device count changed: %u -> %u\n", last_acm_count, curr_acm_count);
            last_acm_count = curr_acm_count;
        }

        /* Get the handle for the first CDC-ACM device */
        acmh_handle_t *acm = cdch_acm_get_handle(0);

        /* Update LED status based on CDC-ACM connection and open state */
        led_write(acm ? (cdch_acm_is_open(acm) ? LED_1 : 0) | LED_0 : 0);

        /* Skip the rest of the loop if no CDC-ACM device is connected */
        if (!acm) continue;

        /* Read data from the CDC-ACM device */
        cdch_acm_read(acm, read_buf, sizeof(read_buf), 0, acm_read_cb);

        /* Read the current button status and detect changes */
        static uint8_t last_btn_status = 0;
        uint8_t curr_status = button_read();
        uint8_t changed = curr_status ^ last_btn_status;

        /* Skip the rest of the loop if no button status change is detected */
        if (!changed) continue;
        last_btn_status = curr_status;

        /* Open CDC-ACM on button 0 press */
        if (changed & curr_status & BUTTON_0)
        {
            line_coding.bit_rate = baud_rate_list[baud_index];
            baud_index = (baud_index + 1) % USB_ARRAY_SIZE(baud_rate_list);
            cdch_acm_open(acm, &line_coding, acm_open_cb);
        }

        /* Close CDC-ACM on button 1 press */
        else if (changed & curr_status & BUTTON_1)
        {
            printf("CDC-ACM0: Close\n");
            cdch_acm_close(acm);
        }

        /* Send test message on button 1 press */
        if (changed & curr_status & BUTTON_2)
        {
            static __attribute__((aligned(4))) char test_message[] = "Hello, CDC-ACM!\n";
            cdch_acm_write(acm, test_message, sizeof(test_message), 0, acm_write_cb);
        }

        /* Set DTR, RTS, and break signals based on button presses */
        if (changed & BUTTON_3) cdch_acm_set_dtr(acm, curr_status & BUTTON_3 ? true : false);
        if (changed & BUTTON_4) cdch_acm_set_rts(acm, curr_status & BUTTON_4 ? true : false);
        if (changed & BUTTON_5) cdch_acm_set_break(acm, curr_status & BUTTON_5 ? true : false);
    }

    return 0;
}
