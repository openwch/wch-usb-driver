/********************************** (C) COPYRIGHT *******************************
* File Name          : msc.h
* Author             : WCH
* Version            : V1.0.0
* Date               : 2026/10/20
* Description        : USB MSC class definition header file
*********************************************************************************
* Copyright (c) 2026 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

#ifndef MSC_H
#define MSC_H

/* @include */
#include "usb_define.h"

#ifdef __cplusplus
extern "C" {
#endif

/* @define */
#define MSC_CBW_SIGNATURE            0x43425355
#define MSC_CSW_SIGNATURE            0x53425355

#define SCSI_SENSE_DATA_FIXED_FORMAT 0x70

/* @enum */
typedef enum
{
    MSC_SUBCLASS_RBC = 0x01,
    MSC_SUBCLASS_MMC = 0x02,
    MSC_SUBCLASS_QIC = 0x03,
    MSC_SUBCLASS_UFI = 0x04,
    MSC_SUBCLASS_SFF = 0x05,
    MSC_SUBCLASS_SCSI = 0x06,
} msc_subclass_t;

typedef enum
{
    MSC_PROTOCOL_CBI = 0x00,
    MSC_PROTOCOL_CBI_NO_INTERRUPT = 0x01,
    MSC_PROTOCOL_OBSOLETE = 0x02,
    MSC_PROTOCOL_BULK_ONLY = 0x50,
    MSC_PROTOCOL_UAS = 0x62,
} msc_protocol_t;

typedef enum
{
    MSC_REQ_ADSC = 0x00,
    MSC_REQ_GET_REQUESTS = 0xFC,
    MSC_REQ_PUT_REQUESTS = 0xFD,
    MSC_REQ_GET_MAX_LUN = 0xFE,
    MSC_REQ_BULK_ONLY_RESET = 0xFF,
} msc_request_t;

typedef enum
{
    MSC_BULK_ONLY_CSW_STATUS_PASSED = 0x00,
    MSC_BULK_ONLY_CSW_STATUS_FAILED = 0x01,
    MSC_BULK_ONLY_CSW_STATUS_PHASE_ERROR = 0x02,
} msc_bulk_only_csw_status_t;

typedef enum
{
    SCSI_OPCODE_TEST_UNIT_READY = 0x00,
    SCSI_OPCODE_REZERO_UNIT = 0x01,
    SCSI_OPCODE_REQUEST_SENSE = 0x03,
    SCSI_OPCODE_FORMAT_UNIT = 0x04,
    SCSI_OPCODE_REASSIGN_BLOCKS = 0x07,
    SCSI_OPCODE_READ_6 = 0x08,
    SCSI_OPCODE_WRITE_6 = 0x0A,
    SCSI_OPCODE_SEEK_6 = 0x0B,
    SCSI_OPCODE_INQUIRY = 0x12,
    SCSI_OPCODE_MODE_SELECT_6 = 0x15,
    SCSI_OPCODE_MODE_RESERVE_6 = 0x16,
    SCSI_OPCODE_RELEASE_6 = 0x17,
    SCSI_OPCODE_MODE_SENSE_6 = 0x1A,
    SCSI_OPCODE_START_STOP_UNIT = 0x1B,
    SCSI_OPCODE_RECEIVE_DIAGNOSTIC_RESULTS = 0x1C,
    SCSI_OPCODE_SEND_DIAGNOSTIC = 0x1D,
    SCSI_OPCODE_READ_CAPACITY_10 = 0x25,
    SCSI_OPCODE_READ_10 = 0x28,
    SCSI_OPCODE_WRITE_10 = 0x2A,
    SCSI_OPCODE_SEEK_EXTENDED_10 = 0x2B,
    SCSI_OPCODE_WRITE_AND_VERIFY_10 = 0x2E,
    SCSI_OPCODE_VERIFY_10 = 0x2F,
    SCSI_OPCODE_SYNCHRONIZE_CACHE_10 = 0x35,
    SCSI_OPCODE_READ_DEFECT_DATA_10 = 0x37,
    SCSI_OPCODE_WRITE_BUFFER = 0x3B,
    SCSI_OPCODE_READ_BUFFER_10 = 0x3C,
    SCSI_OPCODE_READ_LONG_10 = 0x3E,
    SCSI_OPCODE_WRITE_LONG = 0x3F,
    SCSI_OPCODE_CHANGE_DEFINITION = 0x40,
    SCSI_OPCODE_WRITE_SAME = 0x41,
    SCSI_OPCODE_UNMAP = 0x42,
    SCSI_OPCODE_SANITIZE = 0x48,
    SCSI_OPCODE_LONG_SELECT = 0x4C,
    SCSI_OPCODE_LONG_SENSE = 0x4D,
    SCSI_OPCODE_MODE_SELECT_10 = 0x55,
    SCSI_OPCODE_RESERVE_10 = 0x56,
    SCSI_OPCODE_RELEASE_10 = 0x57,
    SCSI_OPCODE_MODE_SENSE_10 = 0x5A,
    SCSI_OPCODE_PERSISTENT_RESERVE_IN = 0x5E,
    SCSI_OPCODE_PERSISTENT_RESERVE_OUT = 0x5F,
    SCSI_OPCODE_WRITE_STREAM_32 = 0x7F,
    SCSI_OPCODE_READ_32 = 0x7F,
    SCSI_OPCODE_VERIFY_32 = 0x7F,
    SCSI_OPCODE_WRITE_32 = 0x7F,
    SCSI_OPCODE_WRITE_AND_VERIFY_32 = 0x7F,
    SCSI_OPCODE_WRITE_SAME_32 = 0x7F,
    SCSI_OPCODE_GET_LBA_STATUS_32 = 0x7F,
    SCSI_OPCODE_WRITE_ATOMIC_32 = 0x7F,
    SCSI_OPCODE_READ_16 = 0x88,
    SCSI_OPCODE_WRITE_16 = 0x8A,
    SCSI_OPCODE_WRITE_AND_VERIFY_16 = 0x8E,
    SCSI_OPCODE_VERIFY_16 = 0x8F,
    SCSI_OPCODE_SYNCHRONIZE_CACHE_16 = 0x91,
    SCSI_OPCODE_WRITE_SAME_16 = 0x93,
    SCSI_OPCODE_WRITE_STREAM_16 = 0x9A,
    SCSI_OPCODE_READ_BUFFER_16 = 0x9B,
    SCSI_OPCODE_WRITE_ATOMIC_16 = 0x9C,
    SCSI_OPCODE_READ_CAPACITY_16 = 0x9E,
    SCSI_OPCODE_READ_LONG_16 = 0x9E,
    SCSI_OPCODE_GET_LBA_STATUS_16 = 0x9E,
    SCSI_OPCODE_STREAM_CONTROL = 0x9E,
    SCSI_OPCODE_BACKGROUND_CONTROL = 0x9E,
    SCSI_OPCODE_GET_STREAM_STATUS = 0x9E,
    SCSI_OPCODE_GET_PHYSICAL_ELEMENT_STATUS = 0x9E,
    SCSI_OPCODE_REMOVE_ELEMENT_AND_TRUNCATE = 0x9E,
    SCSI_OPCODE_RESTORE_ELEMENT_AND_REBUILD = 0x9E,
    SCSI_OPCODE_WRITE_LONG_16 = 0x9F,
    SCSI_OPCODE_REPORT_LUNS = 0xA0,
    SCSI_OPCODE_SECURITY_PROTOCOL_IN = 0xA2,
    SCSI_OPCODE_REPORT_IDENTIFYING_INFORMATION = 0xA3,
    SCSI_OPCODE_REPORT_SUPPORTED_OPERATION_CODES = 0xA3,
    SCSI_OPCODE_REPORT_SUPPORTED_TASK_MANAGEMENT_FUNCTIONS = 0xA3,
    SCSI_OPCODE_REPORT_TIMESTAMP = 0xA3,
    SCSI_OPCODE_SET_IDENTIFYING_INFORMATION = 0xA4,
    SCSI_OPCODE_SET_TIMESTAMP = 0xA4,
    SCSI_OPCODE_READ_12 = 0xA8,
    SCSI_OPCODE_WRITE_12 = 0xAA,
    SCSI_OPCODE_WRITE_AND_VERIFY_12 = 0xAE,
    SCSI_OPCODE_VERIFY_12 = 0xAF,
    SCSI_OPCODE_SECURITY_PROTOCOL_OUT = 0xB5,
    SCSI_OPCODE_READ_DEFECT_DATA_12 = 0xB7,
} scsi_opcode_t;

typedef enum
{
    SCSI_SENSE_KEY_NO_SENSE = 0x00,
    SCSI_SENSE_KEY_RECOVERED_ERROR = 0x01,
    SCSI_SENSE_KEY_NOT_READY = 0x02,
    SCSI_SENSE_KEY_MEDIUM_ERROR = 0x03,
    SCSI_SENSE_KEY_HARDWARE_ERROR = 0x04,
    SCSI_SENSE_KEY_ILLEGAL_REQUEST = 0x05,
    SCSI_SENSE_KEY_UNIT_ATTENTION = 0x06,
    SCSI_SENSE_KEY_DATA_PROTECT = 0x07,
    SCSI_SENSE_KEY_BLANK_CHECK = 0x08,
    SCSI_SENSE_KEY_VENDOR_SPECIFIC = 0x09,
    SCSI_SENSE_KEY_COPY_ABORTED = 0x0A,
    SCSI_SENSE_KEY_ABORTED_COMMAND = 0x0B,
    SCSI_SENSE_KEY_VOLUME_OVERFLOW = 0x0D,
    SCSI_SENSE_KEY_MISCOMPARE = 0x0E,
    SCSI_SENSE_KEY_COMPLETED = 0x0F,
} scsi_sense_key_t;

typedef enum
{
    SCSI_ASC_NO_ADDITIONAL_SENSE_INFORMATION = 0x00,
    SCSI_ASC_WRITE_ERROR = 0x0C,
    SCSI_ASC_UNRECOVERED_READ_ERROR = 0x11,
    SCSI_ASC_INVALID_COMMAND_OPERATION_CODE = 0x20,
    SCSI_ASC_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE = 0x21,
    SCSI_ASC_INVALID_FIELD_IN_CDB = 0x24,
    SCSI_ASC_WRITE_PROTECTED = 0x27,
    SCSI_ASC_MEDIUM_NOT_PRESENT = 0x3A,
    SCSI_ASC_INTERNAL_TARGET_FAILURE = 0x44,
} scsi_additional_sense_code_t;

typedef enum
{
    SCSI_DEVICE_TYPE_DIRECT_ACCESS = 0x00,
    SCSI_DEVICE_TYPE_SEQUENTIAL_ACCESS = 0x01,
    SCSI_DEVICE_TYPE_PRINTER = 0x02,
    SCSI_DEVICE_TYPE_PROCESSOR = 0x03,
    SCSI_DEVICE_TYPE_WRITE_ONCE = 0x04,
    SCSI_DEVICE_TYPE_CD_DVD = 0x05,
    SCSI_DEVICE_TYPE_SCANNER = 0x06,
    SCSI_DEVICE_TYPE_OPTICAL_MEMORY = 0x07,
    SCSI_DEVICE_TYPE_MEDIUM_CHANGER = 0x08,
    SCSI_DEVICE_TYPE_COMMUNICATION = 0x09,
    SCSI_DEVICE_TYPE_STORAGE_ARRAY_CONTROLLER = 0x0C,
    SCSI_DEVICE_TYPE_ENCLOSURE_SERVICES = 0x0D,
    SCSI_DEVICE_TYPE_SIMPLIFIED_DIRECT_ACCESS = 0x0E,
    SCSI_DEVICE_TYPE_OPTICAL_CARD_READER = 0x0F,
    SCSI_DEVICE_TYPE_BRIDGE_CONTROLLER = 0x10,
    SCSI_DEVICE_TYPE_OBJECT_BASED_STORAGE = 0x11,
    SCSI_DEVICE_TYPE_AUTOMATION = 0x12,
    SCSI_DEVICE_TYPE_UNKNOWN = 0x1F,
} scsi_device_type_t;

typedef enum
{
    SCSI_VERSION_SPC = 0x03,
    SCSI_VERSION_SPC_2 = 0x04,
    SCSI_VERSION_SPC_3 = 0x05,
    SCSI_VERSION_SPC_4 = 0x06,
    SCSI_VERSION_SPC_6 = 0x07,
} scsi_version_t;

/* @struct */
typedef struct __attribute__((packed))
{
    uint32_t dCBWSignature;
    uint32_t dCBWTag;
    uint32_t dCBWDataTransferLength;
    uint8_t bmCBWFlags;
    uint8_t bCBWLUN;
    uint8_t bCBWCBLength;
    uint8_t CBWCB[16];
} msc_cbw_t;

typedef struct __attribute__((packed))
{
    uint32_t dCSWSignature;
    uint32_t dCSWTag;
    uint32_t dCSWDataResidue;
    uint8_t bCSWStatus;
} msc_csw_t;

typedef struct __attribute__((packed))
{
    uint8_t operation_code;
    uint8_t reserved[4];
    uint8_t control;
} scsi_test_unit_ready_cmd_t;

typedef struct __attribute__((packed))
{
    uint8_t operation_code;
    uint8_t desc : 1;
    uint8_t : 7;
    uint8_t reserved[2];
    uint8_t allocation_length;
    uint8_t control;
} scsi_request_sense_cmd_t;

typedef struct __attribute__((packed))
{
    uint8_t operation_code;
    uint8_t evpd : 1;
    uint8_t obsolete : 1;
    uint8_t : 6;
    uint8_t page_code;
    uint16_t allocation_length;
    uint8_t control;
} scsi_inquiry_cmd_t;

typedef struct __attribute__((packed))
{
    uint8_t operation_code;
    uint8_t obsolete : 1;
    uint8_t : 7;
    uint32_t logical_block_address;
    uint8_t reserved[3];
    uint8_t control;
} scsi_read_capacity_10_cmd_t;

typedef struct __attribute__((packed))
{
    uint8_t operation_code;
    uint8_t immed : 1;
    uint8_t : 7;
    uint8_t reserved;
    uint8_t power_condition_modifier : 4;
    uint8_t : 4;
    uint8_t start : 1;
    uint8_t loej : 1;
    uint8_t no_flush : 1;
    uint8_t : 1;
    uint8_t power_condition : 4;
    uint8_t control;
} scsi_start_stop_unit_cmd_t;

typedef struct __attribute__((packed))
{
    uint8_t operation_code;
    uint8_t obsolete : 2;
    uint8_t rarc : 1;
    uint8_t fua : 1;
    uint8_t dpo : 1;
    uint8_t rdprotect : 3;
    uint32_t logical_block_address;
    uint8_t group_number : 5;
    uint8_t : 3;
    uint16_t transfer_length;
    uint8_t control;
} scsi_read_10_cmd_t;

typedef struct __attribute__((packed))
{
    uint8_t operation_code;
    uint8_t obsolete : 2;
    uint8_t rarc : 1;
    uint8_t fua : 1;
    uint8_t dpo : 1;
    uint8_t rdprotect : 3;
    uint32_t logical_block_address;
    uint8_t group_number : 5;
    uint8_t : 3;
    uint16_t transfer_length;
    uint8_t control;
} scsi_write_10_cmd_t;

typedef struct __attribute__((packed))
{
    uint8_t peripheral_device_type : 5;
    uint8_t peripheral_qualifier : 3;
    uint8_t : 7;
    uint8_t rmb : 1;
    uint8_t version;
    uint8_t response_data_format : 4;
    uint8_t hisup : 1;
    uint8_t normaca : 1;
    uint8_t obsolete : 2;
    uint8_t additional_length;
    uint8_t protect : 1;
    uint8_t : 2;
    uint8_t _3pc : 1;
    uint8_t tpgs : 2;
    uint8_t acc : 1;
    uint8_t sccs : 1;
    uint8_t obsolete2 : 4;
    uint8_t multip : 1;
    uint8_t vs : 1;
    uint8_t encserv : 1;
    uint8_t obsolete3 : 1;
    uint8_t vs2 : 1;
    uint8_t cmdque : 1;
    uint8_t obsolete4 : 6;
    uint8_t vendor_identification[8];
    uint8_t product_identification[16];
    uint8_t product_revision_level[4];
    uint8_t driver_serial_number[8];
    uint8_t vendor_unique[12];
    uint8_t reserved[2];
    uint16_t version_descriptor[8];
    uint8_t reserved2[22];
    uint8_t vendor_specific[];
} scsi_inquiry_data_t;

typedef struct __attribute__((packed))
{
    uint32_t returned_logical_block_address;
    uint32_t block_length_in_bytes;
} scsi_read_capacity_10_data_t;

typedef struct __attribute__((packed))
{
    uint8_t response_code : 7;
    uint8_t valid : 1;
    uint8_t obsolete;
    uint8_t sense_key : 4;
    uint8_t : 1;
    uint8_t ili : 1;
    uint8_t eom : 1;
    uint8_t filemark : 1;
    uint8_t information[4];
    uint8_t additional_sense_length;
    uint8_t command_specific_information[4];
    uint8_t additional_sense_code;
    uint8_t additional_sense_code_qualifier;
    uint8_t field_replaceable_unit_code;
    uint8_t : 7;
    uint8_t sksv : 1;
    uint8_t sense_key_specific[2];
} scsi_sense_data_t;

#ifdef __cplusplus
}
#endif

#endif // MSC_H
