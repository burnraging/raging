/**
  ******************************************************************************
  * @file    system.h
  * @author  MCD Application Team
  * @version V1.1.0
  * @date    11-January-2013
  * @brief   CMSIS Cortex-M4 Device System Source File for STM32F4xx devices.       
  ******************************************************************************  
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT 2013 STMicroelectronics</center></h2>
  *
  * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License");
  * You may not use this file except in compliance with the License.
  * You may obtain a copy of the License at:
  *
  *        http://www.st.com/software_license_agreement_liberty_v2
  *
  * Unless required by applicable law or agreed to in writing, software 
  * distributed under the License is distributed on an "AS IS" BASIS, 
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  ******************************************************************************  
  */ 

#include <stdint.h>


#ifndef _SYSTEM_H
#define _SYSTEM_H
 

extern uint32_t SystemCoreClock;          /*!< System Clock Frequency (Core Clock) */

  
extern void SystemInit(void);
extern void SystemCoreClockUpdate(void);

/* Interrupt Definitions */

void Reset_Handler(void);
void NMI_Handler(void);
void HardFault_Handler(void);
void MemManage_Handler(void);
void BusFault_Handler(void);
void UsageFault_Handler(void);
void SVC_Handler(void);
void DebugMon_Handler(void);
void PendSV_Handler(void);
void SysTick_Handler(void);




#endif /*SYSTEM_H */

/**
  * @}
  */
  
/**
  * @}
  */  
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
