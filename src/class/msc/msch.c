/********************************** (C) COPYRIGHT *******************************
* File Name          : msch.c
* Author             : WCH
* Version            : V1.0.0
* Date               : 2026/10/20
* Description        : USB MSC host class driver
*********************************************************************************
* Copyright (c) 2026 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

/* @define */
#define USB_LOG_TAG "MSCH"

/* @include */
#include "usb_driver.h"
#include "host/usbh_driver_private.h"

#ifdef USB_CLASS_MSCH_DRIVER_EN

/* @define */
#ifndef MSCH_HANDLE_POOL_SIZE
#define MSCH_HANDLE_POOL_SIZE 1
#endif

#ifndef MSCH_LUN_MAX_COUNT
#define MSCH_LUN_MAX_COUNT 4
#endif

#define CTRL_XFER_TIMEOUT         500
#define BULK_ONLY_CBW_TIMEOUT     1000
#define BULK_ONLY_CSW_TIMEOUT     5000
#define BULK_ONLY_DATA_TIMEOUT    5000

#define MAX_LUN_LIMIT             15
#define MAX_SECTOR_COUNT_PER_XFER 65535
#define SCSI_SENSE_RESPONSE_MASK  0x7E
#define SCSI_SENSE_RESPONSE_FIXED 0x70
#define LUN_READY_RETRY_COUNT     50
#define LUN_READY_RETRY_INTERVAL  100
#define TEST_READY_INTERVAL       1000

/* @enum */
typedef enum
{
    ENUM_STAGE_IDLE,
    ENUM_STAGE_GET_MAX_LUN,
    ENUM_STAGE_ENUM_FAILED,
    ENUM_STAGE_END,
} enum_stage_t;

typedef enum
{
    RUN_STAGE_IDLE,
    RUN_STAGE_INQUIRY,
    RUN_STAGE_INQUIRY_DONE,
    RUN_STAGE_TEST_UNIT_READY,
    RUN_STAGE_TEST_UNIT_READY_DONE,
    RUN_STAGE_READ_CAPACITY,
    RUN_STAGE_READ_CAPACITY_DONE,
    RUN_STAGE_REQUEST_SENSE,
    RUN_STAGE_REQUEST_SENSE_DONE,
    RUN_STAGE_RETRY_WAIT,
    RUN_STAGE_READ,
    RUN_STAGE_READ_DONE,
    RUN_STAGE_WRITE,
    RUN_STAGE_WRITE_DONE,
} run_stage_t;

typedef enum
{
    BULK_ONLY_STAGE_IDLE,
    BULK_ONLY_STAGE_CMD,
    BULK_ONLY_STAGE_DATA,
    BULK_ONLY_STAGE_DATA_CLEAR_HALT,
    BULK_ONLY_STAGE_STATUS,
    BULK_ONLY_STAGE_CSW_CLEAR_HALT,
    BULK_ONLY_STAGE_RESET,
} bulk_only_stage_t;

typedef enum
{
    BULK_ONLY_STATUS_PASSED,
    BULK_ONLY_STATUS_FAILED,
    BULK_ONLY_STATUS_ERROR,
} bulk_only_status_t;

/* @struct */
typedef struct
{
    bool ready;
    uint32_t sector_size;
    uint32_t sector_count;
    char vendor_id[9];
    char product_id[17];
} msch_lun_t;

typedef struct msch_handle
{
    struct msch_handle *next;
    usbh_device_t *dev;

    /* MSC interface information */
    const usb_desc_interface_t *itf_desc;
    const usb_desc_endpoint_t *in_ep;
    const usb_desc_endpoint_t *out_ep;

    /* Logical unit context */
    uint8_t lun_count;
    msch_lun_t luns[MSCH_LUN_MAX_COUNT];

    /* Run-time context */
    uint8_t enum_stage;
    uint8_t run_stage;
    uint8_t cur_lun;
    uint8_t retry_count;
    bool lun_reinit;
    bool sense_for_io;
    bool test_ready_poll;
    uint32_t retry_timestamp;
    uint32_t test_ready_timestamp;

    /* Application read/write request */
    uint8_t io_lun;
    void *io_buf;
    uint32_t io_sector;
    uint32_t io_count;
    msch_xfer_cb io_cb;

    /* Bulk-only transport context */
    bool bulk_only_busy;
    uint8_t bulk_only_stage;
    uint8_t bulk_only_status;
    bool csw_retried;
    uint32_t cbw_tag;
    uint32_t data_len;
    void *data_buf;

    __attribute__((aligned(4))) msc_cbw_t cbw;
    __attribute__((aligned(4))) msc_csw_t csw;
    __attribute__((aligned(4))) uint8_t ctrl_buf[4];
    __attribute__((aligned(4))) uint8_t cmd_buf[64];
} msch_handle_t;

/* @global */
static msch_handle_t *msc_list = NULL;
static msch_handle_t handle_pool[MSCH_HANDLE_POOL_SIZE] = {0};

