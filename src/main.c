/******************************************************************************
 * FreeRTOS Demo for the NEORV32 RISC-C Processor
 * https://github.com/stnolting/neorv32
 ******************************************************************************
* FreeRTOS Kernel V10.4.4
* Copyright (C) 2022 Amazon.com, Inc. or its affiliates. All Rights Reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy of
* this software and associated documentation files (the "Software"), to deal in
* the Software without restriction, including without limitation the rights to
* use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
* the Software, and to permit persons to whom the Software is furnished to do so,
* subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
* FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
* COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
* IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
* CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*
* http://www.FreeRTOS.org
* http://aws.amazon.com/freertos
******************************************************************************/

/* Standard libraries */
#include <stdint.h>
/* FreeRTOS kernel */
#include <FreeRTOS.h>
#include <task.h>
/* NEORV32 HAL */
#include <neorv32.h>
/* Application headers */
#include <utils_common.h>
#include <utils.h>

/* Platform UART configuration */
#define NEO_UART_BAUDRATE       (115200)         // transmission speed
#define NEO_PIN_LED_HEARTBEAT   0
#define NEO_PIN_INTERRUPT   
#define NEO_PIN_BUTTON          2


/* FreeRTOS assembler-written RISC-V trap handler (see portASM.S) */
extern void freertos_risc_v_trap_handler(void); // FreeRTOS core

/* Prototypes for the standard FreeRTOS callback/hook functions implemented
* within this file. See https://www.freertos.org/a00016.html 
   So declare the weak FreeRTOS functions prototypes
*/
void vApplicationMallocFailedHook(void);
void vApplicationIdleHook(void);
void vApplicationStackOverflowHook(TaskHandle_t pxTask, char *pcTaskName);
void vApplicationTickHook(void);
/* Application static function prototypes */
static void neo_init(void);
void heartbeat_led(void* pvParameters);


int main(void)
{
    BaseType_t status;
    const char* pHeartBeatTaskDesc = "heartbeat_led";
    
    /* Initialize the NeoRV32 RISC-V chip */
    neo_init();

    /* Display the FreeRTOS kernel version */
    NEO_CONSOLE_PRINTF("<<< NeoRV32 is running FreeRTOS %s >>>\n\n", tskKERNEL_VERSION_NUMBER);

    status = xTaskCreate(heartbeat_led, pHeartBeatTaskDesc, configMINIMAL_STACK_SIZE, (void*)NEO_PIN_LED_HEARTBEAT,
                    tskIDLE_PRIORITY + 1, NULL);
    if(status != pdPASS)
    {
        NEO_CONSOLE_PRINTF("Error! Failed to create \'%s\' FreeRTOS task", pHeartBeatTaskDesc);
    }

    // run actual application code
    vTaskStartScheduler();

    // we should never reach this
    NEO_CONSOLE_PRINTSTR("ERROR! 'main' has returned!\n");
    while(1);
    __builtin_unreachable();
}


/******************************************************************************
 * Handle NEORV32-/application-specific interrupts.
 ******************************************************************************/
void freertos_risc_v_application_interrupt_handler(void)
{
    /* Define the interrupt cause */
    register uint32_t mcause = neorv32_cpu_csr_read(CSR_MCAUSE);

    /* Is it a general-purpose timer interrupt? */
    if(mcause == GPTMR_TRAP_CODE) 
    { 
        neorv32_gptmr_irq_ack(); // clear GPTMR timer-match interrupt
        NEO_CONSOLE_PRINTSTR("GPTMR IRQ Tick\n");
        register uint32_t value = neorv32_gpio_pin_get(NEO_PIN_BUTTON) >> NEO_PIN_BUTTON;
        NEO_CONSOLE_PRINTF("User button T2 (GPIO_I2) value - %d\n", value);
    }
    /* An undefined interrupt has occured */
    else 
    {
        /* This variables are defined if unexpected interrupt occured only in case of optimizations */
        register uint32_t mepc = neorv32_cpu_csr_read(CSR_MEPC);
        register uint32_t mtval = neorv32_cpu_csr_read(CSR_MTVAL);
        NEO_CONSOLE_PRINTF("\nUnexpected IRQ! mcause=0x%x @ mepc=0x%x @ mtval=0x%x\n", 
                                mcause, mepc, mtval); // debug output
    }
}

