/*
 * rgb_led.h
 *
 *  Created on: Feb 26, 2025
 *      Author: vikramkarmarkar
 */

#ifndef MAIN_RGB_LED_H_
#define MAIN_RGB_LED_H_

/* Define the RGB LED GPIOs*/
#define RGB_LED_RED_GPIO		21
#define RGB_LED_GREEN_GPIO		22
#define RGB_LED_BLUE_GPIO		23

/* RGB LED color mix channels */
							  //3 for red, green and blue
#define RGB_LED_CHANNEL_NUM		3 

/* Define the configuration structure */
typedef struct{
	int channel;
	int gpio;
	int mode;
	int timer_index;
} ledc_info_t;

/* Color for indicating WiFi application has started */
void rgb_led_wifi_app_started(void);

/* HTTP Server Started */
void rgb_led_http_server_started(void);

/* ESP32 is connected to an access point */
void rgb_led_wifi_connected(void);

#endif /* MAIN_RGB_LED_H_ */
