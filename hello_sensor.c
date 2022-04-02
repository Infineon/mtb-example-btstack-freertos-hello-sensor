/******************************************************************************
* File Name: hello_sensor.c
*
* Description: This is the source code for the
*              Hello Sensor Example for ModusToolbox.
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
*        Header Files
*******************************************************************************/

#include "wiced_bt_stack.h"
#include "cybsp.h"
#include "cy_retarget_io.h"
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <string.h>
#include "cybt_platform_trace.h"
#include "wiced_memory.h"
#include "cyhal.h"
#include "stdio.h"
#include "GeneratedSource/cycfg_gatt_db.h"
#include "GeneratedSource/cycfg_bt_settings.h"
#include "GeneratedSource/cycfg_gap.h"
#include "wiced_bt_dev.h"
#include "app_bt_utils.h"
#include "hello_sensor.h"
#include "wiced_timer.h"
#include "task.h"
#include "app_bt_bonding.h"
#include "app_serial_flash.h"
#include "inttypes.h"
#ifdef ENABLE_BT_SPY_LOG
#include "cybt_debug_uart.h"
#endif

/*******************************************************************************
* Macros
********************************************************************************/

/* Hello Sensor App Timer Timeout in seconds  */
#define HELLO_SENSOR_APP_TIMEOUT_IN_MS                      1000

/* Hello Sensor App Fine Timer Timeout in milli seconds  */
#define HELLO_SENSOR_APP_FINE_TIMEOUT_IN_MS                 1

#define GPIO_INTERRUPT_PRIORITY                             4

/******************************************************************************
 *                                Structures
 ******************************************************************************/
typedef struct
{
    wiced_bt_device_address_t remote_addr;  // remote peer device address
    uint32_t  timer_count;                  // timer count used to count the timeout value in seconds
    uint32_t  fine_timer_count;             // fine timer count in milliseconds
    uint16_t  conn_id;                      // connection ID referenced by the stack
    uint16_t  peer_mtu;                     // peer MTU
    uint8_t   num_to_write;                 // Num of messages to send. Incremented on each button interrupt
    uint8_t   flag_indication_sent;         // indicates waiting for ack/cfm
    uint8_t   flag_stay_connected;          // stay connected or disconnect after all messages are sent
    uint8_t   battery_level;                // dummy battery level

} hello_sensor_state_t;


/*******************************************************************************
* Variable Definitions
*******************************************************************************/

/* Holds the global state of the hello sensor application */
hello_sensor_state_t hello_sensor_state;

/* Host information saved in  NVRAM */
host_info_t hello_sensor_hostinfo;

/* App timer objects */

/* hello_sensor_second_timer is a periodic timer that ticks every second. Upon
   expiring, the current time in seconds is displayed in the serial terminal */
/* hello_sensor_ms_timer is a periodic timer that ticks every millsecond. This
   timer is used to measure the duration of button press events */
TimerHandle_t hello_sensor_second_timer,hello_sensor_ms_timer;

/* This is a periodic timer that expires every second. This timer is used to control
   the duty cycle of the LED blinks */
wiced_timer_t hello_sensor_led_timer;

/* Variables to hold the timer count values */
uint8_t   hello_sensor_led_blink_count  = 0;
uint16_t  hello_sensor_led_on_ms        = 0;
uint16_t  hello_sensor_led_off_ms       = 0;

/* Handle of the button task */
TaskHandle_t button_handle;

typedef void  (*pfn_free_buffer_t) (uint8_t *);

/*******************************************************************************
* Function Prototypes
********************************************************************************/
static void  hello_sensor_application_init             (void);
static void  hello_sensor_interrput_config             (void);
static void  gpio_interrupt_handler                    (void *handler_arg, cyhal_gpio_event_t event);
static void* hello_sensor_alloc_buffer                 (int len);
static void  hello_sensor_adv_stop_handler             (void);
static void  hello_sensor_encryption_changed           (wiced_result_t result, uint8_t* bd_addr);
static void  hello_sensor_send_message                 (void);
static void  hello_sensor_free_buffer                  (uint8_t *p_event_data);
static void  hello_sensor_gatts_increment_notify_value (void);
static void  hello_sensor_timeout                      (TimerHandle_t timer_handle);
static void  hello_sensor_fine_timeout                 (TimerHandle_t timer_handle);
static void  hello_sensor_led_timeout                  (void *count);
static void  hello_sensor_led_blink                    (uint16_t on_ms, uint16_t off_ms, uint8_t num_of_blinks);
static void  hello_sensor_smp_bond_result              (uint8_t result);
static void  hello_sensor_load_keys_for_address_resolution(void);

static wiced_bt_gatt_status_t hello_sensor_gatts_callback         (wiced_bt_gatt_evt_t event,  wiced_bt_gatt_event_data_t *p_data);
static wiced_bt_gatt_status_t hello_sensor_gatts_conn_status_cb   (wiced_bt_gatt_connection_status_t *p_conn_status);
static wiced_bt_gatt_status_t hello_sensor_gatts_req_cb           (wiced_bt_gatt_attribute_request_t *p_attr_req);
static wiced_bt_gatt_status_t hello_sensor_gatts_req_conf_handler (uint16_t conn_id, uint16_t handle);
static wiced_bt_gatt_status_t hello_sensor_gatts_connection_up    (wiced_bt_gatt_connection_status_t *p_status);
static wiced_bt_gatt_status_t hello_sensor_gatts_connection_down  (wiced_bt_gatt_connection_status_t *p_status);
static wiced_bt_gatt_status_t hello_sensor_set_value              (uint16_t attr_handle, uint8_t *p_val, uint16_t len);

static gatt_db_lookup_table_t *hello_sensor_find_by_handle   (uint16_t handle);

/* GATT Event Callback Functions */
static wiced_bt_gatt_status_t hello_sensor_gatts_req_write_handler  (uint16_t conn_id,
                                                                     wiced_bt_gatt_opcode_t opcode,
                                                                     wiced_bt_gatt_write_req_t *p_write_req,
                                                                     uint16_t len_req);
static wiced_bt_gatt_status_t hello_sensor_gatts_req_read_handler   (uint16_t conn_id,
                                                                     wiced_bt_gatt_opcode_t opcode,
                                                                     wiced_bt_gatt_read_t *p_read_req,
                                                                     uint16_t len_req);
static wiced_bt_gatt_status_t hello_sensor_gatts_req_cb             (wiced_bt_gatt_attribute_request_t *p_attr_req);
static wiced_bt_gatt_status_t hello_sensor_gatt_req_read_by_type_handler (uint16_t conn_id,
                                                                    wiced_bt_gatt_opcode_t opcode,
                                                                    wiced_bt_gatt_read_by_type_t *p_read_req, uint16_t len_requested);

cyhal_gpio_callback_data_t btn_cb_data =
    {
        .callback     = gpio_interrupt_handler,
        .callback_arg = NULL
    };