/* @function declarations */
static bool msc_drv_bind_cb(usbh_device_t *dev, const void *desc, uint16_t len);
static bool msc_drv_unbind_cb(usbh_device_t *dev, uint8_t itf_num);
static void endp_event_callback(void *handle, usb_endp_t endp, usbh_endp_event_t event);
static void bulk_only_xfer_cb(void *handle, bool rst, usb_endp_t endp, const void *buf, size_t len);

static const usbh_drv_info_t msc_drv_info = {
    .vid = 0x0000,
    .pid = 0x0000,
    .dev_class = 0x00,
    .dev_subclass = 0x00,
    .dev_protocol = 0x00,
    .itf_class = USB_CLASS_MSC,
    .itf_subclass = MSC_SUBCLASS_SCSI,
    .itf_protocol = MSC_PROTOCOL_BULK_ONLY,
    .match_bitmap = (USBH_DRV_MATCH_ITF_CLASS | USBH_DRV_MATCH_ITF_SUBCLASS | USBH_DRV_MATCH_ITF_PROTOCOL),
    .bind = msc_drv_bind_cb,
    .unbind = msc_drv_unbind_cb,
};

static void enum_ctrl_xfer_cb(void *handle, bool rst, const usb_setup_t *setup, const void *buf, uint16_t len)
{
    msch_handle_t *msc = (msch_handle_t *)handle;
    if (!rst)
    {
        USB_LOGE("Handle %p: Enumeration control transfer failed at stage %d", msc, msc->enum_stage);
        msc->enum_stage = ENUM_STAGE_ENUM_FAILED;
        return;
    }

    switch (msc->enum_stage)
    {
    case ENUM_STAGE_GET_MAX_LUN:
    {
        uint8_t max_lun = USB_MIN(msc->ctrl_buf[0], MAX_LUN_LIMIT);
        msc->lun_count = USB_MIN(max_lun + 1, MSCH_LUN_MAX_COUNT);
        USB_LOGI("Handle %p: Max LUN %d", msc, max_lun);

        if (!usbh_endp_open(msc->dev, msc, msc->in_ep, bulk_only_xfer_cb))
        {
            msc->enum_stage = ENUM_STAGE_ENUM_FAILED;
            USB_LOGE("Handle %p: Failed to open IN endpoint", msc);
            return;
        }
        if (!usbh_endp_open(msc->dev, msc, msc->out_ep, bulk_only_xfer_cb))
        {
            msc->enum_stage = ENUM_STAGE_ENUM_FAILED;
            usbh_endp_close(msc->dev, msc->in_ep->bEndpointAddress);
            USB_LOGE("Handle %p: Failed to open OUT endpoint", msc);
            return;
        }
        if (!usbh_endp_event_register(msc->dev, msc->in_ep->bEndpointAddress, endp_event_callback) ||
            !usbh_endp_event_register(msc->dev, msc->out_ep->bEndpointAddress, endp_event_callback))
        {
            msc->enum_stage = ENUM_STAGE_ENUM_FAILED;
            usbh_endp_close(msc->dev, msc->in_ep->bEndpointAddress);
            usbh_endp_close(msc->dev, msc->out_ep->bEndpointAddress);
            USB_LOGE("Handle %p: Failed to register endpoint event callback", msc);
            return;
        }

        msc->enum_stage = ENUM_STAGE_END;
        msc->run_stage = RUN_STAGE_INQUIRY;
        USB_LOGI("Handle %p: Enumeration completed", msc);
        break;
    }
    }
}

static void msc_enum_task(msch_handle_t *msc)
{
    switch (msc->enum_stage)
    {
    case ENUM_STAGE_IDLE:
    {
        if (usbh_is_ctrl_xfering(msc->dev)) return;
        msc->enum_stage = ENUM_STAGE_GET_MAX_LUN;
        usb_setup_t *request = &msc->dev->ctrl_setup;
        request->bmRequestType = USB_SET_REQ(USB_DIR_IN, USB_REQ_TYPE_CLASS, USB_REQ_RCPT_INTERFACE);
        request->bRequest = MSC_REQ_GET_MAX_LUN;
        request->wValue = 0;
        request->wIndex = msc->itf_desc->bInterfaceNumber;
        request->wLength = 1;
        usbh_ctrl_xfer(msc->dev, msc, request, msc->ctrl_buf, CTRL_XFER_TIMEOUT, enum_ctrl_xfer_cb);
        break;
    }

    case ENUM_STAGE_ENUM_FAILED:
        msc->enum_stage = ENUM_STAGE_END;
        msc_drv_unbind_cb(msc->dev, msc->itf_desc->bInterfaceNumber);
        break;
    }
}

