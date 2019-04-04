#ifndef WIFI_AIRKISS_H_
#define WIFI_AIRKISS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "airkiss.h"
  
#define AKS_STOP_PBM_TMO        1000

  
void adw_wifi_stop_aks(void *fn);

void adw_wifi_start_aks(void *fn);

void adw_wifi_aks_send_fin();

  
#ifdef __cplusplus
}
#endif

#endif /* WIFI_AIRKISS_H_ */
