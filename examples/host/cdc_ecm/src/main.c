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
#include "button.h"
#include "led.h"

#include "usb_driver.h"

/* @define */
#ifndef USBH_INDEX
#define USBH_INDEX 0
#endif

#if USBH_INDEX >= USB_COUNT
#error "Not supported: USBH_INDEX exceeds USB_COUNT"
#endif

/* @define */
#define NET_ETH_MIN_LEN       60
#define NET_ETH_TYPE_IPV4     0x0800
#define NET_ETH_TYPE_ARP      0x0806
#define NET_ARP_HW_ETHERNET   1
#define NET_ARP_OP_REQUEST    1
#define NET_ARP_OP_REPLY      2
#define NET_IPV4_VERSION      4
#define NET_IPV4_TTL          64
#define NET_IPV4_PROTO_ICMP   1
#define NET_IPV4_FLAG_MF      1
#define NET_ICMP_ECHO_REQUEST 8
#define NET_ICMP_ECHO_REPLY   0

/* @enum */
typedef enum
{
    ETH_COMM_STAGE_IDLE,
    ETH_COMM_STAGE_REQUEST,
    ETH_COMM_STAGE_REPLY,
    ETH_COMM_STAGE_DONE,
} eth_comm_stage_t;

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
static usbh_handle_t *usbh_handle;
static eth_comm_stage_t arp_stage;
static eth_comm_stage_t ping_stage;
static uint16_t ping_seq;
static uint8_t target_mac_addr[6];
static const uint8_t *local_mac_addr;
static const uint8_t local_ip_addr[4] = {192, 168, 1, 2};
static const uint8_t target_ip_addr[4] = {192, 168, 1, 1};
static __attribute__((aligned(4))) uint8_t read_buf[2048];
static __attribute__((aligned(4))) uint8_t write_buf[2048];

static uint16_t checksum(uint8_t *data, size_t len)
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

static void icmp_protocol_process(const void *buf, size_t length)
{
    if (length < sizeof(icmp_echo_header_t)) return;

    const icmp_echo_header_t *recv_icmp = (const icmp_echo_header_t *)buf;
    if (recv_icmp->type != NET_ICMP_ECHO_REPLY || recv_icmp->code != 0) return;
    if (checksum((uint8_t *)buf, length) != 0) return;

    printf("Received ICMP echo reply: seq=%u\n", recv_icmp->seq);
    ping_stage = ETH_COMM_STAGE_DONE;
}

static void ipv4_protocol_process(const void *buf, size_t length)
{
    if (length < sizeof(ipv4_header_t)) return;

    const ipv4_header_t *recv_ip = (const ipv4_header_t *)buf;
    if (recv_ip->version != NET_IPV4_VERSION) return;

    size_t ip_hdr_len = (size_t)recv_ip->ihl * 4;
    if (ip_hdr_len < sizeof(ipv4_header_t) || ip_hdr_len > length) return;

    size_t ip_total_len = recv_ip->total_len;
    if (ip_total_len < ip_hdr_len || ip_total_len > length) return;
    if (recv_ip->fragment_offset != 0 || (recv_ip->flags & NET_IPV4_FLAG_MF) != 0) return;
    if (memcmp(recv_ip->dst_ip, local_ip_addr, sizeof(local_ip_addr)) != 0) return;
    if (checksum((uint8_t *)buf, ip_hdr_len) != 0) return;
    if (recv_ip->protocol != NET_IPV4_PROTO_ICMP) return;
    icmp_protocol_process(buf + ip_hdr_len, ip_total_len - ip_hdr_len);
}

static void arp_protocol_process(const void *buf, size_t length)
{
    size_t header_len = sizeof(arp_header_t);
    if (length < header_len) return;

    const arp_header_t *recv_arp = (const arp_header_t *)buf;
    if (recv_arp->hw_type != NET_ARP_HW_ETHERNET || recv_arp->proto_type != NET_ETH_TYPE_IPV4) return;
    if (recv_arp->hw_size != 6 || recv_arp->proto_size != 4 || recv_arp->opcode != NET_ARP_OP_REPLY) return;
    if (memcmp(recv_arp->target_ip, local_ip_addr, sizeof(local_ip_addr)) != 0) return;

    memcpy(target_mac_addr, recv_arp->sender_mac, sizeof(target_mac_addr));
    printf("Received ARP reply from target MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", target_mac_addr[0],
           target_mac_addr[1], target_mac_addr[2], target_mac_addr[3], target_mac_addr[4], target_mac_addr[5]);
    arp_stage = ETH_COMM_STAGE_DONE;
}

