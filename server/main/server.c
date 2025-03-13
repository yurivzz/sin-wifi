#include <stdio.h>
#include <string.h>
#include <math.h>
#include <errno.h>          // Adicionado para errno
#include <sys/socket.h>      // Adicionado para socket API
#include <netinet/in.h>      // Adicionado para sockaddr_in e constantes
#include <arpa/inet.h>       // Adicionado para htonl/htons
#include <unistd.h>          // Adicionado para close()

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"

#define PORT 3333
#define SAMPLE_RATE 1000
#define BUFFER_SIZE 512

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static const char *TAG = "wifi_ap";

float generate_sine(float amplitude, float frequency, float phase, int index) {
    return amplitude * sin(2 * M_PI * frequency * index / SAMPLE_RATE + phase);
}

void wifi_init_softap(void) {
    esp_netif_init();
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = "ESP32_AP",
            .ssid_len = strlen("ESP32_AP"),
            .channel = 1,
            .password = "123456789",
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void tcp_server_task(void *pvParameters) {
    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY)
    };

    int listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (listen_sock < 0) {
        ESP_LOGE(TAG, "Failed to create socket: errno %d", errno);
        return;
    }

    if (bind(listen_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0) {
        ESP_LOGE(TAG, "Bind failed: errno %d", errno);
        close(listen_sock);
        return;
    }

    if (listen(listen_sock, 1) != 0) {
        ESP_LOGE(TAG, "Listen failed: errno %d", errno);
        close(listen_sock);
        return;
    }

    while(1) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int client_sock = accept(listen_sock, (struct sockaddr *)&client_addr, &addr_len);
        
        if(client_sock < 0) {
            ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
            continue;
        }

        ESP_LOGI(TAG, "Connection accepted from %s", inet_ntoa(client_addr.sin_addr));

        float buffer[BUFFER_SIZE];
        float phase = 0;
        int index = 0;
        
        while(1) {
            for(int i = 0; i < BUFFER_SIZE; i++) {
                buffer[i] = generate_sine(1.0f, 50.0f, phase, index++);
                phase += 2 * M_PI * 50.0f / SAMPLE_RATE;
                if(phase >= 2 * M_PI) phase -= 2 * M_PI;
            }
            
            int err = send(client_sock, buffer, sizeof(buffer), 0);
            if(err < 0) {
                ESP_LOGE(TAG, "Error sending data: errno %d", errno);
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        shutdown(client_sock, SHUT_RDWR);
        close(client_sock);
    }
}

void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    wifi_init_softap();
    xTaskCreate(tcp_server_task, "tcp_server", 4096, NULL, 5, NULL);
}