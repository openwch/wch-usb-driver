/********************************** (C) COPYRIGHT *******************************
* File Name          : hubh.c
* Author             : WCH
* Version            : V1.0.0
* Date               : 2026/10/15
* Description        : USB HUB host class driver
*********************************************************************************
* Copyright (c) 2026 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

/* @define */
#define USB_LOG_TAG "HUBH"

/* @include */
#include "usb_driver.h"
#include "host/usbh_driver_private.h"

#ifdef USB_CLASS_HUBH_DRIVER_EN

/* @define */
#ifndef HUBH_HANDLE_POOL_SIZE
#define HUBH_HANDLE_POOL_SIZE 1
#endif

#define CTRL_XFER_TIMEOUT 100
#define HUBH_MAX_PORT_NUM 31

/* @enum */
typedef enum
{
    ENUM_STAGE_IDLE,
    ENUM_STAGE_GET_HUB_DESC,
    ENUM_STAGE_SET_INTERFACE,
    ENUM_STAGE_SET_PORT_POWER,
    ENUM_STAGE_WAIT_PORT_POWER,
    ENUM_STAGE_ENUM_FAILED,
    ENUM_STAGE_END,
} enum_stage_t;

/* @struct */
typedef struct hubh_handle
{
    struct hubh_handle *next;
    usbh_device_t *dev;

    /* Hub interface information */
    const usb_desc_interface_t *itf_desc;
    const usb_desc_endpoint_t *in_ep;
    uint16_t power_delayms;
    uint8_t port_num;

    /* Run-time context */
    uint8_t enum_stage;
    uint32_t start_wait_timestamp;
    uint8_t port_feature[HUBH_MAX_PORT_NUM];
    __attribute__((aligned(4))) hub_status_t hub_status;
    __attribute__((aligned(4))) hub_port_status_t port_status[HUBH_MAX_PORT_NUM];
    __attribute__((aligned(4))) uint32_t status_change_bitmap;
    __attribute__((aligned(4))) usb_setup_t ctrl_setup;
    __attribute__((aligned(4))) uint8_t ctrl_buf[16];
} hubh_handle_t;

/* @global */
static hubh_handle_t *hub_list = {0};
static hubh_handle_t handle_pool[HUBH_HANDLE_POOL_SIZE] = {0};

/* @function declarations */
static bool hub_drv_bind_cb(usbh_device_t *dev, const void *desc, uint16_t length);
static bool hub_drv_unbind_cb(usbh_device_t *dev, uint8_t itf_num);

static const usbh_drv_info_t hub_drv_info = {
    .vid = 0x0000,
    .pid = 0x0000,
    .dev_class = USB_CLASS_HUB,
    .dev_subclass = 0x00,
    .dev_protocol = 0x00,
    .itf_class = USB_CLASS_HUB,
    .itf_subclass = 0x00,
    .itf_protocol = 0x00,
    .match_bitmap = (USBH_DRV_MATCH_DEV_CLASS | USBH_DRV_MATCH_ITF_CLASS),
    .bind = hub_drv_bind_cb,
    .unbind = hub_drv_unbind_cb,
};

static uint8_t check_hub_desc(const usb_desc_hub_t *hub_desc)
{
    if (!hub_desc) return 1;
    if (hub_desc->bDescriptorType != HUB_DESC_HUB) return 2;
    if (hub_desc->bNbrPorts == 0 || hub_desc->bNbrPorts > HUBH_MAX_PORT_NUM) return 3;
    return 0;
}

