  /*
 * Copyright 2017 Ayla Networks, Inc.  All rights reserved.
 */

/*
 * Ayla device agent demo of a simple lights and buttons evaluation board
 * using the "simple" property manager.
 *
 * The property names are chosen to be compatible with the Ayla Control
 * App.  E.g., the LED property is Blue_LED even though the color is yellow.
 * Button1 sends the Blue_button property, even though the button is white.
 */
#define  HAVE_UTYPES
#include "lwip/ip_addr.h"

#include <ayla/utypes.h>
#include <ayla/log.h>
#include <sys/types.h>
#include <ada/libada.h>
#include <ada/sprop.h>
#include <ada/task_label.h>
#include "conf.h"
#include "demo.h"
#include "led_key.h"
#include "PinNames.h"
#include "flash_api.h"


#define BUILD_PID                  "SN0-8888888"   //PID �豸����+�����
#define BUILD_PROGNAME             "smartplug"
#define BUILD_VERSION              "ASW-01"  //ģ������
#define BUILD_STRING	            BUILD_VERSION " "  "V001-01" " " __DATE__ " " __TIME__  //V001 ��ģ��汾��  01�ǹ̼��汾��
   
/* 
 * The oem and oem_model strings determine the host name for the
 * Ayla device service and the device template on the service.
 *
 * If these are changed, the encrypted OEM secret must be re-encrypted
 * unless the oem_model was "*" (wild-card) when the oem_key was encrypted.
 */
char oem[] = DEMO_OEM_ID;
char oem_model[] = DEMO_LEDEVB_MODEL;

static u8 switch_control;
static unsigned blue_button;
static u8 blue_led;
static u8 green_led;
static int input;
static int output;
static int decimal_in;
static int decimal_out;


static char version[] =BUILD_PID " " BUILD_PROGNAME " " BUILD_STRING;   //�̼����version
static char demo_host_version[] = "V001";	/* property template version  ģ��汾*/
static char cmd_buf[TLV_MAX_STR_LEN + 1];

/* ��������*/
static unsigned char key_driver_low(void);
static unsigned char key_driver_high(void);
//led��io��ز���
void Led_Indicate(void);
void led_thread(void *param);
//wifi״ָ̬ʾ��
void STATE_LED_Fast(void);
void STATE_LED_Slow(void);
void STATE_LED_LightOn(void);
void STATE_LED_LightOff(void);
//�̵���ָʾ��
void SW_LED_Fast(void);
void SW_LED_Slow(void);
void SW_LED_LightOn(void);
void SW_LED_LightOff(void);
//����
void KEY_Indicate(void);
void key_thread(void);

static enum ada_err demo_led_set(struct ada_sprop *, const void *, size_t);
static enum ada_err demo_int_set(struct ada_sprop *, const void *, size_t);
static enum ada_err demo_cmd_set(struct ada_sprop *, const void *, size_t);
void prop_send_by_name(const char *name);

//�����߳���ض���
#define STACKSIZE_UART                                     1024     //�����߳�����ջ��С
#define STACKSIZE_UART_CLEARRECV                           256      //����жϽ���flag

//���ܲ�����ض���
#define STACKSIZE_LED                                      512      //ָʾ������ջ��С
#define STACKSIZE_KEY                                      512      //��������ջ��С

/* ���� ���� IO PIN ����ذ���״̬*/
#define key_state_0        0
#define key_state_1        1
#define key_state_2        2
#define key_state_3        3
 
#define key_no 	           0 
#define key_click	       1
#define key_double	       2
#define key_long	       3
#define key_long_long      4 

/* ���� Key/STATE_LED/SW_LED/OPT_IO PIN */
/*uint32_t PIN_DYNAMIC_KEY=PA_12;//Ĭ��ֵ
uint32_t PIN_DYNAMIC_OPT=PA_15;
uint32_t PIN_DYNAMIC_STATE_LED=PA_22; 
uint32_t PIN_DYNAMIC_SW_LED=PA_22; */
uint32_t PIN_DYNAMIC_KEY;//Ĭ��ֵ
uint32_t PIN_DYNAMIC_OPT;
uint32_t PIN_DYNAMIC_STATE_LED; 
uint32_t PIN_DYNAMIC_SW_LED; 

