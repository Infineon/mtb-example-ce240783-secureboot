/******************************************************************************
* File Name:   main.c
*
* Description: This is code example demonstrates PSOC Control MCU secure boot.
* There are 2 steps involved to boot the application securely.
* 1. The device should be provisioned with right boot configuration and
* OEM private key.
*   a. In this demonstration boot_cfg_id is configured as BOOT_ONE_SLOT.
*   b. BOOT_ONE_SLOT - Only one image shall be allowed in the user flash area.
* 2. The user application .hex should be converted in to MCUboot image format
* and the image should be signed with OEM public key.
* 3. Point 2 is added as a postbuild.mk script in this project.
*
* Related Document: See README.md
*
*
*******************************************************************************
* Copyright 2024, Cypress Semiconductor Corporation (an Infineon company) or
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
#include "cy_pdl.h"
#include "cybsp.h"
#include "cy_retarget_io.h"
#include "mtb_hal.h"
#include <string.h>


/*******************************************************************************
* Macros
*******************************************************************************/


/*******************************************************************************
* Global Variables
*******************************************************************************/
const cy_stc_sysint_t intrCfg1 =
{
        .intrSrc = TCPWM_COUNTER_IRQ,
        .intrPriority = 7u
};

volatile bool timer_interrupt_flag = false;
bool led_blink_active_flag = true;

/* Variable for storing character read from terminal */
uint8_t uart_read_value;

/* For the Retarget -IO (Debug UART) usage */
static cy_stc_scb_uart_context_t    DEBUG_UART_context;           /** UART context */
static mtb_hal_uart_t               DEBUG_UART_hal_obj;           /** Debug UART HAL object  */


/*******************************************************************************
* Function Prototypes
*******************************************************************************/
void timer_init(void);
void isr_timer();


/*******************************************************************************
* Function Name: main
********************************************************************************
* Summary:
* This is the main function for PSOC Control MCU secure boot code example.
* This function sets up a 1Hz periodic timer to blink the LED.
* The while loop monitors the "Enter" key press and stops/restarts the LED blink
* GPIO
*
* Parameters:
*  none
*
* Return:
*  int
*
*******************************************************************************/
int main(void)
{
    cy_rslt_t result;
    int secureBootOption = 0x00;

#if defined (CY_DEVICE_SECURE)
    cyhal_wdt_t wdt_obj;

    /* Clear watchdog timer so that it doesn't trigger a reset */
    result = cyhal_wdt_init(&wdt_obj, cyhal_wdt_get_max_timeout_ms());
    CY_ASSERT(CY_RSLT_SUCCESS == result);
    cyhal_wdt_free(&wdt_obj);
#endif

    /* Initialize the device and board peripherals */
    result = cybsp_init();

    /* Board init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Enable global interrupts */
    __enable_irq();

    /* Debug UART init */
    result = (cy_rslt_t)Cy_SCB_UART_Init(DEBUG_UART_HW, &DEBUG_UART_config, &DEBUG_UART_context);

    /* UART init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    Cy_SCB_UART_Enable(DEBUG_UART_HW);

    /* Setup the HAL UART */
    result = mtb_hal_uart_setup(&DEBUG_UART_hal_obj, &DEBUG_UART_hal_config, &DEBUG_UART_context, NULL);

    /* HAL UART init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    result = cy_retarget_io_init(&DEBUG_UART_hal_obj);

    /* HAL retarget_io init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* \x1b[2J\x1b[;H - ANSI ESC sequence for clear screen */
    printf("\x1b[2J\x1b[;H");

    printf("****************** "
           "PSOC Control MCU Secure Boot Application Example "
           "****************** \r\n\n");

    secureBootOption = strcmp (SECURE_BOOT_MSG, "TRUE");
    printf("Application is successfully launched\r\n\n");

    printf("Secure boot:    %s\r\nVersion:  %s\r\n\n",
                (secureBootOption)? "DISABLED":"ENABLED", IMG_VER_MSG);

    printf("For more projects, "
           "visit our code examples repositories:\r\n\n");

    printf("https://github.com/Infineon/"
           "Code-Examples-for-ModusToolbox-Software\r\n\n");

    printf("Press 'Enter' key to pause or "
           "resume blinking the user LED \r\n\r\n");

    /* Initialize timer to toggle the LED */
    timer_init();

    for (;;)
    {
        /* Check if 'Enter' key was pressed */
        if (mtb_hal_uart_get(&DEBUG_UART_hal_obj, &uart_read_value, 1)
             == CY_RSLT_SUCCESS)
        {
            if (uart_read_value == '\r')
            {
                /* Pause LED blinking by stopping the timer */
                if (led_blink_active_flag)
                {
                    Cy_TCPWM_TriggerStopOrKill_Single(TCPWM_COUNTER_HW, TCPWM_COUNTER_NUM);
                    printf("LED blinking paused \r\n");
                }
                else /* Resume LED blinking by starting the timer */
                {
                    Cy_TCPWM_TriggerStart_Single(TCPWM_COUNTER_HW, TCPWM_COUNTER_NUM);
                    printf("LED blinking resumed\r\n");
                }
                /* Move cursor to previous line */
                printf("\x1b[1F");
                led_blink_active_flag ^= 1;
            }
        }

        /* Check if timer elapsed (interrupt fired) and toggle the LED */
        if (timer_interrupt_flag)
        {
            /* Clear the flag */
            timer_interrupt_flag = false;

            /* Invert the USER LED state */
            Cy_GPIO_Inv(CYBSP_USER_LED_PORT, CYBSP_USER_LED_PIN);
        }
    }
}


