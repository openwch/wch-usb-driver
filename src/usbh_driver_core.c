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
#define ENDP0_DEFAULT_MPS 8

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

            /* Update the host speed if the device is connected directly to the root hub */
            if (dev->hub_addr == 0) h->speed = dev->speed;

            /* Set the default maximum packet size for endpoint 0 */
            dev->ep0_mps = ENDP0_DEFAULT_MPS;

            /* Initialize the control endpoint for the device */
            usbh_endpoint_t *ctrl_endp = &dev->ctrl_endp;
            memset(ctrl_endp, 0, sizeof(usbh_endpoint_t));
            ctrl_endp->ping_en = dev->speed == USB_SPEED_HIGH ? true : false;
            ctrl_endp->interval = endpoint_interval_calc(dev->speed, USB_ENDP_TYPE_CTRL, 0);
            ctrl_endp->class_handle = dev;

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
        }
        break;

    case ENUM_STAGE_ENUM_FAILED:
        usbh_device_remove(h, dev->hub_addr, dev->hub_port);
        break;
    }
}

void usbh_drv_task(usbh_handle_t *h)
{
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
