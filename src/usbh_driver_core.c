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
    ENUM_STAGE_GET_STRING_DESC,
    ENUM_STAGE_GET_CONFIG_DESC_SIZE,
    ENUM_STAGE_GET_CONFIG_DESC,
    ENUM_STAGE_BIND_DRIVER,
    ENUM_STAGE_SET_CONFIG,
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
        if (!endpoint_pool[i].is_used)
        {
            endpoint_pool[i].is_used = true;
            return &endpoint_pool[i];
        }
    }
    return NULL;
}

static void endpoint_handle_free(usbh_endpoint_t *ep)
{
    if (ep)
    {
        ep->is_used = false;
    }
}

static uint16_t endpoint_interval_calc(uint8_t host_speed, uint8_t dev_speed, uint8_t type, uint8_t interval)
{
    if (type == USB_ENDP_TYPE_ISOC || type == USB_ENDP_TYPE_INTR)
    {
        if (host_speed == USB_SPEED_HIGH)
        {
            if (dev_speed == USB_SPEED_HIGH)
            {
                return 1 << (interval - 1);
            }
            else if (type == USB_ENDP_TYPE_ISOC)
            {
                return (1 << (interval - 1)) << 3;
            }
            else
            {
                return interval << 3;
            }
        }
        else if (type == USB_ENDP_TYPE_ISOC)
        {
            return 1 << (interval - 1);
        }
        else
        {
            return interval;
        }
    }
    return 0;
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

static uint8_t check_device_desc(const usb_desc_device_t *dev_desc)
{
    return 0;
}

static void print_device_desc(usbh_device_t *dev, const usb_desc_device_t *dev_desc)
{
    USB_LOGI("=== Handle: %p Device Descriptor ===", dev);
    USB_LOGI("bLength: %d", dev_desc->bLength);
    USB_LOGI("bDescriptorType: %d", dev_desc->bDescriptorType);
    USB_LOGI("bcdUSB: %04x", dev_desc->bcdUSB);
    USB_LOGI("bDeviceClass: %d", dev_desc->bDeviceClass);
    USB_LOGI("bDeviceSubClass: %d", dev_desc->bDeviceSubClass);
    USB_LOGI("bDeviceProtocol: %d", dev_desc->bDeviceProtocol);
    USB_LOGI("bMaxPacketSize0: %d", dev_desc->bMaxPacketSize0);
    USB_LOGI("idVendor: %04x", dev_desc->idVendor);
    USB_LOGI("idProduct: %04x", dev_desc->idProduct);
    USB_LOGI("bcdDevice: %04x", dev_desc->bcdDevice);
    USB_LOGI("iManufacturer: %d", dev_desc->iManufacturer);
    USB_LOGI("iProduct: %d", dev_desc->iProduct);
    USB_LOGI("iSerialNumber: %d", dev_desc->iSerialNumber);
    USB_LOGI("bNumConfigurations: %d", dev_desc->bNumConfigurations);
    USB_LOGI("============================================");
}

static uint8_t check_config_desc(const void *config_desc, uint16_t length)
{
    return 0;
}

static void print_config_desc(usbh_device_t *dev, const usb_desc_config_t *config_desc)
{
    USB_LOGI("=== Handle: %p Configuration Descriptor ===", dev);
    USB_LOGI("bLength: %d", config_desc->bLength);
    USB_LOGI("bDescriptorType: %d", config_desc->bDescriptorType);
    USB_LOGI("wTotalLength: %d", config_desc->wTotalLength);
    USB_LOGI("bNumInterfaces: %d", config_desc->bNumInterfaces);
    USB_LOGI("bConfigurationValue: %d", config_desc->bConfigurationValue);
    USB_LOGI("iConfiguration: %d", config_desc->iConfiguration);
    USB_LOGI("bmAttributes: %02x", config_desc->bmAttributes);
    USB_LOGI("bMaxPower: %d", config_desc->bMaxPower);
    USB_LOGI("===================================================");
}

static void enum_ctrl_xfer_cb(void *handle, bool rst, const usb_setup_t *setup, const void *buf, uint16_t length)
{
    usbh_device_t *dev = (usbh_device_t *)handle;

    if (!rst)
    {
        USB_LOGE("Handle: %p Enumeration control transfer failed at stage %d", dev, dev->enum_stage);
        dev->enum_stage = ENUM_STAGE_ENUM_FAILED;
        return;
    }

    switch (dev->enum_stage)
    {
    case ENUM_STAGE_GET_EP0_SIZE:
    {
        uint8_t ep0_mps = ((usb_desc_device_t *)buf)->bMaxPacketSize0;
        if (ep0_mps >= 8 && ep0_mps <= 64)
        {
            dev->enum_stage = ENUM_STAGE_SET_ADDR;
            dev->ctrl_endp.mps = ep0_mps;
            USB_LOGI("Handle: %p EP0 max packet size is %d", dev, ep0_mps);

            usb_setup_t *request = (usb_setup_t *)setup;
            request->bmRequestType = USB_SET_REQ(USB_DIR_OUT, USB_REQ_TYPE_STANDARD, USB_REQ_RCPT_DEVICE);
            request->bRequest = USB_REQ_SET_ADDRESS;
            request->wValue = dev->address;
            request->wIndex = 0;
            request->wLength = 0;
            usbh_ctrl_xfer(dev, dev, request, (void *)buf, enum_ctrl_xfer_cb);
        }
        else
        {
            dev->enum_stage = ENUM_STAGE_ENUM_FAILED;
            USB_LOGE("Handle: %p Invalid EP0 max packet size: %d", dev, ep0_mps);
        }
        break;
    }

    case ENUM_STAGE_SET_ADDR:
    {
        dev->enum_stage = ENUM_STAGE_GET_DEVICE_DESC;
        dev->ctrl_endp.xfer_unit.dev_addr = dev->address;
        USB_LOGI("Handle: %p Set bus address %d", dev, dev->address);

        usb_setup_t *request = (usb_setup_t *)setup;
        request->bmRequestType = USB_SET_REQ(USB_DIR_IN, USB_REQ_TYPE_STANDARD, USB_REQ_RCPT_DEVICE);
        request->bRequest = USB_REQ_GET_DESCRIPTOR;
        request->wValue = USB_DESC_DEVICE << 8;
        request->wIndex = 0;
        request->wLength = sizeof(usb_desc_device_t);
        usbh_ctrl_xfer(dev, dev, request, (void *)buf, enum_ctrl_xfer_cb);
        break;
    }

    case ENUM_STAGE_GET_DEVICE_DESC:
    {
        uint8_t check_rst = check_device_desc((usb_desc_device_t *)buf);
        if (check_rst == 0)
        {
            dev->enum_stage = ENUM_STAGE_GET_STRING_DESC;
            memcpy(&dev->dev_desc, buf, sizeof(usb_desc_device_t));
            print_device_desc(dev, &dev->dev_desc);

            usb_setup_t *request = (usb_setup_t *)setup;
            request->bmRequestType = USB_SET_REQ(USB_DIR_IN, USB_REQ_TYPE_STANDARD, USB_REQ_RCPT_DEVICE);
            request->bRequest = USB_REQ_GET_DESCRIPTOR;
            request->wValue = USB_DESC_STRING << 8;
            request->wIndex = 0;
            request->wLength = 4;
            usbh_ctrl_xfer(dev, dev, request, (void *)buf, enum_ctrl_xfer_cb);
        }
        else
        {
            dev->enum_stage = ENUM_STAGE_ENUM_FAILED;
            USB_LOGE("Handle: %p Invalid device descriptor error code: %d", dev, check_rst);
        }
        break;
    }

    case ENUM_STAGE_GET_STRING_DESC:
    {
        /* Check if the string descriptor is valid */
        if (((uint8_t *)buf)[0] == 4 && ((uint8_t *)buf)[1] == USB_DESC_STRING)
        {
            dev->enum_stage = ENUM_STAGE_GET_CONFIG_DESC_SIZE;
            memcpy(&dev->language_id, buf + 2, sizeof(uint16_t));
            USB_LOGI("Handle: %p Language ID is 0x%04X", dev, dev->language_id);

            usb_setup_t *request = (usb_setup_t *)setup;
            request->bmRequestType = USB_SET_REQ(USB_DIR_IN, USB_REQ_TYPE_STANDARD, USB_REQ_RCPT_DEVICE);
            request->bRequest = USB_REQ_GET_DESCRIPTOR;
            request->wValue = USB_DESC_CONFIGURATION << 8;
            request->wIndex = 0;
            request->wLength = 4;
            usbh_ctrl_xfer(dev, dev, request, (void *)buf, enum_ctrl_xfer_cb);
        }
        else
        {
            dev->enum_stage = ENUM_STAGE_ENUM_FAILED;
            USB_LOGE("Handle: %p Invalid language string descriptor", dev);
        }
        break;
    }

    case ENUM_STAGE_GET_CONFIG_DESC_SIZE:
    {
        usb_desc_config_t *config_desc = (usb_desc_config_t *)buf;

        /* Check if the configuration descriptor fits within the buffer */
        if (config_desc->wTotalLength <= USBH_DESC_BUF_SIZE)
        {
            dev->enum_stage = ENUM_STAGE_GET_CONFIG_DESC;
            USB_LOGI("Handle: %p Configuration descriptor size is %d", dev, config_desc->wTotalLength);

            /* Keep the configuration index requested by the driver-binding stage */
            usb_setup_t *request = (usb_setup_t *)setup;
            request->bmRequestType = USB_SET_REQ(USB_DIR_IN, USB_REQ_TYPE_STANDARD, USB_REQ_RCPT_DEVICE);
            request->bRequest = USB_REQ_GET_DESCRIPTOR;
            request->wIndex = 0;
            request->wLength = config_desc->wTotalLength;
            usbh_ctrl_xfer(dev, dev, request, (void *)buf, enum_ctrl_xfer_cb);
        }
        else
        {
            dev->enum_stage = ENUM_STAGE_ENUM_FAILED;
            USB_LOGE("Handle: %p Configuration descriptor size %d exceeds buffer size %d", dev,
                     config_desc->wTotalLength, USBH_DESC_BUF_SIZE);
        }
        break;
    }

    case ENUM_STAGE_GET_CONFIG_DESC:
    {
        usb_desc_config_t *config_desc = (usb_desc_config_t *)buf;

        /* Check if the configuration descriptor size matches the expected length */
        if (config_desc->wTotalLength != length)
        {
            dev->enum_stage = ENUM_STAGE_ENUM_FAILED;
            USB_LOGE("Handle: %p Configuration descriptor size mismatch: expected %d, got %d", dev,
                     config_desc->wTotalLength, length);
            break;
        }

        /* Validate the configuration descriptor */
        uint8_t check_rst = check_config_desc(buf, length);
        if (check_rst == 0)
        {
            dev->enum_stage = ENUM_STAGE_BIND_DRIVER;
            print_config_desc(dev, (const usb_desc_config_t *)buf);
        }
        else
        {
            dev->enum_stage = ENUM_STAGE_ENUM_FAILED;
            USB_LOGE("Handle: %p Invalid configuration descriptor error code: %d", dev, check_rst);
        }
        break;
    }

    case ENUM_STAGE_SET_CONFIG:
    {
        dev->enum_stage = ENUM_STAGE_END;
        USB_LOGI("Handle: %p Enumeration complete, set configuration %d", dev, setup->wValue);
        break;
    }
    }
}

static bool bind_class_driver(usbh_handle_t *h, usbh_device_t *dev)
{
    return true;
}

static void unbind_class_driver(usbh_handle_t *h, usbh_device_t *dev)
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
            USB_LOGW("Handle: %p Enumeration on the stage %d failed, does not support hub port operations", h,
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

            USB_LOGI("Handle: %p Reset complete link speed: %s speed", dev,
                     dev->speed == USB_SPEED_LOW ? "low" : (dev->speed == USB_SPEED_HIGH ? "high" : "full"));

            /* Update the host speed if the device is connected directly to the root hub */
            if (dev->hub_addr == 0) h->speed = dev->speed;

            /* Initialize the control endpoint for the device */
            usbh_endpoint_t *ctrl_endp = &dev->ctrl_endp;
            memset(ctrl_endp, 0, sizeof(usbh_endpoint_t));
            ctrl_endp->ping_en = dev->speed == USB_SPEED_HIGH ? true : false;
            ctrl_endp->mps = ENDP0_DEFAULT_MPS;
            ctrl_endp->interval = endpoint_interval_calc(h->speed, dev->speed, USB_ENDP_TYPE_CTRL, 0);
            ctrl_endp->handle = dev;
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

            usb_setup_t *request = &h->enum_setup;
            request->bmRequestType = USB_SET_REQ(USB_DIR_IN, USB_REQ_TYPE_STANDARD, USB_REQ_RCPT_DEVICE);
            request->bRequest = USB_REQ_GET_DESCRIPTOR;
            request->wValue = USB_DESC_DEVICE << 8;
            request->wIndex = 0;
            request->wLength = 8;
            usbh_ctrl_xfer(dev, dev, request, h->enum_desc_buf, enum_ctrl_xfer_cb);
        }
        break;

    case ENUM_STAGE_BIND_DRIVER:
        if (bind_class_driver(h, dev))
        {
            dev->enum_stage = ENUM_STAGE_SET_CONFIG;
            usb_desc_config_t *config_desc = (usb_desc_config_t *)h->enum_desc_buf;
            USB_LOGI("Handle: %p Binding driver for configuration %d", dev, config_desc->bConfigurationValue);

            usb_setup_t *request = &h->enum_setup;
            request->bmRequestType = USB_SET_REQ(USB_DIR_OUT, USB_REQ_TYPE_STANDARD, USB_REQ_RCPT_DEVICE);
            request->bRequest = USB_REQ_SET_CONFIGURATION;
            request->wValue = config_desc->bConfigurationValue;
            request->wIndex = 0;
            request->wLength = 0;
            usbh_ctrl_xfer(dev, dev, request, h->enum_desc_buf, enum_ctrl_xfer_cb);
        }
        else
        {
            /* Try the next configuration if the current one is not supported */
            uint8_t config_index = USB_U16_LOW(h->enum_setup.wValue);
            if (config_index + 1 < dev->dev_desc.bNumConfigurations)
            {
                dev->enum_stage = ENUM_STAGE_GET_CONFIG_DESC_SIZE;
                USB_LOGW("Handle: %p configuration %d not supported, trying next", dev, config_index);

                usb_setup_t *request = &h->enum_setup;
                request->bmRequestType = USB_SET_REQ(USB_DIR_IN, USB_REQ_TYPE_STANDARD, USB_REQ_RCPT_DEVICE);
                request->bRequest = USB_REQ_GET_DESCRIPTOR;
                request->wValue = (USB_DESC_CONFIGURATION << 8) | (config_index + 1);
                request->wIndex = 0;
                request->wLength = 4;
                usbh_ctrl_xfer(dev, dev, request, h->enum_desc_buf, enum_ctrl_xfer_cb);
            }
            else
            {
                dev->enum_stage = ENUM_STAGE_ENUM_FAILED;
                USB_LOGE("Handle: %p Bind driver failed", dev);
            }
        }

        break;

    case ENUM_STAGE_ENUM_FAILED:
        // TODO: Suspend the device
        dev->enum_stage = ENUM_STAGE_END;
        break;
    }
}

