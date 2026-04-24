#ifndef SOCKET_APP_H
#define SOCKET_APP_H

#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
// #include "protocol_examples_common.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"

void tcp_client_task(void *pvParameters);

void udp_client_task(void *pvParameters);

void process_cmd(char *rx_buffer, int sock, struct sockaddr_storage *source_addr, socklen_t socklen);

void send_reply(int sock, const char *msg, struct sockaddr_storage *addr, socklen_t addr_len);

esp_err_t read_server_IP(void);

esp_err_t save_server_IP(const char *sever_IP);

#endif