/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <cstdio>
#include <cerrno>
#include <cstring>
#include <string>
#include <ctime>
#include <esp_heap_trace.h>
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "udp_broadcast.h"
#include "esp_http_server.h"
#include "config_wifi.h"
#include "handles.hpp"


#include <sys/time.h>
#define ESP_WIFI_SSID "testap"
#define ESP_WIFI_PASS "testtest"
// #define ESP_WIFI_SSID "ChinaUnicom-4DUDHP"
// #define ESP_WIFI_PASS "12345678"
#define ESP_MAXIMUM_RETRY 5
#define TAG "ESP_SPK"



static httpd_handle_t start_webserver(void) {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;
    config.server_port = 8080;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &hello);
        httpd_register_uri_handler(server, &echo);
        // httpd_register_uri_handler(server, &ctrl);
        httpd_register_uri_handler(server, &api_Devices);
        httpd_register_uri_handler(server, &api_AbilityRunning);
        httpd_register_uri_handler(server, &api_AbilitySupport);
        httpd_register_uri_handler(server, &api_AbilityRequest);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

static esp_err_t stop_webserver(httpd_handle_t server) {
    // Stop the httpd server
    return httpd_stop(server);
}

static void disconnect_handler(void *arg, esp_event_base_t event_base,
    int32_t event_id, void *event_data) {
    httpd_handle_t *server = (httpd_handle_t *) arg;
    if (*server) {
        ESP_LOGI(TAG, "Stopping webserver");
        if (stop_webserver(*server) == ESP_OK) {
            *server = NULL;
        }
        else {
            ESP_LOGE(TAG, "Failed to stop http server");
        }
    }
}

static void connect_handler(void *arg, esp_event_base_t event_base,
    int32_t event_id, void *event_data) {
    httpd_handle_t *server = (httpd_handle_t *) arg;
    if (*server == NULL) {
        ESP_LOGI(TAG, "Starting webserver");
        *server = start_webserver();
    }
}


extern "C" void app_main(void) {
    static httpd_handle_t server = NULL;
    printf("Hello world!\n");
    ESP_LOGE("HEAP", "start free_heap_size = %lu\n", esp_get_free_heap_size());
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    esp_netif_t *sta_netif = wifi_init_sta();

    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, sta_netif, &server));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler, sta_netif, &server));
    ESP_LOGI(TAG, "wifi start");
    wifi_start_and_connect(sta_netif, ESP_WIFI_SSID, ESP_WIFI_PASS);
    ESP_LOGE("HEAP", "init free_heap_size = %lu\n", esp_get_free_heap_size());

    create_broad_task((std::string(TAG) + std::string("+Idle,Stable+") +
        std::to_string(std::time(nullptr))).c_str());

    server = start_webserver();
    ESP_LOGE("HEAP", "idle free_heap_size = %lu\n", esp_get_free_heap_size());
}


// void app_main(void) {
//     wifi_init_sta();


//     xTaskCreate(speaker_task, "tcp_server", 18000, (void *) AF_INET, 5, NULL);

//     while (1) {
//         vTaskDelay(4000 / portTICK_PERIOD_MS);
//     }
// }