/**************************************************************************************************
* Function Name: app_bt_management_callback
***************************************************************************************************
* Summary:
*   This is a Bluetooth stack event handler function to receive management events from
*   the LE stack and process as per the application.
*
* Parameters:
*   wiced_bt_management_evt_t event             : LE event code of one byte length
*   wiced_bt_management_evt_data_t *p_event_data: Pointer to LE management event structures
*
* Return:
*  wiced_result_t: Error code from WICED_RESULT_LIST or BT_RESULT_LIST
*
*************************************************************************************************/
wiced_result_t hello_sensor_management_callback(wiced_bt_management_evt_t event,
                                          wiced_bt_management_evt_data_t *p_event_data)
{
    wiced_result_t wiced_result = WICED_BT_SUCCESS;
    cy_rslt_t rslt;
    wiced_bt_dev_encryption_status_t    *p_status;
    wiced_bt_ble_advert_mode_t          *p_mode;
    wiced_bt_dev_ble_pairing_info_t     *p_info;
    wiced_bt_device_address_t bda = { 0 };

    printf("Event:%s\r\n", get_bt_event_name(event));

    switch (event)
    {
        case BTM_ENABLED_EVT:
            /* Bluetooth Controller and Host Stack Enabled */
            if (WICED_BT_SUCCESS == p_event_data->enabled.status)
            {
                wiced_bt_dev_read_local_addr(bda);
                printf("Local Bluetooth Address: ");
                print_bd_address(bda);

                /* Perform application-specific initialization */
                hello_sensor_application_init();
            }
            else
            {
                printf( "Bluetooth enable failed \r\n" );
            }
            break;

        case BTM_DISABLED_EVT:
            break;

        case BTM_USER_CONFIRMATION_REQUEST_EVT:
            printf("Numeric_value: %"PRIu32" \r\n", p_event_data->user_confirmation_request.numeric_value);
            wiced_bt_dev_confirm_req_reply( WICED_BT_SUCCESS , p_event_data->user_confirmation_request.bd_addr);
            break;

        case BTM_PASSKEY_NOTIFICATION_EVT:
            printf("\r\nPassKey Notification");
            print_bd_address(p_event_data->user_passkey_notification.bd_addr);
            printf(",PassKey %"PRIu32" \r\n", p_event_data->user_passkey_notification.passkey );
            wiced_bt_dev_confirm_req_reply(WICED_BT_SUCCESS, p_event_data->user_passkey_notification.bd_addr );
            break;

        case BTM_PAIRING_IO_CAPABILITIES_BLE_REQUEST_EVT:
            p_event_data->pairing_io_capabilities_ble_request.local_io_cap      = BTM_IO_CAPABILITIES_NONE;
            p_event_data->pairing_io_capabilities_ble_request.oob_data          = BTM_OOB_NONE;
            p_event_data->pairing_io_capabilities_ble_request.auth_req          = BTM_LE_AUTH_REQ_SC_BOND;
            p_event_data->pairing_io_capabilities_ble_request.max_key_size      = 0x10;
            p_event_data->pairing_io_capabilities_ble_request.init_keys         = BTM_LE_KEY_PENC|BTM_LE_KEY_PID|BTM_LE_KEY_PCSRK|BTM_LE_KEY_LENC;
            p_event_data->pairing_io_capabilities_ble_request.resp_keys         = BTM_LE_KEY_PENC|BTM_LE_KEY_PID|BTM_LE_KEY_PCSRK|BTM_LE_KEY_LENC;
            break;

        case BTM_PAIRING_COMPLETE_EVT:
            p_info =  &p_event_data->pairing_complete.pairing_complete_info.ble;
            printf( "Hello sensor, Pairing Complete Reason: %s ",
                                            get_bt_smp_status_name((wiced_bt_smp_status_t) p_info->reason));
            hello_sensor_smp_bond_result( p_info->reason );
            break;


        case BTM_BLE_ADVERT_STATE_CHANGED_EVT:
            p_mode = &p_event_data->ble_advert_state_changed;
            printf( "Advertisement State Change: %s\n", get_bt_advert_mode_name(*p_mode));
            if ( *p_mode == BTM_BLE_ADVERT_OFF )
            {
                hello_sensor_adv_stop_handler();
            }
            break;

        case BTM_PAIRED_DEVICE_LINK_KEYS_UPDATE_EVT:
             /* save device keys to Flash */
             rslt = app_bt_save_device_link_keys(&(p_event_data->paired_device_link_keys_update));
             if (CY_RSLT_SUCCESS == rslt)
             {
                 printf("Successfully Bonded to ");
                 print_bd_address(p_event_data->paired_device_link_keys_update.bd_addr);
             }
             else
             {
                 printf("Failed to bond! \r\n");
             }
            break;

        case  BTM_PAIRED_DEVICE_LINK_KEYS_REQUEST_EVT:
            /* Paired Device Link Keys Request */
            printf("Paired Device Link keys Request Event for device ");
            print_bd_address((uint8_t *)(p_event_data->paired_device_link_keys_request.bd_addr));

            /* Need to search to see if the BD_ADDR we are looking for is in Flash.
             * If not, we return WICED_BT_ERROR and the stack will generate keys
             * and will then call BTM_PAIRED_DEVICE_LINK_KEYS_UPDATE_EVT so that
             * they can be stored
             */

            /* Assume the device won't be found. If it is, we will set this back to WICED_BT_SUCCESS */
            wiced_result = WICED_BT_ERROR;

            if(app_bt_find_device_in_flash(p_event_data->paired_device_link_keys_request.bd_addr))
            {
                /* Copy the keys to where the stack wants it */
                memcpy(&(p_event_data->paired_device_link_keys_request), &peer_link_keys, sizeof(wiced_bt_device_link_keys_t));
                wiced_result = WICED_BT_SUCCESS;
            }
            else
            {
                printf("Device Link Keys not found in the database! \n");

            }
            break;

        case BTM_LOCAL_IDENTITY_KEYS_UPDATE_EVT:
            /* Update of local privacy keys - save to Flash */
            rslt = app_bt_save_local_identity_key(p_event_data->local_identity_keys_update);
            if (CY_RSLT_SUCCESS != rslt)
            {
                wiced_result = WICED_BT_ERROR;
            }
            break;

        case  BTM_LOCAL_IDENTITY_KEYS_REQUEST_EVT:
            /* read keys from NVRAM */
            app_kv_store_init();
            /*Read Local Identity Resolution Keys*/
            rslt = app_bt_read_local_identity_keys();
            if(CY_RSLT_SUCCESS == rslt)
            {
                memcpy(&(p_event_data->local_identity_keys_request), &(identity_keys), sizeof(wiced_bt_local_identity_keys_t));
                print_array(&identity_keys, sizeof(wiced_bt_local_identity_keys_t));
                wiced_result = WICED_BT_SUCCESS;
            }
            else
            {
                wiced_result = WICED_BT_ERROR;
            }

            break;

        case BTM_ENCRYPTION_STATUS_EVT:
            p_status = &p_event_data->encryption_status;
            hello_sensor_encryption_changed( p_status->result, p_status->bd_addr );
            break;

        case BTM_SECURITY_REQUEST_EVT:
            wiced_bt_ble_security_grant( p_event_data->security_request.bd_addr, WICED_BT_SUCCESS );
            break;


        case BTM_BLE_CONNECTION_PARAM_UPDATE:
            printf("Connection parameter update status:%d, Connection Interval: %d, Connection Latency: %d, Connection Timeout: %d\n",
                    p_event_data->ble_connection_param_update.status,
                    p_event_data->ble_connection_param_update.conn_interval,
                    p_event_data->ble_connection_param_update.conn_latency,
                    p_event_data->ble_connection_param_update.supervision_timeout);
            break;

        case BTM_BLE_PHY_UPDATE_EVT:

            printf("Selected TX PHY - %dM\r\n Selected RX PHY - %dM\r\n",
                                        p_event_data->ble_phy_update_event.tx_phy,
                                        p_event_data->ble_phy_update_event.rx_phy);
            break;

        default:
            printf("Unhandled Bluetooth Management Event: 0x%x %s\n", event, get_bt_event_name(event));
            break;
    }

    return wiced_result;
}

