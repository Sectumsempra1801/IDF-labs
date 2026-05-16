#include "http_request.h"

static const char *TAG = "HTTP_REQ";

char g_url[256] = "";
QueueHandle_t http_queue = NULL;
bool http_task_created = false;
struct NetworkContext
{
    int socket_fd;
};

static int32_t transport_recv(NetworkContext_t *pNetworkContext, void *pBuffer, size_t bytesToRecv)
{
    int ret = recv(pNetworkContext->socket_fd, pBuffer, bytesToRecv, 0);
    return (ret >= 0) ? ret : -1;
}

static int32_t transport_send(NetworkContext_t *pNetworkContext, const void *pBuffer, size_t bytesToSend)
{
    int ret = send(pNetworkContext->socket_fd, pBuffer, bytesToSend, 0);
    return (ret >= 0) ? ret : -1;
}

void http_get_task(void *pvParameters)
{
    http_cmd_t cmd;

    size_t buffer_size = 24 * 1024;
    uint8_t *buffer = (uint8_t *)malloc(buffer_size);

    if (buffer == NULL)
    {
        ESP_LOGE(TAG, "Insufficient RAM to allocate 24KB HTTP buffer!");
        vTaskDelete(NULL);
    }

    while (1)
    {
        if (xQueueReceive(http_queue, &cmd, portMAX_DELAY) == pdPASS)
        {
            ESP_LOGI(TAG, "Executing %s request to: %s", cmd.method, cmd.url);

            char host[128] = {0};
            char path[128] = "/";
            int port = 80;

            const char *p = strstr(cmd.url, "http://");
            if (p)
                p += 7;
            else
                p = cmd.url;

            const char *slash = strchr(p, '/');
            if (slash)
            {
                int host_len = slash - p;
                strncpy(host, p, host_len);
                strncpy(path, slash, sizeof(path) - 1);
            }
            else
            {
                strncpy(host, p, sizeof(host) - 1);
            }

            char *colon = strchr(host, ':');
            if (colon)
            {
                *colon = '\0';
                port = atoi(colon + 1);
            }

            struct hostent *he = gethostbyname(host);
            if (!he)
            {
                ESP_LOGE(TAG, "DNS resolution failed for host: %s", host);
                continue;
            }

            int sock = socket(AF_INET, SOCK_STREAM, 0);
            if (sock < 0)
                continue;

            struct sockaddr_in server_addr;
            server_addr.sin_family = AF_INET;
            server_addr.sin_port = htons(port);
            server_addr.sin_addr.s_addr = ((struct in_addr *)he->h_addr)->s_addr;

            // Set Socket timeout to 5 seconds to prevent hanging
            struct timeval timeout;
            timeout.tv_sec = 5;
            timeout.tv_usec = 0;
            setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

            if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0)
            {
                ESP_LOGE(TAG, "Socket connection failed");
                close(sock);
                continue;
            }
            ESP_LOGI(TAG, "Connected to Socket at %s:%d", host, port);

            // Configure Network Context for coreHTTP
            NetworkContext_t networkContext = {.socket_fd = sock};
            TransportInterface_t transport;
            transport.pNetworkContext = &networkContext;
            transport.send = transport_send;
            transport.recv = transport_recv;

            HTTPRequestInfo_t requestInfo = {0};
            requestInfo.pMethod = cmd.method;
            requestInfo.methodLen = strlen(cmd.method);
            requestInfo.pHost = host;
            requestInfo.hostLen = strlen(host);
            requestInfo.pPath = path;
            requestInfo.pathLen = strlen(path);

            memset(buffer, 0, buffer_size);

            HTTPRequestHeaders_t requestHeaders = {0};
            requestHeaders.pBuffer = buffer;
            requestHeaders.bufferLen = buffer_size;

            HTTPStatus_t headerStatus = HTTPClient_InitializeRequestHeaders(&requestHeaders, &requestInfo);
            if (headerStatus != HTTPSuccess)
            {
                ESP_LOGE(TAG, "Error: Failed to initialize HTTP Headers");
                close(sock);
                continue;
            }

            HTTPResponse_t response = {0};
            response.pBuffer = buffer;
            response.bufferLen = buffer_size;

            // Send HTTP Request
            HTTPStatus_t httpStatus = HTTPClient_Send(&transport, &requestHeaders, NULL, 0, &response, 0);

            if (httpStatus == HTTPSuccess)
            {
                ESP_LOGI(TAG, "HTTP Response Code: %d", response.statusCode);
                ESP_LOGI(TAG, "Received Data:\n%.*s", (int)response.bodyLen, (char *)response.pBody);
            }
            else if (httpStatus == HTTPInsufficientMemory)
            {
                ESP_LOGE(TAG, "Error: coreHTTP Insufficient Memory (Code 5). Printing partial data:");
                ESP_LOGI(TAG, "Partial Data:\n%.*s", (int)response.bodyLen, (char *)response.pBody);
            }
            else
            {
                ESP_LOGE(TAG, "coreHTTP Send failed. Error code: %d", httpStatus);
            }

            close(sock);
        }
    }
}

void http_client_init(void)
{
    if (!http_task_created)
    {
        http_queue = xQueueCreate(5, sizeof(http_cmd_t));
        xTaskCreate(http_get_task, "http_get_task", 5120, NULL, 5, NULL);
        http_task_created = true;
    }
}