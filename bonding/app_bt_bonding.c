/******************************************************************************
* File Name:   app_bt_bonding.c
*
* Description: This is the source code for bonding implementation using kv-store
*              library for the Hello Server Example for ModusToolbox.
*
* Related Document: See README.md
*
*******************************************************************************
* Copyright 2021-2022, Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
*
* This software, including source code, documentation and related
* materials ("Software") is owned by Cypress Semiconductor Corporation
* or one of its affiliates ("Cypress") and is protected by and subject to
* worldwide patent protection (United States and foreign),
* United States copyright laws and international treaty provisions.
* Therefore, you may use this Software only as provided in the license
* agreement accompanying the software package from which you
* obtained this Software ("EULA").
* If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* non-transferable license to copy, modify, and compile the Software
* source code solely for use in connection with Cypress's
* integrated circuit products.  Any reproduction, modification, translation,
* compilation, or representation of this Software except as specified
* above is prohibited without the express written permission of Cypress.
*
* Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
* reserves the right to make changes to the Software without notice. Cypress
* does not assume any liability arising out of the application or use of the
* Software or any product or circuit described in the Software. Cypress does
* not authorize its products for use in any products where a malfunction or
* failure of the Cypress product may reasonably be expected to result in
* significant property damage, injury or death ("High Risk Product"). By
* including Cypress's product in a High Risk Product, the manufacturer
* of such system or application assumes all risk of such use and in doing
* so agrees to indemnify Cypress against all liability.
*******************************************************************************/

/*******************************************************************************
*        Header Files
*******************************************************************************/
#include "wiced_bt_stack.h"
#include "cybsp.h"
#include "cyhal.h"
#include "cy_retarget_io.h"
#include <FreeRTOS.h>
#include <task.h>
#include "cycfg_bt_settings.h"
#include "app_bt_bonding.h"
#include "mtb_kvstore.h"
#include "app_bt_utils.h"
#include "cy_serial_flash_qspi.h"
#include "app_bt_bonding.h"
#include "hello_sensor.h"
#include <inttypes.h>

/*******************************************************************
 * Variable Definitions
 ******************************************************************/

mtb_kvstore_t                               kvstore_obj;

const  uint32_t                             qspi_bus_freq_hz = QSPI_BUS_FREQ;

/*Local Identity Key*/
wiced_bt_local_identity_keys_t              identity_keys={0};

/* Variable that goes into flash that holds peer link keys */
wiced_bt_device_link_keys_t                 peer_link_keys;

extern host_info_t hello_sensor_hostinfo;

/*******************************************************************************
 *                              FUNCTION DEFINITIONS
 ******************************************************************************/

/**
* Function Name:
* app_kv_store_init
*
* Function Description:
* @brief   This function initializes the SMIF and kv-store library
*
* @param   None
*
* @return  None
*/

void  app_kv_store_init(void)
{
    uint32_t sector_size = 0;
    uint32_t length = 0;
    uint32_t start_addr = 0x7E000;
    cy_rslt_t rslt;

    /* Initialize the QSPI*/
    rslt = cy_serial_flash_qspi_init(smifMemConfigs[0], CYBSP_QSPI_D0, CYBSP_QSPI_D1, CYBSP_QSPI_D2, CYBSP_QSPI_D3, NC,
                                     NC, NC, NC, CYBSP_QSPI_SCK, CYBSP_QSPI_SS, qspi_bus_freq_hz);

    /*Check if the QSPI initialization was successful */
    if (rslt == CY_RSLT_SUCCESS)
    {
        printf("successfully initialized QSPI \r\n");
    }
    else
    {
        printf("failed to initialize QSPI \r\n");
        CY_ASSERT(0);
    }

    /*Define the space to be used for Bond Data Storage*/
    sector_size = cy_serial_flash_qspi_get_erase_size(start_addr);
    length = sector_size * 2;

    /*Initialize kv-store library*/
    rslt = mtb_kvstore_init(&kvstore_obj, start_addr, length, &block_device);
    /*Check if the kv-store initialization was successful*/
    if (CY_RSLT_SUCCESS != rslt)
    {
        printf("failed to initialize kv-store \r\n");
        CY_ASSERT(0);
    }
}


/**
* Function Name:
* app_bt_restore_bond_data
*
* Function Description:
* @brief  This function restores the bond information from the Flash
*
* @param   None
*
* @return  cy_rslt_t: CY_RSLT_SUCCESS if the restoration was successful,
*                         an error code otherwise.
*
*/
cy_rslt_t app_bt_restore_bond_data(void)
{
    /* Read and restore contents of Serial flash */
    uint32_t data_size = sizeof(peer_link_keys);
    cy_rslt_t rslt = mtb_kvstore_read(&kvstore_obj, "bond_data", (uint8_t *)&peer_link_keys, &data_size);
    if (rslt != CY_RSLT_SUCCESS)
    {
        printf("Bond data not present in the flash!\r\n");
    }
    return rslt;
}


