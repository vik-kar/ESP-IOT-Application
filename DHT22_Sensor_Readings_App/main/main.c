/* Application entry point */

#include "esp_log.h"
#include "DHT22.h"
#include "nvs_flash.h"
#include "wifi_app.h"
#include "wifi_reset_button.h"
#include "sntp_time_sync.h"

/* logging tag */
static const char TAG[] = "main";

/* this function will be set as our callback */
void wifi_application_connected_events(void){
	ESP_LOGI(TAG, "WiFi Application Connected");
	
	sntp_time_sync_task_start();
}


void app_main(void){
    /* Initialize NVS (non-volatile storage) 
       Initializing NVS attempts to handle errors by erasing the flash and retrying instead of simply aborting as the ESP error check would do
    */
    esp_err_t ret = nvs_flash_init();
    if(ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND){
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);
	
	/* start WiFi */
	wifi_app_start();
	
	/* Configure the WiFi reset button */
	wifi_reset_button_config();
	
	/* Start reading DHT22 */
	DHT22_task_start();
	
	/* Set the connected event callback */
	wifi_app_set_callback(&wifi_application_connected_events);
}
