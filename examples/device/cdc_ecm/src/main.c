/********************************** (C) COPYRIGHT *******************************
* File Name          : main.c
* Author             : WCH
* Version            : V1.0.0
* Date               : 2026/10/20
* Description        : Main program for CDC-ECM example
*********************************************************************************
* Copyright (c) 2026 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

/* @include*/
#include <assert.h>
#include <string.h>

#include "board.h"

#include "usb_driver.h"
#include "descriptor.h"

/* @define */
#ifndef USBD_INDEX
#define USBD_INDEX 0
#endif

#if USBD_INDEX >= USB_COUNT
#error "Not supported: USBD_INDEX exceeds USB_COUNT"
#endif

/* @define */
#define NET_ETH_MIN_LEN       60
#define NET_ETH_TYPE_IPV4     0x0800
#define NET_ETH_TYPE_ARP      0x0806
#define NET_ARP_HW_ETHERNET   1
#define NET_ARP_OP_REQUEST    1
#define NET_ARP_OP_REPLY      2
#define NET_IPV4_VERSION      4
#define NET_IPV4_PROTO_ICMP   1
#define NET_IPV4_FLAG_MF      1
#define NET_ICMP_ECHO_REQUEST 8
#define NET_ICMP_ECHO_REPLY   0

/* @struct */
typedef struct __attribute__((packed, scalar_storage_order("big-endian")))
{
    uint8_t dst_mac[6];
    uint8_t src_mac[6];
    uint16_t eth_type;
} ethernet_header_t;

typedef struct __attribute__((packed, scalar_storage_order("big-endian")))
{
    uint16_t hw_type;
    uint16_t proto_type;
    uint8_t hw_size;
    uint8_t proto_size;
    uint16_t opcode;
    uint8_t sender_mac[6];
    uint8_t sender_ip[4];
    uint8_t target_mac[6];
    uint8_t target_ip[4];
} arp_header_t;

/* Bit-fields are allocated MSB-first because of scalar_storage_order */
typedef struct __attribute__((packed, scalar_storage_order("big-endian")))
{
    uint8_t version : 4;
    uint8_t ihl : 4;
    uint8_t tos;
    uint16_t total_len;
    uint16_t id;
    uint16_t flags : 3;
    uint16_t fragment_offset : 13;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t checksum;
    uint8_t src_ip[4];
    uint8_t dst_ip[4];
} ipv4_header_t;

typedef struct __attribute__((packed, scalar_storage_order("big-endian")))
{
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint16_t id;
    uint16_t seq;
} icmp_echo_header_t;

/* @global */
static volatile bool enum_completed;
static bool sync_connect_status;
static bool sync_speed_status;
static usbd_handle_t *usbd_handle;
static cdcd_ecm_handle_t ecm_handle;
static volatile size_t received_len;
static const uint8_t *mac_addr_ptr;
static const uint8_t ip_addr[4] = {192, 168, 1, 1};
static __attribute__((aligned(4))) uint8_t read_buf[2048];
static __attribute__((aligned(4))) uint8_t write_buf[2048];

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
    else if (index == 3 || index == 4)
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
            "CDC-ECM Example",
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

static void cdc_ecm_set_interface_cb(cdcd_ecm_handle_t *cdcd_ecm, uint16_t alt_setting)
{
    if (alt_setting == 0)
    {
        /* Close the data endpoint when the alternate setting is 0 */
        cdcd_ecm_drv_ctrl_data_endpoint(cdcd_ecm, false);
    }
    else
    {
        /* Open the data endpoint and start reading data */
        cdcd_ecm_drv_ctrl_data_endpoint(cdcd_ecm, true);
        cdcd_ecm_drv_read(cdcd_ecm, read_buf, sizeof(read_buf));
    }
}

static void cdc_ecm_read_cb(cdcd_ecm_handle_t *cdcd_ecm, void *buf, size_t len)
{
    received_len = len;
}

static void reset_event_cb(usbd_handle_t *h, usbd_event_ctx_t *ctx)
{
    enum_completed = false;
    sync_connect_status = false;
    sync_speed_status = false;
    received_len = 0;
}

