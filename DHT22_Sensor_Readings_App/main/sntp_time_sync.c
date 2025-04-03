#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include  "freertos/task.h"
#include "lwip/apps/sntp.h"

/* Other file includes */
#include "tasks_common.h" 
#include "http_server.h"
#include "sntp_time_sync.h"
#include "wifi_app.h"

/* Define the TAG */
static const char TAG[] = "sntp_time_sync";

/* SNTP operating mode set status */
static bool sntp_op_mode_set = 0;

/* Initialize SNTP service using SNTP_OPMODE_POLL mode */
static void sntp_time_sync_init_sntp(void){
	ESP_LOGI(TAG, "Initializing the SNTP service");
	
	if(!sntp_op_mode_set){
		sntp_setoperatingmode(SNTP_OPMODE_POLL);
		sntp_op_mode_set = 1;
	}
	
	sntp_setservername(0, "pool.ntp.org");
	
	/* Initialize the servers */
	sntp_init();
	
	/* Let the HTTP server know that the SNTP service is initialized */
	http_server_monitor_send_message(HTTP_MSG_TIME_SERVICE_INITIALIZED);
}


/* Gets the current time. If current time is not up to date, the sntp_time_sync_init_sntp function is called */
static void sntp_time_sync_obtain_time(void){
	time_t now = 0;
	
	/* Set timezone setting in case it is incorrect at the start */
	setenv("TZ", "PST8PDT,M3.2.0,M11.1.0", 1);
	tzset(); 
	
	/* Create time info struct */
	struct tm time_info = {0};
	
	time(&now);
	
	localtime_r(&now, &time_info);
	
	/* Check the time in case we need to initialize/reinitialize */
	if(time_info.tm_year < (2025 - 1900)){
		sntp_time_sync_init_sntp();
		
		/* Set the local timezone again */
		setenv("TZ", "PST8PDT,M3.2.0,M11.1.0", 1);
		
		/* Initialize the timezone conversion routine */
		tzset(); 
	}
}

/* The SNTP time synchronization task 
   @param arg pvParam (pointer)
*/
static void sntp_time_sync(void *pvParam){
	while(1){
		/* call the obtain time function */
		sntp_time_sync_obtain_time();
		
		/* Delay and check for updated time again */
		vTaskDelay(pdMS_TO_TICKS(10000));
	}
	
	/* Delete the task if it ever breaks out */
	vTaskDelete(NULL);
}

/* Returns local time if set 
   @return local time buffer
*/
char* sntp_time_sync_get_time(void){
	static char time_buffer[100] = {0};
	
	time_t now = 0;
	struct tm time_info = {0};
	
	time(&now);
	localtime_r(&now, &time_info);
	
	if(time_info.tm_year < (2025 - 1900)){
		/* Inaccurate time */
		ESP_LOGI(TAG, "Time is not set yet");
	}
	else {
		/* lets us format the time string */
		strftime(time_buffer, sizeof(time_buffer), "%d.%m.%Y %H:%M:%S", &time_info);
		ESP_LOGI(TAG, "Current time info: %s", time_buffer);
	}
	
	return time_buffer;
}

/* Starts the SNTP server synchronization task */
void sntp_time_sync_task_start(void){
	xTaskCreatePinnedToCore(&sntp_time_sync, "sntp_time_sync", SNTP_TIME_SYNC_TASK_STACK_SIZE, NULL, SNTP_TIME_SYNC_TASK_PRIORITY, NULL, SNTP_TIME_SYNC_TASK_CORE_ID);
}
























