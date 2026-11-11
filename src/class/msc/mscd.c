/********************************** (C) COPYRIGHT *******************************
* File Name          : mscd.c
* Author             : WCH
* Version            : V1.0.0
* Date               : 2026/10/20
* Description        : USB MSC device class driver
*********************************************************************************
* Copyright (c) 2026 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

/* @define */
#define USB_LOG_TAG "MSCD"

/* @include */
#include "usb_driver.h"
#include "device/usbd_driver_private.h"

#ifdef USB_CLASS_MSCD_DRIVER_EN

/* @enum */
typedef enum
{
    BULK_ONLY_STAGE_CMD,
    BULK_ONLY_STAGE_DATA,
    BULK_ONLY_STAGE_STATUS,
} bulk_only_stage_t;

static void scsi_set_sense(mscd_handle_t *msc, scsi_sense_key_t sense_key, scsi_additional_sense_code_t asc,
                           uint8_t ascq)
{
    memset(&msc->sense, 0, sizeof(msc->sense));
    msc->sense.response_code = SCSI_SENSE_DATA_FIXED_FORMAT;
    msc->sense.sense_key = sense_key;
    msc->sense.additional_sense_length = sizeof(msc->sense) - 8;
    msc->sense.additional_sense_code = asc;
    msc->sense.additional_sense_code_qualifier = ascq;
}

static void bulk_only_send_csw(mscd_handle_t *msc, uint8_t status)
{
    msc->bulk_only_stage = BULK_ONLY_STAGE_STATUS;
    msc->csw.dCSWSignature = MSC_CSW_SIGNATURE;
    msc->csw.dCSWTag = msc->cbw.dCBWTag;
    msc->csw.dCSWDataResidue = msc->cbw.dCBWDataTransferLength - msc->actual_xfer_len;
    msc->csw.bCSWStatus = status;
    usbd_endp_write(msc->usbd_handle, msc->in_ep->bEndpointAddress, &msc->csw, sizeof(msc->csw));
}

