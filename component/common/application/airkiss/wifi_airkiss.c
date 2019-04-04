#include "wifi_airkiss.h"
#include "airkiss.h"
extern int flag_work_mode,flag_ap_mode,flag_airkiss_mode,flag_device_down;

void adw_wifi_stop_aks(void *wifi)
{
  printf("->>adw stop airkiss\r\n");
  user_airkiss_stop();
}

void adw_wifi_start_aks(void *wifi)
{
  flag_work_mode=0;
  flag_ap_mode=0;
  flag_airkiss_mode=1;
  flag_device_down=0;
  printf("->>adw start airkiss\r\n");
  user_airkiss_start();
}

void adw_wifi_aks_send_fin()
{
  printf("->>adw airkiss send finish data\r\n");
  user_airkiss_send_notification();
}