#ifdef ENABLE_BT_SPY_LOG
void hci_trace_cback(wiced_bt_hci_trace_type_t type, uint16_t length, uint8_t* p_data)
{
    cybt_debug_uart_send_hci_trace(type, length, p_data);
}
#endif


/**************************************************************************************************
 * Function Name: hello_sensor_application_init
 ***************************************************************************************************
 * Summary:
 *   This function handles application level initialization tasks and is called from the BT
 *   management callback once the LE stack enabled event (BTM_ENABLED_EVT) is triggered
 *   This function is executed in the BTM_ENABLED_EVT management callback.
 *
 * Parameters:
 *   None
 *
 * Return:
 *  None
 *
 *************************************************************************************************/
static void hello_sensor_application_init(void)
{
    wiced_result_t wiced_result = WICED_BT_SUCCESS;
    wiced_bt_gatt_status_t gatt_status = WICED_BT_GATT_SUCCESS;

    #ifdef ENABLE_BT_SPY_LOG
    wiced_bt_dev_register_hci_trace(hci_trace_cback);
    #endif

    printf("\n\rHello sensor application init\r\n");

    hello_sensor_interrput_config();

    cyhal_gpio_init(CYBSP_USER_LED1 , CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, CYBSP_LED_STATE_OFF);

    /* Starting the app timers , seconds timer and the ms timer  */
    hello_sensor_second_timer = xTimerCreate("timeout_sectimer", pdMS_TO_TICKS(HELLO_SENSOR_APP_TIMEOUT_IN_MS), pdTRUE,
                                            NULL, hello_sensor_timeout);
    xTimerStart(hello_sensor_second_timer, HELLO_SENSOR_APP_TIMEOUT_IN_MS);

    hello_sensor_ms_timer = xTimerCreate("timeout_mstimer", pdMS_TO_TICKS(HELLO_SENSOR_APP_FINE_TIMEOUT_IN_MS), pdTRUE,
                                            NULL, hello_sensor_fine_timeout);
    xTimerStart(hello_sensor_ms_timer, HELLO_SENSOR_APP_FINE_TIMEOUT_IN_MS);

    if(app_bt_restore_bond_data() == CY_RSLT_SUCCESS)
    {
        printf("Keys found in flash, add them to Addr Res DB\r\n");
        /* Load previous paired keys for address resolution */
        hello_sensor_load_keys_for_address_resolution();
    }

    /* Allow peer to pair */
    wiced_bt_set_pairable_mode(WICED_TRUE, FALSE);

    /* Set Advertisement Data */
    wiced_bt_ble_set_raw_advertisement_data(CY_BT_ADV_PACKET_DATA_SIZE, cy_bt_adv_packet_data);

    /* Register with BT stack to receive GATT callback */
    gatt_status = wiced_bt_gatt_register(hello_sensor_gatts_callback);
    printf("GATT event Handler registration status: %s \n",get_bt_gatt_status_name(gatt_status));

    /* Initialize GATT Database */
    gatt_status = wiced_bt_gatt_db_init(gatt_database, gatt_database_len, NULL);
    printf("GATT database initialization status: %s \n",get_bt_gatt_status_name(gatt_status));

    /* Start Undirected LE Advertisements on device startup.
     * The corresponding parameters are contained in 'app_bt_cfg.c' */
    wiced_result = wiced_bt_start_advertisements(BTM_BLE_ADVERT_UNDIRECTED_HIGH, 0, NULL);

    /* Failed to start advertisement. Stop program execution */
    if (WICED_BT_SUCCESS != wiced_result)
    {
        printf("failed to start advertisement! \n");
        CY_ASSERT(0);
    }

    /*
     * Set flag_stay_connected to remain connected after all messages are sent
     * Reset flag to 0, to disconnect
     */
    hello_sensor_state.flag_stay_connected = 1;

    wiced_init_timer(&hello_sensor_led_timer, hello_sensor_led_timeout, 0, WICED_MILLI_SECONDS_TIMER);
}

/**************************************************************************************************
 * Function Name: hello_sensor_interrput_config
 ***************************************************************************************************
 * Summary:
 *   This function initializes a pin as input that triggers interrupt on falling edges.
 *
 * Parameters:
 *   None
 *
 * Return:
 *  None
 *
 *************************************************************************************************/
void hello_sensor_interrput_config(void)
{
    cy_rslt_t result = 0;

    /* Initialize the user button */
    result = cyhal_gpio_init(CYBSP_USER_BTN, CYHAL_GPIO_DIR_INPUT, CYHAL_GPIO_DRIVE_PULLDOWN, CYBSP_BTN_OFF);
    UNUSED_VARIABLE(result);
    /* Configure GPIO interrupt */
    cyhal_gpio_register_callback(CYBSP_USER_BTN, &btn_cb_data);
    cyhal_gpio_enable_event(CYBSP_USER_BTN, CYHAL_GPIO_IRQ_FALL, GPIO_INTERRUPT_PRIORITY, true);
}

/**************************************************************************************************
 * Function Name: hello_sensor_adv_stop_handler
 ***************************************************************************************************
 * Summary:
 *   This function handles advertisement stop event.
 *
 * Parameters:
 *   None
 *
 * Return:
 *  None
 *
 *************************************************************************************************/
void hello_sensor_adv_stop_handler( void )
{
    wiced_result_t result;

    if ( hello_sensor_state.flag_stay_connected && !hello_sensor_state.conn_id )
    {
        result =  wiced_bt_start_advertisements( BTM_BLE_ADVERT_UNDIRECTED_HIGH, 0, NULL );
        if(result != WICED_BT_SUCCESS)
            printf( "Advertisement start failed :%d\r\n", result );
    }
    else
    {
        printf( "Stop Advertisement\r\n");
    }

    UNUSED_VARIABLE(result);
}


