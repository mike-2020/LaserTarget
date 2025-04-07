#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include <sys/time.h>
#include "esp_log.h"
#include <esp_netif.h>
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#include "smart_utility.h"

#define UDP_PORT 8000
#define MAX_TASK_FUNC 64


static const char *TAG = "CONFIG_DISCOVERY_C";
static task_func_t g_task_func_map[MAX_TASK_FUNC];
static char g_master_addr[32];
static int g_master_port = 0;
static bool g_is_master = false;
int udp_broadcast_send(int sock, const char *data, int len);

void tf_save_local_server_url(const char *msg, int len)
{
    int rc = 0;
    char url[128]={0};
    if(strstr(msg, "http://")!=msg) {
        ESP_LOGW(TAG, "Invalid payload: %s.", msg);
        return;
    }
    rc = read_local_server_url(url, sizeof(url)-1);
    if(rc==ESP_OK && strcasecmp(url, msg)==0){
        ESP_LOGW(TAG, "The same value (%s) as the one in NVS DB.", url);
        return;
    }
    save_local_server_url(msg);
    ESP_LOGW(TAG, "Save %s to NVS DB.", msg);
    //log_server_notify_change();
}

static void tf_save_master_info(const char *msg, int len)
{
    char buffer[64]={0};
    strncpy(buffer, msg, sizeof(buffer)-1);

    char *p = strstr(buffer, ":");
    if(p==NULL || p==buffer) return;
    *p = '\0';
    char *addr = buffer;
    int port = atoi(p+1);
    if(strcmp(addr, g_master_addr)==0 && g_master_port==port) {
        ESP_LOGW(TAG, "Same value %s:%d, no action.", addr, port);
        return;
    }

    strncpy(g_master_addr, addr, sizeof(g_master_addr)-1);
    g_master_port = port;
}

static void perform_task(char *data, int len)
{
    char cmd = data[0];
    char *msg = &data[1];
    if(cmd > MAX_TASK_FUNC-1 || g_task_func_map[(uint32_t)cmd]==NULL) {
        ESP_LOGW(TAG, "Unknown command %d.", cmd);
        return;
    }
    g_task_func_map[(uint32_t)cmd](msg, len-1);
}

static void udp_listen_task(void *pvParameters)
{
    char rx_buffer[128];
    char addr_str[64];

    ESP_LOGW(TAG, "Configuration Dsicovery Service is starting...");

    while (1) {

        struct sockaddr_in dest_addr;
        dest_addr.sin_addr.s_addr = INADDR_ANY;
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(UDP_PORT);
        int addr_family = AF_INET;
        int ip_protocol = IPPROTO_UDP;

        int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            break;
        }

        int err = bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err < 0) {
            ESP_LOGE(TAG, "Socket unable to bind: err %d", err);
        }

        while (1) {
            int len = 0;
            struct sockaddr_in6 source_addr; 
            socklen_t socklen = sizeof(source_addr);
            len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);

            //Error occurred during receiving
            if (len < 0) {
                ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
                break;
            }
            // Data received
            else {

                // Get the sender's ip address as string
                if (source_addr.sin6_family == PF_INET) {
                    inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr.s_addr, addr_str, sizeof(addr_str) - 1);
                } else if (source_addr.sin6_family == PF_INET6) {
                    inet6_ntoa_r(source_addr.sin6_addr, addr_str, sizeof(addr_str) - 1);
                }

                rx_buffer[len] = 0; // Null-terminate whatever we received and treat like a string
                ESP_LOGI(TAG, "Received %d bytes from %s.", len, addr_str);
                ESP_LOGI(TAG, "%s", rx_buffer);
                perform_task(rx_buffer, len);

            }
        }

        if (sock != -1) {
            UDP_LOGE(TAG, "udp_listen_task: Shutting down socket and restarting...");
            shutdown(sock, 0);
            close(sock);
        }
    }
    vTaskDelete(NULL);
}
#if 0
static void master_declare_task(void *)
{
    int rc = 0;
    int addr_family = AF_INET;
    int ip_protocol = IPPROTO_UDP;

    int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        return;
    }

    esp_netif_ip_info_t ip_info; 
    esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &ip_info);
    char data[64];
    data[0] = (unsigned char)TF_DECLARE_MASTER;
    sprintf(&data[1], "" IPSTR ":%d", IP2STR(&ip_info.ip), UDP_PORT);
    int len = strlen(&data[1]) + 1 + 1;
    while (1)
    {
        udp_broadcast_send(sock, data, len);
        vTaskDelay(5 * 60 * 1000 / portTICK_PERIOD_MS);
    }
    close(sock);
}
#endif
int config_discovery_init()
{
    g_task_func_map[TF_PUSH_LOCAL_SERVER_URL] = tf_save_local_server_url;
    g_task_func_map[TF_DECLARE_MASTER] = tf_save_master_info;

    if(g_is_master==false) {
        //for slave role, set default master IP address
        esp_netif_ip_info_t ip_info; 
        esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &ip_info);
        esp_ip4addr_ntoa(&ip_info.gw, g_master_addr, sizeof(g_master_addr));
        g_master_port = UDP_PORT;

        ESP_LOGI(TAG, "Device is running in slave mode.");
        ESP_LOGI(TAG, "Master node address: %s:%d.", g_master_addr, g_master_port);
    }
    xTaskCreate(udp_listen_task, "udp_listen_task", 4096, NULL, 3, NULL);

    return 0;
}