/******************************************************************************
 * Handle NEORV32-/application-specific exceptions.
 ******************************************************************************/
void freertos_risc_v_application_exception_handler(void)
{
  // mcause identifies the cause of the exception
  register uint32_t mcause = neorv32_cpu_csr_read(CSR_MCAUSE);
  // mepc identifies the address of the exception
  register uint32_t mepc = neorv32_cpu_csr_read(CSR_MEPC);

  // debug output
  NEO_CONSOLE_PRINTF("\nException has occured! mcause=0x%x @ mepc=0x%x\n", mcause, mepc); // debug output
}


static void neo_init()
{
    uint32_t coreClock = 0;
    
    /* install the application trap handler */
    neorv32_cpu_csr_write(CSR_MTVEC, (uint32_t)&freertos_risc_v_trap_handler);

    /* clear GPIO.out port */
    neorv32_gpio_port_set(0);

    /* setup UART0 at default baud rate without interrupts */
    NEO_CONSOLE_INIT(NEO_UART_BAUDRATE);

    /* check the machine timer availability */
    if(neorv32_clint_available() == 0) 
    {
        NEO_CONSOLE_PRINTSTR("WARNING! MTIME machine timer not available!\n");
    }
    else
    {
        neorv32_cpu_csr_set(CSR_MIE, 1 << CSR_MIE_MTIE);
    }

    /* Check the ROM size */
    NEO_CONSOLE_PRINTF("NEORV32 ROM size: %u bytes\n", neo_rom_size_get());
    /* Check the RAM size */
    NEO_CONSOLE_PRINTF("NEORV32 RAM size: %u bytes\n", neo_ram_size_get());
    /* check the heap start address */
    NEO_CONSOLE_PRINTF("NEORV32 heap start address: %p\n", neorv32_heap_begin_c);
    /* check the heap size */
    NEO_CONSOLE_PRINTF("NEORV32 heap size: %u bytes\n", neorv32_heap_size_c);
    /* Print machine information */
    NEO_CONSOLE_MACHINE_INFO();

    /* check clock frequency configuration */
    coreClock = neorv32_sysinfo_get_clk();
    if(coreClock != configCPU_CLOCK_HZ)
    {
        NEO_CONSOLE_PRINTF("Incorrect NEORV32 clock rate settings: the real is %u Hz \
                                while software defined is %u Hz\n", coreClock, configCPU_CLOCK_HZ);
    }
    else
    {
        NEO_CONSOLE_PRINTF("NEORV32 clock rate: %u Hz\n", coreClock);
    }
    
    /* check the general-purpose timer availability */
    if(neorv32_gptmr_available() != 0)
    {
        /* configure timer for in continuous mode with clock divider = 64
         * fire interrupt every 8 seconds (continuous mode) */
        neorv32_gptmr_setup(CLK_PRSC_64, coreClock / 64 * 8, 1);

        /* enable general-purpose timer interrupt */
        neorv32_cpu_csr_set(CSR_MIE, 1 << GPTMR_FIRQ_ENABLE);

        /* logging */
        NEO_CONSOLE_PRINTSTR("GPTMR has been initialized!\n");
    }
    else
    {
        /* logging */ 
        NEO_CONSOLE_PRINTSTR("WARNING! GPTMR timer not available!\n");
    }

    return;
}


void heartbeat_led(void* pvParameters)
{
    uint32_t pinNumber = (uint32_t)pvParameters;
    uint32_t delay = 2000; // ms

    while(1)
    {
        neorv32_gpio_pin_set(pinNumber, 1);
        /* Enter block state for the current task and return control to the scheduler to select another task to execute */
        vTaskDelay(delay);
        neorv32_gpio_pin_set(pinNumber, 0);
        vTaskDelay(delay);
    }
}

