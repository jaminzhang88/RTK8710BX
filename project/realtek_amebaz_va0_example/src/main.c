#include "FreeRTOS.h"
#include "task.h"
#include "diag.h"
#include "main.h"
#include <example_entry.h>

extern void console_init(void);

/**
  * @brief  Main program.
  * @param  None
  * @retval None
  */
void main(void)
{
printf("\n\n--------MAIN  IN AILIAN PRODUCE  MODE-------------\n\n");
#if CONFIG_AYLA
    	if ( rtl_cryptoEngine_init() != 0 ) {
		DiagPrintf("crypto engine init failed\r\n");			
	}	
	else
		printf("init success\n");	
#endif
	/* Initialize log uart and at command service */
	//console_init();	
	ReRegisterPlatformLogUart();

	/* pre-processor of application example */
    //pre_example_entry(); 

	/* wlan intialization */ 
#if defined(CONFIG_WIFI_NORMAL) && defined(CONFIG_NETWORK)
	  wlan_network();
      Led_Indicate();
      KEY_Indicate(); 

#endif

	/* Execute application example */
//	example_entry();

    	/*Enable Schedule, Start Kernel*/
#if defined(CONFIG_KERNEL) && !TASK_SCHEDULER_DISABLED
	#ifdef PLATFORM_FREERTOS
	vTaskStartScheduler();
	#endif
#else
	RtlConsolTaskRom(NULL);
#endif
}
