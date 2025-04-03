#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "http_server.h"
#include "esp_netif_ip_addr.h"
#include "esp_partition.h"
#include "esp_timer.h"
#include "esp_wifi_types_generic.h"
#include "freertos/idf_additions.h"
#include "http_parser.h"
#include "portmacro.h"
#include "protocomm_httpd.h"
#include "tasks_common.h"
#include "wifi_app.h"
#include "esp_ota_ops.h"
#include "sys/param.h"
#include "DHT22.h"
#include "esp_wifi.h"
#include "sntp_time_sync.h"

/* Tag used for ESP serial console messages */
static const char TAG[] = "http_server";

/* WiFi connect status */
static int g_wifi_connect_status = NONE;

/* Global var for fw update status */
static int g_fw_update_status = OTA_UPDATE_PENDING;

/* variable to see if time is set or not (local time status) */
static bool g_is_local_time_set = 0;

/* Create HTTP Server Task handle 
   - This is the HTTP server instance handle that we'll need to use when we call various functions from the API
*/
static httpd_handle_t http_server_handle = NULL;

/* Create the HTTP server monitor task handle */
static TaskHandle_t task_http_server_monitor = NULL;

/* Create the queue handle - used to manipulate the main queue of events */
static QueueHandle_t http_server_monitor_queue_handle;

/* ESP32 timer configuration which is passed to esp timer create */
const esp_timer_create_args_t fw_update_reset_args = {
	.callback = &http_server_fw_update_reset_callback,
	.arg = NULL,
	.dispatch_method = ESP_TIMER_TASK,
	.name = "fw_update_reset"
};
esp_timer_handle_t fw_update_reset;

/* Take care of the embedded files 
   - JQuery, index.html, app.css, app.js, favicon.ico files
*/
extern const uint8_t jquery_3_3_1_min_js_start[]	asm("_binary_jquery_3_3_1_min_js_start");
extern const uint8_t jquery_3_3_1_min_js_end[]		asm("_binary_jquery_3_3_1_min_js_end");

extern const uint8_t index_html_start[]				asm("_binary_index_html_start");
extern const uint8_t index_html_end[]  				asm("_binary_index_html_end");

extern const uint8_t app_css_start[]				asm("_binary_app_css_start");
extern const uint8_t app_css_end[]					asm("_binary_app_css_end");

extern const uint8_t app_js_start[]					asm("_binary_app_js_start");	
extern const uint8_t app_js_end[]					asm("_binary_app_js_end");		
	
extern const uint8_t favicon_ico_start[]			asm("_binary_favicon_ico_start");	
extern const uint8_t favicon_ico_end[]				asm("_binary_favicon_ico_end");

/* Checks the global fw update status (g_fw_update_status) 
   Creates the fw_update_reset_timer if the g_fw_update_status = true
*/
static void http_server_fw_update_reset_timer(void){
	if(g_fw_update_status == OTA_UPDATE_SUCCESSFUL){
		ESP_LOGI(TAG, "http_server_fw_update_reset_timer: FW update successful starting FW udpate reset timer");
		
		/* Give webpage a chance to receive an ack back & initialize the timer */
		ESP_ERROR_CHECK(esp_timer_create(&fw_update_reset_args, &fw_update_reset));
		
		ESP_ERROR_CHECK(esp_timer_start_once(fw_update_reset, 8000000));		
	}
	else{
		ESP_LOGI(TAG, "http_server_fw_update_reset_timer: FW update unsuccessful");
	}
}

