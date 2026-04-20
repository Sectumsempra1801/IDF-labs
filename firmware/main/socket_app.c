#include "socket_app.h"
#include "led_c.h"

#define HOST_IP_ADDR "192.168.90.8"
// #define HOST_IP_ADDR "10.75.243.88"
#define PORT 4444
static const char *TAG = "Socket";

static const char *payload = "Message from ESP32\r\n";

extern led_c_t LED1;
extern led_c_t LED2;

void udp_client_task(void *pvParameters)
{
    char rx_buffer[128];
    char host_ip[] = HOST_IP_ADDR;
    int addr_family = 0;
    int ip_protocol = 0;

    while (1)
    {

        struct sockaddr_in dest_addr;
        dest_addr.sin_addr.s_addr = inet_addr(HOST_IP_ADDR);
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(PORT);
        addr_family = AF_INET;
        ip_protocol = IPPROTO_IP;

        int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
        if (sock < 0)
        {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            break;
        }
        // --- ĐOẠN MỚI: CỐ ĐỊNH CỔNG LẮNG NGHE LÀ 4444 ---
        struct sockaddr_in local_addr;
        local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        local_addr.sin_family = AF_INET;
        local_addr.sin_port = htons(4444);
        if (bind(sock, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0)
        {
            ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        }
        // ------------------------------------------------

        // Set timeout
        struct timeval timeout;
        timeout.tv_sec = 10;
        timeout.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);

        ESP_LOGI(TAG, "Socket created, sending to %s:%d", HOST_IP_ADDR, PORT);

        while (1)
        {

            int err = sendto(sock, payload, strlen(payload), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
            if (err < 0)
            {
                ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
                break;
            }
            ESP_LOGI(TAG, "Message sent");

            struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
            socklen_t socklen = sizeof(source_addr);
            int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);

            // Error occurred during receiving
            if (len < 0) // ***
            {
                ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
                break;
            }
            // Data received
            else
            {
                rx_buffer[len] = 0; // Null-terminate whatever we received and treat like a string
                ESP_LOGI(TAG, "Received %d bytes from %s:", len, host_ip);
                ESP_LOGI(TAG, "%s", rx_buffer);
                if (strncmp(rx_buffer, "OK: ", 4) == 0)
                {
                    ESP_LOGI(TAG, "Received expected message, reconnecting");
                    break;
                }
            }
            process_cmd(rx_buffer, sock);

            vTaskDelay(2000 / portTICK_PERIOD_MS);
        }

        if (sock != -1)
        {
            ESP_LOGE(TAG, "Shutting down socket and restarting...");
            shutdown(sock, 0);
            close(sock);
        }
    }
    vTaskDelete(NULL);
}

void tcp_client_task(void *pvParameters)
{
    char rx_buffer[128];
    char host_ip[] = HOST_IP_ADDR;
    int addr_family = 0;
    int ip_protocol = 0;

    while (1)
    {
        struct sockaddr_in dest_addr;
        inet_pton(AF_INET, host_ip, &dest_addr.sin_addr);
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(PORT);
        addr_family = AF_INET;
        ip_protocol = IPPROTO_IP;

        int sock = socket(addr_family, SOCK_STREAM, ip_protocol);
        if (sock < 0)
        {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            break;
        }
        ESP_LOGI(TAG, "Socket created, connecting to %s:%d", host_ip, PORT);

        int err = connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err != 0)
        {
            ESP_LOGE(TAG, "Socket unable to connect: errno %d", errno);
            break;
        }
        ESP_LOGI(TAG, "Successfully connected");

        while (1)
        {
            int err = send(sock, payload, strlen(payload), 0);
            if (err < 0)
            {
                ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
                break;
            }

            int len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
            // Error occurred during receiving
            if (len < 0)
            {
                ESP_LOGE(TAG, "recv failed: errno %d", errno);
                break;
            }
            // Data received
            else
            {
                rx_buffer[len] = 0; // Null-terminate whatever we received and treat like a string
                ESP_LOGI(TAG, "Received %d bytes from %s:", len, host_ip);
                ESP_LOGI(TAG, "%s", rx_buffer);
                if (strncmp(rx_buffer, "OK: ", 4) == 0)
                {
                    ESP_LOGI(TAG, "Received expected message, reconnecting");
                    break;
                }
            }

            vTaskDelay(2000 / portTICK_PERIOD_MS);
        }

        if (sock != -1)
        {
            ESP_LOGE(TAG, "Shutting down socket and restarting...");
            shutdown(sock, 0);
            close(sock);
        }
    }
    vTaskDelete(NULL);
}
void process_cmd(char *rx_buffer, int sock)
{
    rx_buffer[strcspn(rx_buffer, "\r\n")] = 0;

    int led_id;
    char action[16];
    if (sscanf(rx_buffer, "LED%d_%15s", &led_id, action) == 2)
    {
        led_c_t *target_led = NULL;
        if (led_id == 1)
        {
            target_led = &LED1;
        }
        else if (led_id == 2)
        {
            target_led = &LED2;
        }
        if (target_led == NULL)
        {
            send(sock, "Error: LED not found\r\n", 25, 0);
            return;
        }
        if (strcmp(action, "ON") == 0)
        {
            led_c_on(target_led);
            send(sock, "OK: LED ON\r\n", 12, 0);
        }
        else if (strcmp(action, "OFF") == 0)
        {
            led_c_off(target_led);
            send(sock, "OK: LED OFF\r\n", 13, 0);
        }
        else if (action[0] == 'B')
        {
            int hz = atoi(&action[1]);
            if (hz > 0)
            {
                led_c_blink(hz, target_led);
                char reply[32];
                sprintf(reply, "OK: LED BLINK %dHz\r\n", hz);
                send(sock, reply, strlen(reply), 0);
            }
        }
        else if (action[0] == 'D')
        {
            int pct = atoi(&action[1]);
            led_c_dim(pct, target_led);
            char reply[32];
            sprintf(reply, "OK: LED DIM %d%%\r\n", pct);
            send(sock, reply, strlen(reply), 0);
        }
        else if (strcmp(action, "STOP") == 0)
        {
            led_c_blink_stop(target_led);
            send(sock, "OK: LED STOP BLINK\r\n", 20, 0);
        }
        else
        {
            send(sock, "ERROR: Wrong cmd\r\n", 18, 0);
        }
    }
    else
    {
        send(sock, "ERROR: wrong fomat LEDx_CMD\r\n", 29, 0);
    }
}