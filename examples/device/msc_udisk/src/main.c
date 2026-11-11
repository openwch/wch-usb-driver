/********************************** (C) COPYRIGHT *******************************
* File Name          : main.c
* Author             : WCH
* Version            : V1.0.0
* Date               : 2026/10/20
* Description        : Main program for MSC U-Disk example
*********************************************************************************
* Copyright (c) 2026 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

/* @include*/
#include <assert.h>
#include <string.h>

#include "board.h"
#include "flash.h"

#include "usb_driver.h"
#include "descriptor.h"

/* @define */
#ifndef USBD_INDEX
#define USBD_INDEX 0
#endif

#if USBD_INDEX >= USB_COUNT
#error "Not supported: USBD_INDEX exceeds USB_COUNT"
#endif

/* @global */
static usbd_handle_t *usbd_handle;
static mscd_handle_t msc_handle;
static __attribute__((aligned(4))) uint8_t flash_buf[FLASH_SECTOR_SIZE];

static const void *get_string_desc_cb(uint8_t index, size_t *len)
{
    static __attribute__((aligned(4))) uint8_t string_desc_buf[128];

    uint8_t char_len;
    uint16_t *char_ptr = (uint16_t *)&string_desc_buf[2];

    if (index == 0)
    {
        char_len = 1;
        char_ptr[0] = 0x0409;
    }
    else if (index == 3)
    {
        static const char ascii_char[] = "0123456789ABCDEF";
        const uint8_t *mac_addr = board_get_mac_address();
        char_len = 12;
        for (uint8_t i = 0; i < 6; i++)
        {
            char_ptr[i * 2 + 0] = ascii_char[(mac_addr[i] >> 4) & 0x0F];
            char_ptr[i * 2 + 1] = ascii_char[(mac_addr[i] >> 0) & 0x0F];
        }
    }
    else
    {
        static const char *string_desc[] = {
            "wch.cn",
            "MSC U-Disk Example",
        };

        uint8_t string_index = index - 1;

        if (string_index < USB_ARRAY_SIZE(string_desc))
        {
            char_len = strlen(string_desc[string_index]);
            for (uint8_t i = 0; i < char_len; i++)
            {
                char_ptr[i] = string_desc[string_index][i];
            }
        }
        else
        {
            *len = 0;
            return NULL;
        }
    }

    uint8_t desc_len = char_len * 2 + 2;
    *len = desc_len;
    string_desc_buf[0] = desc_len;
    string_desc_buf[1] = USB_DESC_STRING;
    return string_desc_buf;
}

static const void *get_stand_desc_cb(usbd_handle_t *h, uint8_t type, uint8_t index, usb_speed_t speed, size_t *len)
{
    static uint8_t other_speed_desc[sizeof(config_desc_hs)];

    switch (type)
    {
    case USB_DESC_DEVICE:
        *len = sizeof(device_desc);
        return (const void *)&device_desc;

    case USB_DESC_CONFIGURATION:
        *len = sizeof(config_desc_hs);
        return speed == USB_SPEED_HIGH ? (const void *)&config_desc_hs : (const void *)&config_desc_fs;

    case USB_DESC_STRING:
        return get_string_desc_cb(index, len);

    case USB_DESC_DEVICE_QUALIFIER:
        *len = sizeof(qua_desc);
        return (const void *)&qua_desc;

    case USB_DESC_OTHER_SPEED_CONFIG:
        memcpy(other_speed_desc, speed == USB_SPEED_HIGH ? config_desc_fs : config_desc_hs, sizeof(config_desc_hs));
        other_speed_desc[1] = USB_DESC_OTHER_SPEED_CONFIG;
        *len = sizeof(other_speed_desc);
        return (const void *)other_speed_desc;
    }

    return NULL;
}

static void bulk_only_reset_cb(struct mscd_handle *msc)
{
}

static bool flash_read_sector(struct mscd_handle *msc, uint32_t sector)
{
    return flash_read(sector, flash_buf);
}

static bool flash_write_sector(struct mscd_handle *msc, uint32_t sector)
{
    bool rst = flash_erase(sector);
    return rst && flash_write(sector, flash_buf);
}

static void enum_completed_event_cb(usbd_handle_t *h, usbd_event_ctx_t *ctx)
{
    /* Initialize MSC device handle */
    memset(&msc_handle, 0, sizeof(msc_handle));
    msc_handle.usbd_handle = h;
    msc_handle.itf_num = 0;
    msc_handle.subclass = MSC_SUBCLASS_SCSI;
    msc_handle.protocol = MSC_PROTOCOL_BULK_ONLY;
    msc_handle.device_type = SCSI_DEVICE_TYPE_DIRECT_ACCESS;
    msc_handle.max_lun = 0;
    strcpy(msc_handle.vendor_id, "wch.cn");
    strcpy(msc_handle.product_id, "U-Disk");
    msc_handle.sector_buf = flash_buf;
    msc_handle.sector_size = FLASH_SECTOR_SIZE;
    msc_handle.sector_count = FLASH_SECTOR_COUNT;
    msc_handle.bulk_only_reset_cb = bulk_only_reset_cb;
    msc_handle.read_sector = flash_read_sector;
    msc_handle.write_sector = flash_write_sector;
    if (ctx->enum_completed.link_speed == USB_SPEED_HIGH)
    {
        msc_handle.in_ep = (const usb_desc_endpoint_t *)&config_desc_hs[18];
        msc_handle.out_ep = (const usb_desc_endpoint_t *)&config_desc_hs[25];
    }
    else
    {
        msc_handle.in_ep = (const usb_desc_endpoint_t *)&config_desc_fs[18];
        msc_handle.out_ep = (const usb_desc_endpoint_t *)&config_desc_fs[25];
    }
    assert(mscd_drv_open(&msc_handle));
}

int main(void)
{
    board_init();
    flash_init();

    usbd_handle = board_usb_init(USBD_INDEX, USB_MODE_DEVICE);
    assert(usbd_handle != NULL);

    assert(usbd_register_event_callback(usbd_handle, USBD_EVENT_ENUM_COMPLETED, enum_completed_event_cb));
    assert(usbd_drv_open(usbd_handle, USB_SPEED_HIGH, false, get_stand_desc_cb));

    while (1);

    return 0;
}