/* HTTP Server Monitor task used to track events of the HTTP server 
   @param pvParamters parameter which can be passed to the task
*/
static void http_server_monitor(void *parameter){
	/* Need an instance of the queue message handle */
	http_server_queue_message_t msg;
	
	/* Create the endless loop */
	while(1){
		if(xQueueReceive(http_server_monitor_queue_handle, &msg, portMAX_DELAY)){
			switch(msg.msgID){
				case HTTP_MSG_WIFI_CONNECT_INIT:
					ESP_LOGI(TAG, "HTTP_MSG_WIFI_CONNECT_INIT");
					g_wifi_connect_status = HTTP_WIFI_STATUS_CONNECTING;
					break;
					
				case HTTP_MSG_WIFI_CONNECT_SUCCESS:
					ESP_LOGI(TAG, "HTTP_MSG_WIFI_CONNECT_SUCCESS");
					g_wifi_connect_status = HTTP_WIFI_STATUS_CONNECT_SUCCESS;
					break;
					
				case HTTP_MSG_WIFI_CONNECT_FAIL:
					ESP_LOGI(TAG, "HTTP_MSG_WIFI_CONNECT_FAIL");
					g_wifi_connect_status = HTTP_MSG_WIFI_CONNECT_FAIL;
					break;
					
				case HTTP_MSG_WIFI_USER_DISCONNECT:
					ESP_LOGI(TAG, "HTTP_MSG_WIFI_USER_DISCONNECT");
					g_wifi_connect_status = HTTP_WIFI_STATUS_DISCONNECTED;
					break;
					
				case HTTP_MSG_OTA_UPDATE_SUCCESSFUL:
					ESP_LOGI(TAG, "HTTP_MSG_OTA_UPDATE_SUCCESSFUL");
					g_fw_update_status = OTA_UPDATE_SUCCESSFUL; 
					http_server_fw_update_reset_timer();
					break;
					
				case HTTP_MSG_OTA_UPDATE_FAILED:
					ESP_LOGI(TAG, "HTTP_MSG_OTA_UPDATE_FAILED");
					g_fw_update_status = OTA_UPDATE_FAILED; 
					break;
					
				case HTTP_MSG_OTA_UPDATE_INITIALIZED:
					ESP_LOGI(TAG, "HTTP_MSG_OTA_UPDATE_INITIALIZED");
					break;

				case HTTP_MSG_TIME_SERVICE_INITIALIZED:
					ESP_LOGI(TAG, "HTTP_MSG_TIME_SERVICE_INITIALIZED");
					g_is_local_time_set = 1;
					break;
					
				default:
					break;
			}
		}
	}
}


/* Define the file handlers */

/* jquery get handler is requested when accessing the webpage 
   @param req is the HTTP request for which the URI needs to be handled. 'req' holds all the information for the incoming request
   @return ESP_OK
*/

static esp_err_t http_server_jquery_handler(httpd_req_t *req){
	ESP_LOGI(TAG, "jquery requested");
	
	httpd_resp_set_type(req, "application/javascript");
	
	/* send response 
       @param request, followed by the buffer, and then the buffer length	
	*/
	httpd_resp_send(req, (const char *) jquery_3_3_1_min_js_start, jquery_3_3_1_min_js_end - jquery_3_3_1_min_js_start);
	return ESP_OK;
}

/* Sends the index.html page when it is requested */

static esp_err_t http_server_index_html_handler(httpd_req_t *req){
	ESP_LOGI(TAG, "index.html");
	
	httpd_resp_set_type(req, "text/html");
	
	/* send response 
       @param request, followed by the buffer, and then the buffer length	
	*/
	httpd_resp_send(req, (const char *) index_html_start, index_html_end - index_html_start);
	return ESP_OK;
}

/* app.css get handler is requested when accessing the web page */

static esp_err_t http_server_app_css_handler(httpd_req_t *req){
	ESP_LOGI(TAG, "app.css requested");
	
	httpd_resp_set_type(req, "text/css");
	
	/* send response 
       @param request, followed by the buffer, and then the buffer length	
	*/
	httpd_resp_send(req, (const char *) app_css_start, app_css_end - app_css_start);
	return ESP_OK;
}

/* app.js get handler is requested when accessing the web page */

static esp_err_t http_server_app_js_handler(httpd_req_t *req){
	ESP_LOGI(TAG, "app.js requested");
	
	httpd_resp_set_type(req, "application/javascript");
	
	/* send response 
       @param request, followed by the buffer, and then the buffer length	
	*/
	httpd_resp_send(req, (const char *) app_js_start, app_js_end - app_js_start);
	return ESP_OK;
}

