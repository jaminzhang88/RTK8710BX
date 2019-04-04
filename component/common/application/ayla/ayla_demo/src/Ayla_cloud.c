#include "basic_types.h"
#include "example_wlan_fast_connect.h"
#include "task.h"
#include "log.h"
extern int wlan_wrtie_reconnect_data_to_flash(u8 *data, uint32_t len);
extern int ayla_demo_init();

int ayla_cloud()
{
	  
	p_wlan_init_done_callback = NULL;
	
	if(xTaskCreate((TaskFunction_t)ayla_demo_init, (char const *)"Ayla_entry", 1024*2, NULL, tskIDLE_PRIORITY + 3, NULL) != pdPASS) {
		log_put(LOG_ERR "xTaskCreate failed");
	}
	//ayla_demo_init();
        //printf("init >>>>><<<<<<success\n");
	return 0;
}

void example_ayla_cloud(void)
{
	//have to be called after wlan init done.
	p_wlan_init_done_callback = ayla_cloud;		
	
	// Call back from application layer after wifi_connection success
	p_write_reconnect_ptr = wlan_wrtie_reconnect_data_to_flash;
}

