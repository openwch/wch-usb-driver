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
#include "led.h"

#include "usb_driver.h"

#include "ff.h"
#include "diskio.h"

/* @define */
#ifndef USBH_INDEX
#define USBH_INDEX 0
#endif

#if USBH_INDEX >= USB_COUNT
#error "Not supported: USBH_INDEX exceeds USB_COUNT"
#endif

#define MSC_LUN_INDEX  0
#define SCAN_PATH_MAX  512
#define SCAN_MAX_DEPTH 8

/* @global */
usbh_handle_t *usbh_handle;

static FATFS fatfs;
static FILINFO file_info;
static char scan_path[SCAN_PATH_MAX];
static size_t scan_path_len;
static uint32_t dir_count;
static uint32_t file_count;
static uint64_t total_size;

static bool scan_path_push(const char *name)
{
    size_t name_len = strlen(name);
    size_t sep_len = (scan_path_len == 0 || scan_path[scan_path_len - 1] != '/') ? 1 : 0;

    if (scan_path_len + sep_len + name_len >= sizeof(scan_path)) return false;

    if (sep_len) scan_path[scan_path_len++] = '/';
    memcpy(&scan_path[scan_path_len], name, name_len + 1);
    scan_path_len += name_len;

    return true;
}

static void print_entry(uint8_t depth, const FILINFO *info, bool is_dir)
{
    char size_buf[16];
    char time_buf[24];
    char attr_buf[5];

    if (is_dir)
    {
        strcpy(size_buf, "-");
    }
    else
    {
        snprintf(size_buf, sizeof(size_buf), "%lu", (unsigned long)info->fsize);
    }

    if (info->fdate)
    {
        snprintf(time_buf, sizeof(time_buf), "%04u-%02u-%02u %02u:%02u", 1980 + (info->fdate >> 9),
                 (info->fdate >> 5) & 0x0F, info->fdate & 0x1F, info->ftime >> 11, (info->ftime >> 5) & 0x3F);
    }
    else
    {
        strcpy(time_buf, "----/--/-- --:--");
    }

    attr_buf[0] = is_dir ? 'D' : '-';
    attr_buf[1] = (info->fattrib & AM_RDO) ? 'R' : '-';
    attr_buf[2] = (info->fattrib & AM_HID) ? 'H' : '-';
    attr_buf[3] = (info->fattrib & AM_SYS) ? 'S' : '-';
    attr_buf[4] = '\0';

    printf("%*s%s %10s  %s  [%s] %s%s\r\n", depth * 2, "", is_dir ? "[DIR]" : "[FILE]", size_buf, time_buf, attr_buf,
           info->fname, is_dir ? "/" : "");
}

static void scan_directory(uint8_t depth)
{
    size_t base_len = scan_path_len;
    DIR dir;
    FRESULT res = f_opendir(&dir, scan_path);

    if (res != FR_OK)
    {
        printf("%*s[ERR] Cannot open \"%s\" (%d)\r\n", depth * 2, "", scan_path, res);
        return;
    }

    while ((res = f_readdir(&dir, &file_info)) == FR_OK && file_info.fname[0])
    {
        if (!strcmp(file_info.fname, ".") || !strcmp(file_info.fname, "..")) continue;

        bool is_dir = (file_info.fattrib & AM_DIR) != 0;

        print_entry(depth, &file_info, is_dir);

        if (!is_dir)
        {
            file_count++;
            total_size += file_info.fsize;
            continue;
        }

        dir_count++;

        if (depth >= SCAN_MAX_DEPTH)
        {
            printf("%*s[WARN] Maximum scan depth reached\r\n", (depth + 1) * 2, "");
            continue;
        }

        if (!scan_path_push(file_info.fname))
        {
            printf("%*s[WARN] Path too long\r\n", (depth + 1) * 2, "");
            continue;
        }

        scan_directory(depth + 1);

        /* Restore the path of the current directory */
        scan_path_len = base_len;
        scan_path[base_len] = '\0';
    }

    if (res != FR_OK) printf("%*s[ERR] Cannot read \"%s\" (%d)\r\n", depth * 2, "", scan_path, res);

    f_closedir(&dir);
}

static const char *fat_type_name(BYTE fs_type)
{
    switch (fs_type)
    {
    case FS_FAT12:
        return "FAT12";

    case FS_FAT16:
        return "FAT16";

    case FS_FAT32:
        return "FAT32";

    default:
        return "Unknown";
    }
}

static void scan_usb_disk(msch_handle_t *msc)
{
    printf("\r\nUSB MSC device attached: %s %s\r\n", msch_get_vendor_id(msc, MSC_LUN_INDEX),
           msch_get_product_id(msc, MSC_LUN_INDEX));

    uint32_t sector_size = msch_get_sector_size(msc, MSC_LUN_INDEX);
    if (sector_size != 512)
    {
        printf("Unsupported sector size: %u bytes\r\n", sector_size);
        return;
    }

    usb_disk_attach(msc, MSC_LUN_INDEX);

    FRESULT res = f_mount(&fatfs, "0:", 1);
    if (res != FR_OK)
    {
        printf("Mount failed (%d)\r\n", res);
        usb_disk_detach();
        return;
    }

    DWORD free_clusters = 0;
    FATFS *fs = NULL;
    if (f_getfree("0:", &free_clusters, &fs) == FR_OK && fs != NULL)
    {
        uint64_t total_bytes = (uint64_t)(fs->n_fatent - 2) * fs->csize * 512;

        printf("Volume: %s, %u MiB total\r\n", fat_type_name(fs->fs_type), (total_bytes >> 20));
    }

    strcpy(scan_path, "0:");
    scan_path_len = 2;
    dir_count = 1;
    file_count = 0;
    total_size = 0;

    printf("\r\nDirectory tree of \"0:\":\r\n");
    scan_directory(0);

    printf("\r\nScanned %u directories, %u files, %u bytes\r\n", dir_count, file_count, total_size);

    f_mount(NULL, "0:", 0);
    usb_disk_detach();
}

int main(void)
{
    board_init();
    led_init();

    usbh_handle = (usbh_handle_t *)board_usb_init(USBH_INDEX, USB_MODE_HOST);
    assert(usbh_handle != NULL);

    assert(usbh_drv_open(usbh_handle));
    assert(hubh_drv_open(usbh_handle));
    assert(msch_drv_open(usbh_handle));

    while (1)
    {
        usbh_drv_task(usbh_handle);
        hubh_drv_task();
        msch_drv_task();
        static bool scanned = false;

        /* Check for changes in the number of connected CDC-MSC devices */
        static size_t last_msc_count = 0;
        size_t curr_msc_count = msch_get_count();
        if (curr_msc_count != last_msc_count)
        {
            printf("MSC device count changed: %u -> %u\n", last_msc_count, curr_msc_count);
            last_msc_count = curr_msc_count;
            scanned = false;
        }

        /* Get the handle for the first CDC-MSC device */
        msch_handle_t *msc = msch_get_handle(0);

        /* Update LED status based on CDC-MSC connection and open state */
        led_write(msc ? (msch_lun_is_ready(msc, 0) ? LED_1 : 0) | LED_0 : 0);

        /* Skip the rest of the loop if no CDC-MSC device is connected */
        if (!msc) continue;

        if (!scanned && msch_lun_is_ready(msc, 0))
        {
            scan_usb_disk(msc);
            scanned = true;
        }
    }

    return 0;
}