/* sends the .ico icon file when accessing the webpage */

static esp_err_t http_server_favicon_ico_handler(httpd_req_t *req){
	ESP_LOGI(TAG, "favicon.ico requested");
	
	httpd_resp_set_type(req, "image/x-icon");
	
	/* send response 
       @param request, followed by the buffer, and then the buffer length	
	*/
	httpd_resp_send(req, (const char *) favicon_ico_start, favicon_ico_end - favicon_ico_start);
	return ESP_OK;
}

/* Receives the .bin file via the webpage and handles the firmware update 
   @param: req is the HTTP request for which the URI needs to be handled
   @return ESP_OK otherwise ESP_FAIL if t/o and update cannot be started
*/
esp_err_t http_server_ota_update_handler(httpd_req_t *req){
	esp_ota_handle_t ota_handle;
	
	/* Buffer to hold data received from the webpage */
	char ota_buff[1024];
	
	/* hold content length*/
	int content_length = req->content_len;
	
	/* Track content received and set it to 0 */
	int content_received = 0; 
	
	/* Receive data from each http request function call */
	int recv_len; 
	
	/* Variable telling us when the actual firmware update file content is found (the firmware file is the body) */
	bool is_req_body_started = false; 
	
	/* Var to track the flash status */
	bool flash_successful = false;
	
	/* Now, we need an instance of the ESP partition struct */
	const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
	
	/* Do-while loop to receive the update file */
	do{
		/* Read the data for the request */
		if((recv_len = httpd_req_recv(req, ota_buff, MIN(content_length, sizeof(ota_buff)))) < 0){
			
			/* If less than 0, we need to handle a timeout error case here - check if timeout occurred */
			if(recv_len == HTTPD_SOCK_ERR_TIMEOUT){
				ESP_LOGI(TAG, "http_server_OTA_update_handler: Socket Timeout");
				continue; /* Retry receiving if timeout occurred */
			}
			ESP_LOGI(TAG, "http_server_OTA_update_handler: OTA other error %d", recv_len);
			return ESP_FAIL;
		}
		printf("http_server_OTA_update_handler: OTA RX: %d of %d \r", content_received, content_length);
		
		/* Check if this is the FIRST data that we are receiving, if so, it will have the info in the header that we need */
		if(!is_req_body_started){
			is_req_body_started = true;
			
			/* Get location of the .bin file content (remove the webform data from the request) 
			   Needed to get the body_start pointer to point to the beginning of the data that we want (excluding the webform data) 
			*/
			char *body_start_p = strstr(ota_buff, "\r\n\r\n") + strlen("\r\n\r\n");
			
			/* get the body part length from the receive length */
			int body_part_len = recv_len - (body_start_p - ota_buff);
			
			/* Print some information about the OTA file size, so that it shows up while we are receiving the file - give it content length */
			printf("http_server_OTA_update_handler: OTA file size: %d \r\n", content_length);
			
			esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
			
			if(err != ESP_OK){
				printf("http_server_OTA_update_handler: error with OTA begin, cancelling OTA\n\n");
				return ESP_FAIL;
			}
			else{
				printf("http_server_OTA_update_handler: Writing to partition subtype %d at offset 0x%lx\r\n", update_partition->subtype, (unsigned long) update_partition->address);
			}
			/* Write this first part of the data */
			esp_ota_write(ota_handle, body_start_p, body_part_len);
			content_received += body_part_len;
		}
		/* This is NOT the first data that we are receiving */
		else{
			/* Write OTA Data directly */
			esp_ota_write(ota_handle, ota_buff, recv_len);
			content_received += recv_len;
		}
	} while(recv_len > 0 && content_received < content_length);
	
	if(esp_ota_end(ota_handle) == ESP_OK){
		/* Update the partition */
		if(esp_ota_set_boot_partition(update_partition) == ESP_OK){
			const esp_partition_t *boot_partition = esp_ota_get_boot_partition();
			
			/* Log info about the next boot partition */
			ESP_LOGI(TAG, "http_server_OTA_update_handler: next boot partition subtype %d at offset 0x%lx", boot_partition->subtype, (unsigned long) boot_partition->address);
			flash_successful = true;
		}
		else{
			ESP_LOGI(TAG, "http_server_OTA_update_handler: FLASHED ERRROR");
		}
	}
	else{
		ESP_LOGI(TAG, "http_server_OTA_update_handler: esp_ota_end ERRROR");
	}
	
	/* Send message about the status */
	if(flash_successful){
		http_server_monitor_send_message(HTTP_MSG_OTA_UPDATE_SUCCESSFUL);
	}
	else{
		http_server_monitor_send_message(HTTP_MSG_OTA_UPDATE_FAILED);
	}
	
	return ESP_OK;
}

