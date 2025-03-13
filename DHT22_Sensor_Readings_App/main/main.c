/* Application entry point */

#include "DHT22.h"
#include "nvs_flash.h"
#include "wifi_app.h"


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
	
	/* Start reading DHT22 */
	DHT22_task_start();
}