static void endp_event_callback(void *handle, usb_endp_t endp, usbh_endp_event_t event)
{
    msch_handle_t *msc = (msch_handle_t *)handle;
    if (event == USBH_ENDP_EVENT_CLEAR_STALL)
    {
        msc->bulk_only_stage = BULK_ONLY_STAGE_STATUS;
    }
    else if (event == USBH_ENDP_EVENT_OPERATION_FAILED)
    {
        msc->bulk_only_stage = BULK_ONLY_STAGE_RESET;
    }
}

static void bulk_only_xfer_cb(void *handle, bool rst, usb_endp_t endp, const void *buf, size_t len)
{
    msch_handle_t *msc = (msch_handle_t *)handle;
    usb_endp_t in_endp = msc->in_ep->bEndpointAddress;
    usb_endp_t out_endp = msc->out_ep->bEndpointAddress;
    usb_endp_t data_endp = USB_GET_REQ_DIR(msc->cbw.bmCBWFlags) ? in_endp : out_endp;

    msc->bulk_only_busy = false;
    switch (msc->bulk_only_stage)
    {
    case BULK_ONLY_STAGE_CMD:
        if (rst)
        {
            msc->bulk_only_stage = msc->data_len ? BULK_ONLY_STAGE_DATA : BULK_ONLY_STAGE_STATUS;
        }
        else
        {
            msc->bulk_only_stage = BULK_ONLY_STAGE_RESET;
        }
        break;

    case BULK_ONLY_STAGE_DATA:
        if (rst)
        {
            msc->bulk_only_stage = BULK_ONLY_STAGE_STATUS;
        }
        else if (usbh_is_endp_stalled(msc->dev, data_endp))
        {
            msc->bulk_only_stage = BULK_ONLY_STAGE_DATA_CLEAR_HALT;
            usbh_endp_stall(msc->dev, data_endp, false);
        }
        else
        {
            msc->bulk_only_stage = BULK_ONLY_STAGE_RESET;
        }
        break;

    case BULK_ONLY_STAGE_STATUS:
        if (rst)
        {
            const msc_csw_t *csw = &msc->csw;
            if (len != sizeof(msc_csw_t) || csw->dCSWSignature != MSC_CSW_SIGNATURE ||
                csw->dCSWTag != msc->cbw.dCBWTag || csw->bCSWStatus > MSC_BULK_ONLY_CSW_STATUS_FAILED)
            {
                msc->bulk_only_stage = BULK_ONLY_STAGE_RESET;
            }
            else
            {
                msc->bulk_only_stage = BULK_ONLY_STAGE_IDLE;
                msc->bulk_only_status = csw->bCSWStatus == MSC_BULK_ONLY_CSW_STATUS_PASSED ? BULK_ONLY_STATUS_PASSED
                                                                                           : BULK_ONLY_STATUS_FAILED;
            }
        }
        else if (usbh_is_endp_stalled(msc->dev, in_endp) && !msc->csw_retried)
        {
            msc->csw_retried = true;
            msc->bulk_only_stage = BULK_ONLY_STAGE_CSW_CLEAR_HALT;
            usbh_endp_stall(msc->dev, in_endp, false);
        }
        else
        {
            msc->bulk_only_stage = BULK_ONLY_STAGE_RESET;
        }
        break;
    }
}

static void bulk_only_ctrl_xfer_cb(void *handle, bool rst, const usb_setup_t *setup, const void *buf, uint16_t len)
{
    msch_handle_t *msc = (msch_handle_t *)handle;
    msc->bulk_only_stage = BULK_ONLY_STAGE_IDLE;
    msc->bulk_only_status = BULK_ONLY_STATUS_ERROR;
    if (!rst) msc_drv_unbind_cb(msc->dev, msc->itf_desc->bInterfaceNumber);
}

static void bulk_only_task(msch_handle_t *msc)
{
    if (msc->bulk_only_busy) return;
    usb_endp_t in_endp = msc->in_ep->bEndpointAddress;
    usb_endp_t out_endp = msc->out_ep->bEndpointAddress;

    switch (msc->bulk_only_stage)
    {
    case BULK_ONLY_STAGE_CMD:
        if (usbh_endp_write(msc->dev, out_endp, &msc->cbw, sizeof(msc_cbw_t), BULK_ONLY_CBW_TIMEOUT))
        {
            msc->bulk_only_busy = true;
        }
        else
        {
            msc->bulk_only_stage = BULK_ONLY_STAGE_RESET;
        }
        break;

    case BULK_ONLY_STAGE_DATA:
    {
        uint32_t timeout = BULK_ONLY_DATA_TIMEOUT + (msc->data_len >> 10) * BULK_ONLY_DATA_TIMEOUT;
        if (USB_GET_REQ_DIR(msc->cbw.bmCBWFlags)
                ? usbh_endp_read(msc->dev, in_endp, msc->data_buf, msc->data_len, timeout)
                : usbh_endp_write(msc->dev, out_endp, msc->data_buf, msc->data_len, timeout))
        {
            msc->bulk_only_busy = true;
        }
        else
        {
            msc->bulk_only_stage = BULK_ONLY_STAGE_RESET;
        }
        break;
    }

    case BULK_ONLY_STAGE_STATUS:
        if (usbh_endp_read(msc->dev, in_endp, &msc->csw, sizeof(msc_csw_t), BULK_ONLY_CSW_TIMEOUT))
        {
            msc->bulk_only_busy = true;
        }
        else
        {
            msc->bulk_only_stage = BULK_ONLY_STAGE_RESET;
        }
        break;

    case BULK_ONLY_STAGE_RESET:
    {
        if (usbh_is_ctrl_xfering(msc->dev)) break;
        usb_setup_t *request = &msc->dev->ctrl_setup;
        request->bmRequestType = USB_SET_REQ(USB_DIR_OUT, USB_REQ_TYPE_CLASS, USB_REQ_RCPT_INTERFACE);
        request->bRequest = MSC_REQ_BULK_ONLY_RESET;
        request->wValue = 0;
        request->wIndex = msc->itf_desc->bInterfaceNumber;
        request->wLength = 0;
        usbh_ctrl_xfer(msc->dev, msc, request, NULL, CTRL_XFER_TIMEOUT, bulk_only_ctrl_xfer_cb);
        break;
    }
    }
}

