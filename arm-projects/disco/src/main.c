//#include <stm32f4xx.h>
#include <bsp.h>

#include "raging-global.h"
#include "nufr-platform.h"
#include "nufr-platform-app.h"
#include "nufr-api.h"
#include "nsvc.h"
#include "nsvc-api.h"
#include "nufr-sanity-checks.h"
#include "rnet-app.h"
#include "rnet-dispatch.h"
#include "rnet-intfc.h"
#include "ssp-driver.h"
#include "tx-task.h"
#include "base-task.h"

#include "disco-feature-switches.h"

#if DISCO_CS_NVM_TESTING==1
void nvm_test(void);
#endif

int main(void)
{
#if DISCO_CS_SSP==1
    nsvc_msg_fields_unary_t rx_fields[SSP_NUM_CHANNELS];
    nsvc_msg_fields_unary_t tx_fields[SSP_NUM_CHANNELS];
#endif

    /* CMSIS system initialization */
    SystemInit();

	bsp_initialize();

    // Wrapper to call all nufr init routines
    // Always call nufr_init before enabling PendSV or SysTick
    if (nufr_sane_init(nufrplat_systick_get_reference_time, NULL) == false)
    {
        return 0;
    }

#if DISCO_CS_NVM_TESTING==1
    // NVM testing
    nvm_test();
#endif

#if DISCO_CS_SSP==1
    rx_fields[0].prefix = NSVC_MSG_SSP_RX;
    rx_fields[0].id = ID_SSP_RX_ENTRY;
    rx_fields[0].priority = NUFR_MSG_PRI_MID;
    rx_fields[0].sending_task = NUFR_TID_null;
    rx_fields[0].destination_task = NUFR_TID_BASE;
    rx_fields[0].optional_parameter = 0;      // not used

    tx_fields[0].prefix = NSVC_MSG_SSP_TX;
    tx_fields[0].id = ID_TX_SSP_BUFFER_DISCARD;
    tx_fields[0].priority = NUFR_MSG_PRI_MID;
    tx_fields[0].sending_task = NUFR_TID_null;  // SL will figure it out
    tx_fields[0].destination_task = NUFR_TID_null;
    tx_fields[0].optional_parameter = 0;      // not used

    // hack here...'rx_handler_init_done' handled by rnet code, needed
    // by SSP to enable rx handler
    ssp_init(&rx_fields[0], &tx_fields[0]);
#endif

#if DISCO_CS_RNET==1
    // These all needed for RNET init
    rnet_create_buf_pool();
    rnet_set_msg_prefix(NUFR_TID_BASE, NSVC_MSG_RNET_STACK);
    rnet_intfc_init();
#endif

	__enable_irq();

	bsp_debug_pin_enable(Bsp_Initialized);
	bsp_debug_pin_disable(Bsp_Initialized);


    nufr_launch_task(NUFR_TID_BASE, 0);
    nufr_launch_task(NUFR_TID_TX, 0);

    while (1)
    {
        // nothing to do
    }


#if DISCO_CS_LEGACY_TEST_CODE==1
 // pre-NUFR test code, kept here for reference
	while(1)
	{
		bsp_delay_msecs(200);
		//bsp_led_disable(Red_Led);
		//bsp_led_enable(Green_Led);
		
		//bsp_debug_pin_enable(Debug_Pin_1);
		//bsp_debug_pin_enable(Debug_Pin_2);
		//bsp_debug_pin_enable(Debug_Pin_3);
	
		//bsp_delay_msecs(100);
		
		//bsp_led_enable(Red_Led);
		//bsp_led_disable(Green_Led);
		
		//bsp_debug_pin_disable(Debug_Pin_1);
		//bsp_debug_pin_disable(Debug_Pin_2);
		//bsp_debug_pin_disable(Debug_Pin_3);		

		bsp_uart_send('a');
	}
#endif
	
}