static void xfer_completed_process(usbh_xfer_unit_t *xfer_unit)
{
    bool rst = false;
    usbh_endpoint_t *endp = xfer_unit->endpoint;
    usbh_xfer_ctx_t *xfer_ctx = &endp->xfer_ctx;
    static const uint8_t tog_to_pid[] = {USBH_PID_DATA0, USBH_PID_DATA1, USBH_PID_DATA2, USBH_PID_MDATA};

    /* Process the completion of a transfer unit */
    if (endp->type == USB_ENDP_TYPE_ISOC || xfer_unit->rx_pid == USBH_PID_ACK || xfer_unit->rx_pid == USBH_PID_NYET ||
        xfer_unit->rx_pid == tog_to_pid[xfer_unit->toggle])
    {
        /* Reset the retry counter */
        xfer_ctx->retry = 0;

        /* If the PING is successful, proceed with the OUT transaction */
        if (xfer_unit->token == USBH_PID_PING)
        {
            xfer_unit->token = USBH_PID_OUT;
            return;
        }

        /* If the NYET is received, switch to the PING token */
        if (xfer_unit->rx_pid == USBH_PID_NYET)
        {
            xfer_unit->token = USBH_PID_PING;
        }

        /* Update the data toggle for non-isochronous endpoints */
        if (endp->type != USB_ENDP_TYPE_ISOC)
        {
            xfer_ctx->toggle ^= USBH_TOGGLE_DATA1;
        }

        /* Update the transfer context offset */
        xfer_ctx->offset += xfer_unit->xfer_len;

        /* Process non-control transfer transactions */
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
    /* Process NAK response */
    else if (xfer_unit->rx_pid == USBH_PID_NAK)
    {
        if (endp->ping_en && xfer_unit->token == USBH_PID_OUT)
        {
            xfer_unit->token = USBH_PID_PING;
        }
    }
    /* Process STALL response */
    else if (xfer_unit->rx_pid == USBH_PID_STALL)
    {
        xfer_ctx->is_busy = false;
        xfer_ctx->is_stalled = true;
    }
    /* Process unexpected response */
    else
    {
    }

    /* Update the retry counter and check for maximum retries */
    if (!rst)
    {
        xfer_ctx->retry++;
        uint32_t max_retry = endp->type == USB_ENDP_TYPE_CTRL ? CTRL_XFER_MAX_RETRY : DATA_XFER_MAX_RETRY;
        if (xfer_ctx->retry >= max_retry)
        {
            xfer_ctx->is_busy = false;
        }
    }

    /* Call the transfer completion callback */
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

            /* Append the transfer unit to the linked list */
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
    return last;
}

static usbh_xfer_unit_t **bulk_xfer_process(usbh_handle_t *h, usbh_xfer_unit_t **last, usbh_endpoint_t *list)
{
    return last;
}

static usbh_xfer_unit_t **intr_xfer_process(usbh_handle_t *h, usbh_xfer_unit_t **last, usbh_endpoint_t *list)
{
    return last;
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
        h->enum_tick = h->tick;

        usbh_device_t *dev = h->device_list;
        while (dev)
        {
            /* Save the next device pointer in case the current device is removed during enumeration */
            usbh_device_t *next = dev->next;

            /* Check if the device is in the enumeration process */
            if (dev->enum_stage < ENUM_STAGE_END)
            {
                device_enum_task(h, dev);
            }

            dev = next;
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
        last_xfer_unit = ctrl_xfer_process(h, last_xfer_unit, h->endpoint_list[USB_ENDP_TYPE_CTRL]);
        last_xfer_unit = bulk_xfer_process(h, last_xfer_unit, h->endpoint_list[USB_ENDP_TYPE_BULK]);

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
    usbh_device_remove(h, 0, 0);
    return h->close(h);
}

void usbh_device_insert(usbh_handle_t *h, uint8_t hub_addr, uint8_t hub_port)
{
    /* Allocate a new device handle */
    usbh_device_t *dev = device_handle_alloc();
    if (dev == NULL)
    {
        USB_LOGE("Handle: %p Failed to allocate device handle", h);
        return;
    }

    /* Initialize the device handle */
    memset(dev, 0, sizeof(usbh_device_t));

    /* Allocate a bus address for the new device */
    dev->address = bus_address_alloc(h);
    if (dev->address == 0)
    {
        USB_LOGE("Handle: %p Failed to allocate bus address for device %p", h, dev);
        goto free_device;
    }

    /* Set the host, hub address, and hub port for the new device */
    dev->host = h;
    dev->hub_addr = hub_addr;
    dev->hub_port = hub_port;

    /* Append the new device to the device list */
    usbh_list_append((void **)&h->device_list, dev);

    /* Reset device */
    usbh_device_reset(dev);

    USB_LOGI("Handle: %p Inserted device %p at hub_addr: %d, hub_port: %d", h, dev, hub_addr, hub_port);
    return;

free_device:
    device_handle_free(dev);
}

void usbh_device_remove(usbh_handle_t *h, uint8_t hub_addr, uint8_t hub_port)
{
    usbh_device_t *target = h->device_list;

    /* Traverse the device list to find the target device to remove */
    while (target && (target->hub_addr != hub_addr || target->hub_port != hub_port))
    {
        target = target->next;
    }
    if (!target) return;

    usbh_device_t *child = h->device_list;

    /* Recursively remove all child devices of the target device */
    while (child)
    {
        if (child->hub_addr == target->address)
        {
            usbh_device_remove(h, child->hub_addr, child->hub_port);
            child = h->device_list;
        }
        else
        {
            child = child->next;
        }
    }

    /* Remove the target device from the device list */
    usbh_list_remove((void **)&h->device_list, target);

    /* Unbind all class drivers from the target device */
    unbind_class_driver(h, target);

    /* Close all endpoints of the target device */
    usbh_endp_close(target, 0);
    for (size_t i = 0; i < USB_ARRAY_SIZE(target->data_endp); i++)
    {
        for (size_t j = 0; j < USB_ARRAY_SIZE(target->data_endp[i]); j++)
        {
            if (target->data_endp[i][j])
            {
                usbh_endp_close(target, target->data_endp[i][j]->addr);
            }
        }
    }

    /* Free the bus address and device handle for the target device */
    bus_address_free(h, target->address);
    device_handle_free(target);

    USB_LOGI("Removed device %p from hub_addr: %d, hub_port: %d", target, hub_addr, hub_port);
}

void usbh_device_reset(usbh_device_t *dev)
{
    if (dev == NULL || dev->address == 0) return;

    dev->enum_stage = ENUM_STAGE_RESET;
    if (dev->hub_addr == 0)
    {
        dev->host->root_set_feature(dev->host, USBH_PORT_FEATURE_RESET);
    }
    else if (dev->host->port_set_feature)
    {
        dev->host->port_set_feature(dev->host, USBH_PORT_FEATURE_RESET);
    }
    else
    {
        USB_LOGW("Handle: %p Does not support hub port operations", dev->host);
    }
}

bool usbh_ctrl_xfer(usbh_device_t *dev, const void *handle, usb_setup_t *setup, void *buf, usbh_ctrl_xfer_cb cb)
{
    usbh_xfer_ctx_t *xfer_ctx = &dev->ctrl_endp.xfer_ctx;
    if (!xfer_ctx->is_busy)
    {
        dev->ctrl_endp.handle = (void *)handle;
        dev->ctrl_endp.xfer_cb = (void *)cb;
        memset(xfer_ctx, 0, sizeof(usbh_xfer_ctx_t));
        xfer_ctx->is_busy = true;
        xfer_ctx->length = setup->wLength;
        xfer_ctx->buf = buf;
        xfer_ctx->setup = setup;
        return true;
    }
    return false;
}

bool usbh_endp_open(usbh_device_t *dev, const void *handle, const usb_desc_endpoint_t *ep_desc, usbh_data_xfer_cb cb)
{
    usbh_endpoint_t *ep = endpoint_handle_alloc();
    if (!ep)
    {
        USB_LOGW("Handle: %p Failed to allocate endpoint handle", dev);
        return false;
    }

    uint8_t endp_dir = USB_ENDP_DIR(ep_desc->bEndpointAddress);
    uint8_t endp_num = USB_ENDP_NUM(ep_desc->bEndpointAddress);

    if (endp_num == 0)
    {
        USB_LOGW("Handle: %p Cannot open endpoint 0", dev);
        endpoint_handle_free(ep);
        return false;
    }

    dev->data_endp[endp_dir ? USB_DIR_IN : USB_DIR_OUT][endp_num - 1] = ep;
    return true;
}

bool usbh_endp_close(usbh_device_t *dev, usb_endp_t endp)
{
    uint8_t endp_dir = USB_ENDP_DIR(endp);
    uint8_t endp_num = USB_ENDP_NUM(endp);

    if (endp_num < USB_MAX_ENDP_NUM)
    {
        usbh_endpoint_t *ep;
        if (endp_num == 0)
        {
            ep = &dev->ctrl_endp;
            ep->xfer_ctx.is_busy = false;
            usbh_list_remove((void **)&dev->host->endpoint_list[USB_ENDP_TYPE_CTRL], ep);
        }
        else
        {
            ep = dev->data_endp[endp_dir ? USB_DIR_IN : USB_DIR_OUT][endp_num - 1];
            dev->data_endp[endp_dir ? USB_DIR_IN : USB_DIR_OUT][endp_num - 1] = NULL;
            if (ep)
            {
                ep->xfer_ctx.is_busy = false;
                usbh_list_remove((void **)&dev->host->endpoint_list[USB_MIN(ep->type, 3)], ep);
                endpoint_handle_free(ep);
            }
        }
    }

    return true;
}

bool usbh_endp_read(usbh_device_t *dev, usb_endp_t endp, void *buf, size_t len)
{
    return true;
}

bool usbh_endp_write(usbh_device_t *dev, usb_endp_t endp, const void *buf, size_t len)
{
    return true;
}

#endif // USB_HOST_DRIVER_EN
