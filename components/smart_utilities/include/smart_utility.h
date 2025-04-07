#pragma once
#include <stddef.h>
#include <stdbool.h>

#define WIFI_CRED_LEN   64
#define LOG_INFO 0
#define LOG_WARN 1
#define LOG_ERROR  2

#define POWER_SAVE_NONE_MODE 0
#define POWER_SAVE_MAX_MODE 1
#define POWER_SAVE_MIN_MODE 2

#ifdef __cplusplus 
extern "C" {
#endif
void wifi_init(bool is_nat_server);
bool check_network_reset_button(int gpio);

void ota_check_and_update(const char *name);

int save_local_server_url(const char *url);
int read_local_server_url(char *url, size_t len);
int read_wifi_cred(char *ssid, char *password);
int save_wifi_cred(const char *ssid, const char *password);
const char* get_image_version();

//int log_server_init(const char *name);
//int log_server_send(int level, const char *msg, ...);
void log_server_notify_change();

int enable_power_save_mode(int mode);
int enter_deep_sleep(int deep_sleep_sec);

void start_sntp_service();

/////////////////////////////////////////////////////////////////////
#define DEFAULT_LOCAL_SERVER        "192.168.3.205"

#define TF_PUSH_LOCAL_SERVER_URL    1
#define TF_DECLARE_MASTER           2
#define TF_SAVE_SOIL_HUMIDITY       3
#define TF_SAVE_WATER_LEVEL         4
#define TF_SAVE_LOG                 5
#define TF_PING                     6
#define TF_DISCOVER_LOCAL_SERVER    7
#define TF_SEND_SOLAR_POWER_DATA    8
typedef void (*task_func_t)(const char *msg, int len);

int config_discovery_init();
int udp_send_cmd_to_master(const char cmd, const char *data, int len);
int register_task_func(unsigned int cmd, task_func_t tf);
void tf_save_local_server_url(const char *msg, int len);
bool get_master_role();
void set_master_role(bool mr);
/////////////////////////////////////////////////////////////////////

int log_client_init(const char *name);
bool log_client_send(const char *log_level, const char *tag, const char *msg, ...);
#define UDP_LOGE(TAG, MSG, ...) {log_client_send("E", TAG, MSG, ##__VA_ARGS__);ESP_LOGE(TAG, MSG, ##__VA_ARGS__);}
#define UDP_LOGW(TAG, MSG, ...) {log_client_send("W", TAG, MSG, ##__VA_ARGS__);ESP_LOGW(TAG, MSG, ##__VA_ARGS__);}
#define UDP_LOGI(TAG, MSG, ...) {log_client_send("I", TAG, MSG, ##__VA_ARGS__);ESP_LOGI(TAG, MSG, ##__VA_ARGS__);}

#ifdef __cplusplus 
}
#endif

