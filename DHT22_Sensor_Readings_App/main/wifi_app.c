/* Includes */

/* FreeRTOS & event groups */
#include "esp_event.h"
#include "esp_event_base.h"
#include "esp_interface.h"
#include "esp_log_level.h"
#include "esp_netif_types.h"
#include "esp_wifi_default.h"
#include "esp_wifi_types_generic.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/idf_additions.h"
#include "freertos/task.h"

/* ESP includes */
#include "esp_err.h"
#include "esp_log.h"
#include "esp_wifi.h" //WiFi Driver
#include "lwip/netdb.h"

/* Previous files */
#include "portmacro.h"
#include "rgb_led.h"
#include "tasks_common.h"
#include "wifi_app.h"
#include "http_server.h"
#include "app_nvs.h"

/* Create a tag used for ESP Serial Console messages */
static const char TAG[] = "wifi_app";

/* WiFi application callback */
static wifi_connected_event_callback_t wifi_connected_event_cb;

/* Used for returning the wifi configuration */
wifi_config_t *wifi_config = NULL;

/* Used to track the # of retries when a connection attempt fails */
static int g_retry_number;

/* Define an Event group and status bits for our message cases */
static EventGroupHandle_t wifi_app_event_group;
/* Bits will tell us if we are connecting to WiFi from saved SSID & pwd or SSID & pwd from webpage */
const int WIFI_APP_CONNECTING_USING_SAVED_CREDS_BIT 		= BIT0; //(1U << 0)
const int WIFI_APP_CONNECTING_FROM_HTTP_SERVER_BIT			= BIT1; //(1U << 1)
const int WIFI_APP_MSG_USER_REQUESTED_STA_DISCONNECT_BIT	= BIT2; //(1U << 2)

/* we need another status bit to indicate that the ESP32 is indeed connected to an AP before reacting to disconnect button press */
const int WIFI_APP_STA_CONNECTED_GOT_IP_BIT					= BIT3;


/* Create a queue handle */
static QueueHandle_t wifi_app_queue_handle;

/* Create the network interface objects for the station and access point */
esp_netif_t* esp_netif_sta = NULL;
esp_netif_t* esp_netif_ap  = NULL;

/* Wifi App event handler 
   @param arg data, aside from event data, that is passed to the handler when it is called
   @param event_base, the base ID of the event to register the handler for
   @param event_ID, the ID of the event to register the handler for
   @param event_data event data
*/
static void wifi_app_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data){
	if(event_base == WIFI_EVENT){
		switch (event_id){
			case WIFI_EVENT_AP_START:
			/* Logged when the ESP32 starts acting as an access point */
				ESP_LOGI(TAG, "WIFI_EVENT_AP_START");
				break;
				
			case WIFI_EVENT_AP_STOP:
			/* Logged when the ESP32 stops its access point mode */
				ESP_LOGI(TAG, "WIFI_EVENT_AP_STOP");
				break;
				
			case WIFI_EVENT_AP_STACONNECTED:
			/* Logged when a station connects to the ESP32 when it is in AP mode */
				ESP_LOGI(TAG, "WIFI_EVENT_AP_STACONNECTED");
				break;
				
			case WIFI_EVENT_AP_STADISCONNECTED:
			/* Logged when a station disconnects from the ESP32 when it is in AP mode */
				ESP_LOGI(TAG, "WIFI_EVENT_AP_STADISCONNECTED");
				break;
				
			case WIFI_EVENT_STA_START:
			/* Indicates that the ESP32 has started in station mode, attempting to connect to another AP */
				ESP_LOGI(TAG, "WIFI_EVENT_STA_START");
				break;
				
			case WIFI_EVENT_STA_CONNECTED:
			/* Logged when the ESP32, in station mode, successfully connects to an external WiFi network */
				ESP_LOGI(TAG, "WIFI_EVENT_STA_CONNECTED");
				break;
				
			case WIFI_EVENT_STA_DISCONNECTED:
			/* Logged when the ESP32 disconnects from the WiFi network that it was connected to */
				ESP_LOGI(TAG, "WIFI_EVENT_STA_DISCONNECTED");
				
				/* Print some disconnected error codes */
				wifi_event_sta_disconnected_t *wifi_event_sta_disconnected = (wifi_event_sta_disconnected_t*) malloc(sizeof(wifi_event_sta_disconnected_t));
				*wifi_event_sta_disconnected = *((wifi_event_sta_disconnected_t*)event_data);
				printf("WIFI_EVENT_STA_DISCONNECTED, reason code %d\n", wifi_event_sta_disconnected->reason);
				
				/* Implement some WiFi connection retry logic - since we are in the STA_DISCONNECTED case, we need to connect again */
				if(g_retry_number < MAX_CONNECTION_RETRIES){
					esp_wifi_connect();
					g_retry_number++;
				}
				else{
					/* Send a disconnect message */
					wifi_app_send_message(WIFI_APP_MSG_STA_DISCONNECTED);
				}
				
				break;
		}
	}
	else if(event_base == IP_EVENT){
		switch(event_id){
			case IP_EVENT_STA_GOT_IP:
			/* Logged when the ESP32 operating as a station, obtains an IP address from the connected WiFi network */
				ESP_LOGI(TAG, "IP_EVENT_STA_GOT_IP");
				
				/* Send got ip message */
				wifi_app_send_message(WIFI_APP_MSG_STA_CONNECTED_GOT_IP);
				break;
		}
	}
}

