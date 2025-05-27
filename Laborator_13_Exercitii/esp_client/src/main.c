#include <stdio.h>
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define WIFI_SSID "DIGI-2840"
#define WIFI_PASS "n97748KK"
#define TAG "ESP32_AUTH"

char jwt_token[1024] = {0};

void wifi_init(void) {
    ESP_LOGI(TAG, "Init WiFi STA...");
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
    esp_wifi_connect();
}

esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            if (evt->data_len < sizeof(jwt_token)) {
                memcpy(jwt_token, evt->data, evt->data_len);
                jwt_token[evt->data_len] = '\0';
            }
            break;
        default:
            break;
    }
    return ESP_OK;
}

void login_to_server() {
    const char *post_data = "{\"username\": \"test\", \"password\": \"test\"}";

    esp_http_client_config_t config = {
        .url = "http://192.168.100.25:5000/auth",
        .method = HTTP_METHOD_POST,
        .event_handler = _http_event_handler,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Login OK, token: %s", jwt_token);
    } else {
        ESP_LOGE(TAG, "Login failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}

void send_pir_data() {
    esp_http_client_config_t config = {
        .url = "http://192.168.100.25:5000/sensor/pir1",
        .method = HTTP_METHOD_POST,
        .buffer_size_tx = 2048
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    char *data = "{\"pir\": 1}";

    esp_http_client_set_header(client, "Content-Type", "application/json");

    char auth_header[1100];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", jwt_token);
    esp_http_client_set_header(client, "Authorization", auth_header);

    esp_http_client_set_post_field(client, data, strlen(data));
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "PIR data sent");
    } else {
        ESP_LOGE(TAG, "Send failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}

void app_main() {
    nvs_flash_init();
    wifi_init();

    vTaskDelay(5000 / portTICK_PERIOD_MS);

    login_to_server();
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    send_pir_data();
}
