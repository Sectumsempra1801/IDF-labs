#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "esp_log.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"

#include "core_mqtt.h"
#include "mqtt_app.h"

static const char *TAG = "MQTT_APP";

// Broker settings
#define BROKER_HOSTNAME "broker.hivemq.com"
#define BROKER_PORT 1883
#define SUB_TOPIC "HuyVan/#"
#define PUB_TOPIC "HuyVan/ss1"

struct NetworkContext
{
    int socket_fd;
};

static SemaphoreHandle_t mqttMutex = NULL;
static volatile bool isMqttConnected = false;
static MQTTContext_t mqttContext;
static struct NetworkContext networkContext;
static uint8_t networkBuffer[1024];

static uint32_t get_time_ms(void)
{
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

static int32_t transport_send(NetworkContext_t *pNetworkContext, const void *pBuffer, size_t bytesToSend)
{
    int ret = send(pNetworkContext->socket_fd, pBuffer, bytesToSend, 0);
    if (ret < 0)
    {
        return -1;
    }
    return ret;
}

static int32_t transport_recv(NetworkContext_t *pNetworkContext, void *pBuffer, size_t bytesToRecv)
{
    int ret = recv(pNetworkContext->socket_fd, pBuffer, bytesToRecv, 0);

    if (ret < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return 0;
        }
        return -1;
    }
    return ret;
}

// MQTT EVENT CALLBACK
static void mqtt_event_callback(MQTTContext_t *pMQTTContext, MQTTPacketInfo_t *pPacketInfo, MQTTDeserializedInfo_t *pDeserializedInfo)
{
    if (pPacketInfo->type == MQTT_PACKET_TYPE_PUBLISH)
    {
        MQTTPublishInfo_t *pPublishInfo = pDeserializedInfo->pPublishInfo;

        char topicName[128];
        char payloadStr[256];

        snprintf(topicName, sizeof(topicName), "%.*s", pPublishInfo->topicNameLength, pPublishInfo->pTopicName);
        snprintf(payloadStr, sizeof(payloadStr), "%.*s", (int)pPublishInfo->payloadLength, (const char *)pPublishInfo->pPayload);

        ESP_LOGI("MQTT_RECV", "Data from Topic [%s]: %s", topicName, payloadStr);
    }
    else
    {
        ESP_LOGD("MQTT_RECV", "Received MQTT packet type: %d", pPacketInfo->type);
    }
}

static void mqtt_publisher_task(void *pvParameters)
{
    while (1)
    {
        // send packet every 5 seconds
        vTaskDelay(pdMS_TO_TICKS(5000));

        if (isMqttConnected)
        {
            int temperature = (esp_random() % 21) + 20;

            char payloadString[64];
            snprintf(payloadString, sizeof(payloadString), "{\r\n  \"temperature\": %d\r\n}", temperature);

            MQTTPublishInfo_t publishInfo = {0};
            publishInfo.qos = MQTTQoS0;
            publishInfo.pTopicName = PUB_TOPIC;
            publishInfo.topicNameLength = strlen(publishInfo.pTopicName);
            publishInfo.pPayload = payloadString;
            publishInfo.payloadLength = strlen(payloadString);

            if (xSemaphoreTake(mqttMutex, pdMS_TO_TICKS(1000)) == pdTRUE)
            {
                // QoS 0
                MQTTStatus_t status = MQTT_Publish(&mqttContext, &publishInfo, 0);

                if (status == MQTTSuccess)
                {
                    ESP_LOGI("MQTT_PUB", "Successfully sent to %s: %s", publishInfo.pTopicName, payloadString);
                }
                else
                {
                    ESP_LOGE("MQTT_PUB", "Publish failed! Status: %s", status);
                }
                xSemaphoreGive(mqttMutex);
            }
            else
            {
                ESP_LOGW("MQTT_PUB", "Could not acquire Mutex. Skipping this publish cycle.");
            }
        }
    }
}

