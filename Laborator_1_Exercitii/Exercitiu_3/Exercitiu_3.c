#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"

#define GPIO_OUTPUT_IO 4
#define GPIO_OUTPUT_PIN_SEL (1ULL << GPIO_OUTPUT_IO)
#define GPIO_INPUT_IO 2
#define GPIO_INPUT_PIN_SEL (1ULL << GPIO_INPUT_IO)

static volatile int button_press_count = 0;

// Mutex pentru protejarea accesului la variabila globala
SemaphoreHandle_t xSemaphore = NULL;


void button_task(void *pvParameters) {
    while (1) {
        // Citim starea butonului
        int button_state = gpio_get_level(GPIO_INPUT_IO);

        // ! Butonul este apasat pe 0, adica este activ pe 0
        if (button_state == 0) {
            // Asteptam pentru a evita debouncing-ul
            vTaskDelay(500 / portTICK_PERIOD_MS);

            // Protejam accesul la variabila globala folosind un mutex
            if (xSemaphoreTake(xSemaphore, portMAX_DELAY)) {
                button_press_count++;
                printf("Apasari: %d\n", button_press_count);
                xSemaphoreGive(xSemaphore);
            }
        }

        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void app_main() {
    // Configurarea GPIO pentru LED
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    // Configurarea GPIO pentru buton (GPIO2)
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    io_conf.pull_up_en = 1;  // prevenire la starile flotante
    gpio_config(&io_conf);

    xSemaphore = xSemaphoreCreateMutex();

    if (xSemaphore != NULL) {
        xTaskCreate(button_task, "Button Task", 2048, NULL, 10, NULL);
    }

    while (1) {
        gpio_set_level(GPIO_OUTPUT_IO, 1);
        vTaskDelay(1000 / portTICK_PERIOD_MS);

        gpio_set_level(GPIO_OUTPUT_IO, 0);
        vTaskDelay(500 / portTICK_PERIOD_MS);

        gpio_set_level(GPIO_OUTPUT_IO, 1);
        vTaskDelay(250 / portTICK_PERIOD_MS);

        gpio_set_level(GPIO_OUTPUT_IO, 0);
        vTaskDelay(750 / portTICK_PERIOD_MS);
    }
}