static void enum_ctrl_xfer_cb(void *handle, bool rst, const usb_setup_t *setup, const void *buf, uint16_t length)
{
    hubh_handle_t *hub = (hubh_handle_t *)handle;
    if (!rst)
    {
        USB_LOGE("Handle: %p Enumeration control transfer failed at stage %d", hub, hub->enum_stage);
        hub->enum_stage = ENUM_STAGE_ENUM_FAILED;
        return;
    }

    switch (hub->enum_stage)
    {
    case ENUM_STAGE_GET_HUB_DESC:
    {
        usb_desc_hub_t *hub_desc = (usb_desc_hub_t *)hub->ctrl_buf;
        uint8_t check_rst = check_hub_desc(hub_desc);
        if (check_rst == 0)
        {
            hub->port_num = hub_desc->bNbrPorts;
            hub->power_delayms = hub_desc->bPwrOn2PwrGood * 2;
            USB_LOGI("Handle: %p Hub has %d ports, power delay %d ms", hub, hub->port_num, hub->power_delayms);
            if (hub->itf_desc->bAlternateSetting > 0)
            {
                hub->enum_stage = ENUM_STAGE_SET_INTERFACE;
                USB_LOGI("Handle: %p Setting alternate setting %d for interface %d", hub,
                         hub->itf_desc->bAlternateSetting, hub->itf_desc->bInterfaceNumber);

                usb_setup_t *request = &hub->ctrl_setup;
                request->bmRequestType = USB_SET_REQ(USB_DIR_OUT, USB_REQ_TYPE_STANDARD, USB_REQ_RCPT_INTERFACE);
                request->bRequest = USB_REQ_SET_INTERFACE;
                request->wValue = hub->itf_desc->bAlternateSetting;
                request->wIndex = hub->itf_desc->bInterfaceNumber;
                request->wLength = 0;
                usbh_ctrl_xfer(hub->dev, hub, request, (void *)hub->ctrl_buf, CTRL_XFER_TIMEOUT, enum_ctrl_xfer_cb);
            }
            else
            {
                hub->enum_stage = ENUM_STAGE_SET_PORT_POWER;
                USB_LOGI("Handle: %p Setting port 1 power", hub);

                usb_setup_t *request = &hub->ctrl_setup;
                request->bmRequestType = USB_SET_REQ(USB_DIR_OUT, USB_REQ_TYPE_CLASS, USB_REQ_RCPT_OTHER);
                request->bRequest = USB_REQ_SET_FEATURE;
                request->wValue = HUB_FEATURE_PORT_POWER;
                request->wIndex = 1;
                request->wLength = 0;
                usbh_ctrl_xfer(hub->dev, hub, request, (void *)hub->ctrl_buf, CTRL_XFER_TIMEOUT, enum_ctrl_xfer_cb);
            }
        }
        else
        {
            hub->enum_stage = ENUM_STAGE_ENUM_FAILED;
            USB_LOGE("Handle: %p Invalid hub descriptor error code: %d", hub, check_rst);
        }
        break;
    }
    case ENUM_STAGE_SET_INTERFACE:
    {
        hub->enum_stage = ENUM_STAGE_SET_PORT_POWER;
        USB_LOGI("Handle: %p Setting port 1 power", hub);

        usb_setup_t *request = &hub->ctrl_setup;
        request->bmRequestType = USB_SET_REQ(USB_DIR_OUT, USB_REQ_TYPE_CLASS, USB_REQ_RCPT_OTHER);
        request->bRequest = USB_REQ_SET_FEATURE;
        request->wValue = HUB_FEATURE_PORT_POWER;
        request->wIndex = 1;
        request->wLength = 0;
        usbh_ctrl_xfer(hub->dev, hub, request, (void *)hub->ctrl_buf, CTRL_XFER_TIMEOUT, enum_ctrl_xfer_cb);
        break;
    }
    case ENUM_STAGE_SET_PORT_POWER:
    {
        if (setup->wIndex < hub->port_num)
        {
            hub->enum_stage = ENUM_STAGE_SET_PORT_POWER;
            USB_LOGI("Handle: %p Setting port %d power", hub, setup->wIndex + 1);

            usb_setup_t *request = &hub->ctrl_setup;
            request->bmRequestType = USB_SET_REQ(USB_DIR_OUT, USB_REQ_TYPE_CLASS, USB_REQ_RCPT_OTHER);
            request->bRequest = USB_REQ_SET_FEATURE;
            request->wValue = HUB_FEATURE_PORT_POWER;
            request->wIndex = setup->wIndex + 1;
            request->wLength = 0;
            usbh_ctrl_xfer(hub->dev, hub, request, (void *)hub->ctrl_buf, CTRL_XFER_TIMEOUT, enum_ctrl_xfer_cb);
        }
        else
        {
            hub->enum_stage = ENUM_STAGE_WAIT_PORT_POWER;
            hub->start_wait_timestamp = usbh_drv_get_timestamp(hub->dev->host);
        }
        break;
    }
    }
}

