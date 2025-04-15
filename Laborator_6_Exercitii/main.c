#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/event_groups.h"
#include "esp_http_server.h"
#include "esp_netif.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "soft-ap.h"
#include "http-server.h"
#include "../mdns/include/mdns.h"
#include "driver/gpio.h"

#define DEFAULT_SCAN_LIST_SIZE 10
#define BUTTON_GPIO GPIO_NUM_23
#define PROV_HOLD_TIME_MS 5000

static const char *TAG = "app_main";
static char available_ssids[2048];

void start_mdns_service()
{
    //initialize mDNS service
    esp_err_t err = mdns_init();
    if (err) {
        printf("MDNS Init failed: %d\n", err);
        return;
    }

    //set hostname
    mdns_hostname_set("my-esp32");
    //set default instance
    mdns_instance_name_set("Ionesi's ESP32 Thing");
}

static void wifi_scan(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    uint16_t number = DEFAULT_SCAN_LIST_SIZE;
    wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
    uint16_t ap_count = 0;
    memset(ap_info, 0, sizeof(ap_info));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_scan_start(NULL, true);
    ESP_LOGI(TAG, "Max AP number ap_info can hold = %u", number);
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    ESP_LOGI(TAG, "Total APs scanned = %u, actual AP number ap_info holds = %u", ap_count, number);

    for (int i = 0; i < number; i++) {
      char ssid_entry[128];
      snprintf(ssid_entry, sizeof(ssid_entry), "<option value='%s'>%s</option>", ap_info[i].ssid, ap_info[i].ssid);
      strncat(available_ssids, ssid_entry, sizeof(available_ssids) - strlen(available_ssids) - 1);
    }

    ESP_LOGI(TAG, "SSID List: %s", available_ssids);
}

const char* get_available_ssids() {
    return available_ssids;
}

void save_credentials_to_nvs(const char *ssid, const char *password) {
    nvs_handle_t nvs_handle;
    ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &nvs_handle));
    ESP_ERROR_CHECK(nvs_set_str(nvs_handle, "ssid", ssid));
    ESP_ERROR_CHECK(nvs_set_str(nvs_handle, "pass", password));
    ESP_ERROR_CHECK(nvs_commit(nvs_handle));
    nvs_close(nvs_handle);
    ESP_LOGI(TAG, "WiFi credentials saved! Restarting...");
    esp_restart();
}

bool get_stored_credentials(char *ssid, size_t ssid_len, char *password, size_t pass_len) {
    nvs_handle_t nvs_handle;
    if (nvs_open("storage", NVS_READONLY, &nvs_handle) != ESP_OK) {
        return false;
    }
    esp_err_t ssid_err = nvs_get_str(nvs_handle, "ssid", ssid, &ssid_len);
    esp_err_t pass_err = nvs_get_str(nvs_handle, "pass", password, &pass_len);
    nvs_close(nvs_handle);
    return ssid_err == ESP_OK && pass_err == ESP_OK;
}


void reset_wifi_config() {
    nvs_handle_t nvs_handle;
    if (nvs_open("wifi_creds", NVS_READWRITE, &nvs_handle) == ESP_OK) {
        nvs_erase_key(nvs_handle, "ssid");
        nvs_erase_key(nvs_handle, "pass");
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
    }
    esp_restart();
}

void button_monitor_task(void *arg) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    while (1) {
        if (gpio_get_level(BUTTON_GPIO) == 0) {
            vTaskDelay(pdMS_TO_TICKS(5000));
            if (gpio_get_level(BUTTON_GPIO) == 0) {
                reset_wifi_config();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}


void app_main(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Verifică dacă avem salvate SSID și parolă
    nvs_handle_t nvs_handle;
    size_t required_size;
    char ssid[32] = {0}, pass[64] = {0};
    bool provisioning_mode = true;

    if (nvs_open("wifi_creds", NVS_READONLY, &nvs_handle) == ESP_OK) {
        required_size = sizeof(ssid);
        if (nvs_get_str(nvs_handle, "ssid", ssid, &required_size) == ESP_OK &&
            nvs_get_str(nvs_handle, "pass", pass, &required_size) == ESP_OK) {
            provisioning_mode = false;
        }
        nvs_close(nvs_handle);
    }

    // Inițializează monitorizarea butonului
    xTaskCreate(button_monitor_task, "button_monitor_task", 2048, NULL, 5, NULL);

    if (provisioning_mode) {
        wifi_init_softap();
        wifi_scan();
        start_webserver();
    } else {
        ESP_LOGI(TAG, "Connecting to saved SSID: %s", ssid);
        start_mdns_service();
    }
}
