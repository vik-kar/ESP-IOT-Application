/*
 * wifi_app.h
 *
 *  Created on: Feb 27, 2025
 *      Author: vikramkarmarkar
 */

#ifndef MAIN_WIFI_APP_H_
#define MAIN_WIFI_APP_H_

#include "esp_netif.h"
#include "esp_wifi_types_generic.h"
#include "portmacro.h"

/* Define the WiFi app settings */
#define WIFI_AP_SSID			"ESP32_AP" 		//access point name - Service Set Identifier
#define WIFI_AP_PASSWORD		"password" 		//wifi access point password
#define WIFI_AP_CHANNEL			1		   
#define WIFI_AP_SSID_HIDDEN		0          		// Defines the wifi access point visibility of the ESP32. 0 makes it visible
#define WIFI_AP_MAX_CONNECTIONS 5		   		// Max number of clients
#define WIFI_AP_BEACON_INTERVAL 100        		// Time lag between each beacon sent by AP (ESP32). Lower the value, smaller the time lag
#define WIFI_AP_IP				"192.168.0.1"	// AP default IP
#define WIFI_AP_GATEWAY			"192.168.0.1"   // AP default gateway (should be the same as the IP)
#define WIFI_AP_NETMASK			"255.255.255.0" // AP Netmask - allows all IPs in the range 192.168.0.1 to 192.168.0.254 (255 reserved)
#define WIFI_AP_BANDWIDTH		WIFI_BW_HT20    // From WiFi driver for 20 MHz bandwidth, 40 MHz is also an option
#define WIFI_STA_POWER_SAVE		WIFI_PS_NONE    // Disable modem sleep (more power consumption, but less latency in receiving data)
#define MAX_SSID_LENGTH			32				// IEEE Standard Maximum
#define MAX_PASSWORD_LENGth		64				// IEEE Standard
#define MAX_CONNECTION_RETRIES	5				// Retry number on disconnect

/* Create the network interface objects for the station & access point - `extern` so visible everywhere */
extern esp_netif_t* esp_netif_sta;
extern esp_netif_t* esp_netif_ap;

/* Message IDs for wifi application task 
   @note you can expand this based on your application requirements	- we will do this in the course
*/
typedef enum wifi_app_message{
	WIFI_APP_MSG_START_HTTP_SERVER = 0, 		//when this is received by a wifi app freeRTOS task, it will handle starting HTTP server
	WIFI_APP_MSG_CONNECTING_FROM_HTTP_SERVER,   // (value 1) lets wifi app know when we are connecting via HTTP server
	WIFI_APP_MSG_STA_CONNECTED_GOT_IP,			// (value 2) used to let wifi app know when the ESP is connected to external access point/router and has been assigned IP addr
	WIFI_APP_MSG_STA_DISCONNECTED,
	WIFI_APP_MSG_USER_REQUESTED_STA_DISCONNECT,		
} wifi_app_message_e;

/* Create a structure for the message queue. msgID will hold the value of the corresponding element (0, 1, 2) of the assigned element in wifi_app_message_e*/
typedef struct wifi_app_queue_message{
	wifi_app_message_e msgID;
} wifi_app_queue_message_t;

/* Sending a message to the queue 
   @param msgID which is the msgID from the wifi_app_message_e (the enum)
   @return pdTRUE if an item was sent to the queue, otherwise pdFALSE
*/
BaseType_t wifi_app_send_message(wifi_app_message_e msgID);

/* Starts the WiFi RTOS task*/
void wifi_app_start(void);

/* Gets the wifi configuration */
wifi_config_t* wifi_app_get_wifi_config(void);

#endif /* MAIN_WIFI_APP_H_ */