/**************************************************************************************************
 * Function Name: hello_sensor_encryption_changed
 ***************************************************************************************************
 * Summary:
 *   Process notification from stack that encryption has been set. If connected
 *   client is registered for notification or indication, it is a good time to
 *   send it out
 *
 * Parameters:
 *   wiced_result_t result : Result of BTM_ENCRYPTION_STATUS_EVT event
 *   uint8_t* bd_addr      : Remote device address
 *
 * Return:
 *  None
 *
 *************************************************************************************************/
void hello_sensor_encryption_changed( wiced_result_t result, uint8_t* bd_addr )
{
    printf("Encryption Change BD:");
    print_bd_address(hello_sensor_hostinfo.bdaddr);
    printf("res: %d\r\n", result);

    /* Connection has been encrypted meaning that we have correct/paired device
     * restore values in the database
     */

    if(app_bt_find_device_in_flash(bd_addr))
    {
        app_bt_read_hostinfo();
    }

    /* If there are outstanding messages that we could not send out because
     * connection was not up and/or encrypted, send them now.  If we are sending
     * indications, we can send only one and need to wait for ack. */
    while ( ( hello_sensor_state.num_to_write != 0 ) && !hello_sensor_state.flag_indication_sent )
    {
        hello_sensor_state.num_to_write--;
        hello_sensor_send_message();
    }
}


/**************************************************************************************************
 * Function Name: hello_sensor_encryption_changed
 ***************************************************************************************************
 * Summary:
 *     Check if client has registered for notification/indication
 *     and send message if appropriate
 *
 * Parameters:
 *   None
 *
 * Return:
 *  None
 *
 *************************************************************************************************/
void hello_sensor_send_message( void )
{
    wiced_bt_gatt_status_t status;
    printf( "hello_sensor_send_message: CCC:%d\n", hello_sensor_hostinfo.characteristic_client_configuration );

    /* If client has not registered for indication or notification, no action */
    if ( hello_sensor_hostinfo.characteristic_client_configuration == 0 )
    {
        return;
    }
    else if ( hello_sensor_hostinfo.characteristic_client_configuration & GATT_CLIENT_CONFIG_NOTIFICATION )
    {
        status = wiced_bt_gatt_server_send_notification(hello_sensor_state.conn_id, HDLC_HELLO_SENSOR_NOTIFY_VALUE,
                app_hello_sensor_notify_len, app_hello_sensor_notify, NULL);
        printf("Notification Status: %d\r\n", status);
    }
    else
    {
        if ( !hello_sensor_state.flag_indication_sent )
        {
            hello_sensor_state.flag_indication_sent = TRUE;
            wiced_bt_gatt_server_send_indication(hello_sensor_state.conn_id, HDLC_HELLO_SENSOR_NOTIFY_VALUE,
                            app_hello_sensor_notify_len, app_hello_sensor_notify, NULL);
        }
    }
}
/**************************************************************************************************
* Function Name: le_app_gatt_event_callback
***************************************************************************************************
* Summary:
*   This function handles GATT events from the BT stack.
*
* Parameters:
*   wiced_bt_gatt_evt_t event                   : LE GATT event code of one byte length
*   wiced_bt_gatt_event_data_t *p_event_data    : Pointer to LE GATT event structures
*
* Return:
*  wiced_bt_gatt_status_t: See possible status codes in wiced_bt_gatt_status_e in wiced_bt_gatt.h
*
**************************************************************************************************/
static wiced_bt_gatt_status_t hello_sensor_gatts_callback(wiced_bt_gatt_evt_t event,
                                                         wiced_bt_gatt_event_data_t *p_event_data)
{
    wiced_bt_gatt_status_t gatt_status = WICED_BT_GATT_ERROR;

    wiced_bt_gatt_attribute_request_t *p_attr_req = &p_event_data->attribute_request;
    /* Call the appropriate callback function based on the GATT event type, and pass the relevant event
     * parameters to the callback function */
    switch ( event )
    {
        case GATT_CONNECTION_STATUS_EVT:
            gatt_status = hello_sensor_gatts_conn_status_cb( &p_event_data->connection_status );
            break;

        case GATT_ATTRIBUTE_REQUEST_EVT:
            gatt_status = hello_sensor_gatts_req_cb(p_attr_req);

            break;
        case GATT_GET_RESPONSE_BUFFER_EVT: /* GATT buffer request, typically sized to max of bearer mtu - 1 */
            p_event_data->buffer_request.buffer.p_app_rsp_buffer =
            hello_sensor_alloc_buffer(p_event_data->buffer_request.len_requested);
            p_event_data->buffer_request.buffer.p_app_ctxt = (void *)hello_sensor_free_buffer;
            gatt_status = WICED_BT_GATT_SUCCESS;
            break;
        case GATT_APP_BUFFER_TRANSMITTED_EVT: /* GATT buffer transmitted event,  check \ref wiced_bt_gatt_buffer_transmitted_t*/
        {
            pfn_free_buffer_t pfn_free = (pfn_free_buffer_t)p_event_data->buffer_xmitted.p_app_ctxt;

            /* If the buffer is dynamic, the context will point to a function to free it. */
            if (pfn_free)
                pfn_free(p_event_data->buffer_xmitted.p_app_data);

            gatt_status = WICED_BT_GATT_SUCCESS;
        }
            break;


        default:
            gatt_status = WICED_BT_GATT_SUCCESS;
               break;
    }

    return gatt_status;
}

