/*-----------------------------------------------------------------------*/
/* Low level disk I/O module for USB MSC                                 */
/*-----------------------------------------------------------------------*/
/* This module attaches the USB MSC host class driver to FatFs.          */
/* The example uses the first logical unit (LUN 0) as physical drive 0.  */
/*-----------------------------------------------------------------------*/

#include <stdint.h>
#include <string.h>

#include "ff.h"         /* Basic definitions of FatFs */
#include "diskio.h"     /* Declarations FatFs MAI */
#include "usb_driver.h" /* USB host driver */

/* The application provides this function to process USB host tasks */
void usb_host_poll(void);

/* @define */
#define DEV_USB              0         /* Map USB MSC to physical drive 0 */
#define USB_DISK_SECTOR_SIZE 512       /* Sector size of the MSC drive */
#define USB_DISK_WAIT_LOOPS  10000000U /* Task loop limit while waiting for a transfer */

/* @global */
static msch_handle_t *usb_msc = NULL;
static uint8_t usb_lun = 0;
static volatile bool usb_xfer_done = false;
static volatile bool usb_xfer_ok = false;
static __attribute__((aligned(4))) BYTE usb_sector_buf[USB_DISK_SECTOR_SIZE];

/*-----------------------------------------------------------------------*/
/* Attach / Detach the USB MSC drive                                     */
/*-----------------------------------------------------------------------*/

void usb_disk_attach(struct msch_handle *msc, BYTE lun)
{
    usb_msc = (msch_handle_t *)msc;
    usb_lun = lun;
}

void usb_disk_detach(void)
{
    usb_msc = NULL;
}

static bool usb_disk_ready(void)
{
    return usb_msc != NULL && msch_lun_is_ready(usb_msc, usb_lun) &&
           msch_get_sector_size(usb_msc, usb_lun) == USB_DISK_SECTOR_SIZE;
}

/*-----------------------------------------------------------------------*/
/* Sector transfer                                                       */
/*-----------------------------------------------------------------------*/

static void usb_xfer_cb(msch_handle_t *msc, uint8_t lun, bool rst, void *buf, uint32_t count)
{
    (void)msc;
    (void)lun;
    (void)buf;
    (void)count;
    usb_xfer_ok = rst;
    usb_xfer_done = true;
}

static DRESULT usb_disk_xfer(void *buf, LBA_t sector, UINT count, bool write)
{
    uint32_t loops = 0;
    bool accepted;

    if (!usb_disk_ready()) return RES_NOTRDY;

    usb_xfer_done = false;
    usb_xfer_ok = false;

    /* Wait until the MSC driver is idle and accepts the request */
    while (true)
    {
        accepted = write ? msch_write(usb_msc, usb_lun, (uint32_t)sector, count, buf, usb_xfer_cb)
                         : msch_read(usb_msc, usb_lun, (uint32_t)sector, count, buf, usb_xfer_cb);
        if (accepted) break;

        if (!usb_disk_ready()) return RES_NOTRDY;
        if (++loops >= USB_DISK_WAIT_LOOPS) return RES_ERROR;
        extern usbh_handle_t *usbh_handle;
        usbh_drv_task(usbh_handle);
        hubh_drv_task();
        msch_drv_task();
    }

    /* Wait for the transfer completion callback */
    loops = 0;
    while (!usb_xfer_done)
    {
        if (!usb_disk_ready()) return RES_NOTRDY;
        if (++loops >= USB_DISK_WAIT_LOOPS) return RES_ERROR;
        extern usbh_handle_t *usbh_handle;
        usbh_drv_task(usbh_handle);
        hubh_drv_task();
        msch_drv_task();
    }

    return usb_xfer_ok ? RES_OK : RES_ERROR;
}

/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status(BYTE pdrv /* Physical drive nmuber to identify the drive */
)
{
    if (pdrv != DEV_USB) return STA_NOINIT | STA_NODISK;
    return usb_disk_ready() ? 0 : (STA_NOINIT | STA_NODISK);
}

/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize(BYTE pdrv /* Physical drive nmuber to identify the drive */
)
{
    if (pdrv != DEV_USB) return STA_NOINIT | STA_NODISK;
    return usb_disk_ready() ? 0 : (STA_NOINIT | STA_NODISK);
}

/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read(BYTE pdrv,    /* Physical drive nmuber to identify the drive */
                  BYTE *buff,   /* Data buffer to store read data */
                  LBA_t sector, /* Start sector in LBA */
                  UINT count    /* Number of sectors to read */
)
{
    UINT i;

    if (pdrv != DEV_USB) return RES_PARERR;
    if (count == 0) return RES_OK;

    /* The MSC driver requires a 4-byte aligned buffer */
    if (((uintptr_t)buff & 3U) == 0)
    {
        return usb_disk_xfer(buff, sector, count, false);
    }

    /* Use the bounce buffer for unaligned buffers */
    for (i = 0; i < count; i++)
    {
        DRESULT res = usb_disk_xfer(usb_sector_buf, sector + i, 1, false);
        if (res != RES_OK) return res;
        memcpy(buff + (size_t)i * USB_DISK_SECTOR_SIZE, usb_sector_buf, USB_DISK_SECTOR_SIZE);
    }

    return RES_OK;
}

/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if FF_FS_READONLY == 0

DRESULT disk_write(BYTE pdrv,        /* Physical drive nmuber to identify the drive */
                   const BYTE *buff, /* Data to be written */
                   LBA_t sector,     /* Start sector in LBA */
                   UINT count        /* Number of sectors to write */
)
{
    UINT i;

    if (pdrv != DEV_USB) return RES_PARERR;
    if (count == 0) return RES_OK;

    /* The MSC driver requires a 4-byte aligned buffer */
    if (((uintptr_t)buff & 3U) == 0)
    {
        return usb_disk_xfer((void *)buff, sector, count, true);
    }

    /* Use the bounce buffer for unaligned buffers */
    for (i = 0; i < count; i++)
    {
        DRESULT res;
        memcpy(usb_sector_buf, buff + (size_t)i * USB_DISK_SECTOR_SIZE, USB_DISK_SECTOR_SIZE);
        res = usb_disk_xfer(usb_sector_buf, sector + i, 1, true);
        if (res != RES_OK) return res;
    }

    return RES_OK;
}

#endif

/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl(BYTE pdrv, /* Physical drive nmuber (0..) */
                   BYTE cmd,  /* Control code */
                   void *buff /* Buffer to send/receive control data */
)
{
    if (pdrv != DEV_USB) return RES_PARERR;
    if (!usb_disk_ready()) return RES_NOTRDY;

    switch (cmd)
    {
    case CTRL_SYNC: /* Transfers are completed synchronously */
        return RES_OK;

    case GET_SECTOR_COUNT:
        *(LBA_t *)buff = msch_get_sector_count(usb_msc, usb_lun);
        return RES_OK;

    case GET_SECTOR_SIZE:
        *(WORD *)buff = (WORD)msch_get_sector_size(usb_msc, usb_lun);
        return RES_OK;

    case GET_BLOCK_SIZE:
        *(DWORD *)buff = 1;
        return RES_OK;

    default:
        return RES_PARERR;
    }
}
