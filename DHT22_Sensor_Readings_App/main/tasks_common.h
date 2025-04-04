/*
 * tasks_common.h
 *
 *  Created on: Feb 27, 2025
 *      Author: vikramkarmarkar
 */

#ifndef MAIN_TASKS_COMMON_H_
#define MAIN_TASKS_COMMON_H_

/* Define details for the wifi application task*/
#define WIFI_APP_TASK_STACK_SIZE			4096
#define WIFI_APP_TASK_PRIORITY				5
#define WIFI_APP_TASK_CORE_ID				0

/* Define details for the HTTP server task */
#define HTTP_SERVER_TASK_STACK_SIZE			8192
#define HTTP_SERVER_TASK_PRIORITY			4
#define HTTP_SERVER_TASK_CORE_ID			0

/* Define HTTP server monitor task info */
#define HTTP_SERVER_MONITOR_STACK_SIZE		4096
#define HTTP_SERVER_MONITOR_PRIORITY		3
#define HTTP_SERVER_MONITOR_CORE_ID			0

/* WiFi reset button task */
#define WIFI_RESET_BUTTON_TASK_STACK_SIZE   2048
#define WIFI_RESET_BUTTON_TASK_PRIORITY		6
#define WIFI_RESET_BUTTON_TASK_CORE_ID		0

/* Define parameters for DHT22 functions */
#define DHT22_TASK_STACK_SIZE				4096
#define DHT22_TASK_PRIORITY					5
#define DHT22_TASK_CORE_ID				    1

/* Define parameters for the SNTP time sync task */
#define SNTP_TIME_SYNC_TASK_STACK_SIZE		4096
#define SNTP_TIME_SYNC_TASK_PRIORITY        4
#define SNTP_TIME_SYNC_TASK_CORE_ID			1

/* Define parameters for AWS IoT Task */
#define AWS_IOT_TASK_STACK_SIZE				9216
#define AWS_IOT_TASK_PRIORITY				6
#define AWS_IOT_TASK_CORE_ID				1

#endif /* MAIN_TASKS_COMMON_H_ */