static void bulk_only_cmd_handle(mscd_handle_t *msc, size_t len)
{
    if (len != sizeof(msc->cbw) || msc->cbw.dCBWSignature != MSC_CBW_SIGNATURE)
    {
        usbd_endp_stall(msc->usbd_handle, msc->in_ep->bEndpointAddress, true);
        usbd_endp_stall(msc->usbd_handle, msc->out_ep->bEndpointAddress, true);
        return;
    }

    bool failed = false;
    uint8_t status = MSC_BULK_ONLY_CSW_STATUS_PASSED;
    void *buf = NULL;
    size_t xfer_len = 0;
    msc->actual_xfer_len = 0;

    switch (msc->cbw.CBWCB[0])
    {
    case SCSI_OPCODE_TEST_UNIT_READY:
    {
        memset(&msc->sense, 0, sizeof(msc->sense));

        buf = NULL;
        xfer_len = 0;
        break;
    }
    case SCSI_OPCODE_REQUEST_SENSE:
    {
        scsi_request_sense_cmd_t *request_sense_cmd = (scsi_request_sense_cmd_t *)msc->cbw.CBWCB;
        scsi_sense_data_t *sense_data = (scsi_sense_data_t *)msc->temp_data;

        memcpy(sense_data, &msc->sense, sizeof(scsi_sense_data_t));
        memset(&msc->sense, 0, sizeof(msc->sense));

        buf = sense_data;
        xfer_len = USB_MIN(request_sense_cmd->allocation_length, sizeof(scsi_sense_data_t));
        break;
    }
    case SCSI_OPCODE_INQUIRY:
    {
        scsi_inquiry_data_t *inquiry_data = (scsi_inquiry_data_t *)msc->temp_data;
        memset(inquiry_data, 0, sizeof(scsi_inquiry_data_t));
        inquiry_data->peripheral_device_type = msc->device_type;
        inquiry_data->rmb = 1;
        inquiry_data->version = SCSI_VERSION_SPC_6;
        inquiry_data->response_data_format = 2;
        inquiry_data->additional_length = sizeof(scsi_inquiry_data_t) - 5;
        memcpy(inquiry_data->vendor_identification, msc->vendor_id, sizeof(inquiry_data->vendor_identification));
        memcpy(inquiry_data->product_identification, msc->product_id, sizeof(inquiry_data->product_identification));

        buf = inquiry_data;
        xfer_len = sizeof(scsi_inquiry_data_t);
        break;
    }
    case SCSI_OPCODE_START_STOP_UNIT:
    {
        scsi_start_stop_unit_cmd_t *start_stop_unit_cmd = (scsi_start_stop_unit_cmd_t *)msc->cbw.CBWCB;

        if (start_stop_unit_cmd->loej || start_stop_unit_cmd->power_condition)
        {
            /* The storage does not support medium ejection or power condition changes */
            scsi_set_sense(msc, SCSI_SENSE_KEY_ILLEGAL_REQUEST, SCSI_ASC_INVALID_FIELD_IN_CDB, 0);
            status = MSC_BULK_ONLY_CSW_STATUS_FAILED;
        }
        else
        {
            memset(&msc->sense, 0, sizeof(msc->sense));
        }

        buf = NULL;
        xfer_len = 0;
        break;
    }
    case SCSI_OPCODE_READ_CAPACITY_10:
    {
        scsi_read_capacity_10_data_t *read_capacity_10_data = (scsi_read_capacity_10_data_t *)msc->temp_data;
        memset(read_capacity_10_data, 0, sizeof(scsi_read_capacity_10_data_t));
        read_capacity_10_data->returned_logical_block_address = __builtin_bswap32(msc->sector_count - 1);
        read_capacity_10_data->block_length_in_bytes = __builtin_bswap32(msc->sector_size);

        buf = read_capacity_10_data;
        xfer_len = sizeof(scsi_read_capacity_10_data_t);
        break;
    }
    case SCSI_OPCODE_READ_10:
    {
        scsi_read_10_cmd_t *read_10_cmd = (scsi_read_10_cmd_t *)msc->cbw.CBWCB;
        msc->sector_start = __builtin_bswap32(read_10_cmd->logical_block_address);
        msc->sector_length = __builtin_bswap16(read_10_cmd->transfer_length);
        msc->sector_offset = 0;
        if (!msc->sector_length)
        {
            scsi_set_sense(msc, SCSI_SENSE_KEY_ILLEGAL_REQUEST, SCSI_ASC_INVALID_FIELD_IN_CDB, 0);
            failed = true;
        }
        else if (msc->cbw.dCBWDataTransferLength != (uint32_t)msc->sector_length * msc->sector_size)
        {
            scsi_set_sense(msc, SCSI_SENSE_KEY_ILLEGAL_REQUEST, SCSI_ASC_INVALID_FIELD_IN_CDB, 0);
            failed = true;
        }
        else if (msc->sector_start >= msc->sector_count || msc->sector_length > msc->sector_count - msc->sector_start)
        {
            scsi_set_sense(msc, SCSI_SENSE_KEY_ILLEGAL_REQUEST, SCSI_ASC_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE, 0);
            failed = true;
        }
        else if (!msc->read_sector)
        {
            scsi_set_sense(msc, SCSI_SENSE_KEY_HARDWARE_ERROR, SCSI_ASC_INTERNAL_TARGET_FAILURE, 0);
            failed = true;
        }
        else if (!msc->read_sector(msc, msc->sector_start))
        {
            scsi_set_sense(msc, SCSI_SENSE_KEY_MEDIUM_ERROR, SCSI_ASC_UNRECOVERED_READ_ERROR, 0);
            failed = true;
        }

        buf = msc->sector_buf;
        xfer_len = msc->sector_size;
        break;
    }
    case SCSI_OPCODE_WRITE_10:
    {
        scsi_write_10_cmd_t *write_10_cmd = (scsi_write_10_cmd_t *)msc->cbw.CBWCB;
        msc->sector_offset = 0;
        msc->sector_length = __builtin_bswap16(write_10_cmd->transfer_length);
        msc->sector_start = __builtin_bswap32(write_10_cmd->logical_block_address);
        if (!msc->sector_length)
        {
            scsi_set_sense(msc, SCSI_SENSE_KEY_ILLEGAL_REQUEST, SCSI_ASC_INVALID_FIELD_IN_CDB, 0);
            failed = true;
        }
        else if (msc->cbw.dCBWDataTransferLength != (uint32_t)msc->sector_length * msc->sector_size)
        {
            scsi_set_sense(msc, SCSI_SENSE_KEY_ILLEGAL_REQUEST, SCSI_ASC_INVALID_FIELD_IN_CDB, 0);
            failed = true;
        }
        else if (msc->sector_start >= msc->sector_count || msc->sector_length > msc->sector_count - msc->sector_start)
        {
            scsi_set_sense(msc, SCSI_SENSE_KEY_ILLEGAL_REQUEST, SCSI_ASC_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE, 0);
            failed = true;
        }
        else if (!msc->write_sector)
        {
            scsi_set_sense(msc, SCSI_SENSE_KEY_DATA_PROTECT, SCSI_ASC_WRITE_PROTECTED, 0);
            failed = true;
        }

        buf = msc->sector_buf;
        xfer_len = msc->sector_size;
        break;
    }
    default:
    {
        scsi_set_sense(msc, SCSI_SENSE_KEY_ILLEGAL_REQUEST, SCSI_ASC_INVALID_COMMAND_OPERATION_CODE, 0);
        failed = true;
        break;
    }
    }

    if (failed)
    {
        if (msc->cbw.dCBWDataTransferLength == 0)
        {
            bulk_only_send_csw(msc, MSC_BULK_ONLY_CSW_STATUS_FAILED);
        }
        else if (USB_GET_REQ_DIR(msc->cbw.bmCBWFlags))
        {
            msc->bulk_only_stage = BULK_ONLY_STAGE_STATUS;
            usbd_endp_stall(msc->usbd_handle, msc->in_ep->bEndpointAddress, true);
        }
        else
        {
            msc->bulk_only_stage = BULK_ONLY_STAGE_STATUS;
            usbd_endp_stall(msc->usbd_handle, msc->out_ep->bEndpointAddress, true);
        }
    }
    else if (msc->cbw.dCBWDataTransferLength)
    {
        msc->bulk_only_stage = BULK_ONLY_STAGE_DATA;
        if (USB_GET_REQ_DIR(msc->cbw.bmCBWFlags))
        {
            usbd_endp_write(msc->usbd_handle, msc->in_ep->bEndpointAddress, buf,
                            USB_MIN(msc->cbw.dCBWDataTransferLength, xfer_len));
        }
        else
        {
            usbd_endp_read(msc->usbd_handle, msc->out_ep->bEndpointAddress, buf,
                           USB_MIN(msc->cbw.dCBWDataTransferLength, xfer_len));
        }
    }
    else
    {
        bulk_only_send_csw(msc, status);
    }
}

