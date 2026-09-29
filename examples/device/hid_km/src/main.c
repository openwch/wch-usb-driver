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
#include <string.h>

#include "board.h"

#include "usb_driver.h"
#include "descriptor.h"

/* @global */
static hidd_handle_t hidd_handles[2];
static uint8_t keyboard_report_buf[8];

static const void *get_desc_cb(uint8_t desc_type, uint8_t desc_info, size_t *len)
{
    switch (desc_type)
    {
    case USB_DESC_DEVICE:
        *len = sizeof(device_desc);
        return (const void *)&device_desc;

    case USB_DESC_CONFIGURATION:
        *len = sizeof(config_desc);
        return (const void *)&config_desc;

    case USB_DESC_STRING:
        switch (desc_info)
        {
        case 0:
        {
            static const __attribute__((aligned(4))) uint8_t lang_id_desc[] = {0x04, 0x03, 0x09, 0x04};
            *len = sizeof(lang_id_desc);
            return (const void *)&lang_id_desc;
        }
        }
        break;

    case USB_DESC_DEVICE_QUALIFIER:
        *len = sizeof(qualifier_desc);
        return (const void *)&qualifier_desc;
    }

    return NULL;
}

static void keyboard_set_report(hidd_handle_t *hidd, uint8_t type, uint8_t id, void *buf, size_t len)
{
    printf("Keyboard LED status is %02x\r\n", *(uint8_t *)buf);
}

static void enum_completed_event_cb(usbd_handle_t *h, usbd_event_ctx_t *ctx)
{
    hidd_handle_t *hidd = NULL;

    /* Initialize HID device handle for the keyboard interface */
    hidd = &hidd_handles[0];
    memset(hidd, 0, sizeof(hidd_handle_t));
    hidd->usbd_handle = h;
    hidd->itf_num = 0;
    hidd->hid_desc = &config_desc[18];
    hidd->report_desc = keyboard_report_desc;
    hidd->hid_desc_size = config_desc[18];
    hidd->report_desc_size = sizeof(keyboard_report_desc);
    hidd->in_ep = (usb_desc_endpoint_t *)&config_desc[27];
    hidd->report_buf = keyboard_report_buf;
    hidd->report_buf_size = sizeof(keyboard_report_buf);
    hidd->set_report_comp_cb = keyboard_set_report;
    assert(hidd_drv_open(hidd));

    /* Initialize HID device handle for the mouse interface */
    hidd = &hidd_handles[1];
    memset(hidd, 0, sizeof(hidd_handle_t));
    hidd->usbd_handle = h;
    hidd->itf_num = 1;
    hidd->hid_desc = &config_desc[43];
    hidd->report_desc = mouse_report_desc;
    hidd->hid_desc_size = config_desc[43];
    hidd->report_desc_size = sizeof(mouse_report_desc);
    hidd->in_ep = (usb_desc_endpoint_t *)&config_desc[52];
    assert(hidd_drv_open(hidd));
}

int main(void)
{
    board_init();

    usbd_handle_t *h = board_usbd_init(0);
    assert(h != NULL);

    assert(usbd_drv_open(h, USB_SPEED_FULL, false, get_desc_cb));
    assert(usbd_register_event_callback(h, USBD_EVENT_ENUM_COMPLETED, enum_completed_event_cb));

    while (1);

    return 0;
}