/* OTA status handler responds with the firmware update status after the OTA update is started
   and responds with the compile time and date when the page is first requested
   @param: req is HTTP request for which the URI needs to be handled
   @return ESP_OK
*/
esp_err_t http_server_ota_status_handler(httpd_req_t *req){
	char otaJSON[100];

	ESP_LOGI(TAG, "OTAstatus requested");

	sprintf(otaJSON, "{\"ota_update_status\":%d,\"compile_time\":\"%s\",\"compile_date\":\"%s\"}", g_fw_update_status, __TIME__, __DATE__);

	httpd_resp_set_type(req, "application/json");
	httpd_resp_send(req, otaJSON, strlen(otaJSON));

	return ESP_OK;
}

/* Define the sensor readings handler, responds with the current sensor data
   @param: req is HTTP request for which the URI needs to be handled
   @return ESP_OK
*/
static esp_err_t http_server_get_dht_sensor_readings_json_handler(httpd_req_t *req){
	ESP_LOGI(TAG, "/dhtSensor.json requested");
	
	/* 100 byte buffer to send data through */
	char dhtSensorJSON[100];
	
	/* sprintf is used to format and store a series of characters and values into a string */
	sprintf(dhtSensorJSON, "{\"temp\":\"%.1f\",\"humidity\":\"%.1f\"}", getTemperature(), getHumidity());
	
	httpd_resp_set_type(req, "application/json");
	httpd_resp_send(req, dhtSensorJSON, strlen(dhtSensorJSON));
	
	return ESP_OK;
}

/* this handler is invoked after the connect button is pressed - handles receiving the SSID and password entered by the user
   @param req HTTP request for which the URI needs to be handled
   @ret ESP_OK
*/
static esp_err_t http_server_wifi_connect_json_handler(httpd_req_t *req){
	ESP_LOGI(TAG, "/wifiConnect.json requested");
	
	/* Define vars for SSID and pwd. Need char * to hold the strings that we get */
	size_t len_ssid = 0, len_pass = 0;
	char* ssid_str = NULL, *pass_str = NULL;
	
	/* get SSID header */
	len_ssid = httpd_req_get_hdr_value_len(req, "my-connect-ssid") + 1;
	if(len_ssid > 1){
		
		/* ssid_str should be allocated memory for length of len_ssid */
		ssid_str = malloc(len_ssid);
		
		if(httpd_req_get_hdr_value_str(req, "my-connect-ssid", ssid_str, len_ssid) == ESP_OK){
			ESP_LOGI(TAG, "http_server_wifi_connect_json_handler: found header for my-connect-ssid: %s", ssid_str);
		}
	}
	/* get password header */
	len_pass = httpd_req_get_hdr_value_len(req, "my-connect-pwd") + 1;
	if(len_pass > 1){
		
		/* ssid_str should be allocated memory for length of len_ssid */
		pass_str = malloc(len_pass);
		
		if(httpd_req_get_hdr_value_str(req, "my-connect-pwd", pass_str, len_pass) == ESP_OK){
			ESP_LOGI(TAG, "http_server_wifi_connect_json_handler: found header for my-connect-pass: %s", pass_str);
		}
	}
	
	/* Now that we have the ssid and password, we can update the wifi network's configuration and let the wifi app know */
	wifi_config_t* wifi_config = wifi_app_get_wifi_config();
	memset(wifi_config, 0x00, sizeof(wifi_config_t));
	memcpy(wifi_config->sta.ssid, ssid_str, len_ssid);
	memcpy(wifi_config->sta.password, pass_str, len_pass);
	
	/* Send the WiFi app message */
	wifi_app_send_message(WIFI_APP_MSG_CONNECTING_FROM_HTTP_SERVER);
	
	free(ssid_str);
	free(pass_str);
	
	return ESP_OK;
}

