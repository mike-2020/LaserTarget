#pragma once
#include "string.h"
#include "esp_http_client.h"

class HTTPCmd
{
private:
    static char m_host[64];
    static unsigned int m_port;
    esp_http_client_config_t m_client_config;
    esp_http_client_handle_t m_hHttpClient;
    char *m_pLogBuffer;
    size_t m_dataToReadLen;
    static bool m_connectFlag;
public:
    char m_appName[32];
    bool m_serverUrlChanged;
private:

public:
    enum{
        INFO,
        WARN,
        ERROR
    }LogLevel;
    HTTPCmd(const char *host, int port, bool bWait=false);
    HTTPCmd(const char *name, const char *url, int waitTime=1000);
    ~HTTPCmd();
    int ping();
    int log(int level, const char *msg, ...);

    int open(const char *url, int len=0);
    int read(uint8_t *buffer, int len);
    int close();

    int getNewImageBuildVersion(const char *name, uint8_t *ver, size_t len);
    static bool isConnected();
    esp_http_client_handle_t getClient() { return m_hHttpClient; };
};

