/*******************************************************************************
 * File Name: app_hw_device.c
 *
 * Description: This file contains functions for initialization and usage of
 *              device peripheral GPIO for LED and button. It also
 *              contains FreeRTOS timer implementations.
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
 ******************************************************************************/

/*******************************************************************************
 * Header Files
 ******************************************************************************/

#include "inttypes.h"
#include <FreeRTOS.h>
#include <task.h>
#include "app_bt_bonding.h"
#include "app_flash_common.h"
#include "cycfg_gap.h"
#include "app_bt_utils.h"
#include "app_bt_event_handler.h"
#include "app_bt_gatt_handler.h"
#include "app_hw_device.h"
#ifdef ENABLE_BT_SPY_LOG
#include "cybt_debug_uart.h"
#endif

/*******************************************************************************
 * Macro Definitions
 ******************************************************************************/
#define APP_BTN_PRESS_SHORT            (50)
#define APP_BTN_PRESS_LONG             (10000)
#define APP_TIMEOUT_TEN_SEC_FACTOR     APP_BTN_PRESS_LONG
#define APP_TIMEOUT_ONE_SEC_FACTOR     (1000u)
#define APP_TIMEOUT_MS                 (1)
#define APP_TIMEOUT_LED_INDICATE       (500)
#define APP_TIMEOUT_LED_BLINK          (250)

#define MAXIMUM_LED_BLINK_COUNT        (20)

/* Interrupt priority for GPIO connected to button */
#define GPIO_INTERRUPT_PRIORITY         (4)
/* Stack size for Hello Sensor BTN task */
#define BTN_TASK_STACK_SIZE             (512u)
/* Task Priority of Hello Sensor BTN Task */
#define BTN_TASK_PRIORITY               (2)

/*******************************************************************************
 * Variable Definitions
 ******************************************************************************/

/* This is a one shot timer that is used to blink the LED the number of times 
   written in the led_blink characteristic in GATT database  by the client */
TimerHandle_t timer_led_blink;

/* This timer is used to toggle LED to indicate the 10 sec button press duration */
TimerHandle_t ms_timer_led_indicate;

/* ms_timer is a periodic timer that ticks every millisecond.
 * This timer is used to measure the duration of button press events */
TimerHandle_t ms_timer;

/* Variables to hold the timer count values */
uint8_t led_blink_count;

/* Handle of the button task */
TaskHandle_t button_handle;

/* Variable used to determine if indication should start or stop */
static uint8_t led_indicate_state;

/* For button press interrupt */
cyhal_gpio_callback_data_t btn_cb_data =
{
    .callback     = app_bt_gpio_interrupt_handler,
    .callback_arg = NULL
};

/**
 * Function Name: app_bt_led_blink
 *
 * Function Description:
 *   @brief The function blinks the LED at specified rate.
 *
 *   @param uint8_t num_of_blinks: number written by peer to blink the LED
 *
 * @return None
 *
 */
void app_bt_led_blink(uint8_t num_of_blinks)
{
    if (num_of_blinks)
    {
        led_blink_count = num_of_blinks;
        cyhal_gpio_write(CYBSP_USER_LED1 , CYBSP_LED_STATE_ON);
        xTimerStart(timer_led_blink, 0);
    }
}

/**
 * Function Name: app_bt_timeout_ms
 *
 * Function Description:
 *   @brief The function invoked on timeout of FreeRTOS millisecond timer. It is
 *          used to print logs over the serial terminal every 10 seconds and to
 *          keep store time elapsed required to calculate button press duration.
 *
 *   @param Timerhandle_t timer_handle: unused
 *
 *   @return None
 *
 */
void app_bt_timeout_ms(TimerHandle_t timer_handle)
{
    hello_sensor_state.timer_count_ms++;
    /* print for every 10 seconds */
    if (0 == (hello_sensor_state.timer_count_ms % APP_TIMEOUT_TEN_SEC_FACTOR))
    {
        printf("hello_sensor_timeout in second: %lu\n",
               (unsigned long)hello_sensor_state.timer_count_ms/APP_TIMEOUT_ONE_SEC_FACTOR);
    }
}

/**
 * Function Name: app_bt_timeout_led_indicate
 *
 * Function Description:
 *   @brief The function is invoked on timeout of FreeRTOS milliseconds timer.
 *          It is used to toggle the LED used for indication of button press
 *          duration. The LED will blink for 10 seconds to show that the button
 *          can be released to clear the bond data.
 *
 *   @param Timerhandle_t timer_handle: unused
 *
 *   @return None
 *
 */
