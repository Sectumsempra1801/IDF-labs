#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"

#include "socket_app.h"
#include "led_c.h"
#include "wifi.h"
#include "flash_manager.h"

static const char *TAG = "SOCKET_APP";

#define TCP_PORT 5050
#define UDP_PORT 4040

QueueHandle_t socket_rx_queue = NULL;
QueueHandle_t hardware_cmd_queue = NULL;

extern led_c_t LED1;
extern led_c_t LED2;
void socket_system_init(void)
{
    ESP_LOGI(TAG, "Initializing socket system...");

    socket_rx_queue = xQueueCreate(20, sizeof(socket_msg_t));
    if (socket_rx_queue == NULL)
    {
        ESP_LOGE(TAG, "Failed to create socket_rx_queue");
    }
    ESP_LOGI(TAG, "Queue_1 created (20 items)");

    hardware_cmd_queue = xQueueCreate(20, sizeof(hardware_cmd_t));
    if (hardware_cmd_queue == NULL)
    {
        ESP_LOGE(TAG, "Failed to create hardware_cmd_queue");
    }
    ESP_LOGI(TAG, "Queue_2 created (20 items)");

    // Create tasks
    xTaskCreate(producer, "socket_producer", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "Socket producer task created");

    xTaskCreate(consumer, "socket_consumer", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "Socket consumer task created");

    xTaskCreate(tcp_client_task, "tcp_client", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "TCP task created");

    xTaskCreate(udp_client_task, "udp_client", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "UDP task created");

    ESP_LOGI(TAG, "Socket system initialized!");
}

