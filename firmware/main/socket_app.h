#ifndef SOCKET_APP_H
#define SOCKET_APP_H

#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"

typedef struct
{
    int sock;
    uint8_t type;
    char payload[128];
    struct sockaddr_storage source_addr;
    socklen_t socklen;
} socket_msg_t;

typedef enum
{
    SOCKET_TCP = 0,
    SOCKET_UDP = 1
} socket_type_t;

typedef enum
{
    CMD_LED_ON = 0,
    CMD_LED_OFF = 1,
    CMD_LED_BLINK = 2,
    CMD_LED_DIM = 3,
    CMD_LED_STOP = 4
} led_action_t;

typedef struct
{
    int sock;
    uint8_t net_type;
    struct sockaddr_storage addr;
    socklen_t addr_len;

    int led_id;
    led_action_t action;
    int value;
} hardware_cmd_t;
void socket_system_init(void);

void tcp_client_task(void *pvParameters);
void udp_client_task(void *pvParameters);
void producer(void *pvParameters);
void consumer(void *pvParameters);

void send_reply(int sock, const char *msg, struct sockaddr_storage *addr, socklen_t addr_len);

esp_err_t read_server_IP(void);

esp_err_t save_server_IP(const char *server_IP);

#endif