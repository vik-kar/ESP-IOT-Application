#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_err.h"
#include "esp_interface.h"
#include "esp_log.h"
#include "esp_wifi_types_generic.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "app_nvs.h"
#include "wifi_app.h"

/* Create a tag for ESP_LOGI */
static const char TAG[] = "nvs";

/* NVS namespace used for station mode credentials */
const char app_nvs_sta_creds_namespace[] = "stacreds";

/* Saves station mode WiFi Credentials to NVS 
   @return ESP_OK if successful
*/
esp_err_t app_nvs_save_sta_creds(void){
	/* create a handle to access namespace */
	nvs_handle handle;
	
	/* Declare an esp_err_t variable to store any errors that we may get from function calls down the line 
	   - if `esp_err` is ever != ESP_OK, we break and return
	*/
	esp_err_t esp_err;
	
	/* Quick log */
	ESP_LOGI(TAG, "app_nvs_save_sta_creds: Saving station mode credentials to flash");
	
	/* get the current wifi configuration */
	wifi_config_t *wifi_sta_config = wifi_app_get_wifi_config();
	
	/* Confirm the returned pointer is not null */
	if(wifi_sta_config){
		/* Pointer is valid, begin NVS operations */
		
		/* open the NVS and specifically, open the namespace that we are writing to
	       @param namespace name, mode (we did read and write), handle for the overall NVS partition	
		*/
		esp_err = nvs_open(app_nvs_sta_creds_namespace, NVS_READWRITE, &handle);
		
		/* Check if there was an error opening the NVS */
		if(esp_err != ESP_OK){
			printf("app_nvs_save_sta_creds: Error (%s) opening NVS handle", esp_err_to_name(esp_err));
			return esp_err;
		}
		
		/* NVS opened successfully, so set SSID 
		   @param handle for NVS, key in k-v pair ("ssid"), value in k-v pair (actual ssid of connection), length (MAX_SSID_LENGTH from wifi_app.h)
		*/
		esp_err = nvs_set_blob(handle, "ssid", wifi_sta_config->sta.ssid, MAX_SSID_LENGTH);
		if(esp_err != ESP_OK){
			printf("app_nvs_save_sta_creds: Error (%s) setting SSID to NVS", esp_err_to_name(esp_err));
		}
		
		/* Set Password - similar to what we did for setting ssid*/
		esp_err = nvs_set_blob(handle, "password", wifi_sta_config->sta.password, MAX_PASSWORD_LENGTH);
		if(esp_err != ESP_OK){
			printf("app_nvs_save_sta_creds: Error (%s) setting password to NVS", esp_err_to_name(esp_err));
		}
		
		/* Commit credentials to NVS */
		esp_err = nvs_commit(handle);
		
		/* Error checking from nvs_commit operation */
		if(esp_err != ESP_OK){
			printf("app_nvs_save_sta_creds: Error (%s) committing credentials to NVS", esp_err_to_name(esp_err));
		}
		
		/* Close NVS */
		nvs_close(handle);
		
		ESP_LOGI(TAG, "app_nvs_save_sta_creds: wrote wifi_sta_config: Station SSID: %s Password: %s", wifi_sta_config->sta.ssid, wifi_sta_config->sta.password);
	}
	ESP_LOGI(TAG, "app_nvs_save_sta_creds: returned ESP_OK\n");
	return ESP_OK;
}

