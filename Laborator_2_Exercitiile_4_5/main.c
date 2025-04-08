#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"

#define MAX_APs 20
static const char *TAG = "wifi_scan";

void wifi_init_scan(void) {
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true
    };

    ESP_LOGI(TAG, "Pornesc scanarea WiFi...");
    ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true));

    uint16_t ap_count = 0;
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    if (ap_count > MAX_APs) ap_count = MAX_APs;

    wifi_ap_record_t ap_info[MAX_APs];
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_count, ap_info));

    printf("\n%-30s | %4s | %10s\n", "SSID", "RSSI", "Securitate");
    printf("-------------------------------------------------\n");
    for (int i = 0; i < ap_count; i++) {
        printf("%-30s | %4d | %10s\n",
               (char *)ap_info[i].ssid,
               ap_info[i].rssi,
               (ap_info[i].authmode == WIFI_AUTH_OPEN) ? "Deschis" : "Protejat");
    }
}

void socket_server_select() {
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(8080),
        .sin_addr.s_addr = INADDR_ANY
    };

    bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr));
    listen(server_sock, 1);
    ESP_LOGI(TAG, "Server socket pornit pe portul 8080");

    fd_set read_fds;
    struct timeval timeout;

    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(server_sock, &read_fds);

        timeout.tv_sec = 5;
        timeout.tv_usec = 0;

        int sel = select(server_sock + 1, &read_fds, NULL, NULL, &timeout);

        if (sel > 0 && FD_ISSET(server_sock, &read_fds)) {
            int client_sock = accept(server_sock, NULL, NULL);
            ESP_LOGI(TAG, "Client conectat");

            char buffer[128] = {0};
            int len = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
            if (len > 0) {
                ESP_LOGI(TAG, "Primit: %s", buffer);
                send(client_sock, "Salut de la ESP-IDF!\n", 24, 0);
            }
            close(client_sock);
        } else if (sel == 0) {
            ESP_LOGI(TAG, "Timeout - fara conexiuni noi");
        } else if (sel < 0) {
            ESP_LOGE(TAG, "Eroare la select()");
        }
    }
    close(server_sock);
}

void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_scan();
    socket_server_select();
}
