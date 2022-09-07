/***************************************************************************//**
* File Name: cycfg_gatt_db.c
*
* Description:
* BLE device's GATT database and device configuration.
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

#include "cycfg_gatt_db.h"
#include "wiced_bt_uuid.h"
#include "wiced_bt_gatt.h"

/*************************************************************************************
* GATT server definitions
*************************************************************************************/

const uint8_t gatt_database[] = 
{
    /* Primary Service: Generic Access */
    PRIMARY_SERVICE_UUID16 (HDLS_GAP, __UUID_SERVICE_GENERIC_ACCESS),
        /* Characteristic: Device Name */
        CHARACTERISTIC_UUID16 (HDLC_GAP_DEVICE_NAME, HDLC_GAP_DEVICE_NAME_VALUE, __UUID_CHARACTERISTIC_DEVICE_NAME, GATTDB_CHAR_PROP_READ, GATTDB_PERM_READABLE),
        /* Characteristic: Appearance */
        CHARACTERISTIC_UUID16 (HDLC_GAP_APPEARANCE, HDLC_GAP_APPEARANCE_VALUE, __UUID_CHARACTERISTIC_APPEARANCE, GATTDB_CHAR_PROP_READ, GATTDB_PERM_READABLE),

    /* Primary Service: Generic Attribute */
    PRIMARY_SERVICE_UUID16 (HDLS_GATT, __UUID_SERVICE_GENERIC_ATTRIBUTE),

    /* Primary Service: Hello_Sensor */
    PRIMARY_SERVICE_UUID128 (HDLS_HELLO_SENSOR, __UUID_SERVICE_HELLO_SENSOR),
        /* Characteristic: Notify */
        CHARACTERISTIC_UUID128 (HDLC_HELLO_SENSOR_NOTIFY, HDLC_HELLO_SENSOR_NOTIFY_VALUE, __UUID_CHARACTERISTIC_HELLO_SENSOR_NOTIFY, GATTDB_CHAR_PROP_READ | GATTDB_CHAR_PROP_NOTIFY | GATTDB_CHAR_PROP_INDICATE, GATTDB_PERM_READABLE | GATTDB_PERM_AUTH_READABLE),
            /* Descriptor: Char_Desc */
            CHAR_DESCRIPTOR_UUID16_WRITABLE (HDLD_HELLO_SENSOR_NOTIFY_CHAR_DESC, __UUID_DESCRIPTOR_CLIENT_CHARACTERISTIC_CONFIGURATION, GATTDB_PERM_READABLE | GATTDB_PERM_WRITE_REQ | GATTDB_PERM_AUTH_WRITABLE),
        /* Characteristic: Blink */
        CHARACTERISTIC_UUID128_WRITABLE (HDLC_HELLO_SENSOR_BLINK, HDLC_HELLO_SENSOR_BLINK_VALUE, __UUID_CHARACTERISTIC_HELLO_SENSOR_BLINK, GATTDB_CHAR_PROP_READ | GATTDB_CHAR_PROP_WRITE | GATTDB_CHAR_PROP_WRITE_NO_RESPONSE, GATTDB_PERM_READABLE | GATTDB_PERM_WRITE_REQ | GATTDB_PERM_WRITE_CMD),
};

/* Length of the GATT database */
const uint16_t gatt_database_len = sizeof(gatt_database);

/*************************************************************************************
 * GATT Initial Value Arrays
 ************************************************************************************/
 
uint8_t app_gap_device_name[]               = {'H', 'e', 'l', 'l', 'o', '\0', };
uint8_t app_gap_appearance[]                = {0x00, 0x02, };
uint8_t app_hello_sensor_notify[]           = {'H', 'e', 'l', 'l', 'o', ' ', '0', };
uint8_t app_hello_sensor_notify_char_desc[] = {0x00, 0x00, };
uint8_t app_hello_sensor_blink[]            = {0x00, };
 
 /************************************************************************************
 * GATT Lookup Table
 ************************************************************************************/
 
gatt_db_lookup_table_t app_gatt_db_ext_attr_tbl[] =
{
    /* { attribute handle,                maxlen, curlen, attribute data } */
    { HDLC_GAP_DEVICE_NAME_VALUE,         5,      5,      app_gap_device_name },
    { HDLC_GAP_APPEARANCE_VALUE,          2,      2,      app_gap_appearance },
    { HDLC_HELLO_SENSOR_NOTIFY_VALUE,     7,      7,      app_hello_sensor_notify },
    { HDLD_HELLO_SENSOR_NOTIFY_CHAR_DESC, 2,      2,      app_hello_sensor_notify_char_desc },
    { HDLC_HELLO_SENSOR_BLINK_VALUE,      1,      1,      app_hello_sensor_blink },
};

/* Number of Lookup Table entries */
const uint16_t app_gatt_db_ext_attr_tbl_size = (sizeof(app_gatt_db_ext_attr_tbl) / sizeof(gatt_db_lookup_table_t));

/* Number of GATT initial value arrays entries */
const uint16_t app_gap_device_name_len = 5;
const uint16_t app_gap_appearance_len = (sizeof(app_gap_appearance));
const uint16_t app_hello_sensor_notify_len = (sizeof(app_hello_sensor_notify));
const uint16_t app_hello_sensor_notify_char_desc_len = (sizeof(app_hello_sensor_notify_char_desc));
const uint16_t app_hello_sensor_blink_len = (sizeof(app_hello_sensor_blink));