/* WiFi connect status handler updates the connection status for the webpage 
   @param req HTTP request for which the URI needs to be handled
   @ret ESP_OK
*/
static esp_err_t http_server_wifi_connect_status_json_handler(httpd_req_t *req){
	ESP_LOGI(TAG, "/wifiConnectStatus requested");
	char statusJSON[100];
	sprintf(statusJSON, "{\"wifi_connect_status\":%d}", g_wifi_connect_status);

	httpd_resp_set_type(req, "application/json");
	httpd_resp_send(req, statusJSON, strlen(statusJSON));

	return ESP_OK;
}

/* WiFi connect info.json handler updates the webpage with connection information 
   @param req HTTP request for which the URI needs to be handled
   @ret ESP_OK
*/
static esp_err_t http_server_get_wifi_connect_info_json_handler(httpd_req_t *req){
	ESP_LOGI(TAG, "/wifiConnectInfo.json requested");
	char ipInfoJSON[200];
	memset(ipInfoJSON, 0, sizeof(ipInfoJSON));
	
	/* Buffer to hold IP address - IP4ADDR_STRLEN_MAX is predefiend */
	char ip[IP4ADDR_STRLEN_MAX];
	char netmask[IP4ADDR_STRLEN_MAX];
	char gw[IP4ADDR_STRLEN_MAX];
	
	if(g_wifi_connect_status == HTTP_WIFI_STATUS_CONNECT_SUCCESS){
		wifi_ap_record_t wifi_data;
		ESP_ERROR_CHECK(esp_wifi_sta_get_ap_info(&wifi_data));
		
		char *ssid = (char *)wifi_data.ssid;
		
		/* esp_netif_sta is our station object */
		esp_netif_ip_info_t ip_info;
		ESP_ERROR_CHECK(esp_netif_get_ip_info(esp_netif_sta, &ip_info));
		
		/* Convert IP to human readable 
		   @params
		   - reference to ip_info.ip
		   - ip buffer
		   - length of ip address
		*/
		esp_ip4addr_ntoa(&ip_info.ip, ip, IP4ADDR_STRLEN_MAX);
		esp_ip4addr_ntoa(&ip_info.netmask, netmask, IP4ADDR_STRLEN_MAX);
		esp_ip4addr_ntoa(&ip_info.gw, gw, IP4ADDR_STRLEN_MAX);
		
		sprintf(ipInfoJSON, "{\"ip\":\"%s\",\"netmask\":\"%s\",\"gw\":\"%s\",\"ap\":\"%s\"}", ip, netmask, gw, ssid);
	}
	/* Set response for the outgoing response */
	httpd_resp_set_type(req, "application/json");
	httpd_resp_send(req, ipInfoJSON, strlen(ipInfoJSON));
	
	return ESP_OK;
}

/* WiFi disconnect.json handler handles the click of the disconnect button 
   @param req HTTP request for which the URI needs to be handled
   @ret ESP_OK
*/
static esp_err_t http_server_wifi_disconnect_json_handler(httpd_req_t *req){
	ESP_LOGI(TAG, "/wifiDisconnect.json requested");
	
	wifi_app_send_message(WIFI_APP_MSG_USER_REQUESTED_STA_DISCONNECT);
	
	return ESP_OK;
}

