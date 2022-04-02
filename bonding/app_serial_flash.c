/******************************************************************************
* File Name: app_serial_flash.c
*
* Description: This file contains block device function implementations
*              required by kv-store library
*
* Related Document: See README.md
*
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
 *                              INCLUDES
 ******************************************************************************/

#include "cycfg_qspi_memslot.h"
#include "cy_serial_flash_qspi.h"
#include "mtb_kvstore.h"
#include "app_serial_flash.h"

/*******************************************************************************
 *                              FUNCTION DECLARATIONS
 ******************************************************************************/
static uint32_t bd_read_size(void* context, uint32_t addr);
static uint32_t bd_program_size(void* context, uint32_t addr);
static uint32_t bd_erase_size(void* context, uint32_t addr);
static cy_rslt_t bd_read(void* context, uint32_t addr, uint32_t length, uint8_t* buf);
static cy_rslt_t bd_program(void* context, uint32_t addr, uint32_t length, const uint8_t* buf);
static cy_rslt_t bd_erase(void* context, uint32_t addr, uint32_t length);

/*******************************************************************************
 *                              FUNCTION DEFINITIONS
 ******************************************************************************/

/**
 * Function Name:
 * bd_read_size
 *
 * Function Description:
 * @brief Function to get the read size of the block device for a specific address.
 *
 * @param  void* context : Context object that is passed into mtb_kvstore_init
           uint32_t addr : Address for which the read size is queried. This address is passed in as start_addr + offset.
 *
 *
 * @return uint32_t: Read size of the memory device.
 *
 */
static uint32_t bd_read_size(void* context, uint32_t addr)
{
    (void)context;
    (void)addr;
    return 1;
}

/**
 * Function Name:
 * bd_program_size
 *
 * Function Description:
 * @brief Function to get the program size of the block device for a specific address.
 *
 * @param  void* context: Context object that is passed into mtb_kvstore_init
           uint32_t addr: Address for which the program size is queried. This address is passed in as start_addr + offset.
 *
 *
 * @return uint32_t: Program size of the memory device.
 */
static uint32_t bd_program_size(void* context, uint32_t addr)
{
    (void)context;
    return cy_serial_flash_qspi_get_prog_size(addr);
}

/**
 * Function Name:
 * bd_erase_size
 *
 * Function Description:
 * @brief Function prototype to get the erase size of the block device for a specific address.
 *
 * @param  void* context: Context object that is passed into mtb_kvstore_init
           uint32_t addr: Address for which the program size is queried. This address is passed in as start_addr + offset.
 *
 *
 * @return uint32_t Erase size of the memory device.
 * */
static uint32_t bd_erase_size(void* context, uint32_t addr)
{
    (void)context;
    return cy_serial_flash_qspi_get_erase_size(addr);
}

/**
 * Function Name:
 * bd_read
 *
 * Function Description:
 * @brief Function for reading data from the block device.
 *
 * @param  void* context : Context object that is passed into mtb_kvstore_init
           uint32_t addr: Address to read the data from the block device. This address is passed in as start_addr + offset
           uint32_t length : Length of the data to be read into the buffer
           uint8_t* buf : Buffer to read the data.
 *
 *
 * @return cy_rslt_t: Result of the read operation.
 * */
static cy_rslt_t bd_read(void* context, uint32_t addr, uint32_t length, uint8_t* buf)
{
    (void)context;
    return cy_serial_flash_qspi_read(addr, length, buf);
}

/**
 * Function Name:
 * bd_program
 *
 * Function Description:
 * @brief
 *
 * @param  void* context : Context object that is passed into mtb_kvstore_init
           uint32_t addr: Address to program the data into the block device. This address is passed in as start_addr + offset
           uint32_t length : Length of the data to be written
           uint8_t* buf : Data that needs to be written
 * @return cy_rslt_t: Result of the program operation.
 **/
static cy_rslt_t bd_program(void* context, uint32_t addr, uint32_t length, const uint8_t* buf)
{
    (void)context;
    return cy_serial_flash_qspi_write(addr, length, buf);
}

/**
 * Function Name:
 * bd_erase
 *
 * Function Description:
 * @brief
 *
 * @param  context: Context object that is passed into mtb_kvstore_init
           uint32_t addr: Address to erase the data from the device. This address is passed in as start_addr + offset
           uint32_t length: Length of the data that needs to be erased
 *
 *
 * @return cy_rslt_t : Result of the erase operation.
 **/
static cy_rslt_t bd_erase(void* context, uint32_t addr, uint32_t length)
{
    (void)context;
    return cy_serial_flash_qspi_erase(addr, length);
}
/**
 * Function Name:
 * app_kvstore_bd_init
 *
 * Function Description:
 * @brief  This function provides the pointer to the implementated prototype function for the block device.
 *
 * @param  mtb_kvstore_bd_t : Block device interface
 *
 * @return void
 **/
void app_kvstore_bd_init(mtb_kvstore_bd_t* device)
{
    device->read         = bd_read;
    device->program      = bd_program;
    device->erase        = bd_erase;
    device->read_size    = bd_read_size;
    device->program_size = bd_program_size;
    device->erase_size   = bd_erase_size;
    device->context      = NULL;
}

/* END OF FILE [] */