static void bulk_only_start(msch_handle_t *msc, uint8_t lun, void *buf, uint32_t len)
{
    msc->cbw_tag++;
    msc_cbw_t *cbw = &msc->cbw;
    memset(cbw, 0, sizeof(msc_cbw_t));
    cbw->dCBWSignature = MSC_CBW_SIGNATURE;
    cbw->dCBWTag = msc->cbw_tag;
    cbw->dCBWDataTransferLength = len;
    cbw->bCBWLUN = lun;

    msc->data_buf = buf;
    msc->data_len = len;
    msc->csw_retried = false;
    msc->bulk_only_stage = BULK_ONLY_STAGE_CMD;
}

static void lun_init_next(msch_handle_t *msc)
{
    msc->cur_lun++;
    msc->retry_count = 0;
    if (!msc->lun_reinit && msc->cur_lun < msc->lun_count)
    {
        msc->run_stage = RUN_STAGE_INQUIRY;
        return;
    }
    msc->lun_reinit = false;
    msc->run_stage = RUN_STAGE_IDLE;
}

static void retry_wait(msch_handle_t *msc)
{
    if (usbh_drv_get_timestamp(msc->dev->host) - msc->retry_timestamp >= LUN_READY_RETRY_INTERVAL)
    {
        msc->run_stage = RUN_STAGE_TEST_UNIT_READY;
    }
}

static void inquiry_start(msch_handle_t *msc)
{
    memset(msc->cmd_buf, 0, sizeof(msc->cmd_buf));
    bulk_only_start(msc, msc->cur_lun, msc->cmd_buf, sizeof(msc->cmd_buf));
    msc->cbw.bmCBWFlags = 0x80;
    msc->cbw.bCBWCBLength = sizeof(scsi_inquiry_cmd_t);
    scsi_inquiry_cmd_t *cmd = (scsi_inquiry_cmd_t *)msc->cbw.CBWCB;
    cmd->operation_code = SCSI_OPCODE_INQUIRY;
    cmd->allocation_length = bswap16(sizeof(msc->cmd_buf));
    msc->run_stage = RUN_STAGE_INQUIRY_DONE;
}

static void test_ready_poll_start(msch_handle_t *msc)
{
    uint32_t timestamp = usbh_drv_get_timestamp(msc->dev->host);
    if (timestamp - msc->test_ready_timestamp < TEST_READY_INTERVAL) return;
    msc->test_ready_timestamp = timestamp;
    if (++msc->cur_lun >= msc->lun_count) msc->cur_lun = 0;
    msc->test_ready_poll = true;
    msc->run_stage = RUN_STAGE_TEST_UNIT_READY;
}

static void test_ready_start(msch_handle_t *msc)
{
    bulk_only_start(msc, msc->cur_lun, NULL, 0);
    msc->cbw.bmCBWFlags = 0x00;
    msc->cbw.bCBWCBLength = sizeof(scsi_test_unit_ready_cmd_t);
    scsi_test_unit_ready_cmd_t *cmd = (scsi_test_unit_ready_cmd_t *)msc->cbw.CBWCB;
    cmd->operation_code = SCSI_OPCODE_TEST_UNIT_READY;
    msc->run_stage = RUN_STAGE_TEST_UNIT_READY_DONE;
}

