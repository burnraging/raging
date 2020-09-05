#include <bsp.h>

#include <stm32f4xx.h>

#include "raging-global.h"
#include "nufr-platform-import.h"
#include "nufr-platform-export.h"
#include "nsvc-app.h"
#include "exception-priorities.h"
#include "rx-driver.h"
#include "ssp-driver.h"

#include "disco-feature-switches.h"

#define TEMP_DEBUG

#ifdef TEMP_DEBUG
    uint8_t  linear_buf[1000];
    unsigned linear_length;
#endif

void bsp_debug_pin_Initialize(void);

volatile unsigned int bsp_SysTickCounter;


void assert_failed(uint8_t *file, uint32_t line)
{
    int counter = 1;

    UNUSED(file);
    UNUSED(line);

    while (counter > 0)
    {
    }
}

// Set a single interrupt's priority level
static void set_interrupt_priority(IRQn_Type  which_irq,
                                   bsp_prl_t  preempt_priority,
                                   bsp_subl_t sub_priority)
{
    uint32_t x;

    x = NVIC_EncodePriority(AIRCR_PRIORITY_GROUP, preempt_priority, sub_priority);
    NVIC_SetPriority(which_irq, x);
}

// http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.dui0553a/Cihehdge.html
// http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.dui0553a/CIAGECDD.html
void bsp_config_interrupt_priority_levels(void)
{
    unsigned                  table_length;
    unsigned                  i;
    const bsp_irq_settings_t *irq_settings_table;
    const bsp_irq_settings_t *irq_settings;

    // Set PRIGROUP bits in AIRCR register.
    // This establishes the number of sub-priorities.
    // This M4 is configured for 4 priority bits, defined by __NVIC_PRIO_BITS,
    // but we'll just use 3, for backwards compatibility.
    NVIC_SetPriorityGrouping(AIRCR_PRIORITY_GROUP);

    irq_settings_table = ep_get_irq_priority_table(&table_length);

    for (i = 0; i < table_length; i++)
    {
        irq_settings = &irq_settings_table[i];

        set_interrupt_priority(irq_settings->irq,
                               irq_settings->preempt_priority,
                               irq_settings->sub_priority);
    }
}

void bsp_timer_decrement(void)
{
    if (bsp_SysTickCounter != 0x00)
    {
        bsp_SysTickCounter--;
    }
}

void bsp_mco_initialize(void);

void bsp_led_initialize(void);


void bsp_initialize(void)
{
    bsp_config_interrupt_priority_levels();

    // _IMPORT_CPU_CLOCK_SPEED not used here
    SysTick_Config((SystemCoreClock / MILLISECS_PER_SEC) * NUFR_TICK_PERIOD);
#if 0    // old test code, delete later
    //  SystemCoreClock / 1000      = 1 ms
    //  SystemCoreClock / 100000    = 10 us
    //  SystemCoreClock / 1000000   = 1 us

    //  Set System Tick to 1 us
    SysTick_Config(SystemCoreClock / 1000000);
#endif

    //NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
    
    bsp_mco_initialize();

    bsp_led_initialize();

    bsp_debug_pin_Initialize();

//    bsp_uart_initialize(115200);
//   temp slow-down for bringup
    bsp_uart_initialize(9600);
}



/*********************************************
 * 
 *  MCO section
 * 
 ********************************************/

void bsp_mco_initialize(void)
{
    //  Using MCO2 on PC9
    GPIO_InitTypeDef mco2_pin;

    GPIO_StructInit(&mco2_pin);

    mco2_pin.GPIO_Pin = GPIO_Pin_9;
    mco2_pin.GPIO_OType = GPIO_OType_PP;
    mco2_pin.GPIO_Mode = GPIO_Mode_AF;
    mco2_pin.GPIO_PuPd = GPIO_PuPd_UP;
    mco2_pin.GPIO_Speed = GPIO_Speed_100MHz;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);

    GPIO_Init(GPIOC, &mco2_pin);

    RCC_MCO2Config(RCC_MCO2Source_SYSCLK,RCC_MCO2Div_1);
}


/*********************************************
 * 
 *  LED section
 * 
 ********************************************/