static void hub_enum_task(hubh_handle_t *hub)
{
    switch (hub->enum_stage)
    {
    case ENUM_STAGE_IDLE:
    {
        usb_setup_t *request = &hub->ctrl_setup;
        request->bmRequestType = USB_SET_REQ(USB_DIR_IN, USB_REQ_TYPE_CLASS, USB_REQ_RCPT_DEVICE);
        request->bRequest = USB_REQ_GET_DESCRIPTOR;
        request->wValue = HUB_DESC_HUB << 8;
        request->wIndex = 0;
        request->wLength = sizeof(hub->ctrl_buf);
        if (usbh_ctrl_xfer(hub->dev, hub, request, hub->ctrl_buf, CTRL_XFER_TIMEOUT, enum_ctrl_xfer_cb))
        {
            hub->enum_stage = ENUM_STAGE_GET_HUB_DESC;
        }
        break;
    }

    case ENUM_STAGE_WAIT_PORT_POWER:
    {
        uint32_t timestamp = usbh_drv_get_timestamp(hub->dev->host);
        if (timestamp - hub->start_wait_timestamp >= hub->power_delayms)
        {
            if (usbh_endp_open(hub->dev, hub, hub->in_ep, NULL))
            {
                hub->enum_stage = ENUM_STAGE_END;
                hub->status_change_bitmap = 0xFFFFFFFF;
                USB_LOGI("Handle: %p Enumeration completed", hub);
            }
            else
            {
                hub->enum_stage = ENUM_STAGE_ENUM_FAILED;
                USB_LOGE("Handle: %p Open endpoint failed", hub);
            }
        }
        break;
    }

    case ENUM_STAGE_ENUM_FAILED:
        hub->enum_stage = ENUM_STAGE_END;
        hub_drv_unbind_cb(hub->dev, hub->itf_desc->bInterfaceNumber);
        break;
    }
}

static void hub_wakeup_cb(usbh_device_t *dev)
{
    hubh_handle_t *hub = hub_list;
    while (hub)
    {
        if (hub->dev == dev)
        {
            hub->status_change_bitmap = 0xFFFFFFFF;
            return;
        }
        hub = hub->next;
    }
}

static void hub_run_get_status(hubh_handle_t *hub, uint8_t index)
{
    if (index == 0)
    {
        usb_setup_t *request = &hub->ctrl_setup;
        request->bmRequestType = USB_SET_REQ(USB_DIR_IN, USB_REQ_TYPE_CLASS, USB_REQ_RCPT_DEVICE);
        request->bRequest = USB_REQ_GET_STATUS;
        request->wValue = 0;
        request->wIndex = 0;
        request->wLength = sizeof(hub->hub_status);
        usbh_ctrl_xfer(hub->dev, hub, request, (void *)&hub->hub_status, CTRL_XFER_TIMEOUT, NULL);
    }
    else
    {
        usb_setup_t *request = &hub->ctrl_setup;
        request->bmRequestType = USB_SET_REQ(USB_DIR_IN, USB_REQ_TYPE_CLASS, USB_REQ_RCPT_OTHER);
        request->bRequest = USB_REQ_GET_STATUS;
        request->wValue = 0;
        request->wIndex = index;
        request->wLength = sizeof(hub->port_status[index - 1]);
        usbh_ctrl_xfer(hub->dev, hub, request, (void *)&hub->port_status[index - 1], CTRL_XFER_TIMEOUT, NULL);
    }
}

static void hub_run_set_feature(hubh_handle_t *hub, uint8_t index)
{
    if (index)
    {
        usb_setup_t *request = &hub->ctrl_setup;
        request->bmRequestType = USB_SET_REQ(USB_DIR_OUT, USB_REQ_TYPE_CLASS, USB_REQ_RCPT_OTHER);
        request->bRequest = USB_REQ_SET_FEATURE;
        request->wIndex = index;
        request->wLength = 0;
        request->wValue = hub->port_feature[index - 1];
        usbh_ctrl_xfer(hub->dev, hub, request, NULL, CTRL_XFER_TIMEOUT, NULL);
        hub->port_feature[index - 1] = 0;
        hub->status_change_bitmap |= 1 << index;
    }
}