static void ecm_open_cb(ecmh_handle_t *ecm, bool rst)
{
    printf("CDC-ECM0: Opened: %s\n", rst ? "Success" : "Failed");
}

static void ecm_connect_change_cb(ecmh_handle_t *ecm, bool connected)
{
    ping_seq = 0;
    arp_stage = connected ? ETH_COMM_STAGE_REQUEST : ETH_COMM_STAGE_IDLE;
    printf("CDC-ECM0: Connection status changed: %s\n", connected ? "Connected" : "Disconnected");
}

static void ecm_speed_change_cb(ecmh_handle_t *ecm, uint32_t upload_speed, uint32_t download_speed)
{
    printf("CDC-ECM0: Speed changed: Upload %u bps, Download %u bps\n", upload_speed, download_speed);
}

static void ecm_read_cb(ecmh_handle_t *ecm, const void *buf, size_t length)
{
    size_t header_len = sizeof(ethernet_header_t);
    if (length < header_len) return;

    const ethernet_header_t *recv_eth = (const ethernet_header_t *)buf;
    if (memcmp(recv_eth->dst_mac, local_mac_addr, 6) != 0) return;

    switch (recv_eth->eth_type)
    {
    case NET_ETH_TYPE_ARP:
        arp_protocol_process(buf + header_len, length - header_len);
        break;

    case NET_ETH_TYPE_IPV4:
        ipv4_protocol_process(buf + header_len, length - header_len);
        break;
    }
}

static void ecm_write_cb(ecmh_handle_t *ecm, const void *buf, size_t length)
{
}

