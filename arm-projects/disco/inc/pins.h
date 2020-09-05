#ifndef PINS_H
#define PINS_H

/*
 *  LED pins
 *      PD12 --> Green
 *      PD13 --> Orange
 *      PD14 --> Red
 *      PD15 --> Blue
 */
#define LED_PIN_PORT    GPIOD
#define LED_CLK         RCC_AHB1Periph_GPIOD
#define LED_GREEN       GPIO_Pin_12
#define LED_ORANGE      GPIO_Pin_13
#define LED_RED         GPIO_Pin_14
#define LED_BLUE        GPIO_Pin_15

/*
 *  Debug Pins
 */
#define DEBUG_PIN_PORT  GPIOC
#define DEBUG_PIN_CLK   RCC_AHB1Periph_GPIOC
#define DEBUG_PIN_00    GPIO_Pin_0
#define DEBUG_PIN_01    GPIO_Pin_1
#define DEBUG_PIN_02    GPIO_Pin_2
#define DEBUG_PIN_03    GPIO_Pin_3

/*
 *  USART Serial Driver Pins
 */
#define USART_PIN_PORT  GPIOA
#define USART_PIN_CLK   RCC_AHB1Periph_GPIOA
#define USART_PIN_TX    GPIO_Pin_2
#define USART_PIN_RX    GPIO_Pin_3
#define USART_NUM       USART2
#define USART_CLK       RCC_APB1Periph_USART2
#define USART_TX_PINSRC GPIO_PinSource2
#define USART_RX_PINSRC GPIO_PinSource3
#define USART_PINSRC_FUNC   GPIO_AF_USART2


#endif
