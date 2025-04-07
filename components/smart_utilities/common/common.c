#include "smart_utility.h"
#include "esp_app_desc.h"

const char* get_image_version()
{
    const esp_app_desc_t *pApp = esp_app_get_description();
    return pApp->version;
}

