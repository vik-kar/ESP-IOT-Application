#ifndef MAIN_APP_NVS_H_
#define MAIN_APP_NVS_H

#include "esp_err.h"
#include <stdbool.h>

/* Saves station mode WiFi Credentials to NVS 
   @return ESP_OK if successful
*/
esp_err_t app_nvs_save_sta_creds(void);

/* Loads prev saved credentials from NVS
   @return TRUE if prev saved credentials were found
*/
bool app_nvs_load_sta_creds(void);

/* Clears station mode credentials from NVS
   @return ESP_OK if successful
*/
esp_err_t app_nvs_clear_sta_creds(void);


#endif