void tcp_client_task(void *pvParameters)
{
    ESP_LOGI(TAG, "TCP client task started");

    char rx_buffer[128];
    int sock = -1;

    while (1)
    {
        // Wait for WiFi
        xEventGroupWaitBits(wifi_get_event_group(), WIFI_CONNECTED_BIT,
                            pdFALSE, pdTRUE, portMAX_DELAY);

        // Create socket
        if (sock < 0)
        {
            sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
            if (sock < 0)
            {
                ESP_LOGW(TAG, "TCP socket creation failed");
                vTaskDelay(pdMS_TO_TICKS(1000));
                continue;
            }
            // Set timeout
            struct timeval timeout = {.tv_sec = 5, .tv_usec = 0};
            setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
            // Connect
            struct sockaddr_in dest_addr = {
                .sin_family = AF_INET,
                .sin_port = htons(TCP_PORT)};
            inet_pton(AF_INET, g_server_ip, &dest_addr.sin_addr);

            if (connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) != 0)
            {
                ESP_LOGW(TAG, "TCP connect failed");
                close(sock);
                sock = -1;
                vTaskDelay(pdMS_TO_TICKS(1000));
                continue;
            }

            ESP_LOGI(TAG, "TCP connected to %s:%d", g_server_ip, TCP_PORT);
        }

        // Receive
        int len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);

        if (len > 0)
        {
            rx_buffer[len] = 0;

            if (socket_rx_queue != NULL)
            {
                socket_msg_t msg = {
                    .sock = sock,
                    .type = SOCKET_TCP,
                    .socklen = 0};
                strncpy(msg.payload, rx_buffer, sizeof(msg.payload) - 1);
                msg.payload[sizeof(msg.payload) - 1] = '\0';
                if (xQueueSend(socket_rx_queue, &msg, pdMS_TO_TICKS(100)) != pdPASS)
                {
                    ESP_LOGW(TAG, "TCP: Queue_1 full, dropped message");
                }
                else
                {
                    ESP_LOGD(TAG, "[TCP] → Queue_1: %s", rx_buffer);
                }
            }
        }
        else if (len == 0 || (len < 0 && errno != EAGAIN && errno != EWOULDBLOCK))
        {
            // Connection closed or error
            ESP_LOGW(TAG, "TCP connection lost");
            close(sock);
            sock = -1;
            vTaskDelay(pdMS_TO_TICKS(2000));
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void udp_client_task(void *pvParameters)
{
    ESP_LOGI(TAG, "UDP client task started");

    char rx_buffer[128];
    struct sockaddr_in local_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(UDP_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY)};

    // Create socket
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0)
    {
        ESP_LOGE(TAG, "UDP socket creation failed");
        vTaskDelete(NULL);
        return;
    }

    if (bind(sock, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0)
    {
        ESP_LOGE(TAG, "UDP bind failed");
        close(sock);
        vTaskDelete(NULL);
        return;
    }

    // Set timeout
    struct timeval timeout = {.tv_sec = 5, .tv_usec = 0};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    ESP_LOGI(TAG, "UDP listening on port %d", UDP_PORT);

    while (1)
    {
        struct sockaddr_storage source_addr;
        socklen_t socklen = sizeof(source_addr);

        // Receive
        int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0,
                           (struct sockaddr *)&source_addr, &socklen);

        if (len > 0)
        {
            rx_buffer[len] = 0;

            if (socket_rx_queue != NULL)
            {
                socket_msg_t msg = {
                    .sock = sock,
                    .type = SOCKET_UDP,
                    .socklen = socklen};
                strncpy(msg.payload, rx_buffer, sizeof(msg.payload) - 1);
                memcpy(&msg.source_addr, &source_addr, sizeof(source_addr));

                if (xQueueSend(socket_rx_queue, &msg, pdMS_TO_TICKS(100)) != pdPASS)
                {
                    ESP_LOGW(TAG, "UDP: Queue_1 full, dropped message");
                }
                else
                {
                    ESP_LOGD(TAG, "[UDP] → Queue_1: %s", rx_buffer);
                }
            }
        }
        else if (len < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
        {
            ESP_LOGW(TAG, "UDP recvfrom error");
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void producer(void *pvParameters)
{
    ESP_LOGI(TAG, "Parser task started");

    socket_msg_t rx_msg;

    while (1)
    {
        // Stage 1: Get from Queue_1
        if (xQueueReceive(socket_rx_queue, &rx_msg, portMAX_DELAY) != pdPASS)
        {
            continue;
        }

        ESP_LOGI(TAG, "[Parser] Got from Queue_1: %s", rx_msg.payload);

        // Remove trailing newline
        rx_msg.payload[strcspn(rx_msg.payload, "\r\n")] = 0;

        // Stage 2: Parse command
        hardware_cmd_t cmd = {0};
        cmd.sock = rx_msg.sock;
        cmd.net_type = rx_msg.type;
        memcpy(&cmd.addr, &rx_msg.source_addr, sizeof(rx_msg.source_addr));
        cmd.addr_len = rx_msg.socklen;

        int led_id = 0;
        char action_str[16] = {0};

        // Parse: LEDx_ACTION[value]
        if (sscanf(rx_msg.payload, "LED%d_%15s", &led_id, action_str) == 2)
        {
            cmd.led_id = led_id;
            bool is_valid = true;

            // Convert action string to enum
            if (strcmp(action_str, "ON") == 0)
            {
                cmd.action = CMD_LED_ON;
            }
            else if (strcmp(action_str, "OFF") == 0)
            {
                cmd.action = CMD_LED_OFF;
            }
            else if (action_str[0] == 'B')
            {
                cmd.action = CMD_LED_BLINK;
                cmd.value = atoi(&action_str[1]);
                if (cmd.value == 0)
                    is_valid = false;
            }
            else if (action_str[0] == 'D')
            {
                cmd.action = CMD_LED_DIM;
                cmd.value = atoi(&action_str[1]);
                if (cmd.value > 100)
                    cmd.value = 100;
                if (cmd.value == 0)
                    is_valid = false;
            }
            else if (strcmp(action_str, "STOP") == 0)
            {
                cmd.action = CMD_LED_STOP;
            }
            else
            {
                is_valid = false;
            }

            // Stage 3: Put to Queue_2 or send error
            if (is_valid && cmd.led_id >= 1 && cmd.led_id <= 2)
            {
                if (xQueueSend(hardware_cmd_queue, &cmd, pdMS_TO_TICKS(100)) == pdPASS)
                {
                    ESP_LOGI(TAG, "[Parser] → Queue_2: LED%d", cmd.led_id);
                }
                else
                {
                    ESP_LOGW(TAG, "Queue_2 full!");
                    send_reply(rx_msg.sock, "ERROR: System busy\r\n",
                               (rx_msg.type == SOCKET_UDP) ? &rx_msg.source_addr : NULL,
                               rx_msg.socklen);
                }
            }
            else
            {
                ESP_LOGW(TAG, "Invalid command: %s", rx_msg.payload);
                send_reply(rx_msg.sock, "ERROR: Invalid action\r\n",
                           (rx_msg.type == SOCKET_UDP) ? &rx_msg.source_addr : NULL,
                           rx_msg.socklen);
            }
        }
        else
        {
            // Wrong format
            ESP_LOGW(TAG, "Parse error: %s", rx_msg.payload);
            send_reply(rx_msg.sock, "ERROR: Format is LEDx_ACTION\r\n",
                       (rx_msg.type == SOCKET_UDP) ? &rx_msg.source_addr : NULL,
                       rx_msg.socklen);
        }
    }
}

void consumer(void *pvParameters)
{
    ESP_LOGI(TAG, "Executor task started");

    hardware_cmd_t cmd;
    char reply_buf[64];
    while (1)
    {
        // Stage 1: Get from Queue_2
        if (xQueueReceive(hardware_cmd_queue, &cmd, portMAX_DELAY) != pdPASS)
        {
            continue;
        }

        ESP_LOGI(TAG, "[Executor] Got from Queue_2: LED%d", cmd.led_id);

        // Get target LED
        led_c_t *target_led = NULL;
        if (cmd.led_id == 1)
        {
            target_led = &LED1;
        }
        else if (cmd.led_id == 2)
        {
            target_led = &LED2;
        }
        else
        {
            ESP_LOGE(TAG, "Invalid LED ID: %d", cmd.led_id);
            continue;
        }

        switch (cmd.action)
        {
        case CMD_LED_ON:
            led_c_on(target_led);
            snprintf(reply_buf, sizeof(reply_buf),
                     "OK: LED%d ON\r\n", cmd.led_id);
            ESP_LOGI(TAG, "[Executor] LED%d ON", cmd.led_id);
            break;

        case CMD_LED_OFF:
            led_c_off(target_led);
            snprintf(reply_buf, sizeof(reply_buf),
                     "OK: LED%d OFF\r\n", cmd.led_id);
            ESP_LOGI(TAG, "[Executor] LED%d OFF", cmd.led_id);
            break;

        case CMD_LED_BLINK:
            if (cmd.value > 0)
            {
                led_c_blink(cmd.value, target_led);
                snprintf(reply_buf, sizeof(reply_buf),
                         "OK: LED%d BLINK %dHz\r\n", cmd.led_id, cmd.value);
                ESP_LOGI(TAG, "[Executor] LED%d BLINK %dHz", cmd.led_id, cmd.value);
            }
            break;

        case CMD_LED_DIM:
            if (cmd.value > 0 && cmd.value <= 100)
            {
                led_c_dim(cmd.value, target_led);
                snprintf(reply_buf, sizeof(reply_buf),
                         "OK: LED%d DIM %d%%\r\n", cmd.led_id, cmd.value);
                ESP_LOGI(TAG, "[Executor] LED%d DIM %d%%", cmd.led_id, cmd.value);
            }
            break;

        case CMD_LED_STOP:
            led_c_blink_stop(target_led);
            snprintf(reply_buf, sizeof(reply_buf),
                     "OK: LED%d STOP\r\n", cmd.led_id);
            ESP_LOGI(TAG, "[Executor] LED%d STOP", cmd.led_id);
            break;

        default:
            snprintf(reply_buf, sizeof(reply_buf),
                     "ERROR: Unknown action\r\n");
            break;
        }

        // Stage 3: Send reply
        send_reply(cmd.sock, reply_buf,
                   (cmd.net_type == SOCKET_UDP) ? &cmd.addr : NULL,
                   cmd.addr_len);
    }
}

void send_reply(int sock, const char *msg, struct sockaddr_storage *addr, socklen_t addr_len)
{
    if (addr == NULL)
    {
        // TCP: use send()
        int err = send(sock, msg, strlen(msg), 0);
        if (err < 0)
        {
            ESP_LOGW(TAG, "TCP send failed");
        }
    }
    else
    {
        // UDP: use sendto()
        int err = sendto(sock, msg, strlen(msg), 0, (struct sockaddr *)addr, addr_len);
        if (err < 0)
        {
            ESP_LOGW(TAG, "UDP sendto failed");
        }
    }
}

esp_err_t read_server_IP(void)
{
    esp_err_t err = nvs_read_string("wifi_data", "server_IP", g_server_ip, sizeof(g_server_ip));
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "Server IP not found in NVS");
    }
    return err;
}

esp_err_t save_server_IP(const char *server_IP)
{
    esp_err_t err = nvs_write_string("wifi_data", "server_IP", server_IP);
    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "Server IP saved");
    }
    return err;
}