/**
* Function Name:
* app_bt_save_device_link_keys
*
* Function Description:
* @brief This function saves peer device link keys to the Flash
*
* @param link_key: Save link keys of the peer device.
*
* @return cy_rslt_t: CY_RSLT_SUCCESS if the save was successful,
*              an error code otherwise.
*
*/
cy_rslt_t app_bt_save_device_link_keys(wiced_bt_device_link_keys_t *link_key)
{
    cy_rslt_t rslt = CY_RSLT_TYPE_ERROR;
    memcpy((uint8_t *) &peer_link_keys, (uint8_t *)(link_key), sizeof(wiced_bt_device_link_keys_t));

    rslt = mtb_kvstore_write(&kvstore_obj, "bond_data", (uint8_t *)&peer_link_keys, sizeof(peer_link_keys));
    if (CY_RSLT_SUCCESS != rslt)
    {
        printf("Flash Write Error,Error code: %" PRIu32 "\r\n", rslt );
    }
    return rslt;
}

/**
* Function Name:
* app_bt_find_device_in_flash
*
* Function Description:
* @brief This function searches provided bd_addr in bonded devices list
*
* @param *bd_addr: pointer to the address of the device to be searched
*
* @return bool: true: Device present in flash
*               false: device not present in flash
*
*/
bool app_bt_find_device_in_flash(uint8_t *bd_addr)
{
    if (memcmp(peer_link_keys.bd_addr, bd_addr, sizeof(wiced_bt_device_address_t)) == 0)
    {
        printf("Found device in the flash!\r\n");
        return (true);
    }
    return false;
}

/**
* Function Name:
* app_bt_read_local_identity_keys
*
* Function Description:
* @brief This function saves local device identity keys to the Flash
*
* @param None
*
* @return cy_rslt_t: CY_RSLT_SUCCESS if the read was successful,
*              an error code otherwise.
*
*/
cy_rslt_t app_bt_read_local_identity_keys(void)
{
    uint32_t data_size = sizeof(identity_keys);
    cy_rslt_t rslt = mtb_kvstore_read(&kvstore_obj, "local_irk", NULL, NULL);
    if (rslt != CY_RSLT_SUCCESS)
    {
        printf("Error Reading Keys! New Keys need to be generated! \r\n");
    }
    else
    {
        printf("Identity keys are available in the database.\r\n");
        rslt = mtb_kvstore_read(&kvstore_obj, "local_irk", (uint8_t *)&identity_keys, &data_size);
        printf("Local identity keys read from Flash: \r\n");
    }
    return rslt;
}

/**
* Function Name:
* app_bt_save_local_identity_key
*
* Function Description:
* @briefThis function saves local device identity keys to the Flash
*
* @param id_key: Local identity keys to store in the flash.
*
* @return cy_rslt_t: CY_RSLT_SUCCESS if the save was successful,
*              an error code otherwise.
*
*/
cy_rslt_t app_bt_save_local_identity_key(wiced_bt_local_identity_keys_t id_key)
{
    memcpy(&identity_keys, (uint8_t *)&(id_key), sizeof(wiced_bt_local_identity_keys_t));
    cy_rslt_t rslt = mtb_kvstore_write(&kvstore_obj, "local_irk", (uint8_t *)&identity_keys, sizeof(wiced_bt_local_identity_keys_t));
    if (CY_RSLT_SUCCESS == rslt)
    {
        printf("Local identity Keys saved to Flash \r\n");
    }
    else
    {
        printf("Flash Write Error,Error code: %" PRIu32 "\r\n", rslt );
    }

    return rslt;
}

/**
* Function Name:
* app_bt_update_hostinfo
*
* Function Description:
* @brief This function updates the host info to the Flash
*
* @param void: Local identity keys to store in the flash.
*
* @return cy_rslt_t: CY_RSLT_SUCCESS if the save was successful,
*              an error code otherwise.
*
*/
cy_rslt_t app_bt_update_hostinfo(void)
{
    cy_rslt_t rslt = mtb_kvstore_write(&kvstore_obj, "host_info", (uint8_t *)&hello_sensor_hostinfo, sizeof(host_info_t));
    return rslt;
}


/**
* Function Name:
* app_bt_save_local_identity_key
*
* Function Description:
* @briefThis function saves local device identity keys to the Flash
*
* @param id_key: Local identity keys to store in the flash.
*
* @return cy_rslt_t: CY_RSLT_SUCCESS if the save was successful,
*              an error code otherwise.
*
*/
cy_rslt_t app_bt_read_hostinfo(void)
{
    uint32_t data_size = sizeof(host_info_t);
    cy_rslt_t rslt = mtb_kvstore_read(&kvstore_obj, "host_info", (uint8_t *)&hello_sensor_hostinfo,&data_size);
    return rslt;
}

/* END OF FILE [] */
