#include <bsp.h>

#include "exception-priorities.h"

// The interrupt priority table
const bsp_irq_settings_t irq_priority_table[] = {

    {MemoryManagement_IRQn,     INT_PRL_EXC_LOW, INT_SUBL_MID},
    {BusFault_IRQn,             INT_PRL_EXC_LOW, INT_SUBL_MID},
    {UsageFault_IRQn,           INT_PRL_EXC_LOW, INT_SUBL_MID},

#if __NVIC_PRIO_BITS == 2
// For Cortex M0, typical
// PendSV is the only exception to occupy lowest priority
// For lack of number of levels, SVC and SysTick must be set at
// same priority as most IRQs
    {SVCall_IRQn,               INT_PRL_IRQ_MID, INT_SUBL_LOW},
    {DebugMonitor_IRQn,         INT_PRL_IRQ_MID, INT_SUBL_MID},
    {PendSV_IRQn,               INT_PRL_SWE_LOW, INT_SUBL_LOW},
    {SysTick_IRQn,              INT_PRL_IRQ_MID, INT_SUBL_MID},
#else
// For Cortex M3, M4, typical
// PendSV is the only exception to occupy lowest priority
    {SVCall_IRQn,               INT_PRL_SWE_MID, INT_SUBL_LOW},
    {DebugMonitor_IRQn,         INT_PRL_SWE_MID, INT_SUBL_MID},
    {PendSV_IRQn,               INT_PRL_SWE_LOW, INT_SUBL_LOW},
    {SysTick_IRQn,              INT_PRL_SWE_MID, INT_SUBL_MID},
#endif

    {WWDG_IRQn,                 INT_PRL_IRQ_MID, INT_SUBL_MID},    /*!< Window WatchDog Interrupt                                         */
    {PVD_IRQn,                  INT_PRL_IRQ_MID, INT_SUBL_MID},    /*!< PVD through EXTI Line detection Interrupt                         */
    {TAMP_STAMP_IRQn,           INT_PRL_IRQ_MID, INT_SUBL_MID},    /*!< Tamper and TimeStamp interrupts through the EXTI line             */
    {RTC_WKUP_IRQn,             INT_PRL_IRQ_MID, INT_SUBL_MID},    /*!< RTC Wakeup interrupt through the EXTI line                        */
    {FLASH_IRQn,                INT_PRL_IRQ_MID, INT_SUBL_MID},    /*!< FLASH global Interrupt                                            */
    {RCC_IRQn,                  INT_PRL_IRQ_MID, INT_SUBL_MID},    /*!< RCC global Interrupt                                              */
    {EXTI0_IRQn,                INT_PRL_IRQ_MID, INT_SUBL_MID},    /*!< EXTI Line0 Interrupt                                              */
    {EXTI1_IRQn,                INT_PRL_IRQ_MID, INT_SUBL_MID},    /*!< EXTI Line1 Interrupt                                              */
    {EXTI2_IRQn,                INT_PRL_IRQ_MID, INT_SUBL_MID},    /*!< EXTI Line2 Interrupt                                              */
    {EXTI3_IRQn,                INT_PRL_IRQ_MID, INT_SUBL_MID},    /*!< EXTI Line3 Interrupt                                              */
    {EXTI4_IRQn,                INT_PRL_IRQ_MID, INT_SUBL_MID},    /*!< EXTI Line4 Interrupt                                              */
    {DMA1_Stream0_IRQn,         INT_PRL_IRQ_MID, INT_SUBL_MID},    /*!< DMA1 Stream 0 global Interrupt                                    */
    {DMA1_Stream1_IRQn,         INT_PRL_IRQ_MID, INT_SUBL_MID},    /*!< DMA1 Stream 1 global Interrupt                                    */
    {DMA1_Stream2_IRQn,         INT_PRL_IRQ_MID, INT_SUBL_MID},    /*!< DMA1 Stream 2 global Interrupt                                    */
    {DMA1_Stream3_IRQn,         INT_PRL_IRQ_MID, INT_SUBL_MID},    /*!< DMA1 Stream 3 global Interrupt                                    */
    {DMA1_Stream4_IRQn,         INT_PRL_IRQ_MID, INT_SUBL_MID},    /*!< DMA1 Stream 4 global Interrupt                                    */
    {DMA1_Stream5_IRQn,         INT_PRL_IRQ_MID, INT_SUBL_MID},    /*!< DMA1 Stream 5 global Interrupt                                    */
    {DMA1_Stream6_IRQn,         INT_PRL_IRQ_MID, INT_SUBL_MID},    /*!< DMA1 Stream 6 global Interrupt                                    */
    {ADC_IRQn,                  INT_PRL_IRQ_MID, INT_SUBL_MID},    /*!< ADC1, ADC2 and ADC3 global Interrupts                             */

#if defined(STM32F40_41xxx)
    {CAN1_TX_IRQn,              INT_PRL_IRQ_MID, INT_SUBL_MID},     /*!< CAN1 TX Interrupt                                                 */
    {CAN1_RX0_IRQn,             INT_PRL_IRQ_MID, INT_SUBL_MID},     /*!< CAN1 RX0 Interrupt                                                */
    {CAN1_RX1_IRQn,             INT_PRL_IRQ_MID, INT_SUBL_MID},     /*!< CAN1 RX1 Interrupt                                                */
    {CAN1_SCE_IRQn,             INT_PRL_IRQ_MID, INT_SUBL_MID},     /*!< CAN1 SCE Interrupt                                                */
    {EXTI9_5_IRQn,              INT_PRL_IRQ_MID, INT_SUBL_MID},     /*!< External Line[9:5] Interrupts                                     */
    {TIM1_BRK_TIM9_IRQn,        INT_PRL_IRQ_MID, INT_SUBL_MID},     /*!< TIM1 Break interrupt and TIM9 global interrupt                    */
    {TIM1_UP_TIM10_IRQn,        INT_PRL_IRQ_MID, INT_SUBL_MID},     /*!< TIM1 Update Interrupt and TIM10 global interrupt                  */
    {TIM1_TRG_COM_TIM11_IRQn,   INT_PRL_IRQ_MID, INT_SUBL_MID},     /*!< TIM1 Trigger and Commutation Interrupt and TIM11 global interrupt */
    {TIM1_CC_IRQn,              INT_PRL_IRQ_MID, INT_SUBL_MID},     /*!< TIM1 Capture Compare Interrupt                                    */
    {TIM2_IRQn,                 INT_PRL_IRQ_MID, INT_SUBL_MID},     /*!< TIM2 global Interrupt                                             */
    {TIM3_IRQn,                 INT_PRL_IRQ_MID, INT_SUBL_MID},     /*!< TIM3 global Interrupt                                             */
    {TIM4_IRQn,                 INT_PRL_IRQ_MID, INT_SUBL_MID},     /*!< TIM4 global Interrupt                                             */
    {I2C1_EV_IRQn,              INT_PRL_IRQ_MID, INT_SUBL_MID},     /*!< I2C1 Event Interrupt                                              */
    {I2C1_ER_IRQn,              INT_PRL_IRQ_MID, INT_SUBL_MID},     /*!< I2C1 Error Interrupt                                              */
    {I2C2_EV_IRQn,              INT_PRL_IRQ_MID, INT_SUBL_MID},     /*!< I2C2 Event Interrupt                                              */
    {I2C2_ER_IRQn,              INT_PRL_IRQ_MID, INT_SUBL_MID},     /*!< I2C2 Error Interrupt                                              */
    {SPI1_IRQn,                 INT_PRL_IRQ_MID, INT_SUBL_MID},     /*!< SPI1 global Interrupt                                             */
    {SPI2_IRQn,                 INT_PRL_IRQ_MID, INT_SUBL_MID},     /*!< SPI2 global Interrupt                                             */
    {USART1_IRQn,               INT_PRL_IRQ_MID, INT_SUBL_MID},     /*!< USART1 global Interrupt                                           */
    {USART2_IRQn,               INT_PRL_IRQ_MID, INT_SUBL_MID},     /*!< USART2 global Interrupt                                           */
    {USART3_IRQn,               INT_PRL_IRQ_MID, INT_SUBL_MID},     /*!< USART3 global Interrupt                                           */
    {EXTI15_10_IRQn,            INT_PRL_IRQ_MID, INT_SUBL_MID},     /*!< External Line[15:10] Interrupts                                   */
    {RTC_Alarm_IRQn,            INT_PRL_IRQ_MID, INT_SUBL_MID},     /*!< RTC Alarm (A and B) through EXTI Line Interrupt                   */
    {OTG_FS_WKUP_IRQn,          INT_PRL_IRQ_MID, INT_SUBL_MID},     /*!< USB OTG FS Wakeup through EXTI line interrupt                     */
    {TIM8_BRK_TIM12_IRQn,       INT_PRL_IRQ_MID, INT_SUBL_MID},     /*!< TIM8 Break Interrupt and TIM12 global interrupt                   */
    {TIM8_UP_TIM13_IRQn,        INT_PRL_IRQ_MID, INT_SUBL_MID},     /*!< TIM8 Update Interrupt and TIM13 global interrupt                  */
    {TIM8_TRG_COM_TIM14_IRQn,   INT_PRL_IRQ_MID, INT_SUBL_MID},     /*!< TIM8 Trigger and Commutation Interrupt and TIM14 global interrupt */
    {TIM8_CC_IRQn,              INT_PRL_IRQ_MID, INT_SUBL_MID},     /*!< TIM8 Capture Compare Interrupt                                    */
    {DMA1_Stream7_IRQn,         INT_PRL_IRQ_MID, INT_SUBL_MID},     /*!< DMA1 Stream7 Interrupt                                            */
    {FSMC_IRQn,                 INT_PRL_IRQ_MID, INT_SUBL_MID},     /*!< FSMC global Interrupt                                             */
    {SDIO_IRQn,                 INT_PRL_IRQ_MID, INT_SUBL_MID},     /*!< SDIO global Interrupt                                             */
    {TIM5_IRQn,                 INT_PRL_IRQ_MID, INT_SUBL_MID},     /*!< TIM5 global Interrupt                                             */
    {SPI3_IRQn,                 INT_PRL_IRQ_MID, INT_SUBL_MID},     /*!< SPI3 global Interrupt                                             */
    {UART4_IRQn,                INT_PRL_IRQ_MID, INT_SUBL_MID},     /*!< UART4 global Interrupt                                            */
    {UART5_IRQn,                INT_PRL_IRQ_MID, INT_SUBL_MID},     /*!< UART5 global Interrupt                                            */
    {TIM6_DAC_IRQn,             INT_PRL_IRQ_MID, INT_SUBL_MID},     /*!< TIM6 global and DAC1&2 underrun error  interrupts                 */
    {TIM7_IRQn,                 INT_PRL_IRQ_MID, INT_SUBL_MID},     /*!< TIM7 global interrupt                                             */
    {DMA2_Stream0_IRQn,         INT_PRL_IRQ_MID, INT_SUBL_MID},     /*!< DMA2 Stream 0 global Interrupt                                    */
    {DMA2_Stream1_IRQn,         INT_PRL_IRQ_MID, INT_SUBL_MID},     /*!< DMA2 Stream 1 global Interrupt                                    */
    {DMA2_Stream2_IRQn,         INT_PRL_IRQ_MID, INT_SUBL_MID},     /*!< DMA2 Stream 2 global Interrupt                                    */
    {DMA2_Stream3_IRQn,         INT_PRL_IRQ_MID, INT_SUBL_MID},     /*!< DMA2 Stream 3 global Interrupt                                    */
    {DMA2_Stream4_IRQn,         INT_PRL_IRQ_MID, INT_SUBL_MID},     /*!< DMA2 Stream 4 global Interrupt                                    */
    {ETH_IRQn,                  INT_PRL_IRQ_MID, INT_SUBL_MID},     /*!< Ethernet global Interrupt                                         */
    {ETH_WKUP_IRQn,             INT_PRL_IRQ_MID, INT_SUBL_MID},     /*!< Ethernet Wakeup through EXTI line Interrupt                       */
    {CAN2_TX_IRQn,              INT_PRL_IRQ_MID, INT_SUBL_MID},     /*!< CAN2 TX Interrupt                                                 */
    {CAN2_RX0_IRQn,             INT_PRL_IRQ_MID, INT_SUBL_MID},     /*!< CAN2 RX0 Interrupt                                                */
    {CAN2_RX1_IRQn,             INT_PRL_IRQ_MID, INT_SUBL_MID},     /*!< CAN2 RX1 Interrupt                                                */
    {CAN2_SCE_IRQn,             INT_PRL_IRQ_MID, INT_SUBL_MID},     /*!< CAN2 SCE Interrupt                                                */
    {OTG_FS_IRQn,               INT_PRL_IRQ_MID, INT_SUBL_MID},     /*!< USB OTG FS global Interrupt                                       */
    {DMA2_Stream5_IRQn,         INT_PRL_IRQ_MID, INT_SUBL_MID},     /*!< DMA2 Stream 5 global interrupt                                    */
    {DMA2_Stream6_IRQn,         INT_PRL_IRQ_MID, INT_SUBL_MID},     /*!< DMA2 Stream 6 global interrupt                                    */
    {DMA2_Stream7_IRQn,         INT_PRL_IRQ_MID, INT_SUBL_MID},     /*!< DMA2 Stream 7 global interrupt                                    */
    {USART6_IRQn,               INT_PRL_IRQ_MID, INT_SUBL_MID},     /*!< USART6 global interrupt                                           */
    {I2C3_EV_IRQn,              INT_PRL_IRQ_MID, INT_SUBL_MID},     /*!< I2C3 event interrupt                                              */
    {I2C3_ER_IRQn,              INT_PRL_IRQ_MID, INT_SUBL_MID},     /*!< I2C3 error interrupt                                              */
    {OTG_HS_EP1_OUT_IRQn,       INT_PRL_IRQ_MID, INT_SUBL_MID},     /*!< USB OTG HS End Point 1 Out global interrupt                       */
    {OTG_HS_EP1_IN_IRQn,        INT_PRL_IRQ_MID, INT_SUBL_MID},     /*!< USB OTG HS End Point 1 In global interrupt                        */
    {OTG_HS_WKUP_IRQn,          INT_PRL_IRQ_MID, INT_SUBL_MID},     /*!< USB OTG HS Wakeup through EXTI interrupt                          */
    {OTG_HS_IRQn,               INT_PRL_IRQ_MID, INT_SUBL_MID},     /*!< USB OTG HS global interrupt                                       */
    {DCMI_IRQn,                 INT_PRL_IRQ_MID, INT_SUBL_MID},     /*!< DCMI global interrupt                                             */
    {CRYP_IRQn,                 INT_PRL_IRQ_MID, INT_SUBL_MID},     /*!< CRYP crypto global interrupt                                      */
    {HASH_RNG_IRQn,             INT_PRL_IRQ_MID, INT_SUBL_MID},     /*!< Hash and Rng global interrupt                                     */
    {FPU_IRQn,                  INT_PRL_IRQ_MID, INT_SUBL_MID},     /*!< FPU global interrupt                                              */

#else
    #error "ST target has no exception priority support"
#endif
};

const bsp_irq_settings_t *ep_get_irq_priority_table(unsigned *table_length_ptr)
{
    *table_length_ptr = ARRAY_SIZE(irq_priority_table);

    return irq_priority_table;
}