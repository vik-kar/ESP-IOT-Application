#ifndef MAIN_HTTP_SERVER_H_
#define MAIN_HTTP_SERVER_H_

#include "portmacro.h"

/* Define statuses for OTA */
#define OTA_UPDATE_PENDING		0
#define OTA_UPDATE_SUCCESSFUL	1
#define OTA_UPDATE_FAILED		-1
 
/* Messages for the HTTP monitor */
typedef enum http_server_message{
	HTTP_MSG_WIFI_CONNECT_INIT = 0,
	HTTP_MSG_WIFI_CONNECT_SUCCESS,
	HTTP_MSG_WIFI_CONNECT_FAIL,
	HTTP_MSG_OTA_UPDATE_SUCCESSFUL,
	HTTP_MSG_OTA_UPDATE_FAILED,
	HTTP_MSG_OTA_UPDATE_INITIALIZED,
} http_server_message_e;

/* Structure for the message queue */
typedef struct http_server_queue_message{
	http_server_message_e msgID;
} http_server_queue_message_t;

/* Function that sends a message to the queue 
   @param msgID from the http_server_message_enum
   @return pdTRUE if item was successfully sent to queue otherwise pdFALSE
*/
BaseType_t http_server_monitor_send_message(http_server_message_e msgID);

/* Function that starts the HTTP server */
void http_server_start(void);

/* Function that stops the HTTP server */
void http_server_stop(void);

/* Define a timer callback function that calls esp_restart upon successful firmware update */
void http_server_fw_update_reset_callback(void *arg);

#endif /* MAIN_HTTP_SERVER_H_ */