static void read_capacity_start(msch_handle_t *msc)
{
    memset(msc->cmd_buf, 0, sizeof(scsi_read_capacity_10_data_t));
    bulk_only_start(msc, msc->cur_lun, msc->cmd_buf, sizeof(scsi_read_capacity_10_data_t));
    msc->cbw.bmCBWFlags = 0x80;
    msc->cbw.bCBWCBLength = sizeof(scsi_read_capacity_10_cmd_t);
    scsi_read_capacity_10_cmd_t *cmd = (scsi_read_capacity_10_cmd_t *)msc->cbw.CBWCB;
    cmd->operation_code = SCSI_OPCODE_READ_CAPACITY_10;
    msc->run_stage = RUN_STAGE_READ_CAPACITY_DONE;
}

static void request_sense_start(msch_handle_t *msc)
{
    uint8_t lun = msc->sense_for_io ? msc->io_lun : msc->cur_lun;
    memset(msc->cmd_buf, 0, sizeof(scsi_sense_data_t));
    bulk_only_start(msc, lun, msc->cmd_buf, sizeof(scsi_sense_data_t));
    msc->cbw.bmCBWFlags = 0x80;
    msc->cbw.bCBWCBLength = sizeof(scsi_request_sense_cmd_t);
    scsi_request_sense_cmd_t *cmd = (scsi_request_sense_cmd_t *)msc->cbw.CBWCB;
    cmd->operation_code = SCSI_OPCODE_REQUEST_SENSE;
    cmd->allocation_length = sizeof(scsi_sense_data_t);
    msc->run_stage = RUN_STAGE_REQUEST_SENSE_DONE;
}

static void io_start(msch_handle_t *msc)
{
    if (msc->run_stage == RUN_STAGE_READ)
    {
        bulk_only_start(msc, msc->io_lun, msc->io_buf, msc->io_count * msc->luns[msc->io_lun].sector_size);
        msc->cbw.bmCBWFlags = 0x80;
        msc->cbw.bCBWCBLength = sizeof(scsi_read_10_cmd_t);
        scsi_read_10_cmd_t *cmd = (scsi_read_10_cmd_t *)msc->cbw.CBWCB;
        cmd->operation_code = SCSI_OPCODE_READ_10;
        cmd->logical_block_address = bswap32(msc->io_sector);
        cmd->transfer_length = bswap16(msc->io_count);
        msc->run_stage = RUN_STAGE_READ_DONE;
    }
    else
    {
        bulk_only_start(msc, msc->io_lun, msc->io_buf, msc->io_count * msc->luns[msc->io_lun].sector_size);
        msc->cbw.bmCBWFlags = 0x00;
        msc->cbw.bCBWCBLength = sizeof(scsi_write_10_cmd_t);
        scsi_write_10_cmd_t *cmd = (scsi_write_10_cmd_t *)msc->cbw.CBWCB;
        cmd->operation_code = SCSI_OPCODE_WRITE_10;
        cmd->logical_block_address = bswap32(msc->io_sector);
        cmd->transfer_length = bswap16(msc->io_count);
        msc->run_stage = RUN_STAGE_WRITE_DONE;
    }
}

static void inquiry_done(msch_handle_t *msc)
{
    msch_lun_t *lun = &msc->luns[msc->cur_lun];
    const scsi_inquiry_data_t *inquiry = (const scsi_inquiry_data_t *)msc->cmd_buf;

    if (msc->bulk_only_status != BULK_ONLY_STATUS_PASSED)
    {
        USB_LOGW("Handle %p: LUN %d INQUIRY failed", msc, msc->cur_lun);
        lun_init_next(msc);
        return;
    }

    switch (inquiry->peripheral_device_type)
    {
    case SCSI_DEVICE_TYPE_DIRECT_ACCESS:
    case SCSI_DEVICE_TYPE_CD_DVD:
    case SCSI_DEVICE_TYPE_OPTICAL_MEMORY:
    case SCSI_DEVICE_TYPE_SIMPLIFIED_DIRECT_ACCESS:
        break;

    default:
        USB_LOGW("Handle %p: LUN %d unsupported device type 0x%02X", msc, msc->cur_lun,
                 inquiry->peripheral_device_type);
        lun_init_next(msc);
        return;
    }

    memset(lun->vendor_id, 0, sizeof(lun->vendor_id));
    memset(lun->product_id, 0, sizeof(lun->product_id));
    memcpy(lun->vendor_id, inquiry->vendor_identification, sizeof(inquiry->vendor_identification));
    memcpy(lun->product_id, inquiry->product_identification, sizeof(inquiry->product_identification));
    USB_LOGI("Handle %p: LUN %d %s %s", msc, msc->cur_lun, lun->vendor_id, lun->product_id);
    msc->run_stage = RUN_STAGE_TEST_UNIT_READY;
}

static void test_ready_done(msch_handle_t *msc)
{
    if (msc->test_ready_poll)
    {
        if (msc->bulk_only_status == BULK_ONLY_STATUS_FAILED)
        {
            msc->sense_for_io = false;
            msc->run_stage = RUN_STAGE_REQUEST_SENSE;
        }
        else
        {
            msc->test_ready_poll = false;
            msc->run_stage = RUN_STAGE_IDLE;
        }
        return;
    }

    if (msc->bulk_only_status == BULK_ONLY_STATUS_PASSED)
    {
        msc->run_stage = RUN_STAGE_READ_CAPACITY;
    }
    else if (msc->bulk_only_status == BULK_ONLY_STATUS_FAILED)
    {
        msc->sense_for_io = false;
        msc->run_stage = RUN_STAGE_REQUEST_SENSE;
    }
    else
    {
        lun_init_next(msc);
    }
}