bool get_master_role()
{
    return g_is_master;
}

void set_master_role(bool mr)
{
    g_is_master = mr;
}

int udp_broadcast_send(int sock, const char *data, int len)
{
    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(UDP_PORT);

    esp_netif_ip_info_t ip_info; 
    esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &ip_info);
    char *ip = (char*)(&ip_info.ip.addr);
    uint32_t skip_idx = ip[3];
    ESP_LOGW(TAG, "Local IP address: " IPSTR ".", IP2STR(&ip_info.ip));
    ESP_LOGW(TAG, "Sending data to all potential clients in this network");
    dest_addr.sin_addr.s_addr = ip_info.ip.addr;
    for(int i=1; i<255; i++)
    {
        if(i==skip_idx)continue;
        ((char*)(&(dest_addr.sin_addr.s_addr)))[3] = (u_char)i;
        //ESP_LOGW(TAG, "Sending data to %s...", inet_ntoa(dest_addr.sin_addr.s_addr));
        socklen_t socklen = sizeof(dest_addr);
        len = sendto(sock, data, len, 0, (struct sockaddr *)&dest_addr, socklen);
    }

    return ESP_OK;
}

int udp_send_cmd_to_master(const char cmd, const char *data, int len)
{
    int rc = 0;
    int addr_family = AF_INET;
    int ip_protocol = IPPROTO_UDP;

    int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
    if (sock < 0) {
        UDP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        return ESP_FAIL;
    }
    char *tx_buffer = malloc(len+1);
    if(tx_buffer==NULL) {
        close(sock);
        return ESP_FAIL;
    }
    tx_buffer[0] = cmd;
    memcpy(&tx_buffer[1], data, len);
    len++;

    //boradcast send
    if(g_master_port==0){
        UDP_LOGI(TAG, "Send command %d over broadcast.", (int)cmd);
        rc = udp_broadcast_send(sock, tx_buffer, len);
        close(sock);
        return rc;
    }

    //P2P send
    UDP_LOGI(TAG, "Send command %d to master %s:%d.", (int)cmd, g_master_addr, g_master_port);
    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(g_master_port);
    inet_aton(g_master_addr, &dest_addr.sin_addr);
    socklen_t socklen = sizeof(dest_addr);
    rc = sendto(sock, tx_buffer, len, 0, (struct sockaddr *)&dest_addr, socklen);
    if(rc<=0) {
        ESP_LOGW(TAG, "Something wrong.");
    }
    close(sock);
    return 0;
}

int register_task_func(unsigned int cmd, task_func_t tf)
{
    g_task_func_map[cmd] = tf;
    return 0;
}

