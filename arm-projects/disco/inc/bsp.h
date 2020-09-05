#ifndef BSP_H
#define BSP_H

#include <pins.h>
#include <stm32f4xx_conf.h>
#include <stm32f4xx_rcc.h>
#include <stm32f4xx_gpio.h>
#include <stm32f4xx_usart.h>

typedef enum
{
    Green_Led = LED_GREEN,
    Orange_Led = LED_ORANGE,
    Red_Led = LED_RED,
    Blue_Led = LED_BLUE
} bsp_leds_t;

typedef enum
{
    Bsp_Initialized = DEBUG_PIN_00,
    Debug_Pin_1     = DEBUG_PIN_01,
    Debug_Pin_2     = DEBUG_PIN_02,
    Debug_Pin_3     = DEBUG_PIN_03        
} bsp_debug_pin_t;



void bsp_config_interrupt_priority_levels(void);

void bsp_initialize(void);

void bsp_led_enable(bsp_leds_t led);

void bsp_led_disable(bsp_leds_t led);

void bsp_delay_1ms(void);

void bsp_delay_msecs(unsigned int seconds);

void bsp_debug_pin_enable(bsp_debug_pin_t pin);

void bsp_debug_pin_disable(bsp_debug_pin_t pin);

void bsp_debug_pin_toggle(bsp_debug_pin_t pin);

void bsp_uart_initialize(uint32_t buadRate);

void bsp_uart_send(uint8_t data);




//#define BSP_USE_UART_RX_INTERRUPT 1

//#if BSP_USE_UART_RX_INTERRUPT == 1

    void bsp_uart_interrupt(void);

    #define bsp_uart_handle_rx_byte(byte) handle_rx_byte((byte))

    /* Exported function */
    extern void handle_rx_byte(uint8_t byte);

//#else
//    #define bsp_uart_handle_rx_byte(byte) ((void)0)
//#endif

#endif