/**************************************************************************************************
* Function Name: hello_sensor_set_value
***************************************************************************************************
* Summary:
*   This function handles writing to the attribute handle in the GATT database using the
*   data passed from the BT stack. The value to write is stored in a buffer
*   whose starting address is passed as one of the function parameters
*
* Parameters:
* @param attr_handle  GATT attribute handle
* @param p_val        Pointer to LE GATT write request value
* @param len          length of GATT write request
*
*
* Return:
*   wiced_bt_gatt_status_t: See possible status codes in wiced_bt_gatt_status_e in wiced_bt_gatt.h
*
**************************************************************************************************/
static wiced_bt_gatt_status_t hello_sensor_set_value(uint16_t attr_handle,
                                                uint8_t *p_val,
                                                uint16_t len)
{
    wiced_bool_t isHandleInTable = WICED_FALSE;
    wiced_bool_t validLen = WICED_FALSE;
    wiced_bt_gatt_status_t gatt_status = WICED_BT_GATT_INVALID_HANDLE;
    uint8_t *p_attr   = p_val;
    uint8_t  nv_update = WICED_FALSE;

    /* Check for a matching handle entry */
    for (int i = 0; i < app_gatt_db_ext_attr_tbl_size; i++)
    {
        if (app_gatt_db_ext_attr_tbl[i].handle == attr_handle)
        {
            /* Detected a matching handle in external lookup table */
            isHandleInTable = WICED_TRUE;

            /* Check if the buffer has space to store the data */
            validLen = (app_gatt_db_ext_attr_tbl[i].max_len >= len);

            if (validLen)
            {
                /* Value fits within the supplied buffer; copy over the value */
                app_gatt_db_ext_attr_tbl[i].cur_len = len;
                memcpy(app_gatt_db_ext_attr_tbl[i].p_data, p_val, len);
                gatt_status = WICED_BT_GATT_SUCCESS;

                /* Add code for any action required when this attribute is written.
                 * In this case, we update the IAS led based on the IAS alert
                 * level characteristic value */

                switch ( attr_handle )
                {
                /* By writing into Characteristic Client Configuration descriptor
                    * peer can enable or disable notification or indication */
                case HDLD_HELLO_SENSOR_NOTIFY_CHAR_DESC:
                    if ( len != 2 )
                    {
                        return WICED_BT_GATT_INVALID_ATTR_LEN;
                    }
                    hello_sensor_hostinfo.characteristic_client_configuration = p_attr[0] | ( p_attr[1] << 8 );
                    printf("CCCD update:");
                    print_array(p_attr, 2);
                    printf("\r\n");
                    nv_update = WICED_TRUE;

                    break;

                case HDLC_HELLO_SENSOR_BLINK_VALUE:
                    if (len != 1 )
                    {
                        return WICED_BT_GATT_INVALID_ATTR_LEN;
                    }
                    hello_sensor_hostinfo.number_of_blinks = p_attr[0];
                    if ( hello_sensor_hostinfo.number_of_blinks != 0 )
                    {
                        printf( "hello_sensor_write_handler:num blinks: %d\r\n", hello_sensor_hostinfo.number_of_blinks );

                        hello_sensor_led_blink (250, 250, hello_sensor_hostinfo.number_of_blinks);
                        nv_update = WICED_TRUE;
                    }
                    break;

                default:
                    gatt_status = WICED_BT_GATT_INVALID_HANDLE;

                    break;
                }
            }
            else
            {
                /* Value to write does not meet size constraints */
                gatt_status = WICED_BT_GATT_INVALID_ATTR_LEN;
            }
            break;
        }
    }
    if ( nv_update )
     {
         app_bt_update_hostinfo();
         printf("Updated flash with host info\r\n");
     }
    if (!isHandleInTable)
    {
        /* TODO: Add code to read value for handles not contained within generated lookup table.
         * This is a custom logic that depends on the application, and is not used in the
         * current application. If the value for the current handle is successfully written in the
         * below code snippet, then set the result using:
         * res = WICED_BT_GATT_SUCCESS; */
        switch ( attr_handle )
        {
            default:
                /* The write operation was not performed for the indicated handle */
                printf("Write Request to Invalid Handle: 0x%x\n", attr_handle);
                gatt_status = WICED_BT_GATT_WRITE_NOT_PERMIT;
                break;
        }
    }

    return gatt_status;
}

/**************************************************************************************************
* Function Name: hello_sensor_gatts_req_write_handler
***************************************************************************************************
* Summary:
*   This function handles Write Requests received from the client device
*
* Parameters:
*  @param conn_id       Connection ID
*  @param opcode        LE GATT request type opcode
*  @param p_write_req   Pointer to LE GATT write request
*  @param len_req       length of data requested
*
* Return:
*  wiced_bt_gatt_status_t: See possible status codes in wiced_bt_gatt_status_e in wiced_bt_gatt.h
*
**************************************************************************************************/
static wiced_bt_gatt_status_t hello_sensor_gatts_req_write_handler(uint16_t conn_id,
                                                    wiced_bt_gatt_opcode_t opcode,
                                                    wiced_bt_gatt_write_req_t *p_write_req,
                                                    uint16_t len_req)
{
    wiced_bt_gatt_status_t gatt_status = WICED_BT_GATT_INVALID_HANDLE;

    printf("write_handler: conn_id:%d Handle:0x%x offset:%d len:%d\r\n ", conn_id, p_write_req->handle, p_write_req->offset, p_write_req->val_len );


    /* Attempt to perform the Write Request */

    gatt_status = hello_sensor_set_value(p_write_req->handle,
                                p_write_req->p_val,
                               p_write_req->val_len);

    if( WICED_BT_GATT_SUCCESS != gatt_status )
        {
            printf("WARNING: GATT set attr status 0x%x\n", gatt_status);
        }

        return (gatt_status);
}

/**************************************************************************************************
* Function Name: hello_sensor_gatts_req_read_handler
***************************************************************************************************
* Summary:
*   This function handles Read Requests received from the client device
*
* Parameters:
* @param conn_id       Connection ID
* @param opcode        LE GATT request type opcode
* @param p_read_req    Pointer to read request containing the handle to read
* @param len_req       length of data requested
*
* Return:
*  wiced_bt_gatt_status_t: See possible status codes in wiced_bt_gatt_status_e in wiced_bt_gatt.h
*
**************************************************************************************************/
static wiced_bt_gatt_status_t hello_sensor_gatts_req_read_handler( uint16_t conn_id,
                                                    wiced_bt_gatt_opcode_t opcode,
                                                    wiced_bt_gatt_read_t *p_read_req,
                                                    uint16_t len_req)
{

    gatt_db_lookup_table_t  *puAttribute;
    int          attr_len_to_copy;
    uint8_t     *from;
    int          to_send;


    puAttribute = hello_sensor_find_by_handle(p_read_req->handle);
    if ( NULL == puAttribute )
    {
        wiced_bt_gatt_server_send_error_rsp(conn_id, opcode, p_read_req->handle,
                WICED_BT_GATT_INVALID_HANDLE);
        return WICED_BT_GATT_INVALID_HANDLE;
    }
    attr_len_to_copy = puAttribute->cur_len;

    printf("read_handler: conn_id:%d Handle:%x offset:%d len:%d\r\n ", conn_id, p_read_req->handle, p_read_req->offset, attr_len_to_copy);

    if (p_read_req->offset >= puAttribute->cur_len)
    {
        wiced_bt_gatt_server_send_error_rsp(conn_id, opcode, p_read_req->handle,
                WICED_BT_GATT_INVALID_OFFSET);
        return WICED_BT_GATT_INVALID_OFFSET;
    }
    /* Dummy battery value read increment */
    if( p_read_req->handle == HDLC_BAS_BATTERY_LEVEL_VALUE)
    {
        if (app_bas_battery_level[0]++ > 5)
        {
            app_bas_battery_level[0] = 0;
        }
    }

    to_send = MIN(len_req, attr_len_to_copy - p_read_req->offset);
    from = ((uint8_t *)puAttribute->p_data) + p_read_req->offset;
    return wiced_bt_gatt_server_send_read_handle_rsp(conn_id, opcode, to_send, from, NULL); /* No need for context, as buff not allocated */;
}