void bsp_led_initialize(void)
{
    //  Temporary struct for setting up LED GPIO pins
    GPIO_InitTypeDef bsp_leds;

    //  Turn on the peripheral clock for the LED port pins
    RCC_AHB1PeriphClockCmd(LED_CLK, ENABLE);

    //  Initialize the GPIO pin struct
    GPIO_StructInit(&bsp_leds);

    //  Set the pins to use
    bsp_leds.GPIO_Pin = LED_GREEN | LED_ORANGE | LED_RED | LED_BLUE;

    //  Pins are push-pull output type pins
    bsp_leds.GPIO_OType = GPIO_OType_PP;

    //  Pins are setup for output mode
    bsp_leds.GPIO_Mode = GPIO_Mode_OUT;

    //  Pins will use the internal pull-up resistors (pull to VCC ~ 3.3V)
    bsp_leds.GPIO_PuPd = GPIO_PuPd_UP;

    //  Pins are set to high speed ~ 100Mhz pin switch max frequency
    bsp_leds.GPIO_Speed = GPIO_High_Speed;

    //  Initialize the configured pins for this port. (enable the configuration)
    GPIO_Init(LED_PIN_PORT, &bsp_leds);
}

void bsp_led_enable(bsp_leds_t led)
{
    GPIO_SetBits(LED_PIN_PORT, led);
}

void bsp_led_disable(bsp_leds_t led)
{
    GPIO_ResetBits(LED_PIN_PORT, led);
}

void bsp_led_toggle(bsp_leds_t led)
{
    GPIO_ToggleBits(LED_PIN_PORT, led);
}

void bsp_delay_1ms(void)
{
    bsp_SysTickCounter = 1000;
    while (bsp_SysTickCounter != 0)
    {
    }
}

void bsp_delay_msecs(unsigned int msecs)
{
    while (msecs--)
    {
        bsp_delay_1ms();
    }
}

void bsp_debug_pin_Initialize(void)
{
    //  Temporary Struct for setting up debug pins
    GPIO_InitTypeDef bsp_debug_pins;

    //  Turn on the Peripheral Clock for the debug pins
    RCC_AHB1PeriphClockCmd(DEBUG_PIN_CLK, ENABLE);

    //  Initialize the GPIO pin struct
    GPIO_StructInit(&bsp_debug_pins);

    //  Set the pins to use
    bsp_debug_pins.GPIO_Pin = DEBUG_PIN_00 | DEBUG_PIN_01 | DEBUG_PIN_02 | DEBUG_PIN_03;

    //  Pins are push-pull output type pins
    bsp_debug_pins.GPIO_OType = GPIO_OType_PP;

    //  Pins are setup for output mode
    bsp_debug_pins.GPIO_Mode = GPIO_Mode_OUT;

    //  Pins will use the internal pull-up resistors (pull to VCC ~ 3.3V)
    bsp_debug_pins.GPIO_PuPd = GPIO_PuPd_UP;

    //  Pins are set to high speed ~ 100 MHz pin switch max frequency
    bsp_debug_pins.GPIO_Speed = GPIO_High_Speed;

    //  Initialize the configured pins for this port. (enable the configuration)
    GPIO_Init(DEBUG_PIN_PORT, &bsp_debug_pins);
}

void bsp_debug_pin_enable(bsp_debug_pin_t pin)
{
    GPIO_SetBits(DEBUG_PIN_PORT, pin);
}

void bsp_debug_pin_disable(bsp_debug_pin_t pin)
{
    GPIO_ResetBits(DEBUG_PIN_PORT, pin);
}

void bsp_debug_pin_toggle(bsp_debug_pin_t pin)
{
    GPIO_ToggleBits(DEBUG_PIN_PORT, pin);
}