static void hub_run_clear_feature(hubh_handle_t *hub, uint8_t index)
{
    if (index == 0)
    {
        usb_setup_t *request = &hub->ctrl_setup;
        request->bmRequestType = USB_SET_REQ(USB_DIR_OUT, USB_REQ_TYPE_CLASS, USB_REQ_RCPT_DEVICE);
        request->bRequest = USB_REQ_CLEAR_FEATURE;
        request->wIndex = 0;
        request->wLength = 0;

        hub_status_t *hub_status = &hub->hub_status;
        if (hub_status->change_bits.local_power)
            request->wValue = HUB_FEATURE_C_HUB_LOCAL_POWER;
        else if (hub_status->change_bits.over_current)
            request->wValue = HUB_FEATURE_C_HUB_OVER_CURRENT;
        else
            return;

        hub->status_change_bitmap |= 0x01;
        usbh_ctrl_xfer(hub->dev, hub, request, NULL, CTRL_XFER_TIMEOUT, NULL);
    }
    else
    {
        hub_port_status_t *port_status = &hub->port_status[index - 1];
        usb_setup_t *request = &hub->ctrl_setup;
        request->bmRequestType = USB_SET_REQ(USB_DIR_OUT, USB_REQ_TYPE_CLASS, USB_REQ_RCPT_OTHER);
        request->bRequest = USB_REQ_CLEAR_FEATURE;
        request->wIndex = index;
        request->wLength = 0;

        if (port_status->change_bits.connect)
            request->wValue = HUB_FEATURE_C_PORT_CONNECTION;
        else if (port_status->change_bits.enable)
            request->wValue = HUB_FEATURE_C_PORT_ENABLE;
        else if (port_status->change_bits.suspend)
            request->wValue = HUB_FEATURE_C_PORT_SUSPEND;
        else if (port_status->change_bits.over_current)
            request->wValue = HUB_FEATURE_C_PORT_OVER_CURRENT;
        else if (port_status->change_bits.reset)
            request->wValue = HUB_FEATURE_C_PORT_RESET;
        else
            return;

        hub->status_change_bitmap |= 1 << index;
        usbh_ctrl_xfer(hub->dev, hub, request, NULL, CTRL_XFER_TIMEOUT, NULL);
    }
}

static void hub_run_task(hubh_handle_t *hub)
{
    bool has_device = false;
    hub->status_change_bitmap &= (1 << (hub->port_num + 1)) - 1;
    for (size_t i = 0; i <= hub->port_num; i++)
    {
        if (hub->status_change_bitmap & (1 << i))
        {
            hub->status_change_bitmap &= ~(1 << i);
            hub_run_get_status(hub, i);
            return;
        }

        if (i == 0)
        {
            if (hub->hub_status.change)
            {
                hub_run_clear_feature(hub, 0);
                return;
            }
        }
        else if (hub->port_status[i - 1].change)
        {
            hub_port_status_t *port_status = &hub->port_status[i - 1];
            if (port_status->change_bits.connect && port_status->status_bits.connect &&
                port_status->status_bits.enable == 0)
            {
                usbh_device_insert(hub->dev->host, hub->dev->address, i);
            }
            if ((port_status->change_bits.connect && port_status->status_bits.connect == 0) ||
                (port_status->change_bits.enable && port_status->status_bits.enable == 0))
            {
                usbh_device_remove(hub->dev->host, hub->dev->address, i);
            }
            if (port_status->change_bits.suspend && port_status->status_bits.suspend == 0)
            {
                usbh_device_resume(hub->dev->host, hub->dev->address, i);
            }

            hub_run_clear_feature(hub, i);
            return;
        }
        else if (i != 0 && hub->port_feature[i - 1])
        {
            hub_run_set_feature(hub, i);
            return;
        }
        else if (hub->port_status[i - 1].status_bits.connect && hub->port_status[i - 1].status_bits.suspend == 0)
        {
            has_device = true;
        }
    }

    if (has_device)
    {
        /* Read the status change bitmap from the hub's interrupt endpoint */
        usbh_endp_read(hub->dev, hub->in_ep->bEndpointAddress, &hub->status_change_bitmap,
                       sizeof(hub->status_change_bitmap), 0);
    }
    else
    {
        /* Suspend the HUB if no active ports have connected HUB */
        usbh_device_suspend(hub->dev, hub_wakeup_cb);
    }
}

void hubh_drv_task(void)
{
    hubh_handle_t *hub = hub_list;
    while (hub)
    {
        hubh_handle_t *next = hub->next;
        if (hub->enum_stage < ENUM_STAGE_END)
        {
            hub_enum_task(hub);
        }
        else if (!hub->dev->suspended && !usbh_is_ctrl_xfering(hub->dev))
        {
            hub_run_task(hub);
        }
        hub = next;
    }
}

static hubh_handle_t *hub_find_by_dev(usbh_device_t *dev)
{
    hubh_handle_t *hub = hub_list;
    while (hub)
    {
        if (hub->dev->address == dev->hub_addr)
        {
            return hub;
        }
        hub = hub->next;
    }
    return NULL;
}

