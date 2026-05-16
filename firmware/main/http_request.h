#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "sdkconfig.h"
#include "esp_http_client.h"
#include "core_http_client.h"
typedef struct
{
    char url[256];
    char method[16];
} http_cmd_t;

extern char g_url[256];
extern bool http_task_created;
extern QueueHandle_t http_queue;

void http_get_task(void *pvParameters);
void http_client_init(void);

#endif