/* Initializes the WiFi application event handler for WiFi and IP events */
static void wifi_app_event_handler_init(void){
	/* Event Loop for the WiFi driver */
	ESP_ERROR_CHECK(esp_event_loop_create_default());
	
	/* Create the event handler for the connection */
	esp_event_handler_instance_t instance_wifi_event;
	esp_event_handler_instance_t instance_ip_event;
	
	/* The actual registration of event handlers for WiFi and IP events - this is where we configure the event loop to alert the application when the following errors occur 	*/
	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_app_event_handler, NULL, &instance_wifi_event));
	ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, &wifi_app_event_handler, NULL, &instance_ip_event));
	
}

/* Initializes the TCP stack and default WiFi configuration */
static void wifi_app_default_wifi_init(void){
	
	/* Initialize the TCP stack */
	ESP_ERROR_CHECK(esp_netif_init());	
	
	/* Default WiFi config - operations must be in this order 
	   - declare an instance of the wifi init config type
	*/
	wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));
	ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
	
	/* Initialize the esp_netif_sta object */
	esp_netif_sta = esp_netif_create_default_wifi_sta();
	esp_netif_ap = esp_netif_create_default_wifi_ap();
}


/* Configures the WiFi access point settings and assigns the static IP to the SoftAP */
static void wifi_app_soft_ap_config(void){
	/* SoftAP WiFi access point configuration */

	/* Create the WiFi config structure */
	wifi_config_t ap_config = {
		.ap = {
			.ssid = WIFI_AP_SSID,
			.ssid_len = strlen(WIFI_AP_SSID),
			.password = WIFI_AP_PASSWORD,
			.channel = WIFI_AP_CHANNEL,
			.ssid_hidden = WIFI_AP_SSID_HIDDEN,
			.authmode = WIFI_AUTH_WPA2_PSK,
			.max_connection = WIFI_AP_MAX_CONNECTIONS,
			.beacon_interval = WIFI_AP_BEACON_INTERVAL,
		},
	};
	
	/* Configure DHCP for the AP */
	esp_netif_ip_info_t ap_ip_info;
	
	/* Set ap_ip_info to 0 so that we start from a clean slate in case there is some data at this memory location */
	memset(&ap_ip_info, 0x00, sizeof(ap_ip_info));
	
	esp_netif_dhcps_stop(esp_netif_ap);
	
	/* Assign access point's static IP, Gateway, and netmask 
	   This function converts an internet address in its standard text format into its numeric binary form (recognized by ESP32 software)
	*/
	inet_pton(AF_INET, WIFI_AP_IP, &ap_ip_info.ip);
	inet_pton(AF_INET, WIFI_AP_GATEWAY, &ap_ip_info.gw);
	inet_pton(AF_INET, WIFI_AP_NETMASK, &ap_ip_info.netmask);
	
	/* Statically configures the network interface */
	ESP_ERROR_CHECK(esp_netif_set_ip_info(esp_netif_ap, &ap_ip_info));
	
	/* Starts the AP DHCP server (for connecting stations, eg. mobile device) */
	ESP_ERROR_CHECK(esp_netif_dhcps_start(esp_netif_ap));
	
	/* Setting the mode as access point/station mode */
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
	
	/* Set our configuration */
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &ap_config));
	
	/* Our default bandwidth - 20MHz*/
	ESP_ERROR_CHECK(esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_AP_BANDWIDTH));
	
	/* Power save set to "NONE"*/
	ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_STA_POWER_SAVE));
}