/**************************************************************************************************
* Function Name: le_app_connect_handler
***************************************************************************************************
* Summary:
*   This callback function handles connection status changes.
*
* Parameters:
*   wiced_bt_gatt_connection_status_t *p_conn_status  : Pointer to data that has connection details
*
* Return:
*  wiced_bt_gatt_status_t: See possible status codes in wiced_bt_gatt_status_e in wiced_bt_gatt.h
*
**************************************************************************************************/
static wiced_bt_gatt_status_t hello_sensor_gatts_conn_status_cb(wiced_bt_gatt_connection_status_t *p_conn_status)
{
    if (p_conn_status->connected)
    {
        return hello_sensor_gatts_connection_up( p_conn_status );
    }
    else
    {
        return hello_sensor_gatts_connection_down( p_conn_status );
    }
}

/**************************************************************************************************
* Function Name: hello_sensor_gatts_connection_up
***************************************************************************************************
* Summary:
*   This function is invoked when connection is established
*
* Parameters:
*   wiced_bt_gatt_connection_status_t *p_status  : Pointer to data that has connection details
*
* Return:
*  wiced_bt_gatt_status_t: See possible status codes in wiced_bt_gatt_status_e in wiced_bt_gatt.h
*
**************************************************************************************************/
static wiced_bt_gatt_status_t hello_sensor_gatts_connection_up( wiced_bt_gatt_connection_status_t *p_status )
{
    printf("hello_sensor_conn_up ");
    print_bd_address(p_status->bd_addr);
    printf("Connection ID '%d' \r\n", p_status->conn_id);

    /* Update the connection handler.  Save address of the connected device. */
    hello_sensor_state.conn_id = p_status->conn_id;
    memcpy(hello_sensor_state.remote_addr, p_status->bd_addr, sizeof(wiced_bt_device_address_t));

    /* Saving host info in NVRAM  */
    memcpy( hello_sensor_hostinfo.bdaddr, p_status->bd_addr, sizeof( wiced_bt_device_address_t ) );
    hello_sensor_hostinfo.characteristic_client_configuration = 0;
    hello_sensor_hostinfo.number_of_blinks                    = 0;
    app_bt_update_hostinfo();

    /* Update the adv/conn state */
    return WICED_BT_GATT_SUCCESS;
}

/**************************************************************************************************
* Function Name: hello_sensor_gatts_connection_down
***************************************************************************************************
* Summary:
*   This function is invoked when connection is disconnected
*
* Parameters:
*   wiced_bt_gatt_connection_status_t *p_status  : Pointer to data that has connection details
*
* Return:
*  wiced_bt_gatt_status_t: See possible status codes in wiced_bt_gatt_status_e in wiced_bt_gatt.h
*
**************************************************************************************************/
static wiced_bt_gatt_status_t hello_sensor_gatts_connection_down( wiced_bt_gatt_connection_status_t *p_status )
{
    wiced_result_t result;
    printf( "connection_down");
    print_bd_address(p_status->bd_addr);

    printf("conn_id:%d reason:%s\r\n", p_status->conn_id, get_bt_gatt_disconn_reason_name(p_status->reason) );

    /* Resetting the device info */
    memset( hello_sensor_state.remote_addr, 0, 6 );
    hello_sensor_state.conn_id = 0;

    /*
     * If we are configured to stay connected, disconnection was
     * caused by the peer, start low advertisements, so that peer
     * can connect when it wakes up
     */
    if ( hello_sensor_state.flag_stay_connected )
    {
        result =  wiced_bt_start_advertisements( BTM_BLE_ADVERT_UNDIRECTED_HIGH, 0, NULL );
        if(result != WICED_BT_SUCCESS)
            printf( "Start advertisement failed: %d\r\n", result );
    }

    return WICED_BT_GATT_SUCCESS;
}

/**************************************************************************************************
* Function Name: hello_sensor_gatts_req_cb
***************************************************************************************************
* Summary:
*   This function handles GATT server events from the BT stack.
*
* Parameters:
*  p_attr_req     Pointer to LE GATT connection status
*
* Return:
*  wiced_bt_gatt_status_t: See possible status codes in wiced_bt_gatt_status_e in wiced_bt_gatt.h
*
**************************************************************************************************/
static wiced_bt_gatt_status_t hello_sensor_gatts_req_cb (wiced_bt_gatt_attribute_request_t *p_attr_req)
{
    wiced_bt_gatt_status_t gatt_status = WICED_BT_GATT_ERROR;
    switch ( p_attr_req->opcode )
    {
        case GATT_REQ_READ:
        case GATT_REQ_READ_BLOB:
             /* Attribute read request */
            gatt_status = hello_sensor_gatts_req_read_handler( p_attr_req->conn_id,p_attr_req->opcode,
                                                       &p_attr_req->data.read_req,
                                                       p_attr_req->len_requested);
             break;

        case GATT_REQ_WRITE:
        case GATT_CMD_WRITE:
             /* Attribute write request */
             gatt_status = hello_sensor_gatts_req_write_handler(p_attr_req->conn_id, p_attr_req->opcode,
                                           &p_attr_req->data.write_req,
                                           p_attr_req->len_requested );
             if ((p_attr_req->opcode == GATT_REQ_WRITE) && (gatt_status == WICED_BT_GATT_SUCCESS))
             {
                 wiced_bt_gatt_write_req_t *p_write_request = &p_attr_req->data.write_req;
                 wiced_bt_gatt_server_send_write_rsp(p_attr_req->conn_id, p_attr_req->opcode, p_write_request->handle);
             }
             break;

        case GATT_REQ_MTU:
            gatt_status = wiced_bt_gatt_server_send_mtu_rsp(p_attr_req->conn_id,
                                                       p_attr_req->data.remote_mtu,
                                                       CY_BT_MTU_SIZE);
             break;

        case GATT_HANDLE_VALUE_NOTIF:
                    printf("Notification send complete\n");
             break;

        case GATT_REQ_READ_BY_TYPE:
            gatt_status = hello_sensor_gatt_req_read_by_type_handler(p_attr_req->conn_id, p_attr_req->opcode,
                                                           &p_attr_req->data.read_by_type, p_attr_req->len_requested);
            break;

        case GATT_HANDLE_VALUE_CONF:
             gatt_status = hello_sensor_gatts_req_conf_handler( p_attr_req->conn_id, p_attr_req->data.confirm.handle);
             break;

        default:
                printf("ERROR: Unhandled GATT Connection Request case: %d\n", p_attr_req->opcode);
                break;
    }

    return gatt_status;
}

/*******************************************************************************
 * Function Name: hello_sensor_free_buffer
 *******************************************************************************
 * Summary:
 *  This function frees up the memory buffer
 *
 *
 * Parameters:
 *  uint8_t *p_data: Pointer to the buffer to be free
 *
 ******************************************************************************/
static void hello_sensor_free_buffer(uint8_t *p_buf)
{
    vPortFree(p_buf);
}

/*******************************************************************************
 * Function Name: hello_sensor_alloc_buffer
 *******************************************************************************
 * Summary:
 *  This function allocates a memory buffer.
 *
 *
 * Parameters:
 *  int len: Length to allocate
 *
 ******************************************************************************/