static void bulk_only_data_handle(mscd_handle_t *msc, size_t len)
{
    msc->actual_xfer_len += len;
    if (msc->cbw.CBWCB[0] == SCSI_OPCODE_READ_10)
    {
        msc->sector_offset++;
        if (msc->sector_offset * msc->sector_size < msc->cbw.dCBWDataTransferLength)
        {
            if (msc->read_sector && msc->read_sector(msc, msc->sector_start + msc->sector_offset))
            {
                usbd_endp_write(msc->usbd_handle, msc->in_ep->bEndpointAddress, msc->sector_buf,
                                USB_MIN(msc->cbw.dCBWDataTransferLength, msc->sector_size));
            }
            else
            {
                msc->bulk_only_stage = BULK_ONLY_STAGE_STATUS;
                scsi_set_sense(msc, SCSI_SENSE_KEY_MEDIUM_ERROR, SCSI_ASC_UNRECOVERED_READ_ERROR, 0);
                usbd_endp_stall(msc->usbd_handle, msc->in_ep->bEndpointAddress, true);
            }
        }
        else
        {
            bulk_only_send_csw(msc, MSC_BULK_ONLY_CSW_STATUS_PASSED);
        }
    }
    else if (msc->cbw.CBWCB[0] == SCSI_OPCODE_WRITE_10)
    {
        if (msc->write_sector && msc->write_sector(msc, msc->sector_start + msc->sector_offset))
        {
            msc->sector_offset++;
            if (msc->sector_offset * msc->sector_size < msc->cbw.dCBWDataTransferLength)
            {
                usbd_endp_read(msc->usbd_handle, msc->out_ep->bEndpointAddress, msc->sector_buf,
                               USB_MIN(msc->cbw.dCBWDataTransferLength, msc->sector_size));
            }
            else
            {
                bulk_only_send_csw(msc, MSC_BULK_ONLY_CSW_STATUS_PASSED);
            }
        }
        else
        {
            msc->bulk_only_stage = BULK_ONLY_STAGE_STATUS;
            scsi_set_sense(msc, SCSI_SENSE_KEY_MEDIUM_ERROR, SCSI_ASC_WRITE_ERROR, 0);
            usbd_endp_stall(msc->usbd_handle, msc->out_ep->bEndpointAddress, true);
        }
    }
    else
    {
        bulk_only_send_csw(msc, MSC_BULK_ONLY_CSW_STATUS_PASSED);
    }
}

static void bulk_only_status_handle(mscd_handle_t *msc, size_t len)
{
    msc->bulk_only_stage = BULK_ONLY_STAGE_CMD;
    usbd_endp_read(msc->usbd_handle, msc->out_ep->bEndpointAddress, &msc->cbw, sizeof(msc->cbw));
}

static void transfer_callback(void *handle, usb_endp_t endp, void *buf, size_t len)
{
    mscd_handle_t *msc = (mscd_handle_t *)handle;

    switch (msc->bulk_only_stage)
    {
    case BULK_ONLY_STAGE_CMD:
        bulk_only_cmd_handle(msc, len);
        break;

    case BULK_ONLY_STAGE_DATA:
        bulk_only_data_handle(msc, len);
        break;

    case BULK_ONLY_STAGE_STATUS:
        bulk_only_status_handle(msc, len);
        break;
    }
}

