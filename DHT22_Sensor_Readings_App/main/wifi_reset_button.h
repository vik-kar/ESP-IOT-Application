#ifndef MAIN_WIFI_RESET_BUTTON_H_
#define MAIN_WIFI_RESET_BUTTON_H_

/* Create the default interrupt flag */
#define ESP_INTR_FLAG_DEFAULT		0

/* WiFi reset button is the 'boot' button */
#define WIFI_RESET_BUTTON			0

/* Configures WiFi reset button and interrupt configuration */
void wifi_reset_button_config(void);


#endif /* MAIN_WIFI_RESET_BUTTON_H_ */