#define KEY_PIN_SET		      PIN_DYNAMIC_KEY    //���ð���
#define OPT_PIN               PIN_DYNAMIC_OPT    //�̵�����Դ��������
#define STATE_LED             PIN_DYNAMIC_STATE_LED  //wifiָʾ��
#define SW_LED                PIN_DYNAMIC_SW_LED //�̵���ָʾ��



/* ��ȡKEYֵ */
#define KEY1_READ			  GPIO_ReadDataBit(KEY_PIN_SET)
/*��ȡ�̵���IOֵ*/
#define OPT_READ              GPIO_ReadDataBit(OPT_PIN)

//��������flag  ����ģʽ��airkissģʽ��apģʽ����ʼ��Ϊairkissģʽ
int flag_work_mode=0;
int flag_airkiss_mode=1;
int flag_ap_mode=0;

//����δ������·��ע��ʧ��flag
int flag_connect_fail=0;
//�����ֻ���ע��ɹ�flag
int flag_register_ok=0;
//�����豸wifi down��ı�־
int flag_device_down=0;

//�������ģʽ�����־λ
int flag_join_success=0;
int flag_produce_mode=0;


//���嶯̬GPIO�ߵ͵�ƽ����
int flag_state_led; 
int flag_sw_led;
int flag_relay;
int flag_button;

int flag_net_mode1;
int flag_net_mode2;
int flag_cfg_net;
int flag_cfg_relay;


int time_bl=1000;  //���ⶨ����˸ʱ��

unsigned int ADDRESS_MEM=0xcf0000;

//////////////////////UART ADD////////////////////////////
#include "device.h"
#include "serial_api.h"

typedef struct{
	uint16_t	TxLeng;			
	u8_t    	TxIndex;		
	u8_t  	    TxBuff[100];	

	u8_t  	    RxDealStep;		
	u8_t  	    RxDataFlag;		
	uint16_t	RxLeng;			
	u8_t  	    RxCnt;			
	u8_t  	    RxIndex;		
	u8_t  	    RxOverFlag;		
	u8_t  	    RxTimeOutCnt;
	u8_t  	    RxBuff[100];	
	u8_t  	    RxDataSucFlag;		
}T_SENSOR_UART;
T_SENSOR_UART SENSOR_UART;
#define UART_TX      PA_23
#define UART_RX      PA_18

extern serial_t    sobj;
unsigned char rc=0;
/////////////////////////////////END UART ADD////////////////////////////////////////////////////////
static struct ada_sprop demo_props[] = {
	/*
	 * version properties
	 */
	{ "oem_host_version", ATLV_UTF8,
	    demo_host_version, sizeof(demo_host_version),
	    ada_sprop_get_string, NULL},
	{ "version", ATLV_UTF8, &version[0], sizeof(version),
	    ada_sprop_get_string, NULL},
	/*
	 * boolean properties
	 */
	 { "Switch_Control", ATLV_BOOL, &switch_control, sizeof(switch_control),
	    ada_sprop_get_bool, demo_led_set },       
};
/*
 * Initialize property manager.
 */
void demo_init(void)
{   
    #if 1  //��Ϊ0��ʱ�������һ��   ��Ϊ1��ʱ��̬GPIO���ú�����ʹ�����־�����
    char *argv5[] = { "conf", "getdata5" };
    conf_cli(2, argv5);
    vTaskDelay(200);
    char *argv6[] = { "conf", "getdata6" };
    conf_cli(2, argv6);
    vTaskDelay(200);
    char *argv1[] = { "conf", "getdata1" };
    conf_cli(2, argv1);
    vTaskDelay(200);
    char *argv2[] = { "conf", "getdata2" };
    conf_cli(2, argv2);
    vTaskDelay(200);
    char *argv3[] = { "conf", "getdata3" };
    conf_cli(2, argv3);
    vTaskDelay(200);
    char *argv4[] = { "conf", "getdata4" };
    conf_cli(2, argv4);
    vTaskDelay(200);
    //�ϵ缴��ȡһ��oem key
    oem_set_key1();
    flash_t  flash;
    char data2[20];
    memset(data2,0,20);
    flash_stream_read(&flash,ADDRESS_MEM+450,20,data2);
    for(int i=0;i<10;i++){
       printf("data2begin:%c\n",data2[i]); 
    }
    #endif
    ada_sprop_mgr_register("SN0-01-0-001", demo_props, ARRAY_LEN(demo_props));
}

