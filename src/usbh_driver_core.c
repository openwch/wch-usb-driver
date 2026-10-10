/**
 * @file usbh_driver_core.c
 * @author Links (lhd@wch.cn)
 * @brief USB host driver core source file
 *
 * @copyright Copyright (c) 2026
 *
 */

/* @define */
#define USB_LOG_TAG "USBH"

/* @include */
#include "usb_driver.h"
#include "host/usbh_driver_private.h"

#ifdef USB_HOST_DRIVER_EN

/* @define */
#define ENDP0_DEFAULT_MPS   8

#define ENUM_TASK_INTERVAL  100
#define CTRL_XFER_MAX_RETRY 500
#define DATA_XFER_MAX_RETRY 10000

/* @enum */
typedef enum
{
    ENUM_STAGE_RESET,
    ENUM_STAGE_GET_EP0_SIZE,
    ENUM_STAGE_SET_ADDR,
    ENUM_STAGE_GET_DEVICE_DESC,
    ENUM_STAGE_GET_CONFIG_DESC_SIZE,
    ENUM_STAGE_GET_CONFIG_DESC,
    ENUM_STAGE_SET_CONFIG,
    ENUM_STAGE_BIND_DRIVER,
    ENUM_STAGE_ENUM_FAILED,
    ENUM_STAGE_END,
} enum_stage_t;

/* @global */
static usbh_device_t device_pool[USBH_DEVICE_POOL_SIZE];
static usbh_endpoint_t endpoint_pool[USBH_ENDPOINT_POOL_SIZE];

static usbh_device_t *device_handle_alloc(void)
{
    for (size_t i = 0; i < USB_ARRAY_SIZE(device_pool); i++)
    {
        if (device_pool[i].address == 0)
        {
            return &device_pool[i];
        }
    }
    return NULL;
}

static void device_handle_free(usbh_device_t *dev)
{
    if (dev)
    {
        dev->address = 0;
    }
}

static uint8_t bus_address_alloc(usbh_handle_t *h)
{
    for (uint8_t i = 1; i < 128; i++)
    {
        uint8_t index = i >> 5;
        uint8_t bit = i & 0x1F;
        if ((h->bus_address_bitmap[index] & (1 << bit)) == 0)
        {
            h->bus_address_bitmap[index] |= (1 << bit);
            return i;
        }
    }
    return 0;
}

static void bus_address_free(usbh_handle_t *h, uint8_t addr)
{
    h->bus_address_bitmap[addr >> 5] &= ~(1 << (addr & 0x1F));
}

static usbh_endpoint_t *endpoint_handle_alloc(void)
{
    for (size_t i = 0; i < USB_ARRAY_SIZE(endpoint_pool); i++)
    {
        if (endpoint_pool[i].type == 0)
        {
            return &endpoint_pool[i];
        }
    }
    return NULL;
}

static void endpoint_handle_free(usbh_endpoint_t *ep)
{
    if (ep)
    {
        ep->type = 0;
    }
}

static uint16_t endpoint_interval_calc(usb_speed_t speed, uint8_t type, uint8_t interval)
{
}

static void get_split_hub_info(usbh_handle_t *h, usbh_device_t *dev, uint8_t *hub_addr, uint8_t *hub_port)
{
    uint8_t hshub_addr = dev->hub_addr;
    uint8_t hshub_port = dev->hub_port;
    usbh_device_t *curr_dev = h->device_list;

    while (curr_dev)
    {
        if (curr_dev->address != hshub_addr)
        {
            curr_dev = curr_dev->next;
            continue;
        }

        if (curr_dev->speed == USB_SPEED_HIGH)
        {
            break;
        }

        hshub_addr = curr_dev->hub_addr;
        hshub_port = curr_dev->hub_port;
        curr_dev = h->device_list;
    }
    *hub_addr = hshub_addr;
    *hub_port = hshub_port;
}

static void enum_ctrl_xfer_cb(usbh_device_t *dev, bool rst, const usb_setup_t *setup, void *buf, uint16_t length)
{
}