void app_bt_timeout_led_indicate(TimerHandle_t timer_handle)
{
    static uint8_t led_indicate_count;

    led_indicate_count++;
    if(TRUE == led_indicate_state)
    {
        cyhal_gpio_toggle(CYBSP_USER_LED2);
    }
    if(led_indicate_count == MAXIMUM_LED_BLINK_COUNT)
    {
        xTimerStop(ms_timer_led_indicate, 0);
        cyhal_gpio_write(CYBSP_USER_LED2 , CYBSP_LED_STATE_OFF);
        led_indicate_count = 0;
        led_indicate_state = 0;
    }
}

/**
 * Function Name: app_bt_timeout_led_blink
 *
 * Function Description:
 *   @brief The function is for the one shot FreeRTOS timer and it toggles the
 *          LED according to the number written by client in the led_blink
 *          characteristic.
 *
 *   @param TimerHandle_t timer_handle: Unused
 *
 *   @return None
 *
 */
void app_bt_timeout_led_blink(TimerHandle_t timer_handle)
{
    static wiced_bool_t led_on = WICED_TRUE;
    if (led_on)
    {
        cyhal_gpio_write(CYBSP_USER_LED1, CYBSP_LED_STATE_OFF);
        if (--led_blink_count)
        {
            led_on = WICED_FALSE;
            xTimerStart(timer_led_blink, 0);
        }
    }
    else
    {
        led_on = WICED_TRUE;
        cyhal_gpio_write(CYBSP_USER_LED1 , CYBSP_LED_STATE_ON);
        xTimerStart(timer_led_blink, 0);
    }
}

/**
 * Function Name: app_bt_interrupt_config
 *
 * Function Description:
 *   @brief This function initializes a pin as input that triggers interrupt on
 *   falling edges.
 *
 *   @param None
 *
 *   @return None
 *
 */
void app_bt_interrupt_config(void)
{
    cy_rslt_t result=0;

    /* Initialize the user button */
    result = cyhal_gpio_init(CYBSP_USER_BTN,
                             CYHAL_GPIO_DIR_INPUT,
                             CYHAL_GPIO_DRIVE_PULLDOWN,
                             CYBSP_BTN_OFF);
    /* Configure GPIO interrupt */
    cyhal_gpio_register_callback(CYBSP_USER_BTN, &btn_cb_data);

    cyhal_gpio_enable_event(CYBSP_USER_BTN,
                            CYHAL_GPIO_IRQ_BOTH,
                            GPIO_INTERRUPT_PRIORITY,
                            true);
    UNUSED_VARIABLE(result);
}

/**
 * Function Name: app_bt_gpio_interrupt_handler
 *
 * Function Description:
 *   @brief GPIO interrupt handler.
 *
 *   @param void *handler_arg (unused)
 *   @param cyhal_gpio_irq_event_t (unused)
 *
 *   @return None
 *
 */