static void enum_completed_event_cb(usbd_handle_t *h, usbd_event_ctx_t *ctx)
{
    enum_completed = true;

    /* Initialize CDC-ECM device handle */
    memset(&ecm_handle, 0, sizeof(ecm_handle));
    ecm_handle.usbd_handle = h;
    ecm_handle.ctrl_itf_num = 0;
    ecm_handle.data_itf_num = 1;
    ecm_handle.read_comp_cb = cdc_ecm_read_cb;
    ecm_handle.set_interface_cb = cdc_ecm_set_interface_cb;
    if (ctx->enum_completed.link_speed == USB_SPEED_HIGH)
    {
        ecm_handle.notify_ep = (const usb_desc_endpoint_t *)&config_desc_hs[41];
        ecm_handle.in_ep = (const usb_desc_endpoint_t *)&config_desc_hs[66];
        ecm_handle.out_ep = (const usb_desc_endpoint_t *)&config_desc_hs[73];
    }
    else
    {
        ecm_handle.notify_ep = (const usb_desc_endpoint_t *)&config_desc_fs[41];
        ecm_handle.in_ep = (const usb_desc_endpoint_t *)&config_desc_fs[66];
        ecm_handle.out_ep = (const usb_desc_endpoint_t *)&config_desc_fs[73];
    }
    assert(cdcd_ecm_drv_open(&ecm_handle));
}