/* Loads prev saved credentials from NVS
   @return TRUE if prev saved credentials were found
*/
bool app_nvs_load_sta_creds(void){
	/* Create an NVS handle */
	nvs_handle handle;
	
	/* create an error type to check successful function returns */
	esp_err_t esp_err;
	
	ESP_LOGI(TAG, "app_nvs_load_sta_creds: loading WiFi credentials from flash (from NVS)");
	
	/* Check if we can successfully open the NVS and our specific namespace in there */
	if(nvs_open(app_nvs_sta_creds_namespace, NVS_READONLY, &handle) == ESP_OK){
		wifi_config_t *wifi_sta_config = wifi_app_get_wifi_config();
		
		/* check if we even have a current configuration */
		if(wifi_sta_config == NULL){
			/* allocate memory for wifi_sta_config, to fill it with the data we get from NVS later */
			wifi_sta_config = (wifi_config_t*) malloc(sizeof(wifi_config_t));
		}
		
		/* clear existing configurations since our SSID and pwd will come from NVS */
		memset(wifi_sta_config, 0x00, sizeof(wifi_config_t));
		
		/* Allocate a buffer that we'll write the NVS data to */
		size_t wifi_config_size = sizeof(wifi_config_t);
		
		/* uint8_t specifies that each byte of the allocated memory is to be treated as an unsigned 8-bit integer - meaning we can access the allocated memory byte by byte */
		uint8_t *wifi_config_buffer = (uint8_t *) malloc(wifi_config_size);
		
		/* Zero-out the buffer */
		memset(wifi_config_buffer, 0x00, sizeof(wifi_config_size));
		
		/* Load the details we need */
		
		/* get the size of the ssid field in our zero-d out wifi config struct */
		size_t wifi_sta_ssid_size = sizeof(wifi_sta_config->sta.ssid);
		
		/* copy the desired info into wifi_config_buffer */
		esp_err = nvs_get_blob(handle, "ssid", wifi_config_buffer, &wifi_sta_ssid_size);
		
		if(esp_err != ESP_OK){
			/* Operation failed, do memory cleanup */
			free(wifi_config_buffer);
			free(wifi_sta_config);
			printf("app_nvs_load_sta_creds: (%s) no station SSID found in NVS", esp_err_to_name(esp_err));
			return false;
		}
		
		/* copy from buffer and into wifi_config_struct (specifically into the sta.ssid area) */
		memcpy(wifi_sta_config->sta.ssid, wifi_config_buffer, wifi_sta_ssid_size);
		
		/* Similar routine for loading password */
		size_t wifi_sta_pwd_size = sizeof(wifi_sta_config->sta.password);
		
		/* Get the desired info */
		esp_err = nvs_get_blob(handle, "password", wifi_config_buffer, &wifi_sta_pwd_size);
		if(esp_err != ESP_OK){
			/* Operation failed, do memory cleanup */
			free(wifi_config_buffer);
			free(wifi_sta_config);
			printf("app_nvs_load_sta_creds: (%s) no station password found in NVS", esp_err_to_name(esp_err));
			return false;
		}
		
		/* copy from buffer into wifi config struct */
		memcpy(wifi_sta_config->sta.password, wifi_config_buffer, wifi_sta_pwd_size);
		
		/* After successful copying, we can free allocated memory */
		free(wifi_config_buffer);
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
esp_err_t app_nvs_clear_sta_creds(void){
	nvs_handle handle;
	esp_err_t esp_err;
	
	ESP_LOGI(TAG, "app_nvs_clear_sta_creds: Clearing WiFi station mode credentials from flash");
	
	esp_err = nvs_open(app_nvs_sta_creds_namespace, NVS_READWRITE, &handle);
	if(esp_err != ESP_OK){
		printf("app_nvs_clear_sta_creds: Error (%s) opening NVS handle", esp_err_to_name(esp_err));
		return esp_err;
	}
	
	/* Proceed to erase credentials */
	esp_err = nvs_erase_all(handle);
	
	if(esp_err != ESP_OK){
		printf("app_nvs_clear_sta_creds: Error (%s) while erasing station mode credentials", esp_err_to_name(esp_err));
		return esp_err;
	}	
	
	/* Commit the clearing of credentials from NVS */
	esp_err = nvs_commit(handle);
	if(esp_err != ESP_OK){
		printf("app_nvs_clear_sta_creds: Error (%s) in nvs_commit", esp_err_to_name(esp_err));
		return esp_err;
	}
	
	nvs_close(handle);
	printf("app_nvs_clear_sta_creds: Returned ESP_OK");
	return ESP_OK;
}