static void device_enum_task(usbh_handle_t *h, usbh_device_t *dev)
{
    usbh_port_status_t port_status;
    switch (dev->enum_stage)
    {
    case ENUM_STAGE_RESET:
        if (dev->hub_addr == 0)
        {
            h->root_get_status(h, &port_status);
        }
        else if (h->port_get_status)
        {
            h->port_get_status(h, &port_status);
        }
        else
        {
            USB_LOGW("Handle: %p enumeration on the stage %d failed, does not support hub port operations", h,
                     dev->enum_stage);
            dev->enum_stage = ENUM_STAGE_ENUM_FAILED;
            return;
        }

        /* Check if the device is connected, enabled, and not in reset state */
        if (port_status.status_bits.connect && port_status.status_bits.enable && port_status.status_bits.reset == 0)
        {
            dev->enum_stage = ENUM_STAGE_GET_EP0_SIZE;

            /* Determine the device speed based on the port status */
            if (port_status.status_bits.low_speed)
                dev->speed = USB_SPEED_LOW;
            else if (port_status.status_bits.high_speed)
                dev->speed = USB_SPEED_HIGH;
            else
                dev->speed = USB_SPEED_FULL;

            USB_LOGI("Handle: %p reset complete, device speed: %d", dev, dev->speed);

            /* Update the host speed if the device is connected directly to the root hub */
            if (dev->hub_addr == 0) h->speed = dev->speed;

            /* Set the default maximum packet size for endpoint 0 */
            dev->ep0_mps = ENDP0_DEFAULT_MPS;

            /* Initialize the control endpoint for the device */
            usbh_endpoint_t *ctrl_endp = &dev->ctrl_endp;
            memset(ctrl_endp, 0, sizeof(usbh_endpoint_t));
            ctrl_endp->ping_en = dev->speed == USB_SPEED_HIGH ? true : false;
            ctrl_endp->mps = ENDP0_DEFAULT_MPS;
            ctrl_endp->interval = endpoint_interval_calc(dev->speed, USB_ENDP_TYPE_CTRL, 0);
            ctrl_endp->handle = dev;
            ctrl_endp->xfer_cb = enum_ctrl_xfer_cb;
            usbh_list_append((void **)&h->endpoint_list[USB_ENDP_TYPE_CTRL], ctrl_endp);

            /* Initialize the transfer unit for the control endpoint */
            usbh_xfer_unit_t *xfer_unit = &ctrl_endp->xfer_unit;
            xfer_unit->endpoint = ctrl_endp;

            /* Configure split transaction or preamble if the device speed differs from the host speed */
            if (dev->speed != h->speed && h->speed == USB_SPEED_HIGH)
            {
                uint8_t hub_addr;
                uint8_t hub_port;
                get_split_hub_info(h, dev, &hub_addr, &hub_port);
                usbh_split_data_t *split = &xfer_unit->split_data;
                split->hub_addr = hub_addr;
                split->port = hub_port;
                split->s = dev->speed == USB_SPEED_LOW ? 1 : 0;
                split->et = USB_ENDP_TYPE_CTRL;
            }
            else if (dev->speed != h->speed && h->speed == USB_SPEED_FULL)
            {
                xfer_unit->pre = true;
            }

            usb_setup_t *setup = &h->enum_setup;
            setup->bmRequestType = USB_SET_REQ(USB_DIR_IN, USB_REQ_TYPE_STANDARD, USB_REQ_RCPT_DEVICE);
            setup->bRequest = USB_REQ_GET_DESCRIPTOR;
            setup->wValue = USB_DESC_DEVICE << 8;
            setup->wIndex = 0;
            setup->wLength = 8;
            usbh_ctrl_xfer(dev, setup, h->enum_desc_buf);
        }
        break;

    case ENUM_STAGE_ENUM_FAILED:
        usbh_device_remove(h, dev->hub_addr, dev->hub_port);
        break;
    }
}