int main(void)
{
    board_init();
    button_init();
    led_init();

    usbh_handle = (usbh_handle_t *)board_usb_init(USBH_INDEX, USB_MODE_HOST);
    assert(usbh_handle != NULL);

    assert(usbh_drv_open(usbh_handle));
    assert(hubh_drv_open(usbh_handle));
    assert(cdch_ecm_drv_open(usbh_handle));

    while (1)
    {
        usbh_drv_task(usbh_handle);
        hubh_drv_task();
        cdch_ecm_drv_task();

        /* Check for changes in the number of connected CDC-ECM devices */
        static size_t last_ecm_count = 0;
        size_t curr_ecm_count = cdch_ecm_get_count();
        if (curr_ecm_count != last_ecm_count)
        {
            printf("CDC-ECM device count changed: %u -> %u\n", last_ecm_count, curr_ecm_count);
            last_ecm_count = curr_ecm_count;
        }

        /* Get the handle for the first CDC-ECM device */
        ecmh_handle_t *ecm = cdch_ecm_get_handle(0);

        /* Update LED status based on CDC-ECM open state */
        led_write(ecm ? (cdch_ecm_is_open(ecm) ? LED_1 : 0) | LED_0 : 0);

        /* Skip the rest of the loop if no CDC-ECM device is connected */
        if (!ecm) continue;

        /* Get the local MAC address of the CDC-ECM device */
        local_mac_addr = cdch_ecm_get_mac_addr(ecm);

        /* Skip the rest of the loop if the local MAC address is not available */
        if (!local_mac_addr) continue;

        /* Send an ARP request to get the target MAC address if the request has been sent */
        if (arp_stage == ETH_COMM_STAGE_REQUEST)
        {
            static const uint8_t broadcast_addr[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
            arp_header_t *req_arp = (arp_header_t *)(write_buf + sizeof(ethernet_header_t));
            req_arp->hw_type = NET_ARP_HW_ETHERNET;
            req_arp->proto_type = NET_ETH_TYPE_IPV4;
            req_arp->hw_size = 6;
            req_arp->proto_size = 4;
            req_arp->opcode = NET_ARP_OP_REQUEST;
            memcpy(req_arp->sender_mac, local_mac_addr, sizeof(req_arp->sender_mac));
            memcpy(req_arp->sender_ip, local_ip_addr, sizeof(req_arp->sender_ip));
            memcpy(req_arp->target_mac, broadcast_addr, sizeof(req_arp->target_mac));
            memcpy(req_arp->target_ip, target_ip_addr, sizeof(req_arp->target_ip));

            ethernet_header_t *req_eth = (ethernet_header_t *)write_buf;
            memcpy(req_eth->dst_mac, broadcast_addr, sizeof(req_eth->dst_mac));
            memcpy(req_eth->src_mac, local_mac_addr, sizeof(req_eth->src_mac));
            req_eth->eth_type = NET_ETH_TYPE_ARP;

            if (cdch_ecm_write(ecm, write_buf, sizeof(ethernet_header_t) + sizeof(arp_header_t), 0, ecm_write_cb))
            {
                arp_stage = ETH_COMM_STAGE_REPLY;
            }
        }

        /* Handle ICMP echo request (ping) */
        else if (ping_stage == ETH_COMM_STAGE_REQUEST)
        {
            static const uint8_t payload[] = "PING";
            size_t payload_len = sizeof(payload) - 1;
            size_t icmp_len = sizeof(icmp_echo_header_t) + payload_len;
            size_t ip_len = sizeof(ipv4_header_t) + icmp_len;

            icmp_echo_header_t *req_icmp =
                (icmp_echo_header_t *)(write_buf + sizeof(ethernet_header_t) + sizeof(ipv4_header_t));
            req_icmp->type = NET_ICMP_ECHO_REQUEST;
            req_icmp->code = 0;
            req_icmp->id = 0x0001;
            req_icmp->seq = ++ping_seq;
            memcpy((uint8_t *)req_icmp + sizeof(icmp_echo_header_t), payload, payload_len);
            req_icmp->checksum = 0;
            req_icmp->checksum = checksum((uint8_t *)req_icmp, icmp_len);

            ipv4_header_t *req_ip = (ipv4_header_t *)(write_buf + sizeof(ethernet_header_t));
            req_ip->version = NET_IPV4_VERSION;
            req_ip->ihl = sizeof(ipv4_header_t) / 4;
            req_ip->tos = 0;
            req_ip->total_len = ip_len;
            req_ip->id = 0;
            req_ip->flags = 0;
            req_ip->fragment_offset = 0;
            req_ip->ttl = NET_IPV4_TTL;
            req_ip->protocol = NET_IPV4_PROTO_ICMP;
            memcpy(req_ip->src_ip, local_ip_addr, sizeof(req_ip->src_ip));
            memcpy(req_ip->dst_ip, target_ip_addr, sizeof(req_ip->dst_ip));
            req_ip->checksum = 0;
            req_ip->checksum = checksum((uint8_t *)req_ip, sizeof(ipv4_header_t));

            ethernet_header_t *req_eth = (ethernet_header_t *)write_buf;
            memcpy(req_eth->dst_mac, target_mac_addr, sizeof(req_eth->dst_mac));
            memcpy(req_eth->src_mac, local_mac_addr, sizeof(req_eth->src_mac));
            req_eth->eth_type = NET_ETH_TYPE_IPV4;

            if (cdch_ecm_write(ecm, write_buf, sizeof(ethernet_header_t) + ip_len, 0, ecm_write_cb))
            {
                ping_stage = ETH_COMM_STAGE_REPLY;
            }
        }

        /* Read data from the CDC-ECM device */
        cdch_ecm_read(ecm, read_buf, sizeof(read_buf), 0, ecm_read_cb);

        /* Read the current button status and detect changes */
        static uint8_t last_btn_status = 0;
        uint8_t curr_status = button_read();
        uint8_t changed = curr_status ^ last_btn_status;

        /* Skip the rest of the loop if no button status change is detected */
        if (!changed) continue;
        last_btn_status = curr_status;

        /* Toggle CDC-ECM open state on button 0 press */
        if (changed & curr_status & BUTTON_0)
        {
            if (cdch_ecm_is_open(ecm))
            {
                cdch_ecm_close(ecm);
            }
            else
            {
                cdch_ecm_open(ecm, ecm_open_cb, ecm_connect_change_cb, ecm_speed_change_cb);
            }
        }

        /* Send ping request on button 1 press */
        if (changed & curr_status & BUTTON_1)
        {
            if (arp_stage == ETH_COMM_STAGE_DONE)
            {
                ping_stage = ETH_COMM_STAGE_REQUEST;
            }
        }
    }

    return 0;
}