//wifi״ָ̬ʾ�ƿ���
void STATE_LED_Fast(void)
{
    if(flag_state_led==1){
    	GPIO_WriteBit(STATE_LED, 1);
    	vTaskDelay(150);
    	GPIO_WriteBit(STATE_LED, 0);
    }else if(flag_state_led==0){
        GPIO_WriteBit(STATE_LED, 0);
    	vTaskDelay(150);
    	GPIO_WriteBit(STATE_LED, 1);
    }
}
//wifi״ָ̬ʾ������
 void STATE_LED_Slow(void)
{
    if(flag_state_led==1){
    	GPIO_WriteBit(STATE_LED, 0);
    	vTaskDelay(700);
    	GPIO_WriteBit(STATE_LED, 1);
    	vTaskDelay(700);	
	}else if(flag_state_led==0){
        GPIO_WriteBit(STATE_LED, 1);
    	vTaskDelay(700);
    	GPIO_WriteBit(STATE_LED, 0);
    	vTaskDelay(700);
    }
} 
//wifi״̬�Ƴ��� 
void STATE_LED_LightOn(void)
{
    if(flag_state_led==1){
       GPIO_WriteBit(STATE_LED, 1);
    }else if(flag_state_led==0){
       GPIO_WriteBit(STATE_LED, 0);
    }	
}
//wifi״̬��Ϩ��
void STATE_LED_LightOff(void)
{
	if(flag_state_led==1){
       GPIO_WriteBit(STATE_LED, 0);
    }else if(flag_state_led==0){
       GPIO_WriteBit(STATE_LED, 1);
    }
} 


