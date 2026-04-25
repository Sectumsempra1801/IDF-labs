#include "socket_app.h"
#include "led_c.h"
#include "wifi.h"

// #define HOST_IP_ADDR "192.168.90.8"
// #define HOST_IP_ADDR "10.75.243.88"
static const char *TAG = "Socket";
#define TCP_PORT 5050
#define UDP_PORT 4040
static const char *payload_tcp = "PING_TCP\r\n";
static const char *payload_udp = "PING_UDP\r\n";

extern led_c_t LED1;
extern led_c_t LED2;

void udp_client_task(void *pvParameters)
{
    char rx_buffer[128];
    char *host_ip = g_server_ip;
    int addr_family = 0;
    int ip_protocol = 0;

    while (1)
    {
        // CONFIG IPV4
        struct sockaddr_in dest_addr;
        dest_addr.sin_addr.s_addr = inet_addr(g_server_ip);
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(UDP_PORT);
        addr_family = AF_INET;
        ip_protocol = IPPROTO_IP;

        int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
        if (sock < 0)
        {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            break;
        }
        struct sockaddr_in local_addr;
        local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        local_addr.sin_family = AF_INET;
        local_addr.sin_port = htons(UDP_PORT);
        if (bind(sock, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0)
        {
            ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        }

        // Set timeout
        struct timeval timeout;
        timeout.tv_sec = 20;
        timeout.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);

        ESP_LOGI(TAG, "Socket created, sending to %s:%d", host_ip, UDP_PORT);

        while (1)
        {
            // use sendto() because send() function use for TCP(connected)
            int err = sendto(sock, payload_udp, strlen(payload_udp), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
            if (err < 0)
            {
                ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
                break;
            }

            struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
            socklen_t socklen = sizeof(source_addr);
            int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);

            // Error occurred during receiving
            if (len < 0) // ***
            {
                if (errno == 11 || errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    continue;
                }
                else
                {
                    ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
                    break;
                }
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
            // execute command
            process_cmd(rx_buffer, sock, &source_addr, socklen);
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }

        if (sock != -1)
        {
            ESP_LOGE(TAG, "Shutting down socket (UDP) and restarting...");
            shutdown(sock, 0);
            close(sock);
        }
    }
    ESP_LOGW(TAG, "Closing UDP Task");
    vTaskDelete(NULL);
}

void tcp_client_task(void *pvParameters)
{
    char rx_buffer[128];
    char *host_ip = g_server_ip;
    int addr_family = 0;
    int ip_protocol = 0;

    while (1)
    {
        struct sockaddr_in dest_addr;
        inet_pton(AF_INET, host_ip, &dest_addr.sin_addr); //
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(TCP_PORT);
        addr_family = AF_INET;
        ip_protocol = IPPROTO_IP;

        int sock = socket(addr_family, SOCK_STREAM, ip_protocol);
        if (sock < 0)
        {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "Socket created, connecting to %s:%d", host_ip, TCP_PORT);
        // CONNECT
        int err = connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err != 0)
        {
            ESP_LOGE(TAG, "Socket unable to connect: errno %d", errno);
            close(sock);
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }
        ESP_LOGI(TAG, "Successfully connected");
        //
        int err1 = send(sock, payload_tcp, strlen(payload_tcp), 0);
        if (err1 < 0)
        {
            ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
            continue;
        }
        while (1)
        {

            int len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
            // Error occurred during receiving
            if (len < 0)
            {
                if (errno == 11 || errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    vTaskDelay(pdMS_TO_TICKS(10));
                    continue;
                }
                ESP_LOGE(TAG, "recv failed: errno %d", errno);
                break;
            }
            else if (len == 0)
            {
                ESP_LOGW(TAG, "Server disconnected");
                break; // Break để đóng socket và reconnect
            }

            // Data received
            else
            {
                rx_buffer[len] = 0; // Null-terminate whatever we received and treat like a string
                ESP_LOGI(TAG, "Received %d bytes from %s:%d", len, host_ip, TCP_PORT);
                ESP_LOGI(TAG, "%s", rx_buffer);
                // if (strncmp(rx_buffer, "OK: ", 4) == 0)
                // {
                //     ESP_LOGI(TAG, "Received expected message, reconnecting");
                //     break;
                // }
            }
            // execute command
            process_cmd(rx_buffer, sock, NULL, 0);
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }

        if (sock != -1)
        {
            ESP_LOGE(TAG, "Shutting down socket (TCP) and restarting...");
            shutdown(sock, 0);
            close(sock);
        }
    }
    vTaskDelete(NULL);
}