/*******************************************************************************
 * Function Name: timer_init
 ********************************************************************************
 * Summary:
 * This function creates and configures a Timer object. The timer ticks
 * continuously and produces a periodic interrupt on every terminal count
 * event. The period is defined by the 'period' and 'compare_value' of the
 * timer configuration structure 'led_blink_timer_cfg'. Without any changes,
 * this application is designed to produce an interrupt every 1 second.
 *
 * Parameters:
 *  none
 *
 *******************************************************************************/
 void timer_init(void)
{
     /* Enable interrupts */
     __enable_irq();

     /*TCPWM Counter Mode initial*/
     if (CY_TCPWM_SUCCESS != Cy_TCPWM_Counter_Init(TCPWM_COUNTER_HW, TCPWM_COUNTER_NUM, &TCPWM_COUNTER_config))
     {
          CY_ASSERT(0);
     }

     /* Enable the initialized counter */
     Cy_TCPWM_Counter_Enable(TCPWM_COUNTER_HW, TCPWM_COUNTER_NUM);

     /* Configure GPIO interrupt */
     Cy_TCPWM_SetInterruptMask(TCPWM_COUNTER_HW, TCPWM_COUNTER_NUM, CY_GPIO_INTR_EN_MASK);

     /* Configure GPIO interrupt vector for Port 0 */
     Cy_SysInt_Init(&intrCfg1, isr_timer);
     NVIC_EnableIRQ(TCPWM_COUNTER_IRQ);

     /* Start the counter */
     Cy_TCPWM_TriggerStart_Single(TCPWM_COUNTER_HW, TCPWM_COUNTER_NUM);
}

/*******************************************************************************
 * Function Name: isr_timer
 ********************************************************************************
 * Summary:
 * This is the interrupt handler function for the timer interrupt.
 *
 * Parameters:
 *  none
 *
 *******************************************************************************/
void isr_timer(void)
{

     uint32_t interrupts = Cy_TCPWM_GetInterruptStatusMasked(TCPWM_COUNTER_HW, TCPWM_COUNTER_NUM);

     /* Clear the interrupt */
     Cy_TCPWM_ClearInterrupt(TCPWM_COUNTER_HW, TCPWM_COUNTER_NUM, interrupts);

     if (0UL != (CY_TCPWM_INT_ON_TC & interrupts))
     {
          /* Set the interrupt flag and process it from the main while(1) loop */
          timer_interrupt_flag = true;
     }
}

/* [] END OF FILE */