static void mqtt_listener_task(void *pvParameters)
{
    while (1)
    {
        isMqttConnected = false;

        // Resolve DNS
        ESP_LOGI("MQTT_SUB", "Resolving DNS for %s...", BROKER_HOSTNAME);
        struct hostent *hp = gethostbyname(BROKER_HOSTNAME);
        if (!hp)
        {
            ESP_LOGE("MQTT_SUB", "DNS Lookup failed!");
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        struct sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(BROKER_PORT);
        server_addr.sin_addr.s_addr = ((struct in_addr *)(hp->h_addr))->s_addr;

        // open TCP Socket
        networkContext.socket_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (networkContext.socket_fd < 0)
        {
            ESP_LOGE("MQTT_SUB", "Failed to allocate socket!");
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        // set socket receive timeout
        struct timeval timeout = {.tv_sec = 0, .tv_usec = 500000};
        setsockopt(networkContext.socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

        if (connect(networkContext.socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0)
        {
            ESP_LOGE("MQTT_SUB", "TCP Connection failed!");
            close(networkContext.socket_fd);
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }
        ESP_LOGI("MQTT_SUB", "TCP Socket successfully opened!");

        // Initialize coreMQTT Context
        TransportInterface_t transport = {
            .pNetworkContext = &networkContext,
            .send = transport_send,
            .recv = transport_recv};

        MQTTFixedBuffer_t fixedBuffer = {
            .pBuffer = networkBuffer,
            .size = sizeof(networkBuffer)};

        MQTT_Init(&mqttContext, &transport, get_time_ms, mqtt_event_callback, &fixedBuffer);

        //  MQTT CONNECT Packet
        MQTTConnectInfo_t connectInfo = {0};
        connectInfo.cleanSession = true;
        connectInfo.keepAliveSeconds = 60;

        char clientId[32];
        snprintf(clientId, sizeof(clientId), "ESP32_Device_%d", (uint16_t)esp_random());
        connectInfo.pClientIdentifier = clientId;
        connectInfo.clientIdentifierLength = strlen(connectInfo.pClientIdentifier);

        bool sessionPresent = false;
        if (MQTT_Connect(&mqttContext, &connectInfo, NULL, 5000, &sessionPresent) != MQTTSuccess)
        {
            ESP_LOGE("MQTT_SUB", "MQTT connection rejected by broker!");
            close(networkContext.socket_fd);
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }
        ESP_LOGI("MQTT_SUB", "Connected to HiveMQ Broker!");

        // Subscribe to Topic "HuyVan/#"
        MQTTSubscribeInfo_t subInfo = {0};
        subInfo.qos = MQTTQoS0;
        subInfo.pTopicFilter = SUB_TOPIC;
        subInfo.topicFilterLength = strlen(subInfo.pTopicFilter);

        if (MQTT_Subscribe(&mqttContext, &subInfo, 1, 1) != MQTTSuccess)
        {
            ESP_LOGE("MQTT_SUB", "Failed to subscribe to %s!", SUB_TOPIC);
        }
        else
        {
            ESP_LOGI("MQTT_SUB", "Successfully subscribed to: %s", SUB_TOPIC);
        }

        isMqttConnected = true;

        while (1)
        {
            if (xSemaphoreTake(mqttMutex, portMAX_DELAY) == pdTRUE)
            {
                MQTTStatus_t mqttStatus = MQTT_ProcessLoop(&mqttContext);

                xSemaphoreGive(mqttMutex);

                if (mqttStatus != MQTTSuccess)
                {
                    ESP_LOGE("MQTT_SUB", "Connection lost! Status: %s", mqttStatus);
                    break;
                }
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        isMqttConnected = false;
        close(networkContext.socket_fd);
        ESP_LOGW("MQTT_SUB", "Disconnected. Reconnecting in 5 seconds...");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void mqtt_app_init(void)
{
    mqttMutex = xSemaphoreCreateMutex();
    if (mqttMutex == NULL)
    {
        ESP_LOGE(TAG, "Failed to create MQTT Mutex!");
        return;
    }
    xTaskCreate(mqtt_publisher_task, "mqtt_publisher", 1024 * 4, NULL, 4, NULL);
    xTaskCreate(mqtt_listener_task, "mqtt_listener", 1024 * 6, NULL, 5, NULL);
}