static void hub_get_status(usbh_device_t *dev, usbh_port_status_t *status)
{
    hubh_handle_t *hub = hub_find_by_dev(dev);
    if (hub)
    {
        status->status = hub->port_status[dev->hub_port - 1].status;
    }
}

static void hub_set_feature(usbh_device_t *dev, usbh_port_feature_t feature)
{
    hubh_handle_t *hub = hub_find_by_dev(dev);
    if (hub)
    {
        hub->port_feature[dev->hub_port - 1] = feature;
    }
}

bool hubh_drv_open(usbh_handle_t *h)
{
    if (!h) return false;
    if (!usbh_register_driver(h, &hub_drv_info)) return false;
    h->port_get_status = hub_get_status;
    h->port_set_feature = hub_set_feature;
    return true;
}

bool hubh_drv_close(usbh_handle_t *h)
{
    if (!h) return false;
    if (!usbh_unregister_driver(h, &hub_drv_info)) return false;
    h->port_get_status = NULL;
    h->port_set_feature = NULL;
    return true;
}

static hubh_handle_t *handle_alloc(void)
{
    for (size_t i = 0; i < USB_ARRAY_SIZE(handle_pool); i++)
    {
        hubh_handle_t *hub = &handle_pool[i];
        if (hub->dev == NULL)
        {
            memset(hub, 0, sizeof(hubh_handle_t));
            return hub;
        }
    }
    return NULL;
}

static bool hub_drv_bind_cb(usbh_device_t *dev, const void *desc, uint16_t length)
{
    if (!dev || dev->speed == USB_SPEED_LOW || !desc || length == 0) return false;

    const usb_desc_interface_t *itf_desc = NULL;
    const usb_desc_endpoint_t *in_ep = NULL;

    uint8_t endp_count = 0;
    const uint8_t *curr = (const uint8_t *)desc;
    const uint8_t *end = curr + length;

    while (curr < end)
    {
        uint8_t len = curr[0];
        uint8_t type = curr[1];

        switch (type)
        {
        case USB_DESC_INTERFACE:
        {
            const usb_desc_interface_t *curr_itf = (const usb_desc_interface_t *)curr;
            /* Check if the interface protocol and number of endpoints are supported */
            if (curr_itf->bInterfaceProtocol > HUB_PROTOCOL_HIGH_SPEED_MTT || curr_itf->bNumEndpoints > 1) return false;

            /* Update the hub protocol if protocol is higher */
            if (curr_itf->bAlternateSetting == 0 || itf_desc == NULL ||
                curr_itf->bInterfaceProtocol > itf_desc->bInterfaceProtocol)
            {
                itf_desc = curr_itf;
                endp_count = curr_itf->bNumEndpoints;
            }
            break;
        }

        case USB_DESC_ENDPOINT:
        {
            const usb_desc_endpoint_t *endp_desc = (const usb_desc_endpoint_t *)curr;
            uint8_t endp_dir = USB_ENDP_DIR(endp_desc->bEndpointAddress);
            uint8_t endp_type = USB_ENDP_GET_TYPE(endp_desc->bmAttributes);
            if (!endp_dir || endp_type != USB_ENDP_TYPE_INTR) return false;
            if (!endp_count > 0) break;
            endp_count--;
            in_ep = endp_desc;
            break;
        }
        }

        curr += len;
    }

    if (!itf_desc || !in_ep) return false;

    /* Allocate and initialize a new hub handle */
    hubh_handle_t *hub = handle_alloc();
    if (!hub)
    {
        USB_LOGE("Handle %p: Failed to allocate hub handle", dev);
        return false;
    }

    hub->dev = dev;
    hub->itf_desc = itf_desc;
    hub->in_ep = in_ep;
    usbh_list_append((void **)&hub_list, hub);
    return true;
}

static bool hub_drv_unbind_cb(usbh_device_t *dev, uint8_t itf_num)
{
    hubh_handle_t *hub = hub_list;
    while (hub)
    {
        hubh_handle_t *next = hub->next;
        if (hub->dev == dev && (itf_num == USBH_DRV_ALL_INTERFACE || hub->itf_desc->bInterfaceNumber == itf_num))
        {
            usbh_list_remove((void **)&hub_list, hub);
            memset(hub, 0, sizeof(hubh_handle_t));
        }
        hub = next;
    }
    return true;
}

#endif // USB_CLASS_HUBH_DRIVER_EN