static void* hello_sensor_alloc_buffer(int len)
{
    return pvPortMalloc(len);
}

/*******************************************************************************
 * Function Name : hello_sensor_find_by_handle
 * *****************************************************************************
 * Summary :
 *    Find attribute description by handle
 *
 * Parameters:
 *  uint16_t handle    handle to look up
 *
 * Return:
 *  gatt_db_lookup_table_t   pointer containing handle data
 ******************************************************************************/
static gatt_db_lookup_table_t  *hello_sensor_find_by_handle(uint16_t handle)
{
    int i;
    for (i = 0; i < app_gatt_db_ext_attr_tbl_size; i++)
    {
        if (handle == app_gatt_db_ext_attr_tbl[i].handle )
        {
            return (&app_gatt_db_ext_attr_tbl[i]);
        }
    }
    return NULL;
}

/*******************************************************************************
 * Function Name : hello_sensor_gatt_req_read_by_type_handler
 * *****************************************************************************
 * Summary :
 *    Process read-by-type request from peer device
 *
 * Parameters:
 *  uint16_t                      conn_id       : Connection ID
 *  wiced_bt_gatt_opcode_t        opcode        : LE GATT request type opcode
 *  wiced_bt_gatt_read_by_type_t  p_read_req    : Pointer to read request containing the
 *                                                handle to read
 *  uint16_t                      len_requested : Length of data requested
 *
 * Return:
 *  wiced_bt_gatt_status_t  LE GATT status
 ******************************************************************************/
static wiced_bt_gatt_status_t hello_sensor_gatt_req_read_by_type_handler(uint16_t conn_id,
                                                                   wiced_bt_gatt_opcode_t opcode,
                                                                   wiced_bt_gatt_read_by_type_t *p_read_req,
                                                                   uint16_t len_requested)
{
    gatt_db_lookup_table_t *puAttribute;
    uint16_t last_handle = 0;
    uint16_t attr_handle = p_read_req->s_handle;
    uint8_t *p_rsp = hello_sensor_alloc_buffer(len_requested);
    uint8_t pair_len = 0;
    int used_len = 0;

    if (NULL == p_rsp)
    {
        printf("No memory, len_requested: %d!!\r\n",len_requested);
        wiced_bt_gatt_server_send_error_rsp(conn_id, opcode, attr_handle, WICED_BT_GATT_INSUF_RESOURCE);
        return WICED_BT_GATT_INSUF_RESOURCE;
    }

    /* Read by type returns all attributes of the specified type, between the start and end handles */
    while (WICED_TRUE)
    {
        last_handle = attr_handle;
        attr_handle = wiced_bt_gatt_find_handle_by_type(attr_handle, p_read_req->e_handle,
                                                        &p_read_req->uuid);
        if (0 == attr_handle )
            break;

        if ( NULL == (puAttribute = hello_sensor_find_by_handle(attr_handle)))
        {
            printf("found type but no attribute for %d \r\n",last_handle);
            wiced_bt_gatt_server_send_error_rsp(conn_id, opcode, p_read_req->s_handle,
                                                WICED_BT_GATT_ERR_UNLIKELY);
            hello_sensor_free_buffer(p_rsp);
            return WICED_BT_GATT_INVALID_HANDLE;
        }

        {
            int filled = wiced_bt_gatt_put_read_by_type_rsp_in_stream(p_rsp + used_len, len_requested - used_len, &pair_len,
                                                                attr_handle, puAttribute->cur_len, puAttribute->p_data);
            if (0 == filled)
            {
                break;
            }
            used_len += filled;
        }

        /* Increment starting handle for next search to one past current */
        attr_handle++;
    }

    if (0 == used_len)
    {
       printf("attr not found  start_handle: 0x%04x  end_handle: 0x%04x  Type: 0x%04x\r\n",
               p_read_req->s_handle, p_read_req->e_handle, p_read_req->uuid.uu.uuid16);
        wiced_bt_gatt_server_send_error_rsp(conn_id, opcode, p_read_req->s_handle, WICED_BT_GATT_INVALID_HANDLE);
        hello_sensor_free_buffer(p_rsp);
        return WICED_BT_GATT_INVALID_HANDLE;
    }

    /* Send the response */

    return wiced_bt_gatt_server_send_read_by_type_rsp(conn_id, opcode, pair_len, used_len, p_rsp, (void *)hello_sensor_free_buffer);
}

/*******************************************************************************
 * Function Name : hello_sensor_gatts_req_conf_handler
 * *****************************************************************************
 * Summary :
 *    Process indication confirm. If client wanted us to use indication instead of
 *    notifications we have to wait for confirmation after every message sent.
 *    For example if user pushed button twice very fast we will send first message,
 *    then wait for confirmation, then send second message, then wait for
 *    confirmation and if configured start idle timer only after that.
 *
 * Parameters:
 *  uint16_t  conn_id : Connection ID
 *  uint16_t  handle  : Handle of attribute
 *
 * Return:
 *  wiced_bt_gatt_status_t  LE GATT status
 ******************************************************************************/
static wiced_bt_gatt_status_t hello_sensor_gatts_req_conf_handler( uint16_t conn_id, uint16_t handle )
{
    printf( "hello_sensor_indication_cfm, conn %d hdl %d\n", conn_id, handle );

    if ( !hello_sensor_state.flag_indication_sent )
    {
        printf("Hello: Wrong Confirmation!\r\n");
        return WICED_BT_GATT_SUCCESS;
    }

    hello_sensor_state.flag_indication_sent = 0;

    /* We might need to send more indications */
    if ( hello_sensor_state.num_to_write )
    {
        hello_sensor_state.num_to_write--;
        hello_sensor_send_message();
    }

    return WICED_BT_GATT_SUCCESS;
}

/*******************************************************************************
 * Function Name : hello_sensor_smp_bond_result
 * *****************************************************************************
 * Summary :
 *    Process SMP bonding result. If we successfully paired with the central
 *    device, save its BDADDR in the NVRAM and initialize associated data
 *
 *
 * Parameters:
 *    uint8_t result : Result of BTM_PAIRING_COMPLETE_EVT event
 *
 * Return:
 *    None
 ******************************************************************************/
static void hello_sensor_smp_bond_result( uint8_t result )
{
    uint8_t written_byte = 0;

    /* Bonding success */
    if ( result == WICED_BT_SUCCESS )
    {
        /* Pack the data to be stored into the hostinfo structure */
        memcpy( hello_sensor_hostinfo.bdaddr, hello_sensor_state.remote_addr, sizeof( wiced_bt_device_address_t ) );

        app_bt_update_hostinfo();
    }

    UNUSED_VARIABLE(written_byte);
}