/* Connects the ESP 32 to an external access point using the updated station configuration */
static void wifi_app_connect_sta(void){
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, wifi_app_get_wifi_config()));
	ESP_ERROR_CHECK(esp_wifi_connect());
}


/* Define the main task for the wifi application 
   - Initialize wifi, receive all queue messages which determines the flow of the application
   - We'll need to define some set of functions which initialize the event handler and the TCP IP stack & the wifi config as well as softap config
*/
static void wifi_app_task(void *pvParameters){
	/* create an instance of the wifi app queue message struct */
	wifi_app_queue_message_t msg;
	
	/* create event bits type */
	EventBits_t eventBits;
	
	/* Initialize the event handler (NOT DEFINED YET - IGNORE ANY ERRORS) */
	wifi_app_event_handler_init();
	
	/* Initialize the TCP/IP Stack and WiFi config (NOT DEFINED YET - IGNORE ANY ERRORS) */
	wifi_app_default_wifi_init();
	
	/* Initialize the softAP config (software access point) (NOT DEFINED YET - IGNORE ANY ERRORS) */
	wifi_app_soft_ap_config();
	
	/* Start the WiFi module */
	ESP_ERROR_CHECK(esp_wifi_start());
	
	/* Send the first event message */
	wifi_app_send_message(WIFI_APP_MSG_LOAD_SAVED_CREDENTIALS);
	
	/* Begin the endless loop */
	while(1){
		if(xQueueReceive(wifi_app_queue_handle, &msg, portMAX_DELAY)){
			switch(msg.msgID){
				
			case WIFI_APP_MSG_LOAD_SAVED_CREDENTIALS:
				ESP_LOGI(TAG, "WIFI_APP_MSG_LOAD_SAVED_CREDENTIALS");
				if(app_nvs_load_sta_creds()){
					ESP_LOGI(TAG, "Loaded Station Configuration");
					wifi_app_connect_sta();
					xEventGroupSetBits(wifi_app_event_group, WIFI_APP_CONNECTING_USING_SAVED_CREDS_BIT);
				}
				else{
					ESP_LOGI(TAG, "Unable to load station configuration");
				}
				
				/* Next, start the web server */
				wifi_app_send_message(WIFI_APP_MSG_START_HTTP_SERVER);
				
			break;
				
			case WIFI_APP_MSG_START_HTTP_SERVER:
				ESP_LOGI(TAG, "WIFI_APP_MSG_START_HTTP_SERVER");
				/* Define this later */
				http_server_start();
				rgb_led_http_server_started();
			break;
				
			case WIFI_APP_MSG_CONNECTING_FROM_HTTP_SERVER:
				ESP_LOGI(TAG, "WIFI_APP_MSG_CONNECTING_FROM_HTTP_SERVER");
				
				/* Set this bit in the event group if we are connecting from HTTP server, which we are in this case of the switch statement */
				xEventGroupSetBits(wifi_app_event_group, WIFI_APP_CONNECTING_FROM_HTTP_SERVER_BIT);
				
				/* Attempt a connection */
				wifi_app_connect_sta();
				
				/* Set current number of retries to 0 */
				g_retry_number = 0;
				
				/* Let the HTTP server know about the connection attempt */
				http_server_monitor_send_message(HTTP_MSG_WIFI_CONNECT_INIT);
			break;
				
			case WIFI_APP_MSG_STA_CONNECTED_GOT_IP:
				ESP_LOGI(TAG, "WIFI_APP_MSG_STA_CONNECTED_GOT_IP");
				rgb_led_wifi_connected();
				
				xEventGroupSetBits(wifi_app_event_group, WIFI_APP_STA_CONNECTED_GOT_IP_BIT);
				
				/* Send message to HTTP server monitor */
				http_server_monitor_send_message(HTTP_MSG_WIFI_CONNECT_SUCCESS);
				
				/* Need to save credentials if connecting from HTTP server */
				eventBits = xEventGroupGetBits(wifi_app_event_group);
				
				/* saved creds bit is bit 0. This & operation only compares bit 0 from `eventBits` to saved_creds_bit 
				   Save STA creds only if connecting from HTTP server (and NOT loaded from NVS)
				*/
				if(eventBits & WIFI_APP_CONNECTING_USING_SAVED_CREDS_BIT){
					/* clear bits in case we want to disconnect & reconnect and start again */
					xEventGroupClearBits(wifi_app_event_group, WIFI_APP_CONNECTING_USING_SAVED_CREDS_BIT);
				}
				else{
					/* this means that the connection was NOT made using saved credentials, so these are NEW credentials. In which case, they would need to be saved */
					app_nvs_save_sta_creds();
				}
				if(eventBits & WIFI_APP_CONNECTING_FROM_HTTP_SERVER_BIT){
					xEventGroupClearBits(wifi_app_event_group, WIFI_APP_CONNECTING_FROM_HTTP_SERVER_BIT);
				}
				
				/* Check for connection callback. if callback is set, call callback */
				if(wifi_connected_event_cb){
					wifi_app_call_callback();
				}
			break;
				
			case WIFI_APP_MSG_USER_REQUESTED_STA_DISCONNECT:
				ESP_LOGI(TAG, "WIFI_APP_MSG_USER_REQUESTED_STA_DISCONNECT");
				
				eventBits = xEventGroupGetBits(wifi_app_event_group);
				
				if(eventBits & WIFI_APP_STA_CONNECTED_GOT_IP_BIT){
					/* Set the event group bit */
					xEventGroupSetBits(wifi_app_event_group, WIFI_APP_MSG_USER_REQUESTED_STA_DISCONNECT_BIT);
					
					/* We don't want to re-attempt a connection when the disconnect button is pressed */
					g_retry_number = MAX_CONNECTION_RETRIES;
					
					ESP_ERROR_CHECK(esp_wifi_disconnect());
					
					/* clear NVS credentials */
					app_nvs_clear_sta_creds();
					
					/* Change RGB LED Status - in this case, this function means that WiFi is disconnected */
					rgb_led_http_server_started();
				}
				
			break;
			
			case WIFI_APP_MSG_STA_DISCONNECTED:
				ESP_LOGI(TAG, "WIFI_APP_MSG_STA_DISCONNECTED");
				
				eventBits = xEventGroupGetBits(wifi_app_event_group);
				
				if(eventBits & WIFI_APP_CONNECTING_USING_SAVED_CREDS_BIT){
					/* we connected using saved creds */
					ESP_LOGI(TAG, "WIFI_APP_MSG_STA_DISCONNECTED: disconnect attempted using saved credentials");
					xEventGroupClearBits(wifi_app_event_group, WIFI_APP_CONNECTING_USING_SAVED_CREDS_BIT);
					
					/* clear the creds */
					app_nvs_clear_sta_creds();
				}
				else if(eventBits & WIFI_APP_CONNECTING_FROM_HTTP_SERVER_BIT){
					/* we connected using new creds that were just entered */
					ESP_LOGI(TAG, "WIFI_APP_MSG_STA_DISCONNECTED: disconnect attempted using new creds from HTTP server");
					xEventGroupClearBits(wifi_app_event_group, WIFI_APP_CONNECTING_FROM_HTTP_SERVER_BIT);
					
					http_server_monitor_send_message(HTTP_MSG_WIFI_CONNECT_FAIL);
					
				}
				else if(eventBits & WIFI_APP_MSG_USER_REQUESTED_STA_DISCONNECT_BIT){
					ESP_LOGI(TAG, "WIFI_APP_MSG_STA_DISCONNECTED: User requested disconnection");
					xEventGroupClearBits(wifi_app_event_group, WIFI_APP_MSG_USER_REQUESTED_STA_DISCONNECT_BIT);
					
					/* Let the web server know */
					http_server_monitor_send_message(HTTP_MSG_WIFI_USER_DISCONNECT);
				}
				else{
					/* One more case: access point is unavailable */
					ESP_LOGI(TAG, "WIFI_APP_MSG_STA_DISCONNECTED: Attempt failed, check WiFi access point availability");
					
				}
				if(eventBits & WIFI_APP_STA_CONNECTED_GOT_IP_BIT){
					/* Clear the got_ip_bit from event group */
					xEventGroupClearBits(wifi_app_event_group, WIFI_APP_STA_CONNECTED_GOT_IP_BIT);
				}
				
			break;
				
			default:
				break;
			}
		}
	}
}