static void read_capacity_done(msch_handle_t *msc)
{
    if (msc->bulk_only_status == BULK_ONLY_STATUS_PASSED)
    {
        msch_lun_t *lun = &msc->luns[msc->cur_lun];
        const scsi_read_capacity_10_data_t *capacity = (const scsi_read_capacity_10_data_t *)msc->cmd_buf;

        uint32_t last_lba = bswap32(capacity->returned_logical_block_address);
        uint32_t sector_size = bswap32(capacity->block_length_in_bytes);
        if (sector_size == 0)
        {
            USB_LOGW("Handle %p: LUN %d reports an invalid sector size", msc, msc->cur_lun);
            lun_init_next(msc);
            return;
        }

        /* Capacity beyond 2TB can not be described by READ CAPACITY(10), so limit it to the addressable range */
        lun->sector_size = sector_size;
        lun->sector_count = last_lba == 0xFFFFFFFF ? last_lba : last_lba + 1;
        lun->ready = true;
        USB_LOGI("Handle %p: LUN %d ready, %u sectors of %u bytes", msc, msc->cur_lun, (unsigned int)lun->sector_count,
                 (unsigned int)lun->sector_size);
        lun_init_next(msc);
    }
    else if (msc->bulk_only_status == BULK_ONLY_STATUS_FAILED)
    {
        msc->sense_for_io = false;
        msc->run_stage = RUN_STAGE_REQUEST_SENSE;
    }
    else
    {
        lun_init_next(msc);
    }
}

static void request_sense_done(msch_handle_t *msc)
{
    uint8_t sense_key = 0;
    uint8_t asc = 0;
    const scsi_sense_data_t *sense = (const scsi_sense_data_t *)msc->cmd_buf;

    if (msc->bulk_only_status == BULK_ONLY_STATUS_PASSED &&
        (msc->cmd_buf[0] & SCSI_SENSE_RESPONSE_MASK) == SCSI_SENSE_RESPONSE_FIXED)
    {
        sense_key = sense->sense_key;
        asc = sense->additional_sense_code;
    }

    bool not_ready = sense_key == SCSI_SENSE_KEY_NOT_READY || sense_key == SCSI_SENSE_KEY_UNIT_ATTENTION;

    if (msc->test_ready_poll)
    {
        msc->test_ready_poll = false;
        if (not_ready)
        {
            USB_LOGW("Handle %p: LUN %d not ready, sense key 0x%02X ASC 0x%02X", msc, msc->cur_lun, sense_key, asc);
            msc->luns[msc->cur_lun].ready = false;
            msc->lun_reinit = true;
            msc->retry_count = 0;
            msc->run_stage = sense_key == SCSI_SENSE_KEY_UNIT_ATTENTION ? RUN_STAGE_TEST_UNIT_READY : RUN_STAGE_IDLE;
        }
        else
        {
            msc->run_stage = RUN_STAGE_IDLE;
        }
        return;
    }

    /* Failed application request: report it, and re-initialize the LUN if its medium state has changed */
    if (msc->sense_for_io)
    {
        USB_LOGW("Handle %p: LUN %d request failed, sense key 0x%02X ASC 0x%02X", msc, msc->io_lun, sense_key, asc);
        msc->sense_for_io = false;
        if (not_ready)
        {
            msc->luns[msc->io_lun].ready = false;
            msc->cur_lun = msc->io_lun;
            msc->lun_reinit = true;
            msc->retry_count = 0;
            msc->run_stage = RUN_STAGE_TEST_UNIT_READY;
        }
        else
        {
            msc->run_stage = RUN_STAGE_IDLE;
        }
        if (msc->io_cb)
        {
            msc->io_cb(msc, msc->io_lun, false, msc->io_buf, msc->io_count);
        }
        return;
    }

    /* LUN initialization: wait for the medium to become ready */
    if (not_ready && asc != SCSI_ASC_MEDIUM_NOT_PRESENT && msc->retry_count < LUN_READY_RETRY_COUNT)
    {
        msc->retry_count++;
        msc->retry_timestamp = usbh_drv_get_timestamp(msc->dev->host);
        msc->run_stage = sense_key == SCSI_SENSE_KEY_UNIT_ATTENTION ? RUN_STAGE_TEST_UNIT_READY : RUN_STAGE_RETRY_WAIT;
    }
    else
    {
        USB_LOGW("Handle %p: LUN %d not ready, sense key 0x%02X ASC 0x%02X", msc, msc->cur_lun, sense_key, asc);
        lun_init_next(msc);
    }
}

