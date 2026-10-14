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
#ifndef USBH_DEVICE_POOL_SIZE
#define USBH_DEVICE_POOL_SIZE 1
#endif

#ifndef USBH_ENDPOINT_POOL_SIZE
#define USBH_ENDPOINT_POOL_SIZE 4
#endif

#define ENDP0_DEFAULT_MPS      8
#define CTRL_XFER_TIMEOUT      1000
#define SPLIT_MAX_RETRY        6
#define SPLIT_ISO_OUT_MAX_SIZE 188

/* @enum */
typedef enum
{
    ENUM_STAGE_IDLE,
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

typedef enum
{
    SUSPEND_STAGE_IDLE,
    SUSPEND_STAGE_ENABLE_REMOTE_WAKEUP,
    SUSPEND_STAGE_ENTRY_SUSPEND,
    SUSPEND_STAGE_CLEAR_REMOTE_WAKEUP,
    SUSPEND_STAGE_WAKEUP,
} suspend_stage_t;

/* @global */
static usbh_device_t device_pool[USBH_DEVICE_POOL_SIZE] = {0};
static usbh_endpoint_t endpoint_pool[USBH_ENDPOINT_POOL_SIZE] = {0};

static usbh_device_t *device_handle_alloc(void)
{
    for (size_t i = 0; i < USB_ARRAY_SIZE(device_pool); i++)
    {
        usbh_device_t *dev = &device_pool[i];
        if (dev->address == 0)
        {
            memset(dev, 0, sizeof(usbh_device_t));
            return dev;
        }
    }
    return NULL;
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

static usbh_endpoint_t *endpoint_handle_alloc(void)
{
    for (size_t i = 0; i < USB_ARRAY_SIZE(endpoint_pool); i++)
    {
        usbh_endpoint_t *ep = &endpoint_pool[i];
        if (!ep->is_used)
        {
            memset(ep, 0, sizeof(usbh_endpoint_t));
            ep->is_used = true;
            return ep;
        }
    }
    return NULL;
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

static uint8_t check_device_desc(usbh_device_t *dev, const usb_desc_device_t *dev_desc)
{
    if (!dev || !dev->address || dev->speed == USB_SPEED_UNKNOWN || !dev_desc) return 1;
    if (dev_desc->bLength != sizeof(usb_desc_device_t)) return 2;
    if (dev_desc->bDescriptorType != USB_DESC_DEVICE) return 3;
    if ((dev_desc->bcdUSB & 0xFF00) > 0x0200) return 4;
    if (dev->speed == USB_SPEED_LOW)
    {
        if (dev_desc->bMaxPacketSize0 != 8) return 5;
    }
    else if (dev_desc->bMaxPacketSize0 != 8 && dev_desc->bMaxPacketSize0 != 16 && dev_desc->bMaxPacketSize0 != 32 &&
             dev_desc->bMaxPacketSize0 != 64)
        return 6;
    return 0;
}

static uint8_t check_config_desc(usbh_device_t *dev, const void *desc_coll, uint16_t length)
{
    if (!dev || !dev->address || dev->speed == USB_SPEED_UNKNOWN || !desc_coll || length < sizeof(usb_desc_config_t))
        return 1;

    const usb_desc_config_t *config_desc = (const usb_desc_config_t *)desc_coll;
    if (config_desc->bLength != sizeof(usb_desc_config_t)) return 2;
    if (config_desc->bDescriptorType != USB_DESC_CONFIGURATION) return 3;
    if (config_desc->wTotalLength < sizeof(usb_desc_config_t)) return 4;
    if (config_desc->bNumInterfaces == 0 || config_desc->bConfigurationValue == 0) return 5;
    if (!(config_desc->bmAttributes & 0x80) || (config_desc->bmAttributes & 0x1F)) return 6;

    bool iad = false;
    int8_t itf_index = -1;
    uint8_t itf_count = config_desc->bNumInterfaces;
    uint8_t iad_itf_count = 0;
    uint8_t prev_type = USB_DESC_CONFIGURATION;
    uint8_t endp_count = 0;
    uint16_t alt_used_bitmap[2] = {0};
    uint16_t itf_used_bitmap[2] = {0};
    uint16_t endp_used_bitmap[2] = {0};
    const uint8_t *curr = (const uint8_t *)desc_coll + sizeof(usb_desc_config_t);
    const uint8_t *end = (const uint8_t *)desc_coll + length;

    while (curr < end)
    {
        uint8_t len = curr[0];
        if (len < 2 || curr + len > end) return 7;

        uint8_t type = curr[1];
        /* Check descriptor sequence validity */
        switch (prev_type)
        {
        case USB_DESC_CONFIGURATION:
            if (type != USB_DESC_INTERFACE && type != USB_DESC_INTERFACE_ASSOCIATION) return 8;
            break;

        case USB_DESC_INTERFACE_ASSOCIATION:
            if (type != USB_DESC_INTERFACE) return 9;
            break;
        }
        prev_type = type;

        switch (type)
        {
        case USB_DESC_INTERFACE:
        {
            const usb_desc_interface_t *itf_desc = (usb_desc_interface_t *)curr;
            if (itf_desc->bLength != sizeof(usb_desc_interface_t)) return 10;
            itf_used_bitmap[0] |= alt_used_bitmap[0];
            itf_used_bitmap[1] |= alt_used_bitmap[1];
            memset(alt_used_bitmap, 0, sizeof(alt_used_bitmap));

            if (itf_desc->bAlternateSetting == 0)
            {
                itf_index++;
                if (itf_index >= itf_count) return 11;
                if (endp_used_bitmap[0] & itf_used_bitmap[0] || endp_used_bitmap[1] & itf_used_bitmap[1]) return 12;
                endp_used_bitmap[0] |= itf_used_bitmap[0];
                endp_used_bitmap[1] |= itf_used_bitmap[1];
                memset(itf_used_bitmap, 0, sizeof(itf_used_bitmap));

                /* Check if the previous IAD has been fully processed */
                if (iad) iad_itf_count--;
                if (iad_itf_count == 0) iad = false;
            }
            if (itf_desc->bInterfaceNumber != itf_index) return 13;
            if (itf_desc->bNumEndpoints > USB_MAX_ENDP_NUM * 2 - 2) return 14;
            if (endp_count) return 15;
            endp_count = itf_desc->bNumEndpoints;
            break;
        }

        case USB_DESC_ENDPOINT:
        {
            static const uint16_t valid_mps_table[3][4] = {
                // CTRL    ISOC    BULK    INTR
                {0x0040, 0x03FF, 0x0040, 0x0040}, // FULL
                {0x0008, 0x0000, 0x0000, 0x0008}, // LOW
                {0x0040, 0x0400, 0x0200, 0x0400}, // HIGH
            };

            if (len != sizeof(usb_desc_endpoint_t) && len != sizeof(usb_desc_iso_endpoint_t)) return 16;
            const usb_desc_endpoint_t *endp_desc = (usb_desc_endpoint_t *)curr;
            uint8_t endp_dir = USB_ENDP_DIR(endp_desc->bEndpointAddress);
            uint8_t endp_num = USB_ENDP_NUM(endp_desc->bEndpointAddress);
            uint8_t endp_type = USB_ENDP_GET_TYPE(endp_desc->bmAttributes);
            if (endp_num == 0 || endp_type == USB_ENDP_TYPE_CTRL) return 17;
            uint8_t additional = USB_ENDP_GET_ADDITIONAL(endp_desc->wMaxPacketSize);
            uint16_t mps = USB_ENDP_GET_MPS(endp_desc->wMaxPacketSize);
            if (endp_desc->wMaxPacketSize & 0xE000) return 18;
            if (additional == 0x03 || mps == 0 || mps > valid_mps_table[dev->speed - 1][endp_type]) return 19;
            if (endp_type != USB_ENDP_TYPE_INTR && dev->speed == USB_SPEED_LOW) return 20;
            if (endp_type == USB_ENDP_TYPE_ISOC && (endp_desc->bInterval == 0 || endp_desc->bInterval > 16)) return 21;
            if (endp_type == USB_ENDP_TYPE_INTR && endp_desc->bInterval == 0) return 22;
            if (endp_type == USB_ENDP_TYPE_INTR && dev->speed == USB_SPEED_HIGH && endp_desc->bInterval > 16) return 23;
            if (endp_type == USB_ENDP_TYPE_BULK && dev->speed == USB_SPEED_HIGH && mps != 512) return 24;
            if (endp_type == USB_ENDP_TYPE_BULK && dev->speed == USB_SPEED_FULL && mps != 8 && mps != 16 && mps != 32 &&
                mps != 64)
                return 25;
            if ((endp_used_bitmap[endp_dir ? 1 : 0] & (1 << endp_num)) ||
                (alt_used_bitmap[endp_dir ? 1 : 0] & (1 << endp_num)))
                return 26;
            if (endp_count == 0) return 27;
            alt_used_bitmap[endp_dir ? 1 : 0] |= (1 << endp_num);
            endp_count--;
            break;
        }

        case USB_DESC_INTERFACE_ASSOCIATION:
        {
            if (dev->dev_desc.bDeviceClass != USB_CLASS_MISC || dev->dev_desc.bDeviceSubClass != MISC_SUBCLASS_COMMON ||
                dev->dev_desc.bDeviceProtocol != MISC_PROTOCOL_IAD || iad)
                return 28;
            const usb_desc_iad_t *iad_desc = (usb_desc_iad_t *)curr;
            if (iad_desc->bLength != sizeof(usb_desc_iad_t)) return 29;
            if (iad_desc->bFirstInterface != itf_index + 1) return 30;
            if (iad_desc->bInterfaceCount == 0 || itf_index + 1 + iad_desc->bInterfaceCount > itf_count) return 31;
            iad_itf_count = iad_desc->bInterfaceCount;
            iad = true;
            break;
        }

        default:
            /* Ignore unknown descriptor types except for class-specific descriptors */
            if (type < 0x21 || type > 0x2F)
            {
                return 32;
            }
        }
        curr += len;
    }
    itf_used_bitmap[0] |= alt_used_bitmap[0];
    itf_used_bitmap[1] |= alt_used_bitmap[1];
    if (itf_index + 1 != itf_count) return 33;
    if (endp_count != 0) return 34;
    if (endp_used_bitmap[0] & itf_used_bitmap[0] || endp_used_bitmap[1] & itf_used_bitmap[1]) return 35;
    return 0;
}

static void enum_ctrl_xfer_cb(void *handle, bool rst, const usb_setup_t *setup, const void *buf, uint16_t length)
{
    usbh_device_t *dev = (usbh_device_t *)handle;

    if (!rst)
    {
        USB_LOGE("Device handle: %p Enumeration control transfer failed at stage %d", dev, dev->enum_stage);
        dev->enum_stage = ENUM_STAGE_ENUM_FAILED;
        return;
    }

    switch (dev->enum_stage)
    {
    case ENUM_STAGE_GET_EP0_SIZE:
    {
        uint8_t ep0_mps = dev->dev_desc.bMaxPacketSize0;
        if (ep0_mps >= 8 && ep0_mps <= 64)
        {
            dev->enum_stage = ENUM_STAGE_SET_ADDR;
            dev->ctrl_endp.mps = ep0_mps;
            USB_LOGI("Device handle: %p Get endpoint 0 max packet size is %d", dev, ep0_mps);

            usb_setup_t *request = &dev->host->ctrl_setup;
            request->bmRequestType = USB_SET_REQ(USB_DIR_OUT, USB_REQ_TYPE_STANDARD, USB_REQ_RCPT_DEVICE);
            request->bRequest = USB_REQ_SET_ADDRESS;
            request->wValue = dev->address;
            request->wIndex = 0;
            request->wLength = 0;
            usbh_ctrl_xfer(dev, dev, request, NULL, CTRL_XFER_TIMEOUT, enum_ctrl_xfer_cb);
        }
        else
        {
            dev->enum_stage = ENUM_STAGE_ENUM_FAILED;
            USB_LOGE("Device handle: %p Invalid EP0 max packet size: %d", dev, ep0_mps);
        }
        break;
    }

    case ENUM_STAGE_SET_ADDR:
    {
        dev->enum_stage = ENUM_STAGE_GET_DEVICE_DESC;
        dev->ctrl_endp.xfer_unit.dev_addr = dev->address;
        USB_LOGI("Device handle: %p Set bus address %d", dev, dev->address);

        usb_setup_t *request = &dev->host->ctrl_setup;
        request->bmRequestType = USB_SET_REQ(USB_DIR_IN, USB_REQ_TYPE_STANDARD, USB_REQ_RCPT_DEVICE);
        request->bRequest = USB_REQ_GET_DESCRIPTOR;
        request->wValue = USB_DESC_DEVICE << 8;
        request->wIndex = 0;
        request->wLength = sizeof(usb_desc_device_t);
        usbh_ctrl_xfer(dev, dev, request, &dev->dev_desc, CTRL_XFER_TIMEOUT, enum_ctrl_xfer_cb);
        break;
    }

    case ENUM_STAGE_GET_DEVICE_DESC:
    {
        uint8_t check_rst = check_device_desc(dev, &dev->dev_desc);
        if (check_rst == 0)
        {
            dev->enum_stage = ENUM_STAGE_GET_STRING_DESC;
            USB_LOGI("Device handle: %p Get device descriptor VID: 0x%04X PID: 0x%04X", dev, dev->dev_desc.idVendor,
                     dev->dev_desc.idProduct);

            usb_setup_t *request = &dev->host->ctrl_setup;
            request->bmRequestType = USB_SET_REQ(USB_DIR_IN, USB_REQ_TYPE_STANDARD, USB_REQ_RCPT_DEVICE);
            request->bRequest = USB_REQ_GET_DESCRIPTOR;
            request->wValue = USB_DESC_STRING << 8;
            request->wIndex = 0;
            request->wLength = 4;
            usbh_ctrl_xfer(dev, dev, request, dev->config_desc, CTRL_XFER_TIMEOUT, enum_ctrl_xfer_cb);
        }
        else
        {
            dev->enum_stage = ENUM_STAGE_ENUM_FAILED;
            USB_LOGE("Device handle: %p Invalid device descriptor error code: %d", dev, check_rst);
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
            USB_LOGI("Device handle: %p Get language ID is 0x%04X", dev, dev->language_id);

            usb_setup_t *request = &dev->host->ctrl_setup;
            request->bmRequestType = USB_SET_REQ(USB_DIR_IN, USB_REQ_TYPE_STANDARD, USB_REQ_RCPT_DEVICE);
            request->bRequest = USB_REQ_GET_DESCRIPTOR;
            request->wValue = USB_DESC_CONFIGURATION << 8;
            request->wIndex = 0;
            request->wLength = 4;
            usbh_ctrl_xfer(dev, dev, request, dev->config_desc, CTRL_XFER_TIMEOUT, enum_ctrl_xfer_cb);
        }
        else
        {
            dev->enum_stage = ENUM_STAGE_ENUM_FAILED;
            USB_LOGE("Device handle: %p Invalid language string descriptor", dev);
        }
        break;
    }

    case ENUM_STAGE_GET_CONFIG_DESC_SIZE:
    {
        usb_desc_config_t *config_desc = (usb_desc_config_t *)dev->config_desc;
        /* Check if the configuration descriptor fits within the buffer */
        if (config_desc->wTotalLength <= USBH_DESC_BUF_SIZE)
        {
            dev->enum_stage = ENUM_STAGE_GET_CONFIG_DESC;
            USB_LOGI("Device handle: %p Get configuration descriptor size is %d", dev, config_desc->wTotalLength);

            /* Keep the configuration index requested by the driver-binding stage */
            usb_setup_t *request = &dev->host->ctrl_setup;
            request->bmRequestType = USB_SET_REQ(USB_DIR_IN, USB_REQ_TYPE_STANDARD, USB_REQ_RCPT_DEVICE);
            request->bRequest = USB_REQ_GET_DESCRIPTOR;
            request->wIndex = 0;
            request->wLength = config_desc->wTotalLength;
            usbh_ctrl_xfer(dev, dev, request, dev->config_desc, CTRL_XFER_TIMEOUT, enum_ctrl_xfer_cb);
        }
        else
        {
            dev->enum_stage = ENUM_STAGE_ENUM_FAILED;
            USB_LOGE("Device handle: %p Configuration descriptor size %d exceeds buffer size %d", dev,
                     config_desc->wTotalLength, USBH_DESC_BUF_SIZE);
        }
        break;
    }

    case ENUM_STAGE_GET_CONFIG_DESC:
    {
        usb_desc_config_t *config_desc = (usb_desc_config_t *)dev->config_desc;
        /* Check if the configuration descriptor size matches the expected length */
        if (config_desc->wTotalLength != length)
        {
            dev->enum_stage = ENUM_STAGE_ENUM_FAILED;
            USB_LOGE("Device handle: %p Configuration descriptor size mismatch: expected %d, got %d", dev,
                     config_desc->wTotalLength, length);
            break;
        }

        /* Validate the configuration descriptor */
        uint8_t check_rst = check_config_desc(dev, dev->config_desc, length);
        if (check_rst == 0)
        {
            dev->enum_stage = ENUM_STAGE_BIND_DRIVER;
            USB_LOGI("Device handle: %p Get configuration descriptor Configurations: %d Interfaces: %d", dev,
                     config_desc->bConfigurationValue, config_desc->bNumInterfaces);
        }
        else
        {
            dev->enum_stage = ENUM_STAGE_ENUM_FAILED;
            USB_LOGE("Device handle: %p Invalid configuration descriptor error code: %d", dev, check_rst);
        }
        break;
    }

    case ENUM_STAGE_SET_CONFIG:
    {
        dev->enum_stage = ENUM_STAGE_END;
        USB_LOGI("Device handle: %p Set configuration %d enumeration completed", dev, setup->wValue);
        break;
    }
    }
}

static bool match_driver(usbh_handle_t *h, usbh_device_t *dev, const void *desc, size_t size)
{
    usb_desc_device_t *dev_desc = &dev->dev_desc;
    usb_desc_interface_t *itf_desc = (usb_desc_interface_t *)desc;

    for (size_t i = 0; i < USB_ARRAY_SIZE(h->drv_info_list); i++)
    {
        const usbh_drv_info_t *drv_info = h->drv_info_list[i];
        if (!drv_info) continue;
        uint16_t match = drv_info->match_bitmap;
        if ((match & USBH_DRV_MATCH_VID) && dev_desc->idVendor != drv_info->vid) continue;
        if ((match & USBH_DRV_MATCH_PID) && dev_desc->idProduct != drv_info->pid) continue;
        if ((match & USBH_DRV_MATCH_DEV_CLASS) && dev_desc->bDeviceClass != drv_info->dev_class) continue;
        if ((match & USBH_DRV_MATCH_DEV_SUBCLASS) && dev_desc->bDeviceSubClass != drv_info->dev_subclass) continue;
        if ((match & USBH_DRV_MATCH_DEV_PROTOCOL) && dev_desc->bDeviceProtocol != drv_info->dev_protocol) continue;
        if ((match & USBH_DRV_MATCH_ITF_CLASS) && itf_desc->bInterfaceClass != drv_info->itf_class) continue;
        if ((match & USBH_DRV_MATCH_ITF_SUBCLASS) && itf_desc->bInterfaceSubClass != drv_info->itf_subclass) continue;
        if ((match & USBH_DRV_MATCH_ITF_PROTOCOL) && itf_desc->bInterfaceProtocol != drv_info->itf_protocol) continue;
        return drv_info->bind(dev, desc, size);
    }
    return false;
}

static bool bind_class_driver(usbh_handle_t *h, usbh_device_t *dev, const void *desc)
{
    const uint8_t *slow;
    const uint8_t *fast;
    const uint8_t *end;
    uint8_t itf_count = 1;
    uint8_t bind_count = 0;
    usb_desc_config_t *config_desc = (usb_desc_config_t *)desc;
    slow = fast = (const uint8_t *)desc + config_desc->bLength;
    end = (const uint8_t *)desc + config_desc->wTotalLength;

    /* The CDC device has two interfaces */
    if (dev->dev_desc.bDeviceClass == USB_CLASS_CDC) itf_count = 2;

    while (fast < end)
    {
        uint8_t len = fast[0];
        uint8_t type = fast[1];

        if (type == USB_DESC_INTERFACE_ASSOCIATION)
        {
            if (fast > slow && match_driver(h, dev, slow, fast - slow))
            {
                bind_count++;
            }
            const usb_desc_iad_t *iad_desc = (const usb_desc_iad_t *)fast;
            itf_count = iad_desc->bInterfaceCount;
            fast += len;
            slow = fast;
        }
        else if (type == USB_DESC_INTERFACE && fast[3] == 0 && fast > slow)
        {
            itf_count--;
            if (itf_count > 0)
            {
                fast += len;
                continue;
            }

            itf_count = 1;
            if (match_driver(h, dev, slow, fast - slow))
            {
                bind_count++;
            }
            slow = fast;
        }
        else
        {
            fast += len;
        }
    }

    /* Match the last interface */
    if (fast > slow && match_driver(h, dev, slow, fast - slow))
    {
        bind_count++;
    }

    /* Return true if at least one interface was successfully matched */
    return bind_count > 0;
}

static void unbind_class_driver(usbh_handle_t *h, usbh_device_t *dev)
{
    for (size_t i = 0; i < USB_ARRAY_SIZE(h->drv_info_list); i++)
    {
        const usbh_drv_info_t *drv_info = h->drv_info_list[i];
        if (drv_info)
        {
            drv_info->unbind(dev, USBH_DRV_ALL_INTERFACE);
        }
    }
}

static void device_enum_task(usbh_handle_t *h, usbh_device_t *dev)
{
    usbh_port_status_t port_status = {0};
    switch (dev->enum_stage)
    {
    case ENUM_STAGE_IDLE:
        usbh_device_reset(dev);
        dev->enum_stage = ENUM_STAGE_RESET;
        break;

    case ENUM_STAGE_RESET:
        if (dev->hub_addr == 0)
        {
            h->root_get_status(h, &port_status);
        }
        else if (h->port_get_status)
        {
            h->port_get_status(dev, &port_status);
        }
        else
        {
            USB_LOGW("Device handle: %p Enumeration on the stage %d failed, does not support hub port operations", dev,
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

            USB_LOGI("Device handle: %p Reset complete link speed: %s speed", dev,
                     dev->speed == USB_SPEED_LOW ? "low" : (dev->speed == USB_SPEED_HIGH ? "high" : "full"));

            /* Update the host speed if the device is connected directly to the root hub */
            if (dev->hub_addr == 0) h->speed = dev->speed;

            /* Initialize the control endpoint for the device */
            usbh_endpoint_t *ep = &dev->ctrl_endp;
            memset(ep, 0, sizeof(usbh_endpoint_t));
            ep->ping_en = dev->speed == USB_SPEED_HIGH;
            ep->mps = ENDP0_DEFAULT_MPS;
            ep->interval = endpoint_interval_calc(h->speed, dev->speed, USB_ENDP_TYPE_CTRL, 0);
            ep->handle = dev;
            usbh_list_append((void **)&h->endpoint_list[USB_ENDP_TYPE_CTRL], ep);

            /* Initialize the transfer unit for the control endpoint */
            usbh_xfer_unit_t *xfer_unit = &ep->xfer_unit;
            xfer_unit->endpoint = ep;

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

            usb_setup_t *request = &dev->host->ctrl_setup;
            request->bmRequestType = USB_SET_REQ(USB_DIR_IN, USB_REQ_TYPE_STANDARD, USB_REQ_RCPT_DEVICE);
            request->bRequest = USB_REQ_GET_DESCRIPTOR;
            request->wValue = USB_DESC_DEVICE << 8;
            request->wIndex = 0;
            request->wLength = 8;
            usbh_ctrl_xfer(dev, dev, request, &dev->dev_desc, CTRL_XFER_TIMEOUT, enum_ctrl_xfer_cb);
        }
        break;

    case ENUM_STAGE_BIND_DRIVER:
        if (bind_class_driver(h, dev, dev->config_desc))
        {
            dev->enum_stage = ENUM_STAGE_SET_CONFIG;
            usb_desc_config_t *config_desc = (usb_desc_config_t *)dev->config_desc;
            dev->self_powered = config_desc->bmAttributes & 0x40 ? true : false;
            dev->remote_wakeup = config_desc->bmAttributes & 0x20 ? true : false;
            USB_LOGI("Device handle: %p Binding driver for configuration %d", dev, config_desc->bConfigurationValue);

            usb_setup_t *request = &dev->host->ctrl_setup;
            request->bmRequestType = USB_SET_REQ(USB_DIR_OUT, USB_REQ_TYPE_STANDARD, USB_REQ_RCPT_DEVICE);
            request->bRequest = USB_REQ_SET_CONFIGURATION;
            request->wValue = config_desc->bConfigurationValue;
            request->wIndex = 0;
            request->wLength = 0;
            usbh_ctrl_xfer(dev, dev, request, NULL, CTRL_XFER_TIMEOUT, enum_ctrl_xfer_cb);
        }
        else
        {
            /* Try the next configuration if the current one is not supported */
            uint8_t config_index = USB_U16_LOW(dev->host->ctrl_setup.wValue);
            if (config_index + 1 < dev->dev_desc.bNumConfigurations)
            {
                dev->enum_stage = ENUM_STAGE_GET_CONFIG_DESC_SIZE;
                USB_LOGW("Device handle: %p configuration %d not supported, trying next", dev, config_index);

                usb_setup_t *request = &dev->host->ctrl_setup;
                request->bmRequestType = USB_SET_REQ(USB_DIR_IN, USB_REQ_TYPE_STANDARD, USB_REQ_RCPT_DEVICE);
                request->bRequest = USB_REQ_GET_DESCRIPTOR;
                request->wValue = (USB_DESC_CONFIGURATION << 8) | (config_index + 1);
                request->wIndex = 0;
                request->wLength = 4;
                usbh_ctrl_xfer(dev, dev, request, dev->config_desc, CTRL_XFER_TIMEOUT, enum_ctrl_xfer_cb);
            }
            else
            {
                dev->enum_stage = ENUM_STAGE_ENUM_FAILED;
                USB_LOGE("Device handle: %p Bind driver failed", dev);
            }
        }
        break;

    case ENUM_STAGE_ENUM_FAILED:
        dev->enum_stage = ENUM_STAGE_END;
        usbh_device_suspend(dev, NULL);
        break;
    }
}

static void device_suspend_task(usbh_handle_t *h, usbh_device_t *dev)
{
    if (usbh_is_ctrl_xfering(dev)) return;
    switch (dev->suspend_stage)
    {
    case SUSPEND_STAGE_ENABLE_REMOTE_WAKEUP:
    {
        usb_setup_t *request = &dev->host->ctrl_setup;
        request->bmRequestType = USB_SET_REQ(USB_DIR_OUT, USB_REQ_TYPE_STANDARD, USB_REQ_RCPT_DEVICE);
        request->bRequest = USB_REQ_SET_FEATURE;
        request->wValue = USB_FEATURE_REMOTE_WAKEUP;
        request->wIndex = 0;
        request->wLength = 0;
        usbh_ctrl_xfer(dev, dev, request, NULL, CTRL_XFER_TIMEOUT, NULL);
        dev->suspend_stage = SUSPEND_STAGE_ENTRY_SUSPEND;
        break;
    }

    case SUSPEND_STAGE_ENTRY_SUSPEND:
    {
        dev->suspend_stage = SUSPEND_STAGE_IDLE;
        USB_LOGI("Device handle: %p suspended", dev);
        if (dev->hub_addr == 0)
        {
            dev->host->root_set_feature(dev->host, USBH_PORT_FEATURE_SUSPEND);
        }
        else if (dev->host->port_set_feature)
        {
            dev->host->port_set_feature(dev, USBH_PORT_FEATURE_SUSPEND);
        }
        else
        {
            USB_LOGW("Host handle: %p Does not support hub port operations", dev->host);
        }
        break;
    }

    case SUSPEND_STAGE_CLEAR_REMOTE_WAKEUP:
    {
        usb_setup_t *request = &dev->host->ctrl_setup;
        request->bmRequestType = USB_SET_REQ(USB_DIR_OUT, USB_REQ_TYPE_STANDARD, USB_REQ_RCPT_DEVICE);
        request->bRequest = USB_REQ_CLEAR_FEATURE;
        request->wValue = USB_FEATURE_REMOTE_WAKEUP;
        request->wIndex = 0;
        request->wLength = 0;
        usbh_ctrl_xfer(dev, dev, request, NULL, CTRL_XFER_TIMEOUT, NULL);
        dev->suspend_stage = SUSPEND_STAGE_WAKEUP;
        break;
    }

    case SUSPEND_STAGE_WAKEUP:
    {
        dev->suspend_stage = SUSPEND_STAGE_IDLE;
        dev->suspended = false;
        USB_LOGI("Device handle: %p Device wakeup", dev);
        if (dev->wakeup_cb)
        {
            dev->wakeup_cb(dev);
        }
        break;
    }
    }
}

static bool split_xfer_completed_process(usbh_handle_t *h, usbh_xfer_unit_t *xfer_unit)
{
    usbh_split_data_t *split = &xfer_unit->split_data;
    if (split->sc == 0)
    {
        /* Check if there is any remaining split ISO OUT data to be transferred */
        if (xfer_unit->split_iso_out_len)
        {
            xfer_unit->split_iso_out_ofs += xfer_unit->xfer_len;

            /* Update the split ISO OUT offset with the length of the completed transfer */
            if (xfer_unit->split_iso_out_ofs >= xfer_unit->split_iso_out_len)
            {
                xfer_unit->xfer_len = xfer_unit->split_iso_out_ofs;
                xfer_unit->split_iso_out_ofs = 0;
                return true;
            }

            /* Prepare the next split ISO OUT transfer */
            xfer_unit->buf = (uint8_t *)xfer_unit->buf + xfer_unit->xfer_len;
            uint16_t remaining = xfer_unit->split_iso_out_len - xfer_unit->split_iso_out_ofs;
            if (remaining <= SPLIT_ISO_OUT_MAX_SIZE)
            {
                split->s = 0;
                split->e = 1;
                xfer_unit->xfer_len = remaining;
            }
            else
            {
                split->s = 0;
                split->e = 0;
                xfer_unit->xfer_len = SPLIT_ISO_OUT_MAX_SIZE;
            }
            return false;
        }

        /* Check if the split transfer is complete or needs to continue */
        if (split->et == USB_ENDP_TYPE_INTR || split->et == USB_ENDP_TYPE_ISOC || xfer_unit->rx_pid == USBH_PID_ACK)
        {
            split->sc = 1;
            xfer_unit->split_retry = 0;
            xfer_unit->split_tick = h->tick;
            return false;
        }
    }
    /* Complete split stage transfer incomplete */
    else if (xfer_unit->rx_pid == USBH_PID_NYET)
    {
        xfer_unit->split_retry++;
        if (xfer_unit->split_retry > SPLIT_MAX_RETRY)
        {
            split->sc = 0;
        }
        return false;
    }
    /* If the PID is MDATA, it indicates that there is more data to be transferred */
    else if (xfer_unit->rx_pid == USBH_PID_MDATA)
    {
        xfer_unit->buf = (uint8_t *)xfer_unit->buf + xfer_unit->xfer_len;
        return false;
    }

    split->sc = 0;
    return true;
}

static void xfer_completed_process(usbh_handle_t *h, usbh_xfer_unit_t *xfer_unit)
{
    usbh_endpoint_t *ep = xfer_unit->endpoint;
    if (!ep) return;

    bool rst = false;
    usbh_xfer_ctx_t *xfer_ctx = &ep->xfer_ctx;
    static const uint8_t tog_to_pid[] = {USBH_PID_DATA0, USBH_PID_DATA1, USBH_PID_DATA2, USBH_PID_MDATA};

    /* Process the completion of a transfer unit */
    if (ep->type == USB_ENDP_TYPE_ISOC || xfer_unit->rx_pid == USBH_PID_ACK || xfer_unit->rx_pid == USBH_PID_NYET ||
        xfer_unit->rx_pid == tog_to_pid[xfer_unit->toggle])
    {
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
        if (ep->type != USB_ENDP_TYPE_ISOC)
        {
            xfer_ctx->toggle ^= USBH_TOGGLE_DATA1;
        }

        /* Update the transfer context offset */
        xfer_ctx->offset += xfer_unit->xfer_len;

        /* Process non-control transfer transactions */
        if (ep->type != USB_ENDP_TYPE_CTRL)
        {
            if (xfer_ctx->offset >= xfer_ctx->length || xfer_unit->xfer_len < ep->mps)
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
            if (xfer_ctx->offset >= xfer_ctx->length || xfer_unit->xfer_len < ep->mps)
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
        if (ep->ping_en && xfer_unit->token == USBH_PID_OUT)
        {
            xfer_unit->token = USBH_PID_PING;
        }
    }
    /* Process STALL response */
    else if (xfer_unit->rx_pid == USBH_PID_STALL && xfer_ctx->ctrl_stage != USB_CTRL_STAGE_SETUP)
    {
        xfer_ctx->is_busy = false;
        xfer_ctx->is_stalled = true;
    }
    /* Process unexpected response */
    else
    {
    }

    /* Check for transfer timeout */
    if (!rst && xfer_ctx->timeout)
    {
        uint32_t timestamp = usbh_drv_get_timestamp(h);
        if (timestamp - xfer_ctx->timestamp >= xfer_ctx->timeout)
        {
            xfer_ctx->is_busy = false;
        }
    }

    /* Call the transfer completion callback */
    if (!xfer_ctx->is_busy && ep->xfer_cb)
    {
        if (ep->type == USB_ENDP_TYPE_CTRL)
        {
            ((usbh_ctrl_xfer_cb)ep->xfer_cb)(ep->handle, rst, xfer_ctx->setup, xfer_ctx->buf, xfer_ctx->offset);
        }
        else
        {
            ((usbh_data_xfer_cb)ep->xfer_cb)(ep->handle, rst, ep->addr, xfer_ctx->buf, xfer_ctx->offset);
        }
    }
}

static bool before_xfer_process(usbh_handle_t *h, usbh_endpoint_t *ep, usbh_xfer_unit_t ***last)
{
    if (!ep->xfer_ctx.is_busy)
    {
        return false;
    }

    usbh_xfer_unit_t *xfer_unit = &ep->xfer_unit;

    /* Handle split transfer */
    if (xfer_unit->split_data.split_data)
    {
        static const uint8_t valid_microframe[] = {6, 1, 6, 2};
        usbh_split_data_t *split = &xfer_unit->split_data;

        /* Split transfer complete stage */
        if (split->sc || xfer_unit->split_iso_out_ofs)
        {
            if (xfer_unit->split_tick != h->tick)
            {
                xfer_unit->split_tick = h->tick;
                xfer_unit->next = NULL;
                **last = xfer_unit;
                *last = &xfer_unit->next;
            }
            return false;
        }
        /* Split transfer start stage */
        else if (h->microframe < valid_microframe[xfer_unit->endpoint->type])
        {
            if (xfer_unit->endpoint->type == USB_ENDP_TYPE_ISOC && xfer_unit->token == USBH_PID_OUT)
            {
                usbh_xfer_ctx_t *xfer_ctx = &xfer_unit->endpoint->xfer_ctx;
                xfer_unit->split_iso_out_len = USB_MIN(xfer_ctx->length - xfer_ctx->offset, 1023);
                xfer_unit->split_iso_out_ofs = 0;
                split->s = 1;
                split->e = xfer_unit->split_iso_out_len <= SPLIT_ISO_OUT_MAX_SIZE ? 1 : 0;
            }
            return true;
        }
        return false;
    }

    return true;
}

static usbh_xfer_unit_t **ctrl_xfer_process(usbh_handle_t *h, usbh_xfer_unit_t **last, usbh_endpoint_t *list)
{
    usbh_endpoint_t *ep = list;
    while (ep)
    {
        if (before_xfer_process(h, ep, &last))
        {
            usbh_xfer_ctx_t *xfer_ctx = &ep->xfer_ctx;
            usbh_xfer_unit_t *xfer_unit = &ep->xfer_unit;

            /* Handle PING token separately */
            if (xfer_unit->token == USBH_PID_PING)
            {
                xfer_unit->next = NULL;
                *last = xfer_unit;
                last = &xfer_unit->next;
                ep = ep->next;
                continue;
            }

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
                xfer_unit->xfer_len = USB_MIN(xfer_ctx->length - xfer_ctx->offset, ep->mps);
                xfer_unit->buf = (void *)xfer_ctx->buf + xfer_ctx->offset;
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
        ep = ep->next;
    }
    return last;
}

static usbh_xfer_unit_t **isoc_xfer_process(usbh_handle_t *h, usbh_xfer_unit_t **last, usbh_endpoint_t *list)
{
    usbh_endpoint_t *ep = list;
    while (ep)
    {
        if (before_xfer_process(h, ep, &last))
        {
            usbh_xfer_ctx_t *xfer_ctx = &ep->xfer_ctx;
            usbh_xfer_unit_t *xfer_unit = &ep->xfer_unit;

            if (h->tick - xfer_ctx->tick > ep->interval)
            {
                /* Update interval tick */
                xfer_ctx->tick = h->tick;

                /* Prepare the transfer unit for interrupt transfer */
                xfer_unit->toggle = USBH_TOGGLE_DATA0;
                xfer_unit->xfer_len = xfer_unit->split_iso_out_len
                                          ? USB_MIN(xfer_unit->split_iso_out_len, SPLIT_ISO_OUT_MAX_SIZE)
                                          : USB_MIN(xfer_ctx->length - xfer_ctx->offset, ep->mps);
                xfer_unit->buf = (uint8_t *)xfer_ctx->buf + xfer_ctx->offset;

                /* Append the transfer unit to the linked list */
                xfer_unit->next = NULL;
                *last = xfer_unit;
                last = &xfer_unit->next;
            }
        }

        ep = ep->next;
    }
    return last;
}

static usbh_xfer_unit_t **bulk_xfer_process(usbh_handle_t *h, usbh_xfer_unit_t **last, usbh_endpoint_t *list)
{
    usbh_endpoint_t *ep = list;
    while (ep)
    {
        if (before_xfer_process(h, ep, &last))
        {
            usbh_xfer_ctx_t *xfer_ctx = &ep->xfer_ctx;
            usbh_xfer_unit_t *xfer_unit = &ep->xfer_unit;

            /* Handle PING token separately */
            if (xfer_unit->token == USBH_PID_PING)
            {
                xfer_unit->next = NULL;
                *last = xfer_unit;
                last = &xfer_unit->next;
                ep = ep->next;
                continue;
            }

            /* Prepare the transfer unit for bulk transfer */
            xfer_unit->toggle = xfer_ctx->toggle;
            xfer_unit->xfer_len = USB_MIN(xfer_ctx->length - xfer_ctx->offset, ep->mps);
            xfer_unit->buf = (uint8_t *)xfer_ctx->buf + xfer_ctx->offset;

            /* Append the transfer unit to the linked list */
            xfer_unit->next = NULL;
            *last = xfer_unit;
            last = &xfer_unit->next;
        }

        ep = ep->next;
    }
    return last;
}

static usbh_xfer_unit_t **intr_xfer_process(usbh_handle_t *h, usbh_xfer_unit_t **last, usbh_endpoint_t *list)
{
    usbh_endpoint_t *ep = list;
    while (ep)
    {
        if (before_xfer_process(h, ep, &last))
        {
            usbh_xfer_ctx_t *xfer_ctx = &ep->xfer_ctx;
            usbh_xfer_unit_t *xfer_unit = &ep->xfer_unit;

            if (h->tick - xfer_ctx->tick > ep->interval)
            {
                /* Update interval tick */
                xfer_ctx->tick = h->tick;

                /* Prepare the transfer unit for interrupt transfer */
                xfer_unit->toggle = xfer_ctx->toggle;
                xfer_unit->xfer_len = USB_MIN(xfer_ctx->length - xfer_ctx->offset, ep->mps);
                xfer_unit->buf = (uint8_t *)xfer_ctx->buf + xfer_ctx->offset;

                /* Append the transfer unit to the linked list */
                xfer_unit->next = NULL;
                *last = xfer_unit;
                last = &xfer_unit->next;
            }
        }

        ep = ep->next;
    }
    return last;
}

void usbh_drv_task(usbh_handle_t *h)
{
    /* Handle root port status change */
    if (h->root_port_change)
    {
        h->root_port_change = false;
        usbh_port_status_t port_status = {0};
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
    usbh_device_t *dev = h->device_list;
    while (dev)
    {
        if (dev->enum_stage < ENUM_STAGE_END)
        {
            device_enum_task(h, dev);
            break;
        }
        dev = dev->next;
    }

    /* Handle device suspend tasks */
    dev = h->device_list;
    while (dev)
    {
        if (dev->suspend_stage != SUSPEND_STAGE_IDLE)
        {
            device_suspend_task(h, dev);
        }
        dev = dev->next;
    }

    /* Handle transfer tasks */
    if (!h->xfer_busy)
    {
        /* Process transfer completed units */
        usbh_xfer_unit_t *xfer_unit = h->xfer_unit_list;
        while (xfer_unit)
        {
            bool xfer_completed = true;
            /* Check if the transfer unit is part of a split transfer */
            if (xfer_unit->split_data.split_data)
            {
                xfer_completed = split_xfer_completed_process(h, xfer_unit);
            }
            if (xfer_completed)
            {
                xfer_completed_process(h, xfer_unit);
            }
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
    if (!h) return false;
    return h->open(h);
}

bool usbh_drv_close(usbh_handle_t *h)
{
    if (!h) return false;
    usbh_device_remove(h, 0, 0);
    return h->close(h);
}

void usbh_device_insert(usbh_handle_t *h, uint8_t hub_addr, uint8_t hub_port)
{
    /* Allocate a new device handle */
    usbh_device_t *dev = device_handle_alloc();
    if (!dev)
    {
        USB_LOGE("Host handle: %p Failed to allocate device handle", h);
        return;
    }

    /* Allocate a bus address for the new device */
    dev->address = bus_address_alloc(h);
    if (dev->address == 0)
    {
        USB_LOGE("Host handle: %p Failed to allocate bus address for device %p", h, dev);
        return;
    }

    /* Initialize the device handle */
    dev->host = h;
    dev->hub_addr = hub_addr;
    dev->hub_port = hub_port;

    /* Append the new device to the device list */
    usbh_list_append((void **)&h->device_list, dev);

    USB_LOGI("Host handle: %p Inserted device %p at hub_addr: %d, hub_port: %d", h, dev, hub_addr, hub_port);
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

    /* Unbind all class drivers from the target device */
    unbind_class_driver(h, target);

    /* Close all endpoints of the target device */
    usbh_list_remove((void **)&target->host->endpoint_list[USB_ENDP_TYPE_CTRL], &target->ctrl_endp);
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
    h->bus_address_bitmap[target->address >> 5] &= ~(1 << (target->address & 0x1F));

    /* Remove the target device from the device list */
    usbh_list_remove((void **)&h->device_list, target);

    memset(target, 0, sizeof(usbh_device_t));

    USB_LOGI("Host handle: %p Removed device %p from hub_addr: %d, hub_port: %d", h, target, hub_addr, hub_port);
}

void usbh_device_reset(usbh_device_t *dev)
{
    if (!dev || dev->address == 0) return;

    dev->enum_stage = ENUM_STAGE_RESET;
    if (dev->hub_addr == 0)
    {
        dev->host->root_set_feature(dev->host, USBH_PORT_FEATURE_RESET);
    }
    else if (dev->host->port_set_feature)
    {
        dev->host->port_set_feature(dev, USBH_PORT_FEATURE_RESET);
    }
    else
    {
        USB_LOGW("Host handle: %p Does not support hub port operations", dev->host);
    }
}

void usbh_device_suspend(usbh_device_t *dev, usbh_drv_dev_wakeup_cb wakeup_cb)
{
    if (!dev || dev->address == 0) return;

    dev->suspended = true;
    dev->wakeup_cb = wakeup_cb;
    dev->suspend_stage = dev->remote_wakeup ? SUSPEND_STAGE_ENABLE_REMOTE_WAKEUP : SUSPEND_STAGE_ENTRY_SUSPEND;

    /* Clear the busy status of all endpoints */
    dev->ctrl_endp.xfer_ctx.is_busy = false;
    for (size_t i = 0; i < USB_ARRAY_SIZE(dev->data_endp); i++)
    {
        for (size_t j = 0; j < USB_ARRAY_SIZE(dev->data_endp[i]); j++)
        {
            if (dev->data_endp[i][j])
            {
                dev->data_endp[i][j]->xfer_ctx.is_busy = false;
            }
        }
    }
}

void usbh_device_resume(usbh_handle_t *h, uint8_t hub_addr, uint8_t hub_port)
{
    if (!h) return;

    usbh_device_t *dev = h->device_list;
    while (dev)
    {
        if (dev->hub_addr == hub_addr && dev->hub_port == hub_port)
        {
            if (!dev->suspended) return;
            dev->suspend_stage = dev->remote_wakeup ? SUSPEND_STAGE_CLEAR_REMOTE_WAKEUP : SUSPEND_STAGE_WAKEUP;
            return;
        }
        dev = dev->next;
    }
}

bool usbh_register_driver(usbh_handle_t *h, const usbh_drv_info_t *drv_info)
{
    if (!h || !drv_info || !drv_info->bind || !drv_info->unbind) return false;
    for (size_t i = 0; i < USB_ARRAY_SIZE(h->drv_info_list); i++)
    {
        if (h->drv_info_list[i] == NULL)
        {
            h->drv_info_list[i] = drv_info;
            return true;
        }
    }
    return false;
}

bool usbh_unregister_driver(usbh_handle_t *h, const usbh_drv_info_t *drv_info)
{
    if (!h || !drv_info) return false;
    for (size_t i = 0; i < USB_ARRAY_SIZE(h->drv_info_list); i++)
    {
        if (h->drv_info_list[i] == drv_info)
        {
            h->drv_info_list[i] = NULL;
            return true;
        }
    }
    return false;
}

bool usbh_endp_open(usbh_device_t *dev, void *handle, const usb_desc_endpoint_t *ep_desc, usbh_data_xfer_cb cb)
{
    uint8_t endp_dir = USB_ENDP_DIR(ep_desc->bEndpointAddress);
    uint8_t endp_num = USB_ENDP_NUM(ep_desc->bEndpointAddress);

    if (endp_num == 0)
    {
        USB_LOGW("Device handle: %p Cannot open endpoint 0", dev);
        return false;
    }

    usbh_endpoint_t *ep = endpoint_handle_alloc();
    if (!ep)
    {
        USB_LOGW("Device handle: %p Failed to allocate endpoint handle", dev);
        return false;
    }

    if (dev->data_endp[endp_dir ? USB_DIR_IN : USB_DIR_OUT][endp_num - 1])
    {
        memset(ep, 0, sizeof(usbh_endpoint_t));
        USB_LOGW("Device handle: %p Endpoint %d already opened", dev, endp_num);
        return false;
    }

    /* Initialize the endpoint information */
    ep->addr = ep_desc->bEndpointAddress;
    ep->type = USB_ENDP_GET_TYPE(ep_desc->bmAttributes);
    ep->mps = USB_ENDP_GET_MPS(ep_desc->wMaxPacketSize);
    ep->interval = endpoint_interval_calc(dev->host->speed, dev->speed, ep->type, ep_desc->bInterval);
    ep->ping_en = dev->speed == USB_SPEED_HIGH && ep->type == USB_ENDP_TYPE_BULK;
    ep->handle = handle;
    ep->xfer_cb = cb;

    /* Initialize the transfer unit information */
    usbh_xfer_unit_t *xfer_unit = &ep->xfer_unit;
    xfer_unit->dev_addr = dev->address;
    xfer_unit->token = endp_dir ? USBH_PID_IN : USBH_PID_OUT;
    xfer_unit->endp_num = endp_num;
    xfer_unit->iso = ep->type == USB_ENDP_TYPE_ISOC;
    xfer_unit->endpoint = ep;

    if (dev->speed != dev->host->speed)
    {
        /* Initialize split transaction information for high-speed hub */
        if (dev->host->speed == USB_SPEED_HIGH)
        {
            usbh_split_data_t *split = &xfer_unit->split_data;
            split->hub_addr = dev->ctrl_endp.xfer_unit.split_data.hub_addr;
            split->port = dev->ctrl_endp.xfer_unit.split_data.port;
            split->s = dev->speed == USB_SPEED_LOW ? 1 : 0;
            split->et = ep->type;
        }
        /* Enable pre-split for full-speed hub */
        else
        {
            xfer_unit->pre = true;
        }
    }

    /* Set the endpoint handle in the device's data endpoint array */
    dev->data_endp[endp_dir ? USB_DIR_IN : USB_DIR_OUT][endp_num - 1] = ep;

    /* Append the endpoint to the host's endpoint list based on its type */
    usbh_list_append((void **)&dev->host->endpoint_list[USB_MIN(ep->type, 3)], ep);

    return true;
}

bool usbh_endp_close(usbh_device_t *dev, usb_endp_t endp)
{
    uint8_t endp_dir = USB_ENDP_DIR(endp);
    uint8_t endp_num = USB_ENDP_NUM(endp);

    if (endp_num && endp_num < USB_MAX_ENDP_NUM)
    {
        usbh_endpoint_t *ep;

        ep = dev->data_endp[endp_dir ? USB_DIR_IN : USB_DIR_OUT][endp_num - 1];
        dev->data_endp[endp_dir ? USB_DIR_IN : USB_DIR_OUT][endp_num - 1] = NULL;
        if (ep)
        {
            usbh_list_remove((void **)&dev->host->endpoint_list[USB_MIN(ep->type, 3)], ep);
            ep->xfer_ctx.is_busy = false;
            ep->is_used = false;
            ep->xfer_cb = NULL;
            ep->handle = NULL;
        }
    }

    return true;
}

bool usbh_endp_read(usbh_device_t *dev, usb_endp_t endp, void *buf, size_t len, uint32_t timeout)
{
    uint8_t endp_num = USB_ENDP_NUM(endp);
    if (endp_num == 0 || endp_num >= USB_MAX_ENDP_NUM) return false;

    usbh_endpoint_t *ep = dev->data_endp[USB_DIR_IN][endp_num - 1];
    if (!ep) return false;

    usbh_xfer_ctx_t *xfer_ctx = &ep->xfer_ctx;
    if (xfer_ctx->is_busy) return false;

    xfer_ctx->is_busy = true;
    xfer_ctx->buf = buf;
    xfer_ctx->length = len;
    xfer_ctx->timeout = timeout;
    xfer_ctx->offset = 0;
    xfer_ctx->timestamp = usbh_drv_get_timestamp(dev->host);
    return true;
}

bool usbh_endp_write(usbh_device_t *dev, usb_endp_t endp, const void *buf, size_t len, uint32_t timeout)
{
    uint8_t endp_num = USB_ENDP_NUM(endp);
    if (endp_num == 0 || endp_num >= USB_MAX_ENDP_NUM) return false;

    usbh_endpoint_t *ep = dev->data_endp[USB_DIR_OUT][endp_num - 1];
    if (!ep) return false;

    usbh_xfer_ctx_t *xfer_ctx = &ep->xfer_ctx;
    if (xfer_ctx->is_busy) return false;

    xfer_ctx->is_busy = true;
    xfer_ctx->buf = (void *)buf;
    xfer_ctx->length = len;
    xfer_ctx->timeout = timeout;
    xfer_ctx->offset = 0;
    xfer_ctx->timestamp = usbh_drv_get_timestamp(dev->host);
    return true;
}

bool usbh_ctrl_xfer(usbh_device_t *dev, void *handle, const usb_setup_t *setup, void *buf, uint32_t timeout,
                    usbh_ctrl_xfer_cb cb)
{
    usbh_xfer_ctx_t *xfer_ctx = &dev->ctrl_endp.xfer_ctx;
    if (!xfer_ctx->is_busy)
    {
        dev->ctrl_endp.handle = (void *)handle;
        dev->ctrl_endp.xfer_cb = (void *)cb;
        memset(xfer_ctx, 0, sizeof(usbh_xfer_ctx_t));
        xfer_ctx->is_busy = true;
        xfer_ctx->buf = buf;
        xfer_ctx->length = setup->wLength;
        xfer_ctx->timeout = timeout;
        xfer_ctx->offset = 0;
        xfer_ctx->setup = setup;
        xfer_ctx->timestamp = usbh_drv_get_timestamp(dev->host);
        return true;
    }
    return false;
}

bool usbh_is_ctrl_xfering(usbh_device_t *dev)
{
    return dev->ctrl_endp.xfer_ctx.is_busy;
}

bool usbh_is_endp_xfering(usbh_device_t *dev, usb_endp_t endp)
{
    uint8_t endp_num = USB_ENDP_NUM(endp);
    if (endp_num == 0 || endp_num >= USB_MAX_ENDP_NUM) return false;

    usbh_endpoint_t *ep = dev->data_endp[USB_ENDP_DIR(endp) ? USB_DIR_IN : USB_DIR_OUT][endp_num - 1];
    if (!ep) return false;

    return ep->xfer_ctx.is_busy;
}

uint32_t usbh_drv_get_timestamp(usbh_handle_t *h)
{
    return h->speed == USB_SPEED_HIGH ? h->tick >> 3 : h->tick;
}

#endif // USB_HOST_DRIVER_EN