/* Bring over the prototypes from the wifi_app.h file and define them here */

BaseType_t wifi_app_send_message(wifi_app_message_e msgID){
	/* Create an instance of wifi app queue messsage struct */
	wifi_app_queue_message_t msg;
	
	/* Set the message id to message id passed into this function */
	msg.msgID = msgID;
	
	/* Return the result of sending to the queue */
	return xQueueSend(wifi_app_queue_handle, &msg, portMAX_DELAY);
}

wifi_config_t* wifi_app_get_wifi_config(void){
	/* This is the global variable up top */
	return wifi_config;
}

/* Sets the callback function */
void wifi_app_set_callback(wifi_connected_event_callback_t cb){
	wifi_connected_event_cb = cb;
}

/* calls the callback function */
void wifi_app_call_callback(void){
	wifi_connected_event_cb();
}

int8_t wifi_app_get_rssi(void){
	wifi_ap_record_t wifi_data;

	ESP_ERROR_CHECK(esp_wifi_sta_get_ap_info(&wifi_data));

	return wifi_data.rssi;
}

/* defines/starts the WiFi RTOS task and handle some initializations as well*/
void wifi_app_start(void){
	ESP_LOGI(TAG, "STARTING WIFI APPLICATION");
	
	/* Start the wifi_started LED */
	rgb_led_wifi_app_started();
	
	/* disable default wifi logging messages - will log way too many messages */
	esp_log_level_set("wifi", ESP_LOG_NONE);
	
	/* Allocate memory for the WiFi configuration */
	wifi_config = (wifi_config_t*)malloc(sizeof(wifi_config_t));
	memset(wifi_config, 0x00, sizeof(wifi_config));
	
	/* Create a message queue using the queue handle */
	wifi_app_queue_handle = xQueueCreate(3, sizeof(wifi_app_queue_message_t));
	
	/* Create event group. Handle is defined at the top */
	wifi_app_event_group = xEventGroupCreate();
	
	/* Start the WiFi application task */
	/* Pass a reference to the task function that we will create */
	xTaskCreatePinnedToCore(&wifi_app_task, "wifi_app_task", WIFI_APP_TASK_STACK_SIZE, NULL, WIFI_APP_TASK_PRIORITY, NULL, WIFI_APP_TASK_CORE_ID);	
	
}