static uint16_t checksum(const uint8_t *data, size_t len)
{
    uint32_t sum = 0;
    while (len > 1)
    {
        sum += (uint16_t)(((uint16_t)data[0] << 8) | data[1]);
        data += 2;
        len -= 2;
    }
    if (len)
    {
        sum += (uint16_t)((uint16_t)data[0] << 8);
    }
    while (sum >> 16)
    {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return (uint16_t)~sum;
}

static bool icmp_protocol_process(const uint8_t *frame, size_t len, uint8_t *reply, size_t *reply_size)
{
    if (len < sizeof(icmp_echo_header_t)) return false;

    const icmp_echo_header_t *recv_icmp = (const icmp_echo_header_t *)frame;
    if (recv_icmp->type != NET_ICMP_ECHO_REQUEST || recv_icmp->code != 0) return false;
    if (checksum(frame, len) != 0) return false;

    icmp_echo_header_t *reply_icmp = (icmp_echo_header_t *)reply;
    memcpy(reply_icmp, recv_icmp, len);
    reply_icmp->type = NET_ICMP_ECHO_REPLY;
    reply_icmp->code = 0;
    reply_icmp->checksum = 0;
    reply_icmp->checksum = checksum((const uint8_t *)reply_icmp, len);

    *reply_size += len;
    return true;
}

static bool ipv4_protocol_process(const uint8_t *frame, size_t len, uint8_t *reply, size_t *reply_size)
{
    if (len < sizeof(ipv4_header_t)) return false;

    const ipv4_header_t *recv_ip = (const ipv4_header_t *)frame;
    if (recv_ip->version != NET_IPV4_VERSION) return false;

    size_t ip_hdr_len = (size_t)recv_ip->ihl * 4;
    if (ip_hdr_len < sizeof(ipv4_header_t) || ip_hdr_len > len) return false;

    size_t ip_total_len = recv_ip->total_len;
    if (ip_total_len < ip_hdr_len || ip_total_len > len) return false;
    if (recv_ip->fragment_offset != 0 || (recv_ip->flags & NET_IPV4_FLAG_MF) != 0) return false;
    if (memcmp(recv_ip->dst_ip, ip_addr, sizeof(ip_addr)) != 0) return false;
    if (checksum(frame, ip_hdr_len) != 0) return false;
    if (recv_ip->protocol != NET_IPV4_PROTO_ICMP) return false;
    if (!icmp_protocol_process(frame + ip_hdr_len, ip_total_len - ip_hdr_len, reply + ip_hdr_len, reply_size))
        return false;

    ipv4_header_t *reply_ip = (ipv4_header_t *)reply;
    memcpy(reply_ip, recv_ip, ip_hdr_len);
    memcpy(reply_ip->src_ip, recv_ip->dst_ip, sizeof(reply_ip->src_ip));
    memcpy(reply_ip->dst_ip, recv_ip->src_ip, sizeof(reply_ip->dst_ip));
    reply_ip->ttl = 64;
    reply_ip->checksum = 0;
    reply_ip->checksum = checksum(reply, ip_hdr_len);

    *reply_size += ip_hdr_len;
    return true;
}

static bool arp_protocol_process(const uint8_t *frame, size_t len, uint8_t *reply, size_t *reply_size)
{
    size_t header_len = sizeof(arp_header_t);
    if (len < header_len) return false;

    const arp_header_t *recv_arp = (const arp_header_t *)frame;
    if (recv_arp->hw_type != NET_ARP_HW_ETHERNET || recv_arp->proto_type != NET_ETH_TYPE_IPV4) return false;
    if (recv_arp->hw_size != 6 || recv_arp->proto_size != 4 || recv_arp->opcode != NET_ARP_OP_REQUEST) return false;
    if (memcmp(recv_arp->target_ip, ip_addr, sizeof(ip_addr)) != 0) return false;

    arp_header_t *reply_arp = (arp_header_t *)reply;
    reply_arp->hw_type = NET_ARP_HW_ETHERNET;
    reply_arp->proto_type = NET_ETH_TYPE_IPV4;
    reply_arp->hw_size = 6;
    reply_arp->proto_size = 4;
    reply_arp->opcode = NET_ARP_OP_REPLY;
    memcpy(reply_arp->sender_mac, mac_addr_ptr, sizeof(reply_arp->sender_mac));
    memcpy(reply_arp->sender_ip, ip_addr, sizeof(reply_arp->sender_ip));
    memcpy(reply_arp->target_mac, recv_arp->sender_mac, sizeof(reply_arp->target_mac));
    memcpy(reply_arp->target_ip, recv_arp->sender_ip, sizeof(reply_arp->target_ip));

    *reply_size += header_len;
    return true;
}

static bool ethernet_packet_process(const uint8_t *frame, size_t len, uint8_t *reply, size_t *reply_size)
{
    size_t header_len = sizeof(ethernet_header_t);
    if (len < header_len) return false;

    const ethernet_header_t *recv_eth = (const ethernet_header_t *)frame;
    static const uint8_t broadcast_addr[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    if (memcmp(recv_eth->dst_mac, mac_addr_ptr, 6) != 0 && memcmp(recv_eth->dst_mac, broadcast_addr, 6) != 0)
        return false;

    switch (recv_eth->eth_type)
    {
    case NET_ETH_TYPE_ARP:
        if (!arp_protocol_process(frame + header_len, len - header_len, reply + header_len, reply_size)) return false;
        break;

    case NET_ETH_TYPE_IPV4:
        if (!ipv4_protocol_process(frame + header_len, len - header_len, reply + header_len, reply_size)) return false;
        break;

    default:
        return false;
    }

    ethernet_header_t *reply_eth = (ethernet_header_t *)reply;
    memcpy(reply_eth->dst_mac, recv_eth->src_mac, sizeof(reply_eth->dst_mac));
    memcpy(reply_eth->src_mac, mac_addr_ptr, sizeof(reply_eth->src_mac));
    reply_eth->eth_type = recv_eth->eth_type;

    *reply_size = USB_MAX(*reply_size + sizeof(ethernet_header_t), NET_ETH_MIN_LEN);
    return true;
}

static void cdc_ecm_application_handle(void)
{
    /* Synchronize connection status */
    if (!sync_connect_status && cdcd_ecm_drv_connect_change_notify(&ecm_handle, true))
    {
        sync_connect_status = true;
    }

    /* Synchronize speed status */
    if (!sync_speed_status && cdcd_ecm_drv_speed_change_notify(&ecm_handle, 100000000, 100000000))
    {
        sync_speed_status = true;
    }

    /* Process received data and build network reply */
    if (received_len)
    {
        size_t reply_len = 0;
        memset(write_buf, 0, sizeof(write_buf));
        if (ethernet_packet_process(read_buf, received_len, write_buf, &reply_len))
        {
            cdcd_ecm_drv_write(&ecm_handle, write_buf, reply_len);
        }

        received_len = 0;
        cdcd_ecm_drv_read(&ecm_handle, read_buf, sizeof(read_buf));
    }
}

int main(void)
{
    board_init();

    mac_addr_ptr = board_get_mac_address();

    usbd_handle = board_usb_init(USBD_INDEX, USB_MODE_DEVICE);
    assert(usbd_handle != NULL);

    assert(usbd_register_event_callback(usbd_handle, USBD_EVENT_RESET, reset_event_cb));
    assert(usbd_register_event_callback(usbd_handle, USBD_EVENT_ENUM_COMPLETED, enum_completed_event_cb));
    assert(usbd_drv_open(usbd_handle, USB_SPEED_HIGH, false, get_stand_desc_cb));

    while (1)
    {
        if (enum_completed)
        {
            cdc_ecm_application_handle();
        }
    }

    return 0;
}
