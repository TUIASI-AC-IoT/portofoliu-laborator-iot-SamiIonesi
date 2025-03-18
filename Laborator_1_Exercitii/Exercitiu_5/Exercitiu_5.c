#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"

#define GPIO_LED1 4
#define GPIO_LED2 5
#define GPIO_BUTTON 2
#define DEBOUNCE_TIME 50
#define LONG_PRESS_TIME 1000 

volatile int led1_state = 0;  // Stare LED1 (On/Off)
volatile int led2_state = 0;  // Stare LED2 (On/Off)

void button_task(void *pvParameters) {
    int last_state = 1;  // Butonul are pull-up, deci 1 = deconectat
    int pressed_time = 0;

    while (1) {
        int current_state = gpio_get_level(GPIO_BUTTON);

        if (current_state == 0 && last_state == 1) {  // Buton apasat (tranziție HIGH -> LOW)
            pressed_time = esp_timer_get_time() / 1000;
            vTaskDelay(DEBOUNCE_TIME / portTICK_PERIOD_MS);
        }

        if (current_state == 1 && last_state == 0) {  // Buton eliberat (tranziție LOW -> HIGH)
            int release_time = esp_timer_get_time() / 1000;
            int duration = release_time - pressed_time;

            if (duration < LONG_PRESS_TIME) {
                led1_state = !led1_state;  // Apasare scurta → schimbăm LED1
                printf("Apasare scurta: LED1 = %d\n", led1_state);
            } else {
                led2_state = !led2_state;  // Apasare lunga → schimbăm LED2
                printf("Apasare lunga: LED2 = %d\n", led2_state);
            }
        }

        last_state = current_state;
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

// Task pentru controlul LED-urilor
void led_task(void *pvParameters) {
    while (1) {
        gpio_set_level(GPIO_LED1, led1_state);
        gpio_set_level(GPIO_LED2, led2_state);
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void app_main() {
    // Configurare LED-uri
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << GPIO_LED1) | (1ULL << GPIO_LED2);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    // Configurare buton (input cu pull-up)
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << GPIO_BUTTON);
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    // Creare task-uri FreeRTOS
    xTaskCreate(button_task, "Button Task", 2048, NULL, 10, NULL);
    xTaskCreate(led_task, "LED Task", 2048, NULL, 10, NULL);
}
