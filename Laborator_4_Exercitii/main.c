#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "../mdns/include/mdns.h"

#include "lwip/sockets.h"
#include "lwip/netdb.h"

#define CONFIG_ESP_WIFI_SSID      "lab-iot"
#define CONFIG_ESP_WIFI_PASS      "IoT-IoT-IoT"
#define CONFIG_LOCAL_PORT         12345
#define CONTROL_SERVICE_NAME      "control_led"
#define CONTROL_SERVICE_TYPE      "_udp"
#define CONTROL_SERVICE_PORT      CONFIG_LOCAL_PORT
#define HOSTNAME                  "esp32-ionesi"

#define GPIO_OUTPUT_IO            4
#define GPIO_OUTPUT_PIN_SEL       (1ULL<<GPIO_OUTPUT_IO)
#define GPIO_INPUT_IO             2
#define GPIO_INPUT_PIN_SEL        (1ULL<<GPIO_INPUT_IO)

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define BIT_BTN_PRESSED    BIT0

static const char *TAG = "lab5";
static EventGroupHandle_t s_wifi_event_group;
static EventGroupHandle_t s_event_start_udp;

static int s_retry_num = 0;

static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < 5) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

bool wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_ESP_WIFI_SSID,
            .password = CONFIG_ESP_WIFI_PASS,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    return (bits & WIFI_CONNECTED_BIT);
}

void mdns_init_custom()
{
    ESP_ERROR_CHECK(mdns_init());
    ESP_ERROR_CHECK(mdns_hostname_set(HOSTNAME));
    ESP_LOGI(TAG, "mdns hostname set to: [%s]", HOSTNAME);

    // Adăugăm serviciul control_led
    ESP_ERROR_CHECK(mdns_instance_name_set("ESP32 Control LED"));
    ESP_ERROR_CHECK(mdns_service_add("ESP32 Control", CONTROL_SERVICE_TYPE, CONTROL_SERVICE_TYPE,
                                     CONTROL_SERVICE_PORT, NULL, 0));
}

void query_other_devices()
{
    ESP_LOGI(TAG, "Searching for other esp32 devices...");
    mdns_result_t * results = NULL;
    esp_err_t err = mdns_query_a("esp32-popescu", 3000, &results);
    if (err) {
        ESP_LOGE(TAG, "Query Failed: %s", esp_err_to_name(err));
        return;
    }

    mdns_result_t * r = results;
    while (r) {
        if (r->addr) {
            ESP_LOGI(TAG, "Found IP: " IPSTR, IP2STR(&r->addr->addr.u_addr.ip4));
        }
        r = r->next;
    }
    mdns_query_results_free(results);
}

void discover_services()
{
    mdns_result_t * results = NULL;
    ESP_LOGI(TAG, "Discovering services...");
    esp_err_t err = mdns_query_ptr("_services._dns-sd._udp.local", 3000, &results);
    if (err) {
        ESP_LOGE(TAG, "Service query failed: %s", esp_err_to_name(err));
        return;
    }
    mdns_result_t * r = results;
    while (r) {
        ESP_LOGI(TAG, "Service found: %s", r->instance_name);
        r = r->next;
    }
    mdns_query_results_free(results);
}

void send_udp_packet(ip4_addr_t dest_ip, uint16_t port)
{
    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = dest_ip.addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        return;
    }
    char payload[] = "LED_ON";
    sendto(sock, payload, strlen(payload), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    ESP_LOGI(TAG, "Sent datagram to " IPSTR ":%d", IP2STR(&dest_ip), port);
    close(sock);
}

void button_task(void *pvParameters)
{
    uint8_t count = 5;
    int last_state = 1;

    while (1)
    {
        int state = gpio_get_level(GPIO_INPUT_IO);
        if (state != last_state)
            count--;
        else
            count = 5;

        if (!count) {
            last_state = state;
            if (state == 0) {
                ESP_LOGI(TAG, "Button pressed. Searching for service...");
                mdns_result_t * results = NULL;
                esp_err_t err = mdns_query_ptr("_control_led._udp.local", 5000, &results);
                if (!err && results) {
                    int total = 0;
                    mdns_result_t * r = results;
                    while (r) { total++; r = r->next; }

                    if (total > 0) {
                        srand(time(NULL));
                        int pick = rand() % total;
                        r = results;
                        while (pick--) r = r->next;

                        if (r->addr && r->port) {
                            send_udp_packet(r->addr->addr.u_addr.ip4, r->port);
                        }
                    }
                    mdns_query_results_free(results);
                } else {
                    ESP_LOGW(TAG, "No services found.");
                }
            }
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void gpio_init()
{
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    gpio_init();
    bool connected = wifi_init_sta();
    if (!connected) return;

    mdns_init_custom();
    query_other_devices();
    discover_services();

    s_event_start_udp = xEventGroupCreate();
    xTaskCreate(button_task, "button_task", 4096, NULL, 5, NULL);
}