static void xfer_completed_process(usbh_xfer_unit_t *xfer_unit)
{
    bool rst = false;
    usbh_endpoint_t *endp = xfer_unit->endpoint;
    usbh_xfer_ctx_t *xfer_ctx = &endp->xfer_ctx;
    static const uint8_t tog_to_pid[] = {USBH_PID_DATA0, USBH_PID_DATA1, USBH_PID_DATA2, USBH_PID_MDATA};

    if (endp->type == USB_ENDP_TYPE_ISOC || xfer_unit->rx_pid == USBH_PID_ACK || xfer_unit->rx_pid == USBH_PID_NYET ||
        xfer_unit->rx_pid == tog_to_pid[xfer_unit->toggle])
    {
        xfer_ctx->retry = 0;
        if (xfer_unit->token == USBH_PID_PING)
        {
            xfer_unit->token = USBH_PID_OUT;
            return;
        }

        if (xfer_unit->token == USBH_PID_NYET)
        {
            xfer_unit->token = USBH_PID_PING;
        }

        if (endp->type != USB_ENDP_TYPE_ISOC)
        {
            xfer_ctx->toggle ^= USBH_TOGGLE_DATA1;
        }

        xfer_ctx->offset += xfer_unit->xfer_len;

        if (endp->type != USB_ENDP_TYPE_CTRL)
        {
            if (xfer_ctx->offset >= xfer_ctx->length || xfer_unit->xfer_len < endp->mps)
            {
                rst = true;
                xfer_ctx->is_busy = false;
            }
        }
        else if (xfer_ctx->ctrl_stage == USB_CTRL_STAGE_SETUP)
        {
            xfer_ctx->offset = 0;
            xfer_ctx->toggle = USBH_TOGGLE_DATA1;
            xfer_ctx->ctrl_stage = xfer_ctx->length ? USB_CTRL_STAGE_DATA : USB_CTRL_STAGE_STATUS;
        }
        else if (xfer_ctx->ctrl_stage == USB_CTRL_STAGE_DATA)
        {
            if (xfer_ctx->offset >= xfer_ctx->length || xfer_unit->xfer_len < endp->mps)
            {
                xfer_ctx->toggle = USBH_TOGGLE_DATA1;
                xfer_ctx->ctrl_stage = USB_CTRL_STAGE_STATUS;
            }
        }
        else
        {
            rst = true;
            xfer_ctx->is_busy = false;
        }
    }
    else if (xfer_unit->rx_pid == USBH_PID_NAK)
    {
        if (endp->ping_en && xfer_unit->token == USBH_PID_OUT)
        {
            xfer_unit->token = USBH_PID_PING;
        }
    }
    else if (xfer_unit->rx_pid == USBH_PID_STALL)
    {
        xfer_ctx->is_busy = false;
        xfer_ctx->is_stalled = true;
    }
    else
    {
    }

    xfer_ctx->retry++;
    if (!rst)
    {
        uint32_t max_retry = endp->type == USB_ENDP_TYPE_CTRL ? CTRL_XFER_MAX_RETRY : DATA_XFER_MAX_RETRY;
        if (xfer_ctx->retry >= max_retry)
        {
            xfer_ctx->is_busy = false;
        }
    }

    if (!xfer_ctx->is_busy && endp->xfer_cb)
    {
        if (endp->type == USB_ENDP_TYPE_CTRL)
        {
            ((usbh_ctrl_xfer_cb)endp->xfer_cb)(endp->handle, rst, xfer_ctx->setup, xfer_ctx->buf, xfer_ctx->offset);
        }
        else
        {
            ((usbh_data_xfer_cb)endp->xfer_cb)(endp->handle, rst, endp->addr, xfer_ctx->buf, xfer_ctx->offset);
        }
    }
}

