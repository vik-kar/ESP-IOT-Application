#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "nvs_flash.h"

#include "app_nvs.h"
#include "wifi_app.h"

/* Create a tag for ESP_LOGI */
static const char TAG[] = "nvs";

/* NVS namespace used for station mode credentials */
const char app_nvs_sta_creds_namespace[] = "stacreds";

esp_err_t app_nvs_save_sta_creds(void){
	/* create a handle to access namespace */
	nvs_handle handle;
	
	/* Declare an esp_err_t variable to store any errors that we may get from function calls down the line 
	   - if `esp_err` is ever != ESP_OK, we break and return
	*/
	esp_err_t esp_err;
	
	ESP_LOGI(TAG, "app_nvs_save_sta_creds: Saving station mode credentials to flash");

	/* get the current wifi configuration */
	wifi_config_t *wifi_sta_config = wifi_app_get_wifi_config();

	/* Confirm the returned pointer is not null */
	if (wifi_sta_config){
		/* Pointer is valid, begin NVS operations */
		
		/* open the NVS and specifically, open the namespace that we are writing to
	       @param namespace name, mode (we did read and write), handle for the overall NVS partition	
		*/
		
		esp_err = nvs_open(app_nvs_sta_creds_namespace, NVS_READWRITE, &handle);
		
		/* Check if there was an error opening the NVS */
		if (esp_err != ESP_OK){
			printf("app_nvs_save_sta_creds: Error (%s) opening NVS handle!\n", esp_err_to_name(esp_err));
			return esp_err;
		}

		/* NVS opened successfully, so set SSID 
		   @param handle for NVS, key in k-v pair ("ssid"), value in k-v pair (actual ssid of connection), length (MAX_SSID_LENGTH from wifi_app.h)
		*/
		esp_err = nvs_set_blob(handle, "ssid", wifi_sta_config->sta.ssid, MAX_SSID_LENGTH);
		if (esp_err != ESP_OK){
			printf("app_nvs_save_sta_creds: Error (%s) setting SSID to NVS!\n", esp_err_to_name(esp_err));
			return esp_err;
		}

		/* Set Password - similar to what we did for setting ssid */
		esp_err = nvs_set_blob(handle, "password", wifi_sta_config->sta.password, MAX_PASSWORD_LENGTH);
		if (esp_err != ESP_OK){
			printf("app_nvs_save_sta_creds: Error (%s) setting Password to NVS!\n", esp_err_to_name(esp_err));
			return esp_err;
		}

		/* Commit credentials to NVS */
		esp_err = nvs_commit(handle);
		if (esp_err != ESP_OK){
			printf("app_nvs_save_sta_creds: Error (%s) comitting credentials to NVS!\n", esp_err_to_name(esp_err));
			return esp_err;
		}
		
		nvs_close(handle);
		ESP_LOGI(TAG, "app_nvs_save_sta_creds: wrote wifi_sta_config: Station SSID: %s Password: %s", wifi_sta_config->sta.ssid, wifi_sta_config->sta.password);
	}

	printf("app_nvs_save_sta_creds: returned ESP_OK\n");
	return ESP_OK;
}

/* Loads prev saved credentials from NVS
   @return TRUE if prev saved credentials were found
*/
bool app_nvs_load_sta_creds(void)
{
	/* Create an NVS handle */
	nvs_handle handle;
	
	/* create an error type to check successful function returns */
	esp_err_t esp_err;

	ESP_LOGI(TAG, "app_nvs_load_sta_creds: loading WiFi credentials from flash (from NVS)");

	/* Check if we can successfully open the NVS and our specific namespace in there */
	if (nvs_open(app_nvs_sta_creds_namespace, NVS_READONLY, &handle) == ESP_OK){
		wifi_config_t *wifi_sta_config = wifi_app_get_wifi_config();
		
		/* clear existing configurations since our SSID and pwd will come from NVS */
		memset(wifi_sta_config, 0x00, sizeof(wifi_config_t));

		/* Allocate a buffer that we'll write the NVS data to */
		size_t wifi_config_size = sizeof(wifi_config_t);
		
		/* uint8_t specifies that each byte of the allocated memory is to be treated as an unsigned 8-bit integer - meaning we can access the allocated memory byte by byte */
		uint8_t *wifi_config_buff = (uint8_t*)malloc(sizeof(uint8_t) * wifi_config_size);
		
		/* Zero-out the buffer */
		memset(wifi_config_buff, 0x00, sizeof(wifi_config_size));

		/* Load the details we need */
		
		/* get the size of the ssid field in our zero-d out wifi config struct */
		wifi_config_size = sizeof(wifi_sta_config->sta.ssid);
		
		/* copy the desired info into wifi_config_buffer */
		esp_err = nvs_get_blob(handle, "ssid", wifi_config_buff, &wifi_config_size);
		
		if (esp_err != ESP_OK){
			/* Operation failed, do memory cleanup */
			free(wifi_config_buff);
			printf("app_nvs_load_sta_creds: (%s) no station SSID found in NVS\n", esp_err_to_name(esp_err));
			return false;
		}
		
		/* copy from buffer and into wifi_config_struct (specifically into the sta.ssid area) */
		memcpy(wifi_sta_config->sta.ssid, wifi_config_buff, wifi_config_size);

		/* Similar routine for loading password */
		wifi_config_size = sizeof(wifi_sta_config->sta.password);
		esp_err = nvs_get_blob(handle, "password", wifi_config_buff, &wifi_config_size);
		if (esp_err != ESP_OK){
			free(wifi_config_buff);
			printf("app_nvs_load_sta_creds: (%s) retrieving password!\n", esp_err_to_name(esp_err));
			return false;
		}
		memcpy(wifi_sta_config->sta.password, wifi_config_buff, wifi_config_size);

		free(wifi_config_buff);
		nvs_close(handle);

		printf("app_nvs_load_sta_creds: SSID: %s Password: %s\n", wifi_sta_config->sta.ssid, wifi_sta_config->sta.password);
		
		/* return the first character of SSID. If != NULL, then we know it returned true (ie the SSID is loaded) */
		return wifi_sta_config->sta.ssid[0] != '\0';
	}
	else{
		return false;
	}
}

/* Clears station mode credentials from NVS
   @return ESP_OK if successful
*/
esp_err_t app_nvs_clear_sta_creds(void) {
	nvs_handle handle;
	esp_err_t esp_err;
	ESP_LOGI(TAG, "app_nvs_clear_sta_creds: Clearing Wifi station mode credentials from flash");

	esp_err = nvs_open(app_nvs_sta_creds_namespace, NVS_READWRITE, &handle);
	if (esp_err != ESP_OK){
		printf("app_nvs_clear_sta_creds: Error (%s) opening NVS handle!\n", esp_err_to_name(esp_err));
		return esp_err;
	}

	// Erase credentials
	esp_err = nvs_erase_all(handle);
	if (esp_err != ESP_OK){
		printf("app_nvs_clear_sta_creds: Error (%s) erasing station mode credentials!\n", esp_err_to_name(esp_err));
		return esp_err;
	}

	// Commit clearing credentials from NVS
	esp_err = nvs_commit(handle);
	if (esp_err != ESP_OK){
		printf("app_nvs_clear_sta_creds: Error (%s) NVS commit!\n", esp_err_to_name(esp_err));
		return esp_err;
	}
	nvs_close(handle);

	printf("app_nvs_clear_sta_creds: returned ESP_OK\n");
	return ESP_OK;
}












