/* localTime.json responds by sending the local time
   @param req HTTP request for which the URI needs to be handled
   @ret ESP_OK
*/
static esp_err_t http_server_get_local_time_json_handler(httpd_req_t *req){
	ESP_LOGI(TAG, "/localTime.json requested");

	/* 100 byte buffer set to 0 */
	char localTimeJson[100] = {0};

	/* use a global variable to see if the time was set. If it is set, the current local time is formatted into the JSON buffer with sprintf */
	if (g_is_local_time_set){
		sprintf(localTimeJson, "{\"time\":\"%s\"}", sntp_time_sync_get_time());
	}

	/* Set response type */
	httpd_resp_set_type(req, "application/json");
	httpd_resp_send(req, localTimeJson, strlen(localTimeJson));

	return ESP_OK;
}

/* Sets up the default HTTPD server configuration 
   @return http server instance handle if successful, null otherwise
*/
static httpd_handle_t http_server_configure(void){
	/* generate the default configuration */
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();
	
	/* Create HTTP server monitor task */
	xTaskCreatePinnedToCore(&http_server_monitor, "http_server_monitor", HTTP_SERVER_MONITOR_STACK_SIZE, NULL, HTTP_SERVER_MONITOR_PRIORITY, &task_http_server_monitor, HTTP_SERVER_MONITOR_CORE_ID);
	
	/* Create the message queue */
	http_server_monitor_queue_handle = xQueueCreate(3, sizeof(http_server_queue_message_t));
	
	/* Specify core of the HTTP server */
	config.core_id = HTTP_SERVER_TASK_CORE_ID;
	
	/* Adjust default priority to one less than WiFi app task */
	config.task_priority = HTTP_SERVER_TASK_PRIORITY;
	
	/* Increase the stack size from the default 4096 bytes */
	config.stack_size = HTTP_SERVER_TASK_STACK_SIZE;
	
	/* Increase URI handlers */
	config.max_uri_handlers = 20;
	
	/* Increase the timeout limits (10 seconds) */
	config.recv_wait_timeout = 10;
	config.recv_wait_timeout = 10;
	
	/* Log the config information */
	ESP_LOGI(TAG, 
			"http_server_configure: Starting server on port: '%d' with task priority: '%d'", 
			config.server_port, config.task_priority);
			
	/* Start the server */
	if(httpd_start(&http_server_handle, &config) == ESP_OK){
		ESP_LOGI(TAG, "http_server_configure: Registering URI handlers");
		
		/* register the jquery handler */
		httpd_uri_t jquery_js = {
			.uri = "/jquery-3.3.1.min.js",
			.method = HTTP_GET,
			.handler = http_server_jquery_handler,
			.user_ctx = NULL,
		};
		httpd_register_uri_handler(http_server_handle, &jquery_js); 
		
		/* register the index.html handler */
		httpd_uri_t index_html = {
			.uri = "/",
			.method = HTTP_GET,
			.handler = http_server_index_html_handler,
			.user_ctx = NULL,
		};
		httpd_register_uri_handler(http_server_handle, &index_html); 
		
		/* register the app.css handler */
		httpd_uri_t app_css = {
			.uri = "/app.css",
			.method = HTTP_GET,
			.handler = http_server_app_css_handler,
			.user_ctx = NULL,
		};
		httpd_register_uri_handler(http_server_handle, &app_css);
		
		/* register the app.css handler */
		httpd_uri_t app_js = {
			.uri = "/app.js",
			.method = HTTP_GET,
			.handler = http_server_app_js_handler,
			.user_ctx = NULL,
		};
		httpd_register_uri_handler(http_server_handle, &app_js);
		
		/* register the favicon.ico handler */
		httpd_uri_t favicon_ico = {
			.uri = "/favicon.ico",
			.method = HTTP_GET,
			.handler = http_server_favicon_ico_handler,
			.user_ctx = NULL,
		};
		httpd_register_uri_handler(http_server_handle, &favicon_ico);
		
		/* register OTA update handler */
		httpd_uri_t OTA_update = {
			.uri = "/OTAupdate",
			.method = HTTP_POST,
			.handler = http_server_ota_update_handler,
			.user_ctx = NULL,
		};
		httpd_register_uri_handler(http_server_handle, &OTA_update);
		
		/* Register the OTA status handler */
		httpd_uri_t OTA_status = {
			.uri = "/OTAstatus",
			.method = HTTP_POST,
			.handler = http_server_ota_status_handler,
			.user_ctx = NULL,
		};
		httpd_register_uri_handler(http_server_handle, &OTA_status);
		
		/* Register dhtSensor.json handler */
		httpd_uri_t dht_sensor_json = {
			.uri = "/dhtSensor.json",
			.method = HTTP_GET,
			.handler = http_server_get_dht_sensor_readings_json_handler,
			.user_ctx = NULL,
		};
		httpd_register_uri_handler(http_server_handle, &dht_sensor_json);
		
		/* Register wifi_connect.json handler */
		httpd_uri_t wifi_connect_json = {
			.uri = "/wifiConnect.json",
			.method = HTTP_POST,
			.handler = http_server_wifi_connect_json_handler,
			.user_ctx = NULL,
		};
		httpd_register_uri_handler(http_server_handle, &wifi_connect_json);
		
		/* Register wifi_connectStatus.json handler */
		httpd_uri_t wifi_connect_status_json = {
			.uri = "/wifiConnectStatus",
			.method = HTTP_POST,
			.handler = http_server_wifi_connect_status_json_handler,
			.user_ctx = NULL,
		};
		httpd_register_uri_handler(http_server_handle, &wifi_connect_status_json);
		
		/* Register wifiConnectInfo.json handler */
		httpd_uri_t wifi_connect_info_json = {
			.uri = "/wifiConnectInfo.json",
			.method = HTTP_GET,
			.handler = http_server_get_wifi_connect_info_json_handler,
			.user_ctx = NULL,
		};
		httpd_register_uri_handler(http_server_handle, &wifi_connect_info_json);
		
		/* Register the handler to disconnect WiFi */
		httpd_uri_t wifi_disconnect_json = {
			.uri = "/wifiDisconnect.json",
			.method = HTTP_DELETE,
			.handler = http_server_wifi_disconnect_json_handler,
			.user_ctx = NULL,
		};
		httpd_register_uri_handler(http_server_handle, &wifi_disconnect_json);

		/* Register the handler to get local time */
		httpd_uri_t local_time_json = {
			.uri = "/localTime.json",
			.method = HTTP_GET,
			.handler = http_server_get_local_time_json_handler,
			.user_ctx = NULL,
		};
		httpd_register_uri_handler(http_server_handle, &local_time_json);
		
		ESP_LOGI(TAG, "http_server_configure: Registered URI handlers");
		
		/* Return the handle */
		return http_server_handle;
	}
	return NULL;
}

/* Function that starts the HTTP server */
void http_server_start(void){
	if(http_server_handle == NULL){
		http_server_handle = http_server_configure();
	}
}

/* Function that stops the HTTP server */
void http_server_stop(void){
	/* check if the handle has been initialized already */
	if(http_server_handle){
		httpd_stop(http_server_handle);
		ESP_LOGI(TAG, "http_server_stop: stopping HTTP server");
		http_server_handle = NULL;
	}
	/* Stop the monitor as well */
	if(task_http_server_monitor){
		vTaskDelete(task_http_server_monitor);
		ESP_LOGI(TAG, "http_server_stop: stopping http server monitor");
		task_http_server_monitor = NULL;
	}
}

BaseType_t http_server_monitor_send_message(http_server_message_e msgID){
	/* Create an instance of the server queue message type */
	http_server_queue_message_t msg;
	msg.msgID = msgID;
	
	return xQueueSend(http_server_monitor_queue_handle, &msg, portMAX_DELAY);
}

void http_server_fw_update_reset_callback(void *arg){
	ESP_LOGI(TAG, "http_server_fw_update_reset_callback: Timer timed out, restarting the device");
	esp_restart();
}