static bool ctrl_xfer_setup(void *handle, const usb_setup_t *setup, void **buf, size_t *len)
{
    mscd_handle_t *msc = (mscd_handle_t *)handle;

    if (USB_GET_REQ_TYPE(setup->bmRequestType) == USB_REQ_TYPE_CLASS && USB_GET_REQ_DIR(setup->bmRequestType))
    {
        switch (setup->bRequest)
        {
        case MSC_REQ_GET_MAX_LUN:
            *buf = &msc->max_lun;
            *len = sizeof(msc->max_lun);
            return true;
        }
    }
    else if (USB_GET_REQ_TYPE(setup->bmRequestType) == USB_REQ_TYPE_CLASS && !USB_GET_REQ_DIR(setup->bmRequestType))
    {
        switch (setup->bRequest)
        {
        case MSC_REQ_BULK_ONLY_RESET:
            *buf = NULL;
            *len = 0;
            return true;
        }
    }
    return false;
}

static void ctrl_xfer_status(void *handle, const usb_setup_t *setup, void *buf, size_t len)
{
    mscd_handle_t *msc = (mscd_handle_t *)handle;

    if (USB_GET_REQ_TYPE(setup->bmRequestType) == USB_REQ_TYPE_CLASS && setup->bRequest == MSC_REQ_BULK_ONLY_RESET)
    {
        USB_LOGI("Handle %p: BULK_ONLY_RESET", msc);
        msc->bulk_only_stage = BULK_ONLY_STAGE_CMD;
        msc->actual_xfer_len = 0;
        memset(&msc->sense, 0, sizeof(msc->sense));
        if (msc->bulk_only_reset_cb)
        {
            msc->bulk_only_reset_cb(msc);
        }
        usbd_endp_read(msc->usbd_handle, msc->out_ep->bEndpointAddress, &msc->cbw, sizeof(msc->cbw));
    }
}

static void endp_event_callback(void *handle, usb_endp_t endp, usbd_endp_event_t event)
{
    mscd_handle_t *msc = (mscd_handle_t *)handle;

    if (event == USBD_ENDP_EVENT_CLEAR_STALL && msc->bulk_only_stage == BULK_ONLY_STAGE_STATUS)
    {
        bulk_only_send_csw(msc, MSC_BULK_ONLY_CSW_STATUS_FAILED);
    }
}

bool mscd_drv_open(mscd_handle_t *msc)
{
    if (!msc || !msc->usbd_handle || !msc->in_ep || !msc->out_ep || msc->subclass != MSC_SUBCLASS_SCSI ||
        msc->protocol != MSC_PROTOCOL_BULK_ONLY || msc->max_lun > 7 || !msc->sector_buf || !msc->sector_size ||
        !msc->sector_count)
    {
        return false;
    }

    usbd_ctrl_xfer_cbs_t cbs = {
        .setup = ctrl_xfer_setup,
        .data = NULL,
        .status = ctrl_xfer_status,
    };

    if (!usbd_register_itf_req_cb(msc->usbd_handle, msc, msc->itf_num, &cbs)) goto unregister_itf;
    if (!usbd_endp_open(msc->usbd_handle, msc, msc->in_ep, transfer_callback)) goto close_in_ep;
    if (!usbd_endp_open(msc->usbd_handle, msc, msc->out_ep, transfer_callback)) goto close_out_ep;
    if (!usbd_endp_event_register(msc->usbd_handle, msc->in_ep->bEndpointAddress, endp_event_callback) ||
        !usbd_endp_event_register(msc->usbd_handle, msc->out_ep->bEndpointAddress, endp_event_callback))
    {
        goto close_out_ep;
    }

    msc->bulk_only_stage = BULK_ONLY_STAGE_CMD;
    msc->sector_offset = 0;
    msc->sector_length = 0;
    msc->sector_start = 0;
    msc->actual_xfer_len = 0;

    usbd_endp_read(msc->usbd_handle, msc->out_ep->bEndpointAddress, &msc->cbw, sizeof(msc->cbw));

    USB_LOGI("Handle %p: Successfully opened MSC", msc);
    return true;

close_out_ep:
    usbd_endp_close(msc->usbd_handle, msc->out_ep->bEndpointAddress);

close_in_ep:
    usbd_endp_close(msc->usbd_handle, msc->in_ep->bEndpointAddress);

unregister_itf:
    usbd_unregister_itf_req_cb(msc->usbd_handle, msc->itf_num);
    USB_LOGE("Handle %p: Failed to open MSC", msc);
    return false;
}

#endif // USB_CLASS_MSCD_DRIVER_EN
