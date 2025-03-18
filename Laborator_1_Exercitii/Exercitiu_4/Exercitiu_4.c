#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"

#define GPIO_LED1 4
#define GPIO_LED2 5
#define GPIO_OUTPUT_PIN_SEL ((1ULL << GPIO_LED1) | (1ULL << GPIO_LED2))
#define GPIO_BUTTON 2
#define GPIO_INPUT_PIN_SEL (1ULL << GPIO_BUTTON)

// Variabile globale
static volatile int button_press_count = 0;
static volatile int led_state = 0;  // 0 = LED-uri OFF, 1 = LED-uri
SemaphoreHandle_t xSemaphore = NULL;

void button_task(void *pvParameters) {
    while (1) {
        if (gpio_get_level(GPIO_BUTTON) == 0) {

            vTaskDelay(10 / portTICK_PERIOD_MS);  // Debounce delay

            if (gpio_get_level(GPIO_BUTTON) == 0) {  // Verificare dupa debounce
                if (xSemaphoreTake(xSemaphore, portMAX_DELAY)) {
                    button_press_count++;
                    led_state = !led_state;
                    printf("Apasari: %d, Stare LED: %d\n", button_press_count, led_state);
                    xSemaphoreGive(xSemaphore);
                }

                while (gpio_get_level(GPIO_BUTTON) == 0) {
                    vTaskDelay(10 / portTICK_PERIOD_MS);
                }
            }
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void led_flow()
{
    gpio_set_level(GPIO_LED1, 1);
    gpio_set_level(GPIO_LED2, 1);
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    gpio_set_level(GPIO_LED1, 0);
    gpio_set_level(GPIO_LED2, 0);
    vTaskDelay(500 / portTICK_PERIOD_MS);

    gpio_set_level(GPIO_LED1, 1);
    gpio_set_level(GPIO_LED2, 1);
    vTaskDelay(250 / portTICK_PERIOD_MS);

    gpio_set_level(GPIO_LED1, 0);
    gpio_set_level(GPIO_LED2, 0);
    vTaskDelay(750 / portTICK_PERIOD_MS);
}

void led_task(void *pvParameters) {
    while (1) {
        if (led_state == 1) {
            led_flow();
        } else {
            // LED-urile sunt oprite
            gpio_set_level(GPIO_LED1, 1);
            gpio_set_level(GPIO_LED2, 1);
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
    }
}

void app_main() {
    // Configurare LED-uri
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    // Configurare buton
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    xSemaphore = xSemaphoreCreateMutex();

    if (xSemaphore != NULL) {
        xTaskCreate(button_task, "Button Task", 2048, NULL, 10, NULL);
        xTaskCreate(led_task, "LED Task", 2048, NULL, 10, NULL);
    }
}