static usbh_xfer_unit_t **ctrl_xfer_process(usbh_handle_t *h, usbh_xfer_unit_t **last, usbh_endpoint_t *list)
{
    usbh_endpoint_t *endp = list;
    while (endp)
    {
        usbh_xfer_ctx_t *xfer_ctx = &endp->xfer_ctx;
        if (xfer_ctx->is_busy)
        {
            usbh_xfer_unit_t *xfer_unit = &endp->xfer_unit;
            switch (xfer_ctx->ctrl_stage)
            {
            case USB_CTRL_STAGE_SETUP:
                xfer_unit->token = USBH_PID_SETUP;
                xfer_unit->toggle = USBH_TOGGLE_DATA0;
                xfer_unit->xfer_len = sizeof(usb_setup_t);
                xfer_unit->buf = (void *)xfer_ctx->setup;
                break;

            case USB_CTRL_STAGE_DATA:
                xfer_unit->token = USB_ENDP_DIR(xfer_ctx->setup->bmRequestType) ? USBH_PID_IN : USBH_PID_OUT;
                xfer_unit->toggle = xfer_ctx->toggle;
                xfer_unit->xfer_len = USB_MIN(xfer_ctx->length - xfer_ctx->offset, endp->mps);
                xfer_unit->buf = (uint8_t *)xfer_ctx->buf + xfer_ctx->offset;
                break;

            case USB_CTRL_STAGE_STATUS:
                xfer_unit->token = USB_ENDP_DIR(xfer_ctx->setup->bmRequestType) ? USBH_PID_OUT : USBH_PID_IN;
                xfer_unit->toggle = USBH_TOGGLE_DATA1;
                xfer_unit->xfer_len = 0;
                xfer_unit->buf = NULL;
                break;
            }
            xfer_unit->next = NULL;
            *last = xfer_unit;
            last = &xfer_unit->next;
        }

        endp = endp->next;
    }
    return last;
}

static usbh_xfer_unit_t **isoc_xfer_process(usbh_handle_t *h, usbh_xfer_unit_t **last, usbh_endpoint_t *list)
{
}

static usbh_xfer_unit_t **bulk_xfer_process(usbh_handle_t *h, usbh_xfer_unit_t **last, usbh_endpoint_t *list)
{
}

static usbh_xfer_unit_t **intr_xfer_process(usbh_handle_t *h, usbh_xfer_unit_t **last, usbh_endpoint_t *list)
{
}

void usbh_drv_task(usbh_handle_t *h)
{
    /* Handle root port status change */
    if (h->root_port_change)
    {
        h->root_port_change = false;
        usbh_port_status_t port_status;
        h->root_get_status(h, &port_status);
        if (port_status.status_bits.connect)
        {
            usbh_device_insert(h, 0, 0);
        }
        else
        {
            usbh_device_remove(h, 0, 0);
        }
    }

    /* Handle device enumeration tasks */
    if (h->tick - h->enum_tick > ENUM_TASK_INTERVAL)
    {
        usbh_device_t *dev = h->device_list;
        while (dev)
        {
            /* Check if the device is in the enumeration process */
            if (dev->enum_stage < ENUM_STAGE_END)
            {
                device_enum_task(h, dev);
            }

            dev = dev->next;
        }
    }

    /* Handle transfer tasks */
    if (!h->xfer_busy)
    {
        /* Process transfer completed units */
        usbh_xfer_unit_t *xfer_unit = h->xfer_unit_list;
        while (xfer_unit)
        {
            xfer_completed_process(xfer_unit);
            xfer_unit = xfer_unit->next;
        }

        /* Rebuild the transfer unit list priority order: ISOC > INTR > CTRL > BULK*/
        h->xfer_unit_list = NULL;
        usbh_xfer_unit_t **last_xfer_unit = &h->xfer_unit_list;
        if (h->xfer_tick != h->tick)
        {
            h->xfer_tick = h->tick;
            last_xfer_unit = isoc_xfer_process(h, last_xfer_unit, h->endpoint_list[USB_ENDP_TYPE_ISOC]);
            last_xfer_unit = intr_xfer_process(h, last_xfer_unit, h->endpoint_list[USB_ENDP_TYPE_INTR]);
        }
        else
        {
            last_xfer_unit = ctrl_xfer_process(h, last_xfer_unit, h->endpoint_list[USB_ENDP_TYPE_CTRL]);
            last_xfer_unit = bulk_xfer_process(h, last_xfer_unit, h->endpoint_list[USB_ENDP_TYPE_BULK]);
        }

        if (h->xfer_unit_list)
        {
            h->xfer_busy = true;
            h->start_transfer(h);
        }
    }
}

