#include <stdbool.h>

#include "driver/ledc.h"
#include "hal/ledc_types.h"
#include "rgb_led.h"

/* Create an array with the struct type, length = # of channels */
ledc_info_t ledc_ch[RGB_LED_CHANNEL_NUM];

/* Handle for rgb_led_pwm_init */
bool g_pwm_init_handle = false;


/* The following function:
   - Initializes the rgb_led settings per channel, including the GPIO for each color, the mode, and the timer config
   - Will be a static function returning void
*/

static void rgb_led_pwm_init(void){
	/* variable used to iterate through the rgb channels */
	int rgb_ch;
	
	/* Configure the RGB channel structs - ledc_ch is the array we created in the header file 
	   Configurations are stored in the array
	*/
	
	/* RED */
	ledc_ch[0].channel		= LEDC_CHANNEL_0;
	ledc_ch[0].gpio 		= RGB_LED_RED_GPIO;
	ledc_ch[0].mode 		= LEDC_HIGH_SPEED_MODE;
	ledc_ch[0].timer_index  = LEDC_TIMER_0;
	
	/* GREEN */
	ledc_ch[1].channel		= LEDC_CHANNEL_1;
	ledc_ch[1].gpio 		= RGB_LED_GREEN_GPIO;
	ledc_ch[1].mode 		= LEDC_HIGH_SPEED_MODE;
	ledc_ch[1].timer_index  = LEDC_TIMER_0;
	
    /* BLUE */
	ledc_ch[2].channel		= LEDC_CHANNEL_2;
	ledc_ch[2].gpio 		= RGB_LED_BLUE_GPIO;
	ledc_ch[2].mode 		= LEDC_HIGH_SPEED_MODE;
	ledc_ch[2].timer_index  = LEDC_TIMER_0;
	
	/* Configure the timer (timer 0) */
	ledc_timer_config_t ledc_timer = {
		.duty_resolution	= LEDC_TIMER_8_BIT,
		.freq_hz			= 100,
		.speed_mode			= LEDC_HIGH_SPEED_MODE,
		.timer_num			= LEDC_TIMER_0
	};
	
	ledc_timer_config(&ledc_timer);
	
	/* Configure channels */
	for(rgb_ch = 0; rgb_ch < RGB_LED_CHANNEL_NUM; rgb_ch++){
		/* Update the ledc_channel_config struct from the driver */
		ledc_channel_config_t ledc_channel = {
			.channel 	= ledc_ch[rgb_ch].channel,
			.duty 	 	= 0,
			.hpoint	 	= 0,
			.gpio_num	= ledc_ch[rgb_ch].gpio,
			.intr_type	= LEDC_INTR_DISABLE,
			.speed_mode = ledc_ch[rgb_ch].mode,
			.timer_sel	= ledc_ch[rgb_ch].timer_index,
		};
		
		/* Call the channel config function */
		ledc_channel_config(&ledc_channel);
	}	
	g_pwm_init_handle = true;
}

/* Set the RGB Color of the LED */
static void rgb_led_set_color(uint8_t red, uint8_t blue, uint8_t green){
	/* Values should be 0-255 because of the 8-bit duty cycle */
	ledc_set_duty(ledc_ch[0].mode, ledc_ch[0].channel, red);
	ledc_update_duty(ledc_ch[0].mode, ledc_ch[0].channel);
	
	ledc_set_duty(ledc_ch[1].mode, ledc_ch[1].channel, green);
	ledc_update_duty(ledc_ch[1].mode, ledc_ch[1].channel);
	
	ledc_set_duty(ledc_ch[2].mode, ledc_ch[2].channel, blue);
	ledc_update_duty(ledc_ch[2].mode, ledc_ch[2].channel);
}

void rgb_led_wifi_app_started(void){
	if(!g_pwm_init_handle){
		rgb_led_pwm_init();	
	}
	rgb_led_set_color(255,102,255);
}

void rgb_led_http_server_started(void){
	if(!g_pwm_init_handle){
		rgb_led_pwm_init();	
	}
	rgb_led_set_color(204,255, 51);
	
}

void rgb_led_wifi_connected(void){
	if(!g_pwm_init_handle){
		rgb_led_pwm_init();	
	}
	rgb_led_set_color(0,255,153);
}