//�̵���ָʾ�ƿ���
void SW_LED_Fast(void)
{
    if(flag_sw_led==1){
    	GPIO_WriteBit(SW_LED, 1);
    	vTaskDelay(150);
    	GPIO_WriteBit(SW_LED, 0);
    }else if(flag_sw_led==0){
        GPIO_WriteBit(SW_LED, 0);
    	vTaskDelay(150);
    	GPIO_WriteBit(SW_LED, 1);
    }
}
//�̵���ָʾ������
 void SW_LED_Slow(void)
{
    if(flag_sw_led==1){
    	GPIO_WriteBit(SW_LED, 0);
    	vTaskDelay(700);
    	GPIO_WriteBit(SW_LED, 1);
    	vTaskDelay(700);	
	}else if(flag_sw_led==0){
        GPIO_WriteBit(SW_LED, 1);
    	vTaskDelay(700);
    	GPIO_WriteBit(SW_LED, 0);
    	vTaskDelay(700);
    }
} 
//�̵���ָʾ�Ƴ��� 
void SW_LED_LightOn(void)
{
    if(flag_sw_led==1){
       GPIO_WriteBit(SW_LED, 1);
    }else if(flag_sw_led==0){
       GPIO_WriteBit(SW_LED, 0);
    }
}
//�̵���ָʾ��Ϩ��
void SW_LED_LightOff(void)
{
	if(flag_sw_led==1){
       GPIO_WriteBit(SW_LED, 0);
    }else if(flag_sw_led==0){
       GPIO_WriteBit(SW_LED, 1);
    }
} 
//ledָʾ��ִ�к���
void led_thread(void *param)
{   
        sys_jtag_off(); 
        //init_led_key(); 
        for(;;){ 
                vTaskDelay(130);
                if(flag_airkiss_mode==1&&flag_work_mode==0&&flag_ap_mode==0&&flag_connect_fail==0) {STATE_LED_Fast();}		   
    	        if(flag_ap_mode==1&&flag_airkiss_mode==0&&flag_work_mode==0&&flag_connect_fail==0) {STATE_LED_Slow();}
    	        //�����Ѿ����ӵ�wifi����δ����״̬
                if(flag_join_success==1&&flag_work_mode==0){
                    switch(flag_net_mode1){
                     case 0:STATE_LED_LightOff();break;
                     case 1:STATE_LED_LightOn();break;
                     case 2://ͬ�̵���״̬
                           //printf("flag_net_mode1:case 2\n");
                           if(OPT_READ){
                              if(flag_relay==1){
                                STATE_LED_LightOn();
                              }else if(flag_relay==0){
                                STATE_LED_LightOff();
                              }
                           }else{
                              if(flag_relay==1){
                                STATE_LED_LightOff();
                              }else if(flag_relay==0){
                                STATE_LED_LightOn();
                              }
                           }
                           break;
                   }//end switch
                }
                
                //����������״̬
                if(flag_work_mode==1&&flag_ap_mode==0&&flag_airkiss_mode==0){
                   switch(flag_net_mode2){
                     case 0:STATE_LED_LightOff();break;
                     case 1:STATE_LED_LightOn();break;
                     case 2://ͬ�̵���
                           //printf("flag_net_mode2:case 2\n");
                           if(OPT_READ){
                              if(flag_relay==1){
                                STATE_LED_LightOn();
                              }else if(flag_relay==0){
                                STATE_LED_LightOff();
                              }
                           }else{
                              if(flag_relay==1){
                                STATE_LED_LightOff();
                              }else if(flag_relay==0){
                                STATE_LED_LightOn();
                              }
                           }
                          break;
                   }//end switch
                }
               
                if(flag_connect_fail==1){STATE_LED_LightOff();}
                if(flag_produce_mode){ //����ģʽ·����������˸ָʾ��
                         GPIO_WriteBit(STATE_LED, 1);
                         GPIO_WriteBit(OPT_PIN, 1);
	                     vTaskDelay(time_bl);
	                     GPIO_WriteBit(STATE_LED, 0);
                         GPIO_WriteBit(OPT_PIN, 0);
	                     vTaskDelay(time_bl);
	            }
	    }//end for
        vTaskDelete(NULL);
}
//����ledָʾ���߳�����
void Led_Indicate()
{
  if(xTaskCreate(led_thread, ((const char*)"led_light"), STACKSIZE_LED, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS)
		printf("\n\r%s xTaskCreate(Led_Indicate) failed", __FUNCTION__);  
}

/***************************************************************************
�����ܣ�һ�������ĵ����������� 
***************************************************************************/
static unsigned char key_driver_low(void){
	static unsigned char key_state_buffer1 = key_state_0;
	static unsigned char key_timer_cnt1 = 0;
	unsigned char key_return = key_no;
	unsigned char key;
	
	key = KEY1_READ;  //read the key I/O states
    switch(key_state_buffer1){
		case key_state_0:
			if(key == 0)//���������£�״̬ת��������������ȷ��״̬
			  key_state_buffer1 = key_state_1; 
			break;
		case key_state_1:
	        if(key == 0){
				key_timer_cnt1 = 0;
				key_state_buffer1 = key_state_2;
				//������Ȼ���ڰ���״̬
				//������ɣ�key_timer��ʼ׼����ʱ
				//״̬�л�������ʱ���ʱ״̬
	        }
		else{
		    key_state_buffer1 = key_state_0;
		    }//�����Ѿ�̧�𣬻ص�������ʼ״̬
			break;  //����������
		case key_state_2:
		    key_timer_cnt1++;
		    printf("\n---%d----\n",key_timer_cnt1);
		    if(key == 1) {      
				key_return = key_click;  //����̧�𣬲���һ��click����
				key_state_buffer1 = key_state_0;  //ת����������ʼ״̬
			}
			else if(key_timer_cnt1 >= 95){     //����10s
			   if(flag_work_mode||flag_connect_fail){
			      key_return = key_long_long;  //�ͻس����¼�
				  key_state_buffer1 = key_state_3;  //ת�����ȴ������ͷ�״̬
			   }
			}else if(key_timer_cnt1>=45 ){ //����5s
			   if((flag_airkiss_mode==1&&flag_connect_fail==0)||(flag_ap_mode==1&&flag_connect_fail==0)){
			      key_return = key_long;  //�ͻس����¼�
				  key_state_buffer1 = key_state_3;  //ת�����ȴ������ͷ�״̬
				}
			}
		     break; 
		  case key_state_3:  //�ȴ������ͷ�
		    if(key == 1){  //�����ͷ�
			  key_state_buffer1 = key_state_0;  //�лذ�����ʼ״̬
			}
			break;
	   }
	return key_return;
}
static unsigned char key_driver_high(void){
	static unsigned char key_state_buffer1 = key_state_0;
	static unsigned char key_timer_cnt1 = 0;
	unsigned char key_return = key_no;
	unsigned char key;
	
	key = KEY1_READ;  //read the key I/O states
    switch(key_state_buffer1){
		case key_state_0:
			if(key == 1)//���������£�״̬ת��������������ȷ��״̬
			  key_state_buffer1 = key_state_1; 
			break;
		case key_state_1:
	        if(key == 1){
				key_timer_cnt1 = 0;
				key_state_buffer1 = key_state_2;
				//������Ȼ���ڰ���״̬
				//������ɣ�key_timer��ʼ׼����ʱ
				//״̬�л�������ʱ���ʱ״̬
	        }
		else{
		    key_state_buffer1 = key_state_0;
		    }//�����Ѿ�̧�𣬻ص�������ʼ״̬
			break;  //����������
		case key_state_2:
		    key_timer_cnt1++;
		    printf("\n---%d----\n",key_timer_cnt1);
		    if(key == 0) {      
				key_return = key_click;  //����̧�𣬲���һ��click����
				key_state_buffer1 = key_state_0;  //ת����������ʼ״̬
			}
			else if(key_timer_cnt1 >= 95){     //����10s
			   if(flag_work_mode||flag_connect_fail){
			      key_return = key_long_long;  //�ͻس����¼�
				  key_state_buffer1 = key_state_3;  //ת�����ȴ������ͷ�״̬
			   }
			}else if(key_timer_cnt1>=45 ){ //����5s
			   if((flag_airkiss_mode==1&&flag_connect_fail==0)||(flag_ap_mode==1&&flag_connect_fail==0)){
			      key_return = key_long;  //�ͻس����¼�
				  key_state_buffer1 = key_state_3;  //ת�����ȴ������ͷ�״̬
				}
			}
		     break; 
		  case key_state_3:  //�ȴ������ͷ�
		    if(key == 0){  //�����ͷ�
			  key_state_buffer1 = key_state_0;  //�лذ�����ʼ״̬
			}
			break;
	   }
	return key_return;
}
void key_thread(void){   
         unsigned char key;
         for(;;){  
                   vTaskDelay(100);
                   if(flag_button){
                       key=key_driver_high();
                   }else{
                       key=key_driver_low();
                   }
                   switch(key){
                          case 1 ://�̰��̵�������
                                 if(flag_produce_mode){
			                         printf("\n\n\n-----------produce_mode hand key test-------\n\n\n");
                                     time_bl=120;//���ò���ָʾ����˸ʱ�����
                                 }else{
                                     if(OPT_READ){
                                            if(flag_relay==1){
                                                 GPIO_WriteBit(OPT_PIN, 0);
                                                 //if(flag_work_mode){
                    				                switch_control =0;
                    				                flash_t  flash;
                                                    char data1[10]={'0'};
                                                    flash_erase_sector(&flash,  ADDRESS_MEM);
                                                    u8 return_value=flash_stream_write(&flash,  ADDRESS_MEM+500, 10,data1);
                                                    char *argv2[] = { "conf", "save" };
                                                    conf_cli(2, argv2);
                                                    prop_send_by_name("Switch_Control");
            				                        printf("\n\n----prop_send_by_name Switch_Control  0-------\n\n");
            				                     //}
                                             }else if(flag_relay==0){
                                                 GPIO_WriteBit(OPT_PIN, 0);
                                                 //if(flag_work_mode){
                    				                switch_control =1;
                    				                flash_t  flash;
                                                    char data1[10]={'1'};
                                                    flash_erase_sector(&flash,  ADDRESS_MEM);
                                                    u8 return_value=flash_stream_write(&flash,  ADDRESS_MEM, 10,data1);
                                                    char *argv2[] = { "conf", "save" };
                                                    conf_cli(2, argv2);
            				                        prop_send_by_name("Switch_Control");
            				                        printf("\n\n----prop_send_by_name Switch_Control  1-------\n\n");
            				                     //}
                                             }
                                     }else{
				                         if(flag_relay==1){
                                                 GPIO_WriteBit(OPT_PIN, 1);
                                                // if(flag_work_mode){
                    				                switch_control =1;
                    				                flash_t  flash;
                                                    char data1[10]={'1'};
                                                    flash_erase_sector(&flash,  ADDRESS_MEM);
                                                    u8 return_value=flash_stream_write(&flash,  ADDRESS_MEM, 10,data1);
            				                        prop_send_by_name("Switch_Control");
            				                        printf("\n\n----prop_send_by_name Switch_Control  1-------\n\n");
            				                     //}
                                             }else if(flag_relay==0){
                                                 GPIO_WriteBit(OPT_PIN, 1);
                                                 //if(flag_work_mode){
                    				                switch_control =0;
                    				                flash_t  flash;
                                                    char data1[10]={'0'};
                                                    flash_erase_sector(&flash,  ADDRESS_MEM);
                                                    u8 return_value=flash_stream_write(&flash,  ADDRESS_MEM, 10,data1);
            				                        prop_send_by_name("Switch_Control");
            				                        printf("\n\n----prop_send_by_name Switch_Control  0-------\n\n");
            				                     //}
                                            }
				                     } 
    				             }
    				             break;
		                   case 3 ://������������ģʽ�л�5s
		                         if(flag_airkiss_mode==1&&flag_work_mode==0&&flag_ap_mode==0){
    				                 //����ΪAP��ʽ
    				                 printf("------set from airkiss  to AP mode---\n");
                                     char *argv[] = { "wifi", "aks_cls" };
                                     char *argv2[] = { "conf", "save" };
                                     char *argv3[] = { "reset" };
                                     adw_wifi_profile_sta_erase();
                                     vTaskDelay(200);
                                     adw_wifi_cli(2, argv);
                                     vTaskDelay(200);
                                     conf_cli(2, argv2);
                                     vTaskDelay(200);
                                     demo_reset_cmd(1, argv3);
			                     }else if(flag_airkiss_mode==0&&flag_work_mode==0&&flag_ap_mode==1) {  
        		                     //����Ϊairkissģʽ 
        		                     printf("------set from AP to airkiss mode---\n");
                                     char *argv[] = { "wifi", "aks_save" };
                                     char *argv2[] = { "conf", "save" };
                                     char *argv3[] = { "reset" };
                                     adw_wifi_profile_sta_erase();
                                     vTaskDelay(200);
                                     adw_wifi_cli(2, argv);
                                     vTaskDelay(200);
                                     conf_cli(2, argv2);
                                     vTaskDelay(200);
                                     demo_reset_cmd(1, argv3);
        				         }
		                         break;
                            case 4 ://����10s����ָ���������״̬������������ã�����WiFi���ã���ʱ���õȣ�
                                  if(flag_work_mode==1||flag_connect_fail==1) {
                                     printf("------set from work to default airkiss mode and reset factory---\n");
                                     //�ٻָ��������ã��̵���״̬ά�ֲ��䣬����airkissĬ������
                                     conf_reset_factory();
                                     vTaskDelay(500);
                                     char *argv[] = { "wifi", "aks_save" };
                                     char *argv2[] = { "conf", "save" };
                                     char *argv3[] = { "reset" };
                                     adw_wifi_profile_sta_erase();
                                     vTaskDelay(200);
                                     adw_wifi_cli(2, argv);
                                     vTaskDelay(200);
                                     conf_cli(2, argv2);
                                     vTaskDelay(200);
                                     demo_reset_cmd(1, argv3);
                                 }
                                 break;
		                         default: break;
		              }// end switch(key)
	   }//end for
	vTaskDelete(NULL);
}
//���������߳�����
void KEY_Indicate(void)
{
  if(xTaskCreate(key_thread, ((const char*)"key_set"), STACKSIZE_KEY, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS)
		printf("\n\r%s xTaskCreate(key_set) failed", __FUNCTION__);
}
void printf_thread(){
    for(;;){
         vTaskDelay(10000);
         printf("\n\n\n");
         printf("flag_state_led:%d\n",flag_state_led);
         printf("flag_sw_led:%d\n",flag_sw_led);
         printf("flag_relay:%d\n",flag_relay);
         printf("flag_button:%d\n",flag_button);
         printf("flag_net_mode1:%d\n",flag_net_mode1);
         printf("flag_net_mode2:%d\n",flag_net_mode2);
         printf("flag_cfg_net:%d\n",flag_cfg_net);
         printf("flag_cfg_relay:%d\n",flag_cfg_relay);
         printf("\n\n\n");
      }
  vTaskDelete(NULL);
}
//���������߳�����
void Printf_Indicate(void)
{
  if(xTaskCreate(printf_thread, ((const char*)"printf_thread"), 256, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS)
		printf("\n\r%s xTaskCreate(key_set) failed", __FUNCTION__);
}
//�������ݷ���
void uart_send_string(serial_t *sobj, char *pstr){
    unsigned int i=0;
    while (*(pstr+i) != 0) {
        serial_putc(sobj, *(pstr+i));
        i++;
    }
}
//�������ݽ����ж�  
static void uart_irq(uint32_t id, SerialIrq event){
             serial_t    *sobj = (serial_t  *)id;
             if(event == RxIrq) {
                 rc =serial_getc(sobj);
                  //printf("  recv:%x\n",rc);
                  if(SENSOR_UART.RxDealStep>1){
                        SENSOR_UART.RxDataFlag = TRUE;
                        SENSOR_UART.RxTimeOutCnt=0;
                  } 
        	 switch(SENSOR_UART.RxDealStep){
                     case 0:
        	   	        break;
        	     case 1:
        	              if(rc==0x55){	
        	   	           memset(SENSOR_UART.RxBuff,0,100);//�������buf
        		           SENSOR_UART.RxCnt=0;
        		           SENSOR_UART.RxBuff[SENSOR_UART.RxCnt++]=rc;
        		           SENSOR_UART.RxDealStep++;
        		           SENSOR_UART.RxDataSucFlag=FALSE;
        		        }				
        	   	       break;
        	     case 2:
        	   	      if(rc==0xFE){
        	   	          SENSOR_UART.RxBuff[SENSOR_UART.RxCnt++]=rc;
        		          SENSOR_UART.RxDealStep++;
        	   	        }
        		       break;	  
        	      case 3:
        	   	       SENSOR_UART.RxBuff[SENSOR_UART.RxCnt++]=rc;
                               SENSOR_UART.RxLeng=rc;
        		       SENSOR_UART.RxDealStep++;
        		       break;
        	      case 4:
                	       SENSOR_UART.RxBuff[SENSOR_UART.RxCnt++]=rc;
                	       if( (SENSOR_UART.RxLeng+30) == SENSOR_UART.RxCnt){
                                   SENSOR_UART.RxDataSucFlag=TRUE;
                		   SENSOR_UART.RxDealStep=0; 
                		   SENSOR_UART.RxDataFlag = FALSE;
                               }else if( (SENSOR_UART.RxLeng+30) > SENSOR_UART.RxCnt){  //30Ϊ���ݳ��ȣ���Ҫ���յ����ݺܳ���
                                   // ����û�н�����
                	       }else { // ���ݳ��ȳ���  ���½���
                	           SENSOR_UART.RxDealStep =1;
                	       } 			 
                	      break;
        	      default:
        	   	     break;
           }//end switch
     }  //end      if(event == RxIrq) 
}

void sensor_uart_init(void){
    SENSOR_UART.RxLeng = 0;
    SENSOR_UART.RxDataFlag = FALSE;
	SENSOR_UART.RxOverFlag = FALSE;
	SENSOR_UART.RxDataFlag = FALSE;
	SENSOR_UART.RxTimeOutCnt = 0;
	SENSOR_UART.TxLeng = 0;
	SENSOR_UART.RxDealStep = 1;
	SENSOR_UART.RxCnt = 0;
	SENSOR_UART.RxDataSucFlag= FALSE;
}


void uart_thread(void){
    printf("------------uart come in!--------------\n");
    sensor_uart_init();//��־λ��ʼ��
    serial_t    sobj;
    //��ʼ������
    serial_init(&sobj,UART_TX,UART_RX);
    serial_baud(&sobj,9600);//�޸Ĳ�����
    serial_format(&sobj, 8, ParityNone, 1);
    //���ݽ����ж�
    for(;;)
    {
          vTaskDelay(110);
          serial_irq_handler(&sobj, uart_irq, (uint32_t)&sobj);
          serial_irq_set(&sobj, RxIrq, 1);
    }
   vTaskDelete(NULL);
}
//���������շ������߳�����
void RSUart_Indicate(){
  if(xTaskCreate(uart_thread, ((const char*)"uart_thread"), STACKSIZE_UART, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS)
		printf("\n\r%s xTaskCreate(uart_thread) failed", __FUNCTION__);  
}
void recv_flag_clear_thread(){
   for(;;){
        vTaskDelay(400);
        SENSOR_UART.RxDataSucFlag=FALSE;//���½��� ����жϽ��ձ�־λ
        SENSOR_UART.RxDealStep=1;
   }
}
//������������жϽ�������
void RecvFlagClear_Indicate(){
  if(xTaskCreate(recv_flag_clear_thread, ((const char*)"recv_flag_clear_thread"), STACKSIZE_UART_CLEARRECV, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS)
		printf("\n\r%s xTaskCreate(recv_flag_clear_thread) failed", __FUNCTION__);  
}



void prop_send_by_name(const char *name)
{
	enum ada_err err;

	err = ada_sprop_send_by_name(name);
	if (err) {
		log_put(LOG_INFO "demo: %s: send of %s: err %d",
		    __func__, name, err);
	}
}

/*
 * Demo set function for bool properties.
 */


static enum ada_err demo_led_set(struct ada_sprop *sprop,
				const void *buf, size_t len)
{
	int ret = 0;
         ret = ada_sprop_set_bool(sprop, buf, len);
	if (ret) {
		return ret;
	}
	if (sprop->val == &switch_control) {
	    printf("\n\n-----switch_control_* is %d------\n\n",switch_control);
	    //add by KingKa 2019.4.17
        if(switch_control==1){//����
            //�ϵ���� д��flash��
            flash_t  flash;
            char data1[10]="987654321";
            char data2[10];
            flash_erase_sector(&flash,  ADDRESS_MEM);
            u8 return_value=flash_stream_write(&flash,  ADDRESS_MEM, 10,data1);
            char *argv2[] = { "conf", "save" };
            conf_cli(2, argv2);
            flash_stream_read(&flash,ADDRESS_MEM,10,data2);
            for(int i=0;i<10;i++){
                  printf("data2:%c\n",data2[i]); 
            }
        
            if(flag_relay==1){
               GPIO_WriteBit(OPT_PIN,1);
            }else if(flag_relay==0){
               GPIO_WriteBit(OPT_PIN,0);
            }
        }else if(switch_control==0){ //�ر�
           //�ϵ���� д��flash��
            flash_t  flash;
            char data1[10]="123456789";
            char data2[10];
            flash_erase_sector(&flash,  ADDRESS_MEM);
            u8 return_value=flash_stream_write(&flash,  ADDRESS_MEM, 10,data1);
            char *argv2[] = { "conf", "save" };
            conf_cli(2, argv2);
            flash_stream_read(&flash,ADDRESS_MEM,10,data2);
            for(int i=0;i<10;i++){
                  printf("data2--:%c\n",data2[i]);
            }
           if(flag_relay==1){
               GPIO_WriteBit(OPT_PIN,  0);
           }else if(flag_relay==0){
               GPIO_WriteBit(OPT_PIN,  1);
           }
        }
       //end add
    } else if (sprop->val == &green_led) {
		set_led(led_green, green_led);
	}
	log_put(LOG_INFO "%s: %s set to %u",
	    __func__, sprop->name, *(u8 *)sprop->val);
	return AE_OK;
}

/*
 * Demo set function for integer and decimal properties.
 */
static enum ada_err demo_int_set(struct ada_sprop *sprop,
				const void *buf, size_t len)
{
	int ret;

	ret = ada_sprop_set_int(sprop, buf, len);
	if (ret) {
		return ret;
	}

	if (sprop->val == &input) {
		log_put(LOG_INFO "%s: %s set to %d",
		    __func__, sprop->name, input);
		output = input;
		prop_send_by_name("output");
	} else if (sprop->val == &decimal_in) {
		log_put(LOG_INFO "%s: %s set to %d",
		    __func__, sprop->name, decimal_in);
		decimal_out = decimal_in;
		prop_send_by_name("decimal_out");
	} else {
		return AE_NOT_FOUND;
	}
	return AE_OK;
}

/*
 * Demo set function for string properties.
 */
static enum ada_err demo_cmd_set(struct ada_sprop *sprop,
				const void *buf, size_t len)
{
	int ret;

	ret = ada_sprop_set_string(sprop, buf, len);
	if (ret) {
		return ret;
	}

	prop_send_by_name("log");
	log_put(LOG_INFO "%s: cloud set %s to \"%s\"",
	    __func__, sprop->name, cmd_buf);
	return AE_OK;
}


