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
static const char *TAG = "LOG_CLIENT_C";

typedef struct _udp_log_client_{
    int sock;
    struct sockaddr_in server_addr;
    char tx_buffer[1024];
    char name[32];
}udp_log_client_t;
udp_log_client_t g_udp_log_client;
int log_client_verify_server(const char *ip, int port);
int log_client_discover_server();
int log_client_init(const char *name)
{
    int rc = 0;
    int addr_family = AF_INET;
    int ip_protocol = IPPROTO_UDP;
    char *local_server_addr = NULL;
    int port = 0;
    char url[128];
    strncpy(g_udp_log_client.name, name, sizeof(g_udp_log_client.name)-1);
    g_udp_log_client.sock = -1;

    //try default local server addr first
    local_server_addr = DEFAULT_LOCAL_SERVER;
    port = UDP_PORT;
    rc = log_client_verify_server(local_server_addr, port);
    if(rc==ESP_OK) {
        sprintf(url, "http://%s:%d", local_server_addr, port);
        save_local_server_url(url);
        goto CREATE_LOG_CLIENT_SOCK;
    }else{
        ESP_LOGW(TAG, "Failed to verify server %s:%d.", local_server_addr, port);
    }

    //read local server info from NVS
READ_SVR_FROM_NVS:
    rc = read_local_server_url(url, sizeof(url));
    if(rc!=ESP_OK || strlen(url)==0) {
        ESP_LOGW(TAG, "Failed to read local server url from NVS DB");
        return rc;
    }
    char *prefix = "http://";
    if(strstr(url, prefix)==NULL) return -1;
    int len = strlen(prefix);
    local_server_addr = url + len;
    char *p = strstr(local_server_addr, ":");
    if(p==NULL) return -1;
    *p = '\0';
    p++;
    port = atoi(p);

    rc = log_client_verify_server(local_server_addr, port);
    if(rc!=ESP_OK) {
        ESP_LOGW(TAG, "Failed to verify server %s:%d.", local_server_addr, port);
        if(get_master_role()) { //only try to discover local server if device is running in master role
            rc = log_client_discover_server();
            if(rc==ESP_OK) goto READ_SVR_FROM_NVS;
        }
    }

CREATE_LOG_CLIENT_SOCK:
    //create socket for log client
    int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        return ESP_FAIL;
    }

    g_udp_log_client.sock = sock;
    g_udp_log_client.server_addr.sin_family = AF_INET;
    g_udp_log_client.server_addr.sin_port = htons(port);
    inet_aton(local_server_addr, &g_udp_log_client.server_addr.sin_addr);

    UDP_LOGI(TAG, "**************************************************");
    UDP_LOGI(TAG, "Device is starting...");
    UDP_LOGI(TAG, "Image version is %s.", get_image_version());
    if(get_master_role()){
        UDP_LOGI(TAG, "Device is running in MASTER role.");
    }else{
        UDP_LOGI(TAG, "Device is running in SLAVE role.");
    }
    UDP_LOGI(TAG, "**************************************************");
    return 0;
}

bool log_client_send(const char *log_level, const char *tag, const char *msg, ...)
{
    int len = 0;
    int rc = 0;
    char *pBuffer = g_udp_log_client.tx_buffer;
    if(g_udp_log_client.sock==-1) { //skip UDP LOG Send
        return true;
    }
    sprintf(pBuffer, "%c%s[%s][%s]", (unsigned char)TF_SAVE_LOG, log_level, g_udp_log_client.name, tag);
    len = strlen(pBuffer);
    pBuffer = pBuffer + len;

    va_list args;
    va_start (args, msg);
    len = vsprintf (pBuffer, msg, args);
    va_end (args);
    pBuffer[len] = '\0';
    len = strlen(g_udp_log_client.tx_buffer);

    //ESP_LOGW(TAG, "Sending log to %s...", inet_ntoa(g_udp_log_client.server_addr.sin_addr));
    socklen_t socklen = sizeof(g_udp_log_client.server_addr);
    rc = sendto(g_udp_log_client.sock, g_udp_log_client.tx_buffer, len, 0, 
                    (struct sockaddr *)&g_udp_log_client.server_addr, socklen);
    if(rc<=0) {
        ESP_LOGW(TAG, "Something wrong.");
    }
    return true;
}