/*******************************************************************************
* Function Name: gpio_interrupt_handler
********************************************************************************
* Summary:
*   GPIO interrupt handler.
*
* Parameters:
*  void *handler_arg (unused)
*  cyhal_gpio_irq_event_t (unused)
*
* Return
*  None
*
*******************************************************************************/
static void gpio_interrupt_handler(void *handler_arg, cyhal_gpio_event_t event)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if(event == CYHAL_GPIO_IRQ_FALL)
    {
        vTaskNotifyGiveFromISR(button_handle, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
    }
}

/*******************************************************************************
* Function Name: hello_sensor_led_timeout
********************************************************************************
* Summary:
*  The function invoked on timeout of led timer.
*
* Parameters:
*  void * count
*
* Return
*  None
*
*******************************************************************************/
void hello_sensor_led_timeout(void *count)
{
    static wiced_bool_t led_on = WICED_TRUE;
    if ( led_on )
    {
        cyhal_gpio_write(CYBSP_USER_LED1 , CYBSP_LED_STATE_OFF);
        if (--hello_sensor_led_blink_count)
        {
            led_on = WICED_FALSE;
            wiced_start_timer( &hello_sensor_led_timer, hello_sensor_led_off_ms );
        }
    }
    else
    {
        led_on = WICED_TRUE;
        cyhal_gpio_write(CYBSP_USER_LED1 , CYBSP_LED_STATE_ON);
        wiced_start_timer( &hello_sensor_led_timer, hello_sensor_led_on_ms );
    }
}

/*******************************************************************************
* Function Name: hello_sensor_led_blink
********************************************************************************
* Summary:
*  The function blinks the LED at specified rate.
*
* Parameters:
*  uint16_t on_ms
*  uint16_t off_ms
*  uint8_t num_of_blinks
*
* Return
*  None
*
*******************************************************************************/
void hello_sensor_led_blink(uint16_t on_ms, uint16_t off_ms, uint8_t num_of_blinks )
{
    if (num_of_blinks)
    {
        hello_sensor_led_blink_count = num_of_blinks;
        hello_sensor_led_off_ms = off_ms;
        hello_sensor_led_on_ms = on_ms;
        cyhal_gpio_write(CYBSP_USER_LED1 , CYBSP_LED_STATE_ON);
        wiced_stop_timer(&hello_sensor_led_timer);
        wiced_start_timer(&hello_sensor_led_timer, on_ms);
    }
}

/*******************************************************************************
* Function Name: hello_sensor_timeout
********************************************************************************
* Summary:
*  The function is invoked on timeout of app seconds timer.
*
* Parameters:
*  Timerhandle
*
* Return
*  None
*
*******************************************************************************/
void hello_sensor_timeout(TimerHandle_t timer_handle)
{
    hello_sensor_state.timer_count++;
    /* print for first 10 seconds, then once every 10 seconds thereafter */
    if ((hello_sensor_state.timer_count <= 10) || (hello_sensor_state.timer_count % 10 == 0))
        printf("hello_sensor_timeout: %lu, ft:%lu\n",
              (unsigned long)hello_sensor_state.timer_count,(unsigned long)hello_sensor_state.fine_timer_count );
}

/*******************************************************************************
* Function Name: hello_sensor_fine_timeout
********************************************************************************
* Summary:
*  The function invoked on timeout of app milliseconds fine timer
*
* Parameters:
*  Timerhandle
*
* Return
*  None
*
*******************************************************************************/
void hello_sensor_fine_timeout(TimerHandle_t timer_handle)
{
    hello_sensor_state.fine_timer_count++;
}

/*******************************************************************************
* Function Name: hello_sensor_load_keys_for_address_resolution
********************************************************************************
* Summary:
*  Add link keys to address resolution list
*
* Parameters:
*  None
*
* Return
*  None
*
*******************************************************************************/
static void hello_sensor_load_keys_for_address_resolution( void )
{
    wiced_result_t result = wiced_bt_dev_add_device_to_address_resolution_db(&peer_link_keys);

    if (WICED_BT_SUCCESS == result)
    {
        printf("Device added to address resolution database: ");
        print_bd_address((uint8_t *)&peer_link_keys.bd_addr);
    }
    else
    {
        printf("Error adding device to address resolution database, Error Code %d \r\n", result);
    }
}

/*******************************************************************************
* Function Name: hello_sensor_gatts_increment_notify_value
********************************************************************************
* Summary:
 * Keep number of the button pushes in the last byte of the Hello message.
 * That will guarantee that if client reads it, it will have correct data.
*
* Parameters:
*  None
*
* Return
*  None
*
*******************************************************************************/
void hello_sensor_gatts_increment_notify_value( void )
{
    /* Getting the last byte */
    int last_byte = app_hello_sensor_notify_len - 1 ;
    char c = app_hello_sensor_notify[last_byte];

    c++;
    if ( (c < '0') || (c > '9') )
    {
        c = '0';
    }
    app_hello_sensor_notify[last_byte] = c;
}

/*******************************************************************************
* Function Name: button_task
********************************************************************************
* Summary:
 *  This is a FreeRTOS task that handles the button press events.
*
* Parameters:
*  None
*
* Return
*  None
*
*******************************************************************************/
void button_task(void *arg)
{
   static uint32_t previous_button_pressed_timer = 0;
   static uint32_t current_button_pressed_timer  = 0;

    for(;;)
    {
        ulTaskNotifyTake(pdTRUE,  portMAX_DELAY);
        current_button_pressed_timer = hello_sensor_state.fine_timer_count;

       if ((current_button_pressed_timer - previous_button_pressed_timer) > 50)
       {
           printf("button pressed\n");
           previous_button_pressed_timer = current_button_pressed_timer;

            // Blink as configured
            hello_sensor_led_blink( 250, 250, hello_sensor_hostinfo.number_of_blinks);

            /* Increment the last byte of the hello sensor notify value */
            hello_sensor_gatts_increment_notify_value();

            /* Remember how many messages we need to send */
            hello_sensor_state.num_to_write++;

            /* If connection is down, start high duty advertisements, so client can connect */
            if ( hello_sensor_state.conn_id == 0 )
            {
                wiced_result_t result;

                printf( "Starting Undirected High Advertisement\r\n");
                result = wiced_bt_start_advertisements( BTM_BLE_ADVERT_UNDIRECTED_HIGH, 0, NULL );
                if(result != WICED_BT_SUCCESS)
                    printf( "Start advertisement failed: %d\r\n", result );

                UNUSED_VARIABLE(result);
            }

            /*
             * Connection up.
             * Send message if client registered to receive indication
             * or notification. After we send an indication wait for the ack
             * before we can send anything else
             */
            printf("No. to write: %d\r\n", hello_sensor_state.num_to_write );
            printf("flag_indication_sent: %d \r\n", hello_sensor_state.flag_indication_sent);
            while ( ( hello_sensor_state.num_to_write != 0 ) && !hello_sensor_state.flag_indication_sent )
            {
                hello_sensor_state.num_to_write--;
                hello_sensor_send_message();
            }
        }
   }
}

/* END OF FILE [] */