void process_cmd(char *rx_buffer, int sock, struct sockaddr_storage *source_addr, socklen_t socklen)
{
    rx_buffer[strcspn(rx_buffer, "\r\n")] = 0;
    int led_id;
    char action[16];

    if (sscanf(rx_buffer, "LED%d_%15s", &led_id, action) == 2)
    {
        // target which LED to use
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
            send_reply(sock, "Error: LED not found\r\n", source_addr, socklen);
            return;
        }
        // action
        if (strcmp(action, "ON") == 0)
        {
            led_c_on(target_led);
            // send(sock, "OK: LED ON\r\n", 12, 0);
            send_reply(sock, "OK: LED ON\r\n", source_addr, socklen);
        }
        else if (strcmp(action, "OFF") == 0)
        {
            led_c_off(target_led);
            // send(sock, "OK: LED OFF\r\n", 13, 0);
            send_reply(sock, "OK: LED OFF\r\n", source_addr, socklen);
        }
        else if (action[0] == 'B')
        {
            int hz = atoi(&action[1]);
            if (hz > 0)
            {
                led_c_blink(hz, target_led);
                char reply[32];
                sprintf(reply, "OK: LED BLINK %dHz\r\n", hz);
                // send(sock, reply, strlen(reply), 0);
                send_reply(sock, reply, source_addr, socklen);
            }
        }
        else if (action[0] == 'D')
        {
            int pct = atoi(&action[1]);
            led_c_dim(pct, target_led);
            char reply[32];
            sprintf(reply, "OK: LED DIM %d%%\r\n", pct);
            // send(sock, reply, strlen(reply), 0);
            send_reply(sock, reply, source_addr, socklen);
        }
        else if (strcmp(action, "STOP") == 0)
        {
            led_c_blink_stop(target_led);
            // send(sock, "OK: LED STOP BLINK\r\n", 20, 0);
            send_reply(sock, "OK: LED STOP BLINK\r\n", source_addr, socklen);
        }
        else
        {
            // send(sock, "ERROR: Wrong cmd\r\n", 18, 0);
            send_reply(sock, "ERROR: Wrong cmd\r\n", source_addr, socklen);
        }
    }
    else
    {
        // send(sock, "ERROR: wrong fomat LEDx_CMD\r\n", 29, 0);
        send_reply(sock, "ERROR: wrong fomat LEDx_CMD\r\n", source_addr, socklen);
    }
}
// choose which send to use for each protocol
void send_reply(int sock, const char *msg, struct sockaddr_storage *addr, socklen_t addr_len)
{
    if (addr == NULL)
    {
        send(sock, msg, strlen(msg), 0);
    }
    else
    {
        sendto(sock, msg, strlen(msg), 0, (struct sockaddr *)addr, addr_len);
    }
}

esp_err_t read_server_IP(void)
{
    nvs_handle_t my_handle;
    esp_err_t err;
    err = nvs_open("wifi_data", NVS_READONLY, &my_handle);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "Sever IP might not be saved yet");
        return err;
    }
    size_t ip_len = sizeof(g_server_ip);
    err = nvs_get_str(my_handle, "server_IP", g_server_ip, &ip_len);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "sever_IP not found in NVS");
    }

    nvs_close(my_handle);
    return err;
}

esp_err_t save_server_IP(const char *sever_IP)
{
    nvs_handle_t my_handle;
    esp_err_t err;
    err = nvs_open("wifi_data", NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "Sever IP might not be saved yet");
        return err;
    }
    err = nvs_set_str(my_handle, "sever_IP", sever_IP);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error occured while saving sever_IP(%s)", esp_err_to_name(err));
        nvs_close(my_handle);
        return err;
    }
    err = nvs_commit(my_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error occured while committing NVS (%s)", esp_err_to_name(err));
    }
    else
    {
        ESP_LOGI(TAG, "Data has been saved");
    }
    nvs_close(my_handle);
    return err;
}