int log_client_verify_server(const char *ip, int port)
{
    int rc = 0;
    int addr_family = AF_INET;
    int ip_protocol = IPPROTO_UDP;
    char buff[8] = {TF_PING};

    ESP_LOGW(TAG, "Trying to verify %s:%d...", ip, port);

    //create socket
    int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        return ESP_FAIL;
    }
    //set timeout
    struct timeval tv = {2, 0};
    tv.tv_sec = 5;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(struct timeval));

    //build address
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_aton(ip, &server_addr.sin_addr);
    socklen_t socklen = sizeof(server_addr);

    for(int i = 0; i < 3; i++) {    //try 3 times
        buff[0] = TF_PING;
        socklen = sizeof(server_addr);
        rc = sendto(sock, buff, 1, 0, 
                        (struct sockaddr *)&server_addr, socklen);
        if(rc<=0) {
            ESP_LOGW(TAG, "Something wrong.");
        }

        buff[0] = '\0';
        struct sockaddr_in source_addr; 
        socklen = sizeof(struct sockaddr_in);
        rc = recvfrom(sock, buff, sizeof(buff) - 1, 0, (struct sockaddr *)&source_addr, &socklen);
        if(rc<0) {
            ESP_LOGW(TAG, "Failed to verify local server rc = %d, errno=%d.", rc, errno);
        }else{
            return ESP_OK;
        }
    }
    return ESP_FAIL;
}

int log_client_discover_server()
{
    int rc = 0;
    int found = 0;
    char buff[128] = {0};
    int addr_family = AF_INET;
    int ip_protocol = IPPROTO_UDP;
    struct sockaddr_in source_addr; 
    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(UDP_PORT);

    ESP_LOGW(TAG, "Trying to discover local server ....");
    //create socket
    int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        return ESP_FAIL;
    }

    struct timeval tv = {1, 0};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(struct timeval));

    esp_netif_ip_info_t ip_info; 
    esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &ip_info);
    char *ip = (char*)(&ip_info.ip.addr);
    uint32_t skip_idx = ip[3];
    dest_addr.sin_addr.s_addr = ip_info.ip.addr;

    for(int i = 254; i>1; i--) {
        if(i==skip_idx) continue;
        buff[0] = TF_DISCOVER_LOCAL_SERVER;
        socklen_t socklen = sizeof(dest_addr);
        ((char*)(&(dest_addr.sin_addr.s_addr)))[3] = (u_char)i;
        ESP_LOGW(TAG, "Sending discover command to %s...", inet_ntoa(dest_addr.sin_addr.s_addr));
        rc = sendto(sock, buff, 1, 0, 
                        (struct sockaddr *)&dest_addr, socklen);
        if(rc<=0) {
            ESP_LOGW(TAG, "Something wrong.");
        }

        buff[0] = '\0';
        socklen = sizeof(source_addr);
        rc = recvfrom(sock, buff, sizeof(buff) - 1, 0, (struct sockaddr *)&source_addr, &socklen);
        if(rc>0 && buff[0]==TF_DISCOVER_LOCAL_SERVER) {
            ESP_LOGI(TAG, "Found local server: %s.", inet_ntoa(source_addr.sin_addr.s_addr));
            found = 1;
            break;
        }
    }
    if(found){
        tf_save_local_server_url(&buff[1], rc - 1);
        return ESP_OK;
    }else{
        ESP_LOGW(TAG, "Not found local server.");
    }
    return ESP_FAIL;
}

