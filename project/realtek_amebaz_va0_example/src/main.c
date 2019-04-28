#include "FreeRTOS.h"
#include "task.h"
#include "diag.h"
#include "main.h"
#include <example_entry.h>


//////////////////////UART ADD////////////////////////////
#include "device.h"
#include "serial_api.h"
serial_t    sobj;
#define UART_TX    PA_23
#define UART_RX    PA_18

void UART_Indicate()
{
    serial_init(&sobj,UART_TX,UART_RX);
    serial_baud(&sobj,9600);//�޸Ĳ�����
    serial_format(&sobj, 8, ParityNone, 1);
}

//////////////////////UART ADD END ////////////////////////////
extern void console_init(void);
/**
  * @brief  Main program.
  * @param  None
  * @retval None
  */
void main(void)
{ 
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
      Led_Indicate(); //LED�����߳�
      KEY_Indicate(); //�������߳�

      //����ͨ�����
      UART_Indicate();//��ʼ������
      RSUart_Indicate();//���������շ��߳�
      RecvFlagClear_Indicate();//��������жϽ���
      
      Printf_Indicate();//��ӡ����
      
#endif

	/* Execute application example */
    //example_entry();

    	/*Enable Schedule, Start Kernel*/
#if defined(CONFIG_KERNEL) && !TASK_SCHEDULER_DISABLED
	#ifdef PLATFORM_FREERTOS
	vTaskStartScheduler();
	#endif
#else
	RtlConsolTaskRom(NULL);
#endif
}
