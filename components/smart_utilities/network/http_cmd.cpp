#include "string.h"
#include <stdio.h>
#include <stdarg.h>
#include "http_cmd.h"
#include "esp_log.h"
//#include <config.h>
#include "smart_utility.h"


static const char *TAG = "HTTP_CMD";
QueueHandle_t xQueuePlayAudioCmd = NULL;

char HTTPCmd::m_host[64];
unsigned int HTTPCmd::m_port = 0;
bool HTTPCmd::m_connectFlag = false;

HTTPCmd::HTTPCmd(const char *name, const char *url, int waitTime)
{
    memset(&m_client_config, 0, sizeof(m_client_config));
    m_client_config.url = url;
    m_client_config.timeout_ms = waitTime; //network timeout in ms
    m_hHttpClient = esp_http_client_init(&m_client_config);
    strncpy(m_appName, name, sizeof(m_appName));
    m_serverUrlChanged = false;
}

HTTPCmd::HTTPCmd(const char *host, int port, bool bWait)
{
    if(host!=NULL && port != 0){
        strcpy(m_host, host);
        m_port = port;
    }else{
        host = m_host;
        port = m_port;
    }
    m_pLogBuffer = NULL;    //only allocated log buffer when it is used at first time
    memset(&m_client_config, 0, sizeof(m_client_config));
    m_client_config.host = host;
    m_client_config.port = port; 
    m_client_config.path = "/cmd";
    m_hHttpClient = esp_http_client_init(&m_client_config);

    //wait for network ready
    int maxRetry = 1;
    if(bWait) maxRetry = 30;
    for(int i=0; i<maxRetry; i++)
    {
        if(this->ping()==0) {
            ESP_LOGI(TAG, "Successfully pinged HTTP Server.");
            m_connectFlag = true;
            break;
        }else{
            ESP_LOGI(TAG, "Failed to ping HTTP Server. Retrying...\n");
        }
        if(bWait) {
            vTaskDelay(1000/portTICK_PERIOD_MS);
        }
    }
    if(m_connectFlag==false){
        ESP_LOGI(TAG, "Not be able to connect to HTTP Server.");
        //HTTPCmd::play("~/mp3/unable_connect.mp3");
    }else{
        ESP_LOGI(TAG, "Successfully initialized HTTP Client.");
    }
    m_serverUrlChanged = false;
}

HTTPCmd::~HTTPCmd()
{
    esp_err_t rc = ESP_OK;
    rc = esp_http_client_cleanup(m_hHttpClient);
    if(rc!=ESP_OK) {
        ESP_LOGE(TAG, "esp_http_client_cleanup returned error code %d.\n", (rc));
    }
    m_hHttpClient = NULL;
}

bool HTTPCmd::isConnected()
{
    return m_connectFlag;
}


int HTTPCmd::ping()
{
    esp_err_t rc = ESP_OK;
    esp_http_client_set_url(m_hHttpClient, "/cmd/ping");
    esp_http_client_set_method(m_hHttpClient, HTTP_METHOD_GET);
    rc = esp_http_client_perform(m_hHttpClient);
    if(rc!=ESP_OK) {
        ESP_LOGE(TAG, "esp_http_client_perform(/cmd/ping) returned error(%d): %s", rc, esp_err_to_name(rc));
        esp_http_client_close(m_hHttpClient);
        m_connectFlag = false;
        return -1;
    }
    m_connectFlag = true;
    int code = esp_http_client_get_status_code(m_hHttpClient);
    if(code<400) {
        return 0;
    }else{
        ESP_LOGE(TAG, "HTTP server returned error(%d).", code);
        return -1;
    }
}

int HTTPCmd::log(int level, const char *msg, ...)
{
    if(m_pLogBuffer==NULL) {
        m_pLogBuffer = (char*)malloc(512);
        if(m_pLogBuffer==NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for log buffer.");
            return -1;
        }
    }
    int len = 0;
    va_list args;
    va_start (args, msg);
    len = vsprintf (m_pLogBuffer, msg, args);
    va_end (args);
    m_pLogBuffer[len] = '\0';

    esp_err_t rc = ESP_OK;
    char url[32];
    if(level==HTTPCmd::ERROR){
        sprintf(url, "/cmd/log?level=%s","ERROR");
        ESP_LOGE(TAG, "%s", m_pLogBuffer);
    }else if(level==HTTPCmd::WARN){
        sprintf(url, "/cmd/log?level=%s","WARN");
        ESP_LOGW(TAG, "%s", m_pLogBuffer);
    }else{
        sprintf(url, "/cmd/log?level=%s","INFO");
        ESP_LOGI(TAG, "%s", m_pLogBuffer);
    }
    esp_http_client_set_url(m_hHttpClient, url);
    esp_http_client_set_method(m_hHttpClient, HTTP_METHOD_POST);
    esp_http_client_set_post_field(m_hHttpClient, m_pLogBuffer, len);
    rc = esp_http_client_perform(m_hHttpClient);
    if(rc!=ESP_OK) {
        ESP_LOGE(TAG, "esp_http_client_perform(/cmd/log) returned error(%d): %s", rc, esp_err_to_name(rc));
        m_connectFlag = false;
        ping();
        return -1;
    }
    m_connectFlag = true;
    int code = esp_http_client_get_status_code(m_hHttpClient);
    if(code==200) {
        return 0;
    }else{
        ESP_LOGE(TAG, "HTTP server returned error(%d).", code);
        return -1;
    }
}

int HTTPCmd::open(const char *url, int len)
{
    int rc = 0;
    esp_http_client_set_url(m_hHttpClient, url);
    esp_http_client_set_method(m_hHttpClient, HTTP_METHOD_GET);
    esp_http_client_set_header(m_hHttpClient, "Accept", "*/*");
    esp_http_client_set_header(m_hHttpClient, "Content-Type", "application/octet-stream");
    esp_http_client_open(m_hHttpClient, len);
    rc = esp_http_client_fetch_headers(m_hHttpClient);
    if(rc<=0){
        ESP_LOGE(TAG, "HTTPCmd::open: Failed to fetch header for %s, error code = %d.", url, rc);
        m_connectFlag = false;
        return -1;
    }
    m_connectFlag = true;
    rc = esp_http_client_get_status_code(m_hHttpClient);
    if(rc!=200){
        ESP_LOGE(TAG, "HTTPCmd::open: Failed to open %s, HTTP Code = %d.", url, rc);
        return -1 * rc;
    }
    m_dataToReadLen = esp_http_client_get_content_length(m_hHttpClient);
    ESP_LOGI(TAG, "HTTPCmd::open: content length = %d.", m_dataToReadLen);
    return m_dataToReadLen;
}

int HTTPCmd::read(uint8_t *buffer, int len)
{
    if(m_dataToReadLen<=0) return 0;
    int n = len > m_dataToReadLen ? m_dataToReadLen : len;
    n = esp_http_client_read(m_hHttpClient, (char*)buffer, n);
    m_dataToReadLen -= n;
    return n;
}

int HTTPCmd::close()
{
    m_dataToReadLen = 0;
    return esp_http_client_close(m_hHttpClient);
}

int HTTPCmd::getNewImageBuildVersion(const char *name, uint8_t *ver, size_t len)
{
    int rc = 0;
    char url[128];
    sprintf(url, "/upgrade/version?name=%s", name);
    rc = this->open(url, 0);
    if(rc<0) {
        return -1;
    }
    rc = this->read(ver, len);
    if(rc<=0) {
        this->close();
        return -2;
    }
    ver[rc] = '\0';

    this->close();
    return 0;
}


static HTTPCmd* g_http_server = NULL;

void log_server_notify_change()
{
    if(g_http_server==NULL) {
        ESP_LOGE(TAG, "Call log_server_notify before log_server_init.");
        return;
    }
    g_http_server->m_serverUrlChanged = true;
}