/******************************************************************************
 * Assert terminator.
 ******************************************************************************/
void vAssertCalled(void)
{
    int i;

	taskDISABLE_INTERRUPTS();

	/* Clear all LEDs */
    neorv32_gpio_port_set(0);

    NEO_CONSOLE_PRINTSTR("FreeRTOS_FAULT: vAssertCalled called!\n");

	/* Flash the lowest 2 LEDs to indicate that assert was hit - interrupts are off
	here to prevent any further tick interrupts or context switches, so the
	delay is implemented as a busy-wait loop instead of a peripheral timer. */
	while(1)
    {
		for(i = 0; i < (configCPU_CLOCK_HZ / 100); i++)
        {
			__asm volatile("nop");
		}
		neorv32_gpio_pin_toggle(0);
		neorv32_gpio_pin_toggle(1);
	}
}


//#################################################################################################
// FreeRTOS Hooks
//#################################################################################################

/******************************************************************************
 * Hook for failing malloc.
 ******************************************************************************/
void vApplicationMallocFailedHook(void)
{
	/* vApplicationMallocFailedHook() will only be called if
	configUSE_MALLOC_FAILED_HOOK is set to 1 in FreeRTOSConfig.h. It is a hook
	function that will get called if a call to pvPortMalloc() fails.
	pvPortMalloc() is called internally by the kernel whenever a task, queue,
	timer or semaphore is created. It is also called by various parts of the
	demo application. If heap_1.c or heap_2.c are used, then the size of the
	heap available to pvPortMalloc() is defined by configTOTAL_HEAP_SIZE in
	FreeRTOSConfig.h, and the xPortGetFreeHeapSize() API function can be used
	to query the size of free heap space that remains (although it does not
	provide information on how the remaining heap might be fragmented). */

	taskDISABLE_INTERRUPTS();

    NEO_CONSOLE_PRINTSTR("FreeRTOS_FAULT: vApplicationMallocFailedHook \
                            (please, increase 'configTOTAL_HEAP_SIZE' in FreeRTOSConfig.h)\n");

	__asm volatile("ebreak"); // trigger context switch

    neorv32_cpu_sleep();
	while(1);
}


/******************************************************************************
 * Hook for the idle process.
 ******************************************************************************/
void vApplicationIdleHook(void)
{

	/* vApplicationIdleHook() will only be called if configUSE_IDLE_HOOK is set
	to 1 in FreeRTOSConfig.h. It will be called on each iteration of the idle
	task. It is essential that code added to this hook function never attempts
	to block in any way (for example, call xQueueReceive() with a block time
	specified, or call vTaskDelay()). If the application makes use of the
	vTaskDelete() API function (as this demo application does) then it is also
	important that vApplicationIdleHook() is permitted to return to its calling
	function, because it is the responsibility of the idle task to clean up
	memory allocated by the kernel to any task that has since been deleted. */

    neorv32_cpu_sleep(); // cpu wakes up on any interrupt request
}


/******************************************************************************
 * Hook for task stack overflow.
 ******************************************************************************/
void vApplicationStackOverflowHook(TaskHandle_t pxTask, char *pcTaskName)
{
    /* Make the compiler happy ^__^ */
	(void)pcTaskName;
	(void)pxTask;

	/* Run time stack overflow checking is performed if
	configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2. This hook
	function is called if a stack overflow is detected. */
	taskDISABLE_INTERRUPTS();

    NEO_CONSOLE_PRINTSTR("FreeRTOS_FAULT: vApplicationStackOverflowHook \
                            (please, increase 'configISR_STACK_SIZE_WORDS' in FreeRTOSConfig.h)\n");

	__asm volatile("ebreak"); // trigger context switch

	while(1);
}


/******************************************************************************
 * Hook for the application tick (unused).
 ******************************************************************************/
void vApplicationTickHook(void)
{
    __asm volatile( "nop" ); // nothing to do here yet
}