void app_bt_gpio_interrupt_handler(void *handler_arg, cyhal_gpio_event_t event)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if((CYHAL_GPIO_IRQ_RISE == event) || (CYHAL_GPIO_IRQ_FALL == event))
    {
        vTaskNotifyGiveFromISR(button_handle, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

/**
 * Function Name: app_bt_hw_init
 *
 * Function Description:
 *   @brief This function initializes the LEDs and FreeRTOS Timers.
 *
 *   @param None
 *
 *   @return None
 *
 */

void app_bt_hw_init()
{
    cyhal_gpio_init(CYBSP_USER_LED1 , CYHAL_GPIO_DIR_OUTPUT,
                    CYHAL_GPIO_DRIVE_STRONG, CYBSP_LED_STATE_OFF);
    cyhal_gpio_init(CYBSP_USER_LED2 , CYHAL_GPIO_DIR_OUTPUT,
                    CYHAL_GPIO_DRIVE_STRONG, CYBSP_LED_STATE_OFF);

    /* Starting a log print timer for button press duration
     * and application traces  */
    ms_timer = xTimerCreate("ms_timer",
                            pdMS_TO_TICKS(APP_TIMEOUT_MS),
                            pdTRUE,
                            NULL,
                            app_bt_timeout_ms);
    xTimerStart(ms_timer, APP_TIMEOUT_MS);

    /* Starting a 1ms timer for indication LED used to show button press duration */
    ms_timer_led_indicate = xTimerCreate("ms_timer_led_indicate",
                                          pdMS_TO_TICKS(APP_TIMEOUT_LED_INDICATE),
                                          pdTRUE,
                                          NULL,
                                          app_bt_timeout_led_indicate);

    /* Create a one shot timer for LED blink */
    timer_led_blink = xTimerCreate("led_timer",
                                    pdMS_TO_TICKS(APP_TIMEOUT_LED_BLINK),
                                    pdFALSE,
                                    NULL,
                                    app_bt_timeout_led_blink);

    xTaskCreate(button_task,
                "Button task",
                BTN_TASK_STACK_SIZE,
                NULL,
                BTN_TASK_PRIORITY,
                &button_handle);
}

/**
 * Function Name: button_task
 *
 * Function Description:
 *   @brief This is a FreeRTOS task that handles the button press events.
 *
 *   @param None
 *
 *   @return None
 *
 */
void button_task(void *arg)
{
    static uint32_t btn_press_duration = 0;
    static uint32_t btn_press_start = 0;
    cy_rslt_t rslt = CY_RSLT_SUCCESS;

    for (;;)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if(CYBSP_BTN_PRESSED == cyhal_gpio_read(CYBSP_USER_BTN))
        {
            led_indicate_state = TRUE;
            btn_press_start = hello_sensor_state.timer_count_ms;
            cyhal_gpio_write(CYBSP_USER_LED2 , CYBSP_LED_STATE_ON);
            xTimerStart(ms_timer_led_indicate, 0);
        }
        else if(0 != btn_press_start)
        {
            /* Don't toggle the LED for 10 seconds as this is a short button press */
            led_indicate_state = FALSE;

            printf("Button pressed\n");
            cyhal_gpio_write(CYBSP_USER_LED2 , CYBSP_LED_STATE_OFF);
            btn_press_duration = hello_sensor_state.timer_count_ms - btn_press_start;
            if((btn_press_duration > APP_BTN_PRESS_SHORT) &&
               (btn_press_duration <= APP_BTN_PRESS_LONG))
            {
                /* If connection is down, start high duty advertisements,
                 * so client can connect */
                if (0 == hello_sensor_state.conn_id)
                {
                    wiced_result_t result;

                    printf("Starting Undirected High Advertisement\n");
                    result = wiced_bt_start_advertisements(BTM_BLE_ADVERT_UNDIRECTED_HIGH,
                                                           0,
                                                           NULL);
                    if (result != WICED_BT_SUCCESS)
                        printf("Start advertisement failed: %d\n", result);

                    UNUSED_VARIABLE(result);
                }
                else
                {
                    /* Increment the last byte of the hello sensor notify value */
                    app_bt_gatt_increment_notify_value();

                    /* Remember how many messages we need to send */
                    hello_sensor_state.num_to_send++;

                    /* Connection up.
                     * Send message if client registered to receive indication
                     * or notification. After we send an indication wait for the
                     * ack before we can send anything else */

                    printf("No. to write: %d\n",
                           hello_sensor_state.num_to_send);
                    printf("flag_indication_sent: %d \n",
                           hello_sensor_state.flag_indication_sent);
                    while ((0 != hello_sensor_state.num_to_send) &&
                           (FALSE == hello_sensor_state.flag_indication_sent))
                    {
                        hello_sensor_state.num_to_send--;
                        app_bt_send_message();
                    }
                }
            }
            else if(btn_press_duration > APP_BTN_PRESS_LONG)
            {
                printf("Button pressed more than 10 seconds,"
                       "attempting to clear bond info\n");
                if (0 == hello_sensor_state.conn_id)
                {
                    /* Reset Kv-store library, this will clear the flash */
                    rslt = mtb_kvstore_reset(&kvstore_obj);
                    if(CY_RSLT_SUCCESS == rslt)
                    {
                        printf("Successfully reset kv-store library,"
                               "Please reset the device to generate new keys!\n");
                    }
                    else
                    {
                        printf("failed to reset kv-store libray\n");
                    }
                    /* Clear peer link keys and identity keys structure */
                    memset(&bond_info, 0, sizeof(bond_info));
                    memset(&identity_keys, 0, sizeof(identity_keys));
                }
                else
                {
                    printf("Existing connection present, "
                           "Can't reset bonding information\n");
                }
            }
        }
    }
}