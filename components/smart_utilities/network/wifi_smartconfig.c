#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_wpa2.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "esp_netif.h"
#include "lwip/inet.h"
#include "lwip/lwip_napt.h"
#include "lwip/opt.h"
#include "lwip/sys.h"
#include "apps/dhcpserver/dhcpserver.h"
#include "esp_smartconfig.h"
#include "nvs_flash.h"
#include "smart_utility.h"

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
static const int WIFI_CONNECTED_BIT = BIT0;
static const int ESPTOUCH_DONE_BIT = BIT1;
static const int WIFI_FAIL_BIT = BIT2;
static const int SMARTCONFIG_TASK_DONE_BIT = BIT3;

static const char *TAG = "WIFI_SMARTCONFIG";
// extern device_data_tree_t device_data_tree;

static void smartconfig_example_task(void *parm);

#define CONFIG_ESP_WIFI_AUTH_OPEN 1
#if CONFIG_ESP_WIFI_AUTH_OPEN
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN
#elif CONFIG_ESP_WIFI_AUTH_WEP
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WEP
#elif CONFIG_ESP_WIFI_AUTH_WPA_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WAPI_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WAPI_PSK
#endif

#define ESP32_AP_SSID "SmartGarden-GW"
#define ESP32_AP_PASS "mfdl,1234"

static char wifi_ssid[WIFI_CRED_LEN];
static char wifi_password[WIFI_CRED_LEN];
static int needSmartConfigFlag = 1;
static void nat_set_dhcps_options();

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        if (needSmartConfigFlag)
        {
            xTaskCreate(smartconfig_example_task, "smartconfig_example_task", 4096, NULL, 3, NULL);
        }
        else
        {
            esp_wifi_connect();
        }
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        // if it was an AUTH error, we need to run air kiss again.
        wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *)event_data;
        if (event->reason == WIFI_REASON_NOT_AUTHED ||
            event->reason == WIFI_REASON_AUTH_FAIL)
        {
            ESP_LOGW(TAG, "WIFI auth failed.");
        }
        ESP_LOGI(TAG, "retry to connect to the AP (%d)", event->reason);
        esp_wifi_connect();
        // xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        // sprintf(device_data_tree.local_ip, IPSTR, IP2STR(&event->ip_info.ip));
        save_wifi_cred(wifi_ssid, wifi_password);
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
    else if (event_base == SC_EVENT && event_id == SC_EVENT_SCAN_DONE)
    {
        ESP_LOGI(TAG, "Scan done");
    }
    else if (event_base == SC_EVENT && event_id == SC_EVENT_FOUND_CHANNEL)
    {
        ESP_LOGI(TAG, "Found channel");
    }
    else if (event_base == SC_EVENT && event_id == SC_EVENT_GOT_SSID_PSWD)
    {
        ESP_LOGI(TAG, "Got SSID and password");

        smartconfig_event_got_ssid_pswd_t *evt = (smartconfig_event_got_ssid_pswd_t *)event_data;
        wifi_config_t wifi_config;
        // uint8_t ssid[33] = { 0 };
        // uint8_t password[65] = { 0 };
        uint8_t rvd_data[33] = {0};

        bzero(&wifi_config, sizeof(wifi_config_t));
        memcpy(wifi_config.sta.ssid, evt->ssid, sizeof(wifi_config.sta.ssid));
        memcpy(wifi_config.sta.password, evt->password, sizeof(wifi_config.sta.password));
        wifi_config.sta.bssid_set = evt->bssid_set;
        if (wifi_config.sta.bssid_set == true)
        {
            memcpy(wifi_config.sta.bssid, evt->bssid, sizeof(wifi_config.sta.bssid));
        }

        memcpy(wifi_ssid, evt->ssid, sizeof(evt->ssid));
        memcpy(wifi_password, evt->password, sizeof(evt->password));
        ESP_LOGI(TAG, "SSID:%s", wifi_ssid);
        ESP_LOGI(TAG, "PASSWORD:%s", wifi_password);
        if (evt->type == SC_TYPE_ESPTOUCH_V2)
        {
            ESP_ERROR_CHECK(esp_smartconfig_get_rvd_data(rvd_data, sizeof(rvd_data)));
            ESP_LOGI(TAG, "RVD_DATA:");
            for (int i = 0; i < 33; i++)
            {
                printf("%02x ", rvd_data[i]);
            }
            printf("\n");
        }

        ESP_ERROR_CHECK(esp_wifi_disconnect());
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        esp_wifi_connect();
    }
    else if (event_base == SC_EVENT && event_id == SC_EVENT_SEND_ACK_DONE)
    {
        xEventGroupSetBits(s_wifi_event_group, ESPTOUCH_DONE_BIT);
    }
}

void wifi_init(bool is_nat_server)
{
    int rc = ESP_OK;
    if(get_master_role()==false) {  //for salve, use fixed cred
        save_wifi_cred(ESP32_AP_SSID, ESP32_AP_PASS);
    }
    rc = read_wifi_cred(wifi_ssid, wifi_password);
    if (rc != ESP_OK)
    {
        wifi_ssid[0] = '\0';
        wifi_password[0] = '\0';
        needSmartConfigFlag = 1;
    }
    else
    {
        needSmartConfigFlag = 0;
        ESP_LOGI(TAG, "Trying to connect to SSID %s with password %s...", wifi_ssid, wifi_password);
    }

    ESP_ERROR_CHECK(esp_netif_init());
    if (s_wifi_event_group == NULL)
    {
        s_wifi_event_group = xEventGroupCreate();
    }

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    if (is_nat_server == true)
    {
        esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
        assert(ap_netif);
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    esp_event_handler_instance_t instance_sc_any_id;
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, &instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_register(SC_EVENT, ESP_EVENT_ANY_ID, &event_handler, &instance_sc_any_id));
    if (is_nat_server == true)
    {
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    }
    else
    {
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    }
    wifi_config_t wifi_config = {
        .sta = {
            /* Setting a password implies station will connect to all security modes including WEP/WPA.
             * However these modes are deprecated and not advisable to be used. Incase your Access point
             * doesn't support WPA2, these mode can be enabled by commenting below line */
            .threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
            .failure_retry_cnt = 2,
        },
    };
    if (strlen(wifi_ssid) > 0)
    {

        strncpy((char *)wifi_config.sta.ssid, wifi_ssid, sizeof(wifi_config.sta.ssid) - 1);
        strncpy((char *)wifi_config.sta.password, wifi_password, sizeof(wifi_config.sta.password) - 1);
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    }

///////////////////////////////////////////////////////////////////////////////////////
// Code for AP mode (NAT server)
    if (is_nat_server == true)
    {
        /* ESP AP CONFIG */
        wifi_config_t ap_config = {
            .ap = {
                .ssid = ESP32_AP_SSID,
                .channel = 0,
#ifdef CONFIG_ESP_WIFI_SOFTAP_SAE_SUPPORT
                .authmode = WIFI_AUTH_WPA3_PSK,
                .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
#else /* CONFIG_ESP_WIFI_SOFTAP_SAE_SUPPORT */
                .authmode = WIFI_AUTH_WPA2_PSK,
#endif
                .pmf_cfg = {
                        .required = true,
                },
                .password = ESP32_AP_PASS,
                .ssid_hidden = 0,
                .max_connection = 10,
                .beacon_interval = 100}};
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &ap_config));
    }
    ///////////////////////////////////////////////////////////////////////////////////////

    ESP_ERROR_CHECK(esp_wifi_start());

    int bitsToWait = WIFI_CONNECTED_BIT | WIFI_FAIL_BIT;
    if (needSmartConfigFlag)
    {
        bitsToWait = bitsToWait | SMARTCONFIG_TASK_DONE_BIT;
    }
    while (1)
    {
        /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
         * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
        EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                               bitsToWait,
                                               pdTRUE,
                                               pdFALSE,
                                               portMAX_DELAY);

        /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
         * happened. */
        if (bits & WIFI_CONNECTED_BIT)
        {
            ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                     wifi_ssid, wifi_password);
            if (!needSmartConfigFlag)
                break;
        }
        if (bits & WIFI_FAIL_BIT)
        {
            ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                     wifi_ssid, wifi_password);
            break;
        }
        if (bits & SMARTCONFIG_TASK_DONE_BIT)
        {
            ESP_LOGI(TAG, "Smart Config task has finished.");
            break;
        }
    }
    /* The event will not be processed after unregister */
    esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_sc_any_id);
    rc = esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip);
    if (rc != ESP_OK)
        ESP_LOGW(TAG, "Failed to unregister IP_EVENT_STA_GOT_IP: %d.", rc);
    esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id);
    // vEventGroupDelete(s_wifi_event_group); //intended not delete s_wifi_event_group because it will cause issue in other threads
    
    if(is_nat_server == true){
        nat_set_dhcps_options();
#if IP_NAPT
        //Set to ip address of softAP netif (Default is 192.168.4.1)
        u32_t napt_netif_ip = 0xC0A80401;
        ip_napt_enable(htonl(napt_netif_ip), 1);
#endif

    }
    
    ESP_LOGI(TAG, "wifi_init_sta finished.");
}

