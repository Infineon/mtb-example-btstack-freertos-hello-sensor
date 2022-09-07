/***************************************************************************//**
* File Name: cycfg_gatt_db.h
*
* Description:
* Definitions for constants used in the device's GATT database and function
* prototypes.
*
********************************************************************************
* Copyright 2022 Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.
* SPDX-License-Identifier: Apache-2.0
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*******************************************************************************/

#if !defined(CYCFG_GATT_DB_H)
#define CYCFG_GATT_DB_H

#include "stdint.h"

#define __UUID_SERVICE_GENERIC_ACCESS               0x1800
#define __UUID_CHARACTERISTIC_DEVICE_NAME           0x2A00
#define __UUID_CHARACTERISTIC_APPEARANCE            0x2A01
#define __UUID_SERVICE_GENERIC_ATTRIBUTE            0x1801
#define __UUID_SERVICE_HELLO_SENSOR                 0x38, 0x28, 0x2E, 0x5F, 0xA5, 0x1E, 0xC7, 0xA4, 0xC2, 0x46, 0x47, 0x74, 0xB6, 0xC7, 0x81, 0x2F
#define __UUID_CHARACTERISTIC_HELLO_SENSOR_NOTIFY    0xBD, 0xED, 0x5E, 0xC7, 0x28, 0x3A, 0x86, 0xAD, 0xAF, 0x44, 0x45, 0x9F, 0x3D, 0x62, 0xEA, 0x29
#define __UUID_DESCRIPTOR_CLIENT_CHARACTERISTIC_CONFIGURATION    0x2902
#define __UUID_CHARACTERISTIC_HELLO_SENSOR_BLINK    0x8E, 0xB1, 0x5D, 0xFA, 0x97, 0x33, 0x5A, 0xA1, 0x61, 0x41, 0xA7, 0x13, 0x50, 0x36, 0xAD, 0xD2

/* Service Generic Access */
#define HDLS_GAP                                    0x0001
/* Characteristic Device Name */
#define HDLC_GAP_DEVICE_NAME                        0x0002
#define HDLC_GAP_DEVICE_NAME_VALUE                  0x0003
/* Characteristic Appearance */
#define HDLC_GAP_APPEARANCE                         0x0004
#define HDLC_GAP_APPEARANCE_VALUE                   0x0005

/* Service Generic Attribute */
#define HDLS_GATT                                   0x0006

/* Service Hello_Sensor */
#define HDLS_HELLO_SENSOR                           0x0007
/* Characteristic Notify */
#define HDLC_HELLO_SENSOR_NOTIFY                    0x0008
#define HDLC_HELLO_SENSOR_NOTIFY_VALUE              0x0009
/* Descriptor Char_Desc */
#define HDLD_HELLO_SENSOR_NOTIFY_CHAR_DESC          0x000A
/* Characteristic Blink */
#define HDLC_HELLO_SENSOR_BLINK                     0x000B
#define HDLC_HELLO_SENSOR_BLINK_VALUE               0x000C

/* External Lookup Table Entry */
typedef struct
{
    uint16_t handle;
    uint16_t max_len;
    uint16_t cur_len;
    uint8_t  *p_data;
} gatt_db_lookup_table_t;

/* External definitions */
extern const uint8_t  gatt_database[];
extern const uint16_t gatt_database_len;
extern gatt_db_lookup_table_t app_gatt_db_ext_attr_tbl[];
extern const uint16_t app_gatt_db_ext_attr_tbl_size;
extern uint8_t app_gap_device_name[];
extern const uint16_t app_gap_device_name_len;
extern uint8_t app_gap_appearance[];
extern const uint16_t app_gap_appearance_len;
extern uint8_t app_hello_sensor_notify[];
extern const uint16_t app_hello_sensor_notify_len;
extern uint8_t app_hello_sensor_notify_char_desc[];
extern const uint16_t app_hello_sensor_notify_char_desc_len;
extern uint8_t app_hello_sensor_blink[];
extern const uint16_t app_hello_sensor_blink_len;

#endif /* CYCFG_GATT_DB_H */