static void io_done(msch_handle_t *msc)
{
    if (msc->bulk_only_status == BULK_ONLY_STATUS_FAILED)
    {
        msc->run_stage = RUN_STAGE_REQUEST_SENSE;
        msc->sense_for_io = true;
    }
    else
    {
        msc->run_stage = RUN_STAGE_IDLE;
        if (msc->io_cb)
        {
            msc->io_cb(msc, msc->io_lun, msc->bulk_only_status == BULK_ONLY_STATUS_PASSED, msc->io_buf, msc->io_count);
        }
    }
}

static void msc_run_task(msch_handle_t *msc)
{
    if (msc->bulk_only_stage)
    {
        bulk_only_task(msc);
    }
    else
    {
        switch (msc->run_stage)
        {
        case RUN_STAGE_IDLE:
            test_ready_poll_start(msc);
            break;

        case RUN_STAGE_INQUIRY:
            inquiry_start(msc);
            break;

        case RUN_STAGE_INQUIRY_DONE:
            inquiry_done(msc);
            break;

        case RUN_STAGE_TEST_UNIT_READY:
            test_ready_start(msc);
            break;

        case RUN_STAGE_TEST_UNIT_READY_DONE:
            test_ready_done(msc);
            break;

        case RUN_STAGE_READ_CAPACITY:
            read_capacity_start(msc);
            break;

        case RUN_STAGE_READ_CAPACITY_DONE:
            read_capacity_done(msc);
            break;

        case RUN_STAGE_REQUEST_SENSE:
            request_sense_start(msc);
            break;

        case RUN_STAGE_REQUEST_SENSE_DONE:
            request_sense_done(msc);
            break;

        case RUN_STAGE_RETRY_WAIT:
            retry_wait(msc);
            break;

        case RUN_STAGE_READ:
        case RUN_STAGE_WRITE:
            io_start(msc);
            break;

        case RUN_STAGE_READ_DONE:
        case RUN_STAGE_WRITE_DONE:
            io_done(msc);
            break;
        }
    }
}

void msch_drv_task(void)
{
    msch_handle_t *msc = msc_list;
    while (msc)
    {
        msch_handle_t *next = msc->next;
        if (!msc->dev->suspended)
        {
            if (msc->enum_stage < ENUM_STAGE_END)
            {
                msc_enum_task(msc);
            }
            else
            {
                msc_run_task(msc);
            }
        }
        msc = next;
    }
}

bool msch_drv_open(usbh_handle_t *h)
{
    if (!h) return false;
    return usbh_register_driver(h, &msc_drv_info);
}

bool msch_drv_close(usbh_handle_t *h)
{
    if (!h) return false;
    return usbh_unregister_driver(h, &msc_drv_info);
}

size_t msch_get_count(void)
{
    size_t count = 0;
    msch_handle_t *msc = msc_list;
    while (msc)
    {
        if (msc->enum_stage >= ENUM_STAGE_END) count++;
        msc = msc->next;
    }
    return count;
}

msch_handle_t *msch_get_handle(size_t index)
{
    size_t i = 0;
    msch_handle_t *msc = msc_list;
    while (msc)
    {
        if (msc->enum_stage >= ENUM_STAGE_END)
        {
            if (i == index) return msc;
            i++;
        }
        msc = msc->next;
    }
    return NULL;
}

uint8_t msch_get_lun_count(msch_handle_t *msc)
{
    if (!msc || msc->enum_stage < ENUM_STAGE_END) return 0;
    return msc->lun_count;
}

bool msch_lun_is_ready(msch_handle_t *msc, uint8_t lun)
{
    if (!msc || msc->enum_stage < ENUM_STAGE_END || lun >= msc->lun_count) return false;
    msch_lun_t *l = &msc->luns[lun];
    return l->ready;
}

uint32_t msch_get_sector_size(msch_handle_t *msc, uint8_t lun)
{
    if (!msc || msc->enum_stage < ENUM_STAGE_END || lun >= msc->lun_count) return 0;
    msch_lun_t *l = &msc->luns[lun];
    return l->ready ? l->sector_size : 0;
}

uint32_t msch_get_sector_count(msch_handle_t *msc, uint8_t lun)
{
    if (!msc || msc->enum_stage < ENUM_STAGE_END || lun >= msc->lun_count) return 0;
    msch_lun_t *l = &msc->luns[lun];
    return l->ready ? l->sector_count : 0;
}

const char *msch_get_vendor_id(msch_handle_t *msc, uint8_t lun)
{
    if (!msc || msc->enum_stage < ENUM_STAGE_END || lun >= msc->lun_count) return NULL;
    msch_lun_t *l = &msc->luns[lun];
    return l->vendor_id;
}

