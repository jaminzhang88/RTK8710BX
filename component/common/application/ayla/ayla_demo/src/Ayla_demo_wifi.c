/*
 * Copyright 2017 Ayla Networks, Inc.  All rights reserved.
 */
#include <sys/types.h>
#include <string.h>
#include <stdio.h>

#include <ayla/utypes.h>
#include <ada/libada.h>
#include <ayla/assert.h>
#include <ada/err.h>
#include <ayla/log.h>
#include <ada/dnss.h>
#include <net/net.h>
#include <ada/server_req.h>
#include <adw/wifi.h>
#include <adw/wifi_conf.h>
#include <httpd/httpd.h>

#include "conf_wifi.h"
#include "demo.h"
#include "PinNames.h"
/*
 * Event handler.
 * This is called by the Wi-Fi subsystem on connects and disconnects
 * and similar events.
 * This allows the application to start or stop services on those events,
 * and to implement status LEDs, for example.
 */
extern void prop_send_by_name(const char *name);
static void demo_wifi_event_handler(enum adw_wifi_event_id id, void *arg)
{
	static u8 is_cloud_started;
	switch (id) {
	case ADW_EVID_AP_START:
		break;

	case ADW_EVID_AP_UP:
		server_up();
		dnss_up();
		break;

	case ADW_EVID_AP_DOWN:
	        httpd_stop();  //¹Ø±ÕlanÄ£Ê½
		dnss_down();
		break;

	case ADW_EVID_STA_UP:
		log_put(LOG_DEBUG "Wifi associated with a AP!");
		break;

	case ADW_EVID_STA_DHCP_UP:
		ada_client_up();
		/*to enlarge heap size,will not set up web server in 8710BX*/
	#if CONFIG_PLATFORM_8711B
		//server_up();
	#else 
		server_up();
	#endif

		if (!is_cloud_started) {
			/*if (xTaskCreate(demo_idle_enter, "A_LedEvb",
			    DEMO_APP_STACKSZ, NULL, DEMO_APP_PRIO,
			    NULL) != pdPASS) {
				AYLA_ASSERT_NOTREACHED();
			}*/
	
	prop_send_by_name("oem_host_version");
	prop_send_by_name("version");
	is_cloud_started = 1;
		}
		break;

	case ADW_EVID_STA_DOWN:
		ada_client_down();
		break;

	case ADW_EVID_SETUP_START:
	case ADW_EVID_SETUP_STOP:
	case ADW_EVID_ENABLE:
	case ADW_EVID_DISABLE:
		break;

	case ADW_EVID_RESTART_FAILED:
		log_put(LOG_WARN "resetting due to Wi-Fi failure");
		/* sys_msleep(400);
		arch_reboot(); */
		break;

	default:
		break;
	}
}

void demo_wifi_enable(void)
{
	char *argv[] = { "wifi", "enable" };

	adw_wifi_cli(2, argv);
}

void demo_wifi_init(void)
{
	struct ada_conf *cf = &ada_conf;
	int enable_redirect = 1;
	char ssid[32];
	adw_wifi_event_register(demo_wifi_event_handler, NULL);
	adw_wifi_init();
	adw_wifi_page_init(enable_redirect);

	/*
	 * Set the network name for AP mode, for use during Wi-Fi setup.
	 */
	cf->mac_addr = LwIP_GetMAC(&xnetif[0]);
	snprintf(ssid, sizeof(ssid),
	    OEM_AP_SSID_PREFIX "-%2x%2x%2x%2x%2x%2x",
	    cf->mac_addr[0], cf->mac_addr[1], cf->mac_addr[2],
	    cf->mac_addr[3], cf->mac_addr[4], cf->mac_addr[5]);
	adw_wifi_ap_ssid_set(ssid);
	adw_wifi_ios_setup_app_set(OEM_IOS_APP);

}