bool usbh_drv_open(usbh_handle_t *h)
{
    if (h == NULL) return false;
    return h->open(h);
}

bool usbh_drv_close(usbh_handle_t *h)
{
    if (h == NULL) return false;
    return h->close(h);
}

void usbh_device_insert(usbh_handle_t *h, uint8_t hub_addr, uint8_t hub_port)
{
    /* Allocate a new device handle */
    usbh_device_t *dev = device_handle_alloc();
    if (dev == NULL) return;

    /* Initialize the device handle */
    memset(dev, 0, sizeof(usbh_device_t));

    /* Allocate a bus address for the new device */
    dev->address = bus_address_alloc(h);
    if (dev->address == 0) goto free_device;

    /* Set the hub address and port for the new device */
    dev->hub_addr = hub_addr;
    dev->hub_port = hub_port;

    /* Append the new device to the device list */
    usbh_list_append((void **)&h->device_list, dev);

    /* Reset device */
    usbh_device_reset(h, dev);
    USB_LOGI("Handle: %p inserted device %p at hub_addr: %d, hub_port: %d", h, dev, hub_addr, hub_port);
    return;

free_device:
    device_handle_free(dev);
}

void usbh_device_remove(usbh_handle_t *h, uint8_t hub_addr, uint8_t hub_port)
{
    usbh_device_t *dev = h->device_list;

    while (dev)
    {
        if (dev->hub_addr == hub_addr && dev->hub_port == hub_port)
        {
            USB_LOGI("Handle: %p removed device %p at hub_addr: %d, hub_port: %d", h, dev, hub_addr, hub_port);
            usbh_list_remove((void **)&h->device_list, dev);
            bus_address_free(h, dev->address);
            device_handle_free(dev);
            return;
        }
        dev = dev->next;
    }
}

void usbh_device_reset(usbh_handle_t *h, usbh_device_t *dev)
{
    if (h == NULL || dev == NULL || dev->address == 0) return;

    dev->enum_stage = ENUM_STAGE_RESET;
    if (dev->hub_addr == 0)
    {
        h->root_set_feature(h, USBH_PORT_FEATURE_RESET);
    }
    else if (h->port_set_feature)
    {
        h->port_set_feature(h, USBH_PORT_FEATURE_RESET);
    }
    else
    {
        USB_LOGW("Handle: %p does not support hub port operations", h);
    }
}

bool usbh_ctrl_xfer(usbh_device_t *dev, const usb_setup_t *setup, void *buf)
{
    usbh_xfer_ctx_t *xfer_ctx = &dev->ctrl_endp.xfer_ctx;
    if (!xfer_ctx->is_busy)
    {
        memset(xfer_ctx, 0, sizeof(usbh_xfer_ctx_t));
        xfer_ctx->is_busy = true;
        xfer_ctx->length = setup->wLength;
        xfer_ctx->buf = buf;
        xfer_ctx->setup = setup;
        return true;
    }
    return false;
}

bool usbh_endp_open(usbh_handle_t *h, const void *class, const usb_desc_endpoint_t *ep_desc, usbh_data_xfer_cb cb)
{
}

bool usbh_endp_close(usbh_handle_t *h, usb_endp_t endp)
{
}

bool usbh_endp_read(usbh_handle_t *h, usb_endp_t endp, void *buf, size_t len)
{
}

bool usbh_endp_write(usbh_handle_t *h, usb_endp_t endp, const void *buf, size_t len)
{
}

#endif // USB_HOST_DRIVER_EN
