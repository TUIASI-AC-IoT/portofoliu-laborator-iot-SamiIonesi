#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_system.h"

#include "lwip/err.h"
#include "lwip/sys.h"
#include "freertos/event_groups.h"

#include "esp_http_server.h"

/* Our URI handler function to be called during GET /uri request */
static const char *TAG = "http_server";
extern const char* get_available_ssids();

esp_err_t get_handler(httpd_req_t *req) {
    char response[1024];
    snprintf(response, sizeof(response),
        "<html><body><form action='/results.html' method='post'>"
        "<label for='ssid'>Networks found:</label><br>"
        "<select name='ssid'>%s</select><br>"
        "<label for='ipass'>Security key:</label><br>"
        "<input type='password' name='ipass'><br>"
        "<input type='submit' value='Submit'></form></body></html>",
        get_available_ssids());
    httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/* Handler pentru a trimite lista SSID-urilor */
esp_err_t get_ssids_handler(httpd_req_t *req) {
    httpd_resp_send(req, get_available_ssids(), HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/* Definire ruta pentru SSID-uri */
httpd_uri_t uri_get_ssids = {
    .uri      = "/get_ssids",
    .method   = HTTP_GET,
    .handler  = get_ssids_handler,
    .user_ctx = NULL
};

/* Our URI handler function to be called during POST /uri request */
esp_err_t post_handler(httpd_req_t *req) {
    char content[100];
    size_t recv_size = MIN(req->content_len, sizeof(content));
    int ret = httpd_req_recv(req, content, recv_size);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }

    // Parsează SSID și parola din formular (asumăm formatul "ssid=xxx&ipass=yyy")
    char ssid[32] = {0}, pass[64] = {0};
    sscanf(content, "ssid=%31[^&]&ipass=%63s", ssid, pass);
    ESP_LOGI(TAG, "Received SSID: %s, PASS: %s", ssid, pass);

    // Salvează în NVS
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("wifi_creds", NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        nvs_set_str(nvs_handle, "ssid", ssid);
        nvs_set_str(nvs_handle, "pass", pass);
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
        ESP_LOGI(TAG, "Saved credentials. Restarting...");
        httpd_resp_send(req, "<html><body><h2>Configuration saved. Restarting...</h2></body></html>", HTTPD_RESP_USE_STRLEN);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        esp_restart();
    } else {
        ESP_LOGE(TAG, "Failed to open NVS");
        httpd_resp_send(req, "Failed to save config.", HTTPD_RESP_USE_STRLEN);
    }

    return ESP_OK;
}


/* URI handler structure for GET /uri */
httpd_uri_t uri_get = {
    .uri      = "/index.html",
    .method   = HTTP_GET,
    .handler  = get_handler,
    .user_ctx = NULL
};

/* URI handler structure for POST /uri */
httpd_uri_t uri_post = {
    .uri      = "/results.html",
    .method   = HTTP_POST,
    .handler  = post_handler,
    .user_ctx = NULL
};

/* Function for starting the webserver */
httpd_handle_t start_webserver(void)
{
    /* Generate default configuration */
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    /* Empty handle to esp_http_server */
    httpd_handle_t server = NULL;

    /* Start the httpd server */
    if (httpd_start(&server, &config) == ESP_OK) {
        /* Register URI handlers */
        httpd_register_uri_handler(server, &uri_get);
        httpd_register_uri_handler(server, &uri_post);
        httpd_register_uri_handler(server, &uri_get_ssids);
    }
    /* If server failed to start, handle will be NULL */
    return server;
}

/* Function for stopping the webserver */
void stop_webserver(httpd_handle_t server)
{
    if (server) {
        /* Stop the httpd server */
        httpd_stop(server);
    }
}