void bsp_uart_initialize(uint32_t baudRate)
{
    //  Temporary structure for GPIO Pins
    GPIO_InitTypeDef bsp_uart_pins;

    //  Temporary USART structure
    USART_InitTypeDef bsp_uart;

    //  Initialize GPIO Clock
    RCC_AHB1PeriphClockCmd(USART_PIN_CLK, ENABLE);

    //  Initialize USART Clock
    RCC_APB1PeriphClockCmd(USART_CLK, ENABLE);

    //  Initialize UART GPIOs
    GPIO_StructInit(&bsp_uart_pins);

    //  Initialize USART
    USART_StructInit(&bsp_uart);
    
    //  Set the pins to use
    bsp_uart_pins.GPIO_Pin = USART_PIN_TX | USART_PIN_RX;

    //  Pins are Push-pull output type pins
    bsp_uart_pins.GPIO_OType = GPIO_OType_PP;

    //  Pins are setup for Alternate Function mode
    bsp_uart_pins.GPIO_Mode = GPIO_Mode_AF;

    //  Pins will use the internal pull-up resistors (pull to VCC ~ 3.3V)
    bsp_uart_pins.GPIO_PuPd = GPIO_PuPd_UP;

    //  Pins are set to high speed ~ 100 MHz pin switch max frequency
    bsp_uart_pins.GPIO_Speed = GPIO_High_Speed;

    //  Initialize the configured pins for this port. (enable the configuration).
    GPIO_Init(USART_PIN_PORT, &bsp_uart_pins);

    //  Set the pins to be used for USART2 functions
    GPIO_PinAFConfig(USART_PIN_PORT, USART_TX_PINSRC, USART_PINSRC_FUNC);
    GPIO_PinAFConfig(USART_PIN_PORT, USART_RX_PINSRC, USART_PINSRC_FUNC);

    //  Setup the USART to use
    bsp_uart.USART_BaudRate = baudRate;

    //  Setup the wordlength (8 bits)
    bsp_uart.USART_WordLength = USART_WordLength_8b;

    //  Setup the Stop Bits (1)
    bsp_uart.USART_StopBits = USART_StopBits_1;

    //  Setup the Parity (none)
    bsp_uart.USART_Parity = USART_Parity_No;

    //  Setup the Hardware flow control (none)
    bsp_uart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;

    //  Setup the Communications mode
    bsp_uart.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;

    //  Initialize the configured usart. (enable the configuration)
    USART_Init(USART_NUM, &bsp_uart);

    //#if BSP_USE_UART_RX_INTERRUPT == 1

    USART_ITConfig(USART_NUM, USART_IT_RXNE, ENABLE);
    USART_ITConfig(USART_NUM, USART_IT_TXE, DISABLE);

    NVIC_InitTypeDef bsp_uart_rx_irq;
    bsp_uart_rx_irq.NVIC_IRQChannel = USART2_IRQn;
    bsp_uart_rx_irq.NVIC_IRQChannelPreemptionPriority = 0x00;
    bsp_uart_rx_irq.NVIC_IRQChannelSubPriority = 0x05;
    bsp_uart_rx_irq.NVIC_IRQChannelCmd = ENABLE;

    NVIC_Init(&bsp_uart_rx_irq);
    NVIC_EnableIRQ(USART2_IRQn);

    //#endif

    //  Enable the USART
    USART_Cmd(USART_NUM, ENABLE);
}

void bsp_uart_send(uint8_t data)
{
    while(USART_GetFlagStatus(USART_NUM, USART_FLAG_TC) == RESET)
    {
        //  Do nothing until we can send
    }
    USART_SendData(USART_NUM, (uint8_t)data);
}


void bsp_uart_interrupt(void)
{
    uint8_t byte = 0x00;

    //bsp_led_enable(Orange_Led);

    //  If we received a byte on the usart, handle it.
    if(USART_GetITStatus(USART_NUM, USART_IT_RXNE) == SET)
    {
        bsp_debug_pin_enable(DEBUG_PIN_01);

        //  Retrieve the byte from the USART
        byte = USART_ReceiveData(USART_NUM) & 0xFF;

        //  Pass it to the handler function defined by user
        if (rx_handler_init_done)
        {
        #ifdef TEMP_DEBUG
            if (linear_length < sizeof(linear_buf))
                linear_buf[linear_length++] = byte;
        #endif
        #if DISCO_CS_SSP == 1
            ssp_rx_entry(&ssp_desc[0], byte);
        #elif DISCO_CS_RNET==1
        // Pass to RNET stack
            rx_handler_for_ahdlc(&byte, 1);
        #endif
        }
        // Commented out line was test code
        //bsp_uart_handle_rx_byte(byte);

        //  clear the interrupt pending bit.
        USART_ClearITPendingBit(USART_NUM, USART_IT_RXNE);

        bsp_debug_pin_disable(DEBUG_PIN_01);
    }    
    
    //bsp_led_disable(Orange_Led);
}

