#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "driver/gpio.h"
#include "esp_spiffs.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_event.h"

#define PIR_PIN 23
static const char *TAG = "pir_server";

void pir_gpio_init() {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIR_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
}

void init_spiffs() {
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true
    };
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SPIFFS");
    } else {
        ESP_LOGI(TAG, "SPIFFS mounted");
    }
}

void wifi_init_sta() {
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "DIGI-2840",
            .password = "password",
        },
    };
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
    esp_wifi_connect();
}

// === GET /sensor/pir1 ===
esp_err_t pir_get_handler(httpd_req_t *req) {
    int pir_value = gpio_get_level(PIR_PIN);
    char resp[100];
    int len = snprintf(resp, sizeof(resp),
        "{\"sensor_id\":\"pir1\", \"value\": %d}", pir_value);

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, resp, len);
}

// === POST /sensor/pir1 ===
esp_err_t pir_post_handler(httpd_req_t *req) {
    const char* filename = "/spiffs/pir1_config.json";
    FILE* file = fopen(filename, "r");

    if (file) {
        fclose(file);
        const char* msg = "{\"error\":\"Config file already exists\"}";
        httpd_resp_set_status(req, "409 Conflict");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, msg, HTTPD_RESP_USE_STRLEN);
    }

    file = fopen(filename, "w");
    if (!file) {
        const char* msg = "{\"error\":\"Failed to create config file\"}";
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, msg, HTTPD_RESP_USE_STRLEN);
    }

    const char* content = "{\"scale\":\"default\"}";
    fwrite(content, 1, strlen(content), file);
    fclose(file);

    const char* success = "{\"status\":\"Config created\"}";
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, success, HTTPD_RESP_USE_STRLEN);
}

// === PUT /sensor/pir1 ===
esp_err_t pir_put_handler(httpd_req_t *req) {
    const char* filename = "/spiffs/pir1_config.json";

    FILE* file = fopen(filename, "r");
    if (!file) {
        const char* msg = "{\"error\":\"Config file does not exist\"}";
        httpd_resp_set_status(req, "404 Not Found");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, msg, HTTPD_RESP_USE_STRLEN);
    }
    fclose(file);

    file = fopen(filename, "w");
    if (!file) {
        const char* msg = "{\"error\":\"Failed to overwrite config file\"}";
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, msg, HTTPD_RESP_USE_STRLEN);
    }

    char buffer[200];
    int len = httpd_req_recv(req, buffer, sizeof(buffer) - 1);
    if (len <= 0) {
        fclose(file);
        const char* msg = "{\"error\":\"No content received\"}";
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, msg, HTTPD_RESP_USE_STRLEN);
    }
    buffer[len] = '\0';

    fwrite(buffer, 1, len, file);
    fclose(file);

    const char* msg = "{\"status\":\"Config updated\"}";
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, msg, HTTPD_RESP_USE_STRLEN);
}

// === GET / === HTML page
esp_err_t index_handler(httpd_req_t *req) {
    const char* html =
        "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
        "<title>PIR Sensor</title></head><body>"
        "<h1>Senzor PIR</h1>"
        "<p id='pirValue'>Valoare: ?</p>"
        "<button onclick='getPIR()'>Actualizează</button><br><br>"
        "<button onclick='createConfig()'>Creează Config</button><br><br>"
        "<button onclick='updateConfig()'>Actualizează Config</button><br><br>"
        "<p id='status'></p>"
        "<script>"
        "function getPIR() {"
        "  fetch('/sensor/pir1').then(r => r.json()).then(data => {"
        "    document.getElementById('pirValue').textContent = 'Valoare: ' + data.value;"
        "  });"
        "}"
        "function createConfig() {"
        "  fetch('/sensor/pir1', { method: 'POST' }).then(r => r.json()).then(data => {"
        "    document.getElementById('status').textContent = JSON.stringify(data);"
        "  });"
        "}"
        "function updateConfig() {"
        "  fetch('/sensor/pir1', { "
        "    method: 'PUT',"
        "    headers: { 'Content-Type': 'application/json' },"
        "    body: JSON.stringify({ scale: 'updated' })"
        "  }).then(r => r.json()).then(data => {"
        "    document.getElementById('status').textContent = JSON.stringify(data);"
        "  });"
        "}"
        "setInterval(getPIR, 3000);"
        "</script></body></html>";

    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
}

// === Start Webserver ===
httpd_handle_t start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t pir_get_uri = {
            .uri = "/sensor/pir1",
            .method = HTTP_GET,
            .handler = pir_get_handler
        };
        httpd_uri_t pir_post_uri = {
            .uri = "/sensor/pir1",
            .method = HTTP_POST,
            .handler = pir_post_handler
        };
        httpd_uri_t pir_put_uri = {
            .uri = "/sensor/pir1",
            .method = HTTP_PUT,
            .handler = pir_put_handler
        };
        httpd_uri_t index_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = index_handler
        };

        httpd_register_uri_handler(server, &pir_get_uri);
        httpd_register_uri_handler(server, &pir_post_uri);
        httpd_register_uri_handler(server, &pir_put_uri);
        httpd_register_uri_handler(server, &index_uri);
    }
    return server;
}

void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    init_spiffs();
    wifi_init_sta();
    pir_gpio_init();
    start_webserver();
}