const char *msch_get_product_id(msch_handle_t *msc, uint8_t lun)
{
    if (!msc || msc->enum_stage < ENUM_STAGE_END || lun >= msc->lun_count) return NULL;
    msch_lun_t *l = &msc->luns[lun];
    return l->product_id;
}

static bool io_request(msch_handle_t *msc, uint8_t lun, uint32_t sector, uint32_t count, void *buf, msch_xfer_cb cb)
{
    if (!msc || msc->enum_stage < ENUM_STAGE_END || msc->run_stage != RUN_STAGE_IDLE || lun >= msc->lun_count)
        return false;
    msch_lun_t *l = &msc->luns[lun];
    if (!l || !l->ready) return false;
    if (!buf || count == 0 || count > MAX_SECTOR_COUNT_PER_XFER) return false;
    if (sector >= l->sector_count || count > l->sector_count - sector) return false;

    msc->io_lun = lun;
    msc->io_sector = sector;
    msc->io_count = count;
    msc->io_buf = buf;
    msc->io_cb = cb;
    return true;
}

bool msch_read(msch_handle_t *msc, uint8_t lun, uint32_t sector, uint32_t count, void *buf, msch_xfer_cb cb)
{
    if (!io_request(msc, lun, sector, count, buf, cb)) return false;
    msc->run_stage = RUN_STAGE_READ;
    return true;
}

bool msch_write(msch_handle_t *msc, uint8_t lun, uint32_t sector, uint32_t count, const void *buf, msch_xfer_cb cb)
{
    if (!io_request(msc, lun, sector, count, (void *)buf, cb)) return false;
    msc->run_stage = RUN_STAGE_WRITE;
    return true;
}

static msch_handle_t *handle_alloc(void)
{
    for (size_t i = 0; i < USB_ARRAY_SIZE(handle_pool); i++)
    {
        msch_handle_t *msc = &handle_pool[i];
        if (msc->dev == NULL)
        {
            memset(msc, 0, sizeof(msch_handle_t));
            return msc;
        }
    }
    return NULL;
}

static bool msc_drv_bind_cb(usbh_device_t *dev, const void *desc, uint16_t len)
{
    if (!dev || dev->speed == USB_SPEED_LOW || !desc || len == 0) return false;

    const usb_desc_interface_t *itf_desc = NULL;
    const usb_desc_endpoint_t *in_ep = NULL;
    const usb_desc_endpoint_t *out_ep = NULL;

    uint8_t endp_count = 0;
    const uint8_t *curr = (const uint8_t *)desc;
    const uint8_t *end = curr + len;

    while (curr < end)
    {
        uint8_t len = curr[0];
        uint8_t type = curr[1];

        switch (type)
        {
        case USB_DESC_INTERFACE:
        {
            const usb_desc_interface_t *curr_itf = (const usb_desc_interface_t *)curr;
            if (curr_itf->bNumEndpoints > 2) return false;
            if (curr_itf->bAlternateSetting != 0) break;
            itf_desc = curr_itf;
            endp_count = curr_itf->bNumEndpoints;
            break;
        }

        case USB_DESC_ENDPOINT:
        {
            const usb_desc_endpoint_t *ep_desc = (const usb_desc_endpoint_t *)curr;
            uint8_t endp_dir = USB_ENDP_DIR(ep_desc->bEndpointAddress);
            uint8_t endp_type = USB_ENDP_GET_TYPE(ep_desc->bmAttributes);
            if (endp_type != USB_ENDP_TYPE_BULK) return false;
            if (!endp_count) break;
            endp_count--;
            const usb_desc_endpoint_t **target_ep = endp_dir ? &in_ep : &out_ep;
            *target_ep = ep_desc;
            break;
        }
        }

        curr += len;
    }

    if (!itf_desc || !in_ep || !out_ep) return false;

    /* Allocate and initialize a new msc handle */
    msch_handle_t *msc = handle_alloc();
    if (!msc)
    {
        USB_LOGE("Handle %p: Failed to allocate msc handle", dev);
        return false;
    }

    msc->dev = dev;
    msc->itf_desc = itf_desc;
    msc->in_ep = in_ep;
    msc->out_ep = out_ep;
    usb_list_append((void **)&msc_list, msc);
    return true;
}

static bool msc_drv_unbind_cb(usbh_device_t *dev, uint8_t itf_num)
{
    msch_handle_t *msc = msc_list;
    while (msc)
    {
        msch_handle_t *next = msc->next;
        if (msc->dev == dev && (itf_num == USBH_DRV_ALL_INTERFACE || msc->itf_desc->bInterfaceNumber == itf_num))
        {
            usbh_endp_close(msc->dev, msc->in_ep->bEndpointAddress);
            usbh_endp_close(msc->dev, msc->out_ep->bEndpointAddress);
            usb_list_remove((void **)&msc_list, msc);
            memset(msc, 0, sizeof(msch_handle_t));
        }
        msc = next;
    }
    return true;
}

#endif // USB_CLASS_MSCH_DRIVER_EN