static void smartconfig_example_task(void *parm)
{
    ESP_LOGI(TAG, "smartconfig_task is started.");
    EventBits_t uxBits;
    ESP_ERROR_CHECK(esp_smartconfig_set_type(SC_TYPE_ESPTOUCH_AIRKISS));
    smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_smartconfig_start(&cfg));
    const TickType_t xTicksToWait = 1000 / portTICK_PERIOD_MS;
    while (1)
    {
        // turn_on_led();
        ESP_LOGI(TAG, "smartconfig_task waiting wifi config information...");
        uxBits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | ESPTOUCH_DONE_BIT, true, false, xTicksToWait);
        if (uxBits & WIFI_CONNECTED_BIT)
        {
            ESP_LOGI(TAG, "WiFi Connected to ap");
        }
        if (uxBits & ESPTOUCH_DONE_BIT)
        {
            ESP_LOGI(TAG, "smartconfig over");
            esp_smartconfig_stop();
            xEventGroupSetBits(s_wifi_event_group, SMARTCONFIG_TASK_DONE_BIT);
            vTaskDelete(NULL);
        }
        // turn_off_led();
    }
}

void nat_set_dhcps_options()
{
    esp_netif_t *netif = NULL;
    esp_netif_dns_info_t dnsinfo;
    netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_get_dns_info(netif, ESP_NETIF_DNS_MAIN, &dnsinfo);
    ESP_LOGI(TAG, "DNS IP:" IPSTR, IP2STR(&dnsinfo.ip.u_addr.ip4));

    netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    esp_netif_dhcps_stop(netif);

    // ESP_NETIF_IP_ADDRESS_LEASE_TIME, DHCP Option 51, 设置分发的 IP 地址有效时间
    uint32_t dhcps_lease_time = 60*24*30; // 单位是分钟
    ESP_ERROR_CHECK(esp_netif_dhcps_option(netif,ESP_NETIF_OP_SET,ESP_NETIF_IP_ADDRESS_LEASE_TIME,&dhcps_lease_time,sizeof(dhcps_lease_time)));


    esp_netif_dns_info_t dns_info;
    dns_info.ip.u_addr = dnsinfo.ip.u_addr;
    esp_netif_set_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns_info);
 
    uint8_t dns_offer = 1; // 传入 1 使修改的 DNS 生效，如果是 0，那么用 softap 的 gw ip 作为 DNS server (默认是 0)
    ESP_ERROR_CHECK(esp_netif_dhcps_option(netif,ESP_NETIF_OP_SET,ESP_NETIF_DOMAIN_NAME_SERVER,&dns_offer,sizeof(dns_offer)));

    // ESP_NETIF_ROUTER_SOLICITATION_ADDRESS, DHCP Option 3 Router, 传入 0 使 DHCP Option 3(Router) 不出现（默认为 1）
    //uint8_t router_enable = 0;
    //ESP_ERROR_CHECK(esp_netif_dhcps_option(netif,ESP_NETIF_OP_SET,ESP_NETIF_ROUTER_SOLICITATION_ADDRESS,&router_enable, sizeof(router_enable)));

    esp_netif_dhcps_start(netif);
}


bool check_network_reset_button(int gpio)
{
    int rc = ESP_OK;
    gpio_set_direction(gpio, GPIO_MODE_INPUT);
    rc = gpio_get_level(gpio);
    if(rc==0) return false;

    for(int i=0; i<5; i++)
    {
        vTaskDelay(pdMS_TO_TICKS(1000)); //sleep 1s
        rc = gpio_get_level(gpio);
        if(rc==0) return false;
    }
    ESP_LOGW(TAG, "Reseting WIFI network...");
    save_wifi_cred("", "");
    return true;
}


