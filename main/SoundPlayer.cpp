#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "sdkconfig.h"
//#include "audio_common.h"
//#include "http_stream.h"
#include "i2s_stream.h"
#include "mp3_decoder.h"
#include "fatfs_stream.h"
#include "SoundPlayer.h"
#include "board.h"
#include "esp_audio.h"
#include "audio_mem.h"
#include "audio_idf_version.h"
#include "driver/i2s_types.h"
#include "esp_vfs_fat.h"
#include "ASRCOmmandProc.h"
#include "PowerMonitor.h"


static SoundPlayer g_soundPlayer;

static const char *TAG = "SOUND_PLAYER";
static esp_audio_handle_t g_esp_audio_handle;
audio_board_handle_t board_handle = NULL;
static audio_element_handle_t i2s_stream_writer, mp3_decoder;

int mount_filesystem()
{
    int rc = 0;

    ESP_LOGI(TAG, "[ 1 ] Mount fatfs");
    esp_vfs_fat_mount_config_t fatfs_cfg = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .disk_status_check_enable = false
    };
 
    rc = esp_vfs_fat_spiflash_mount_ro("/mp3", "mp3", &fatfs_cfg);
    if(rc!=ESP_OK) ESP_LOGE(TAG, "Failed to mount mp3 partition: %d.", rc);

    return 0;
}

void *audio_setup(void *cb, void *ctx)
{
    if(board_handle==NULL) {
        board_handle = audio_board_init();
        audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_DECODE, AUDIO_HAL_CTRL_START);
    }
    audio_err_t rc = ESP_ERR_AUDIO_NO_ERROR;

    esp_audio_cfg_t cfg = DEFAULT_ESP_AUDIO_CONFIG();
    cfg.vol_handle = board_handle->audio_hal;
    cfg.vol_set = (audio_volume_set)audio_hal_set_volume;
    cfg.vol_get = (audio_volume_get)audio_hal_get_volume;
    cfg.resample_rate = 44100;
    cfg.prefer_type = ESP_AUDIO_PREFER_MEM;
    cfg.cb_func = (esp_audio_event_callback)cb;
    cfg.cb_ctx = ctx;
    esp_audio_handle_t handle = esp_audio_create(&cfg);

    //create fatfs reader
    fatfs_stream_cfg_t fatfs_cfg = FATFS_STREAM_CFG_DEFAULT();
    fatfs_cfg.type = AUDIO_STREAM_READER;
    audio_element_handle_t fs2_stream_reader = fatfs_stream_init(&fatfs_cfg);
    rc = esp_audio_input_stream_add(handle, fs2_stream_reader);
    if(rc != ESP_ERR_AUDIO_NO_ERROR){
        ESP_LOGE(TAG, "Failed to add fatfs input stream (%d).", rc);
    }

    // Add decoders and encoders to esp_audio
    mp3_decoder_cfg_t mp3_dec_cfg = DEFAULT_MP3_DECODER_CONFIG();
    mp3_dec_cfg.task_core = 1;
    mp3_decoder = mp3_decoder_init(&mp3_dec_cfg);
    esp_audio_codec_lib_add(handle, AUDIO_CODEC_TYPE_DECODER, mp3_decoder);

    // Create writers and add to esp_audio
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    //i2s_cfg.i2s_port = I2S_NUM_0;
    i2s_cfg.type = AUDIO_STREAM_WRITER;
    //i2s_cfg.transmit_mode = I2S_COMM_MODE_STD;
    i2s_cfg.std_cfg.clk_cfg.sample_rate_hz = 44100;
    //i2s_cfg.std_cfg.slot_cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
    //i2s_writer.need_expand = (I2S_BITS_PER_SAMPLE_16BIT != I2S_BITS_PER_SAMPLE_16BIT);
    //i2s_cfg.std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
    //i2s_cfg.volume = 0;
    i2s_cfg.use_alc = true;
    i2s_stream_writer = i2s_stream_init(&i2s_cfg);
    esp_audio_output_stream_add(handle, i2s_stream_writer);

    // Set default volume
    //esp_audio_vol_set(handle, 10);

    //rsp_filter_cfg_t rsp_config = DEFAULT_RESAMPLE_FILTER_CONFIG();
    //rsp_config.dest_ch = 1;
    //rsp_config.dest_rate = 44100;
    //audio_element_handle_t rsp_processor = rsp_filter_init(&rsp_config);
    
    AUDIO_MEM_SHOW(TAG);
    ESP_LOGI(TAG, "esp_audio instance is:%p", handle);
    return handle;
}

void http_audio_destroy()
{
    ESP_LOGE(TAG, "Destroying HTTP stream handle...");

    esp_err_t err = ESP_FAIL;
    if (g_esp_audio_handle == NULL) {
        return;
    }

    err = esp_audio_stop(g_esp_audio_handle, TERMINATION_TYPE_NOW);
    err = esp_audio_destroy((esp_audio_handle_t)g_esp_audio_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_audio_destroy Failed");
        return;
    } else {
        g_esp_audio_handle = NULL;
    }
}

int SoundPlayer::stop()
{
    return esp_audio_stop(g_esp_audio_handle, TERMINATION_TYPE_NOW);
}

bool SoundPlayer::isPlaying()
{
    esp_audio_state_t state;
    audio_err_t rc;
    rc = esp_audio_state_get(g_esp_audio_handle, &state);
    if(rc == ESP_ERR_AUDIO_NO_ERROR && state.status==AUDIO_STATUS_RUNNING){
        return true;
    }else{
        return false;
    }
}

void SoundPlayer::proc(void *ctx)
{
    SoundPlayer *handle = (SoundPlayer*)ctx;
    voice_cmd_t cmd;

    ESP_LOGI(TAG, "Sound player task has been started.");
    while(1) {
        if (xQueueReceive(handle->m_cmdQueue, &cmd, pdMS_TO_TICKS(1000))!=pdTRUE) continue;
        ESP_LOGI(TAG, "Received voice command %s.", cmd.name);
        handle->_play(cmd.name, (cmd.voice_type==VOICE_TYPE_SPEACH));
    }
}

int SoundPlayer::init()
{   
    gpio_set_direction(GPIO_SPK_MUTE, GPIO_MODE_OUTPUT);    //this is required, even it will be initialized in PowerMonitor. 
    gpio_set_level(GPIO_SPK_MUTE, 0);                       //this is to play the WELCOME voice

    mount_filesystem();
    g_esp_audio_handle = audio_setup(NULL, NULL);

    m_nVolume = SOUND_VOLUME_MID;

    m_cmdQueue = xQueueCreate(2, sizeof(voice_cmd_t));
    xTaskCreate(SoundPlayer::proc, "sound_player_task", 1024*8, (void*)this, 10, NULL);

    return 0;
}

int SoundPlayer::_play(char *name, bool bIsSpeach)
{
    int rc = 0;
    const char *path = "file://mp3/";
    char uri[32];
    char *p = name;
    bool bExitLoop = false;
    
    esp_audio_media_type_set((esp_audio_handle_t)g_esp_audio_handle, MEDIA_SRC_TYPE_MUSIC_FLASH);
    i2s_alc_volume_set(i2s_stream_writer, m_nVolume);
    ESP_LOGI(TAG, "SoundPlayer::_play: name = %s.", name);

    PowerMonitor::getInstance()->powerOnSpeaker();
    if(bIsSpeach==true) ASRCOmmandProc::sendCmd(CMD_TYPE_CTL, CTL_CMD_DN_ASR);

    int i = 0;
    while(true) {
        if(name[i]==':' || name[i]=='\0') {
            if(name[i]=='\0') bExitLoop = true;
            else name[i]='\0';
            sprintf(uri, "%s/%s.mp3", path, p);
            rc = esp_audio_sync_play((esp_audio_handle_t)g_esp_audio_handle/*, AUDIO_CODEC_TYPE_DECODER*/, uri, 0);
            ESP_LOGW(TAG, "esp_audio_play return %d.", rc);
            if(bExitLoop==true) break;
            i++;
            p = &name[i];
        }
        i++;
    }

    if(bIsSpeach==true) ASRCOmmandProc::sendCmd(CMD_TYPE_CTL, CTL_CMD_EN_ASR);
    PowerMonitor::getInstance()->powerOffSpeaker();

    return rc;
}

int SoundPlayer::play(const char *name, uint8_t nHighPri, bool bIsSpeach, uint16_t nWaitTime)
{
    if(nHighPri > SOUND_PLAY_PRI_NORMAL) {
        xQueueReset(m_cmdQueue);
    }
    if(nHighPri > SOUND_PLAY_PRI_HIGH_1) {
        stop();
    }

    voice_cmd_t cmd;
    cmd.voice_type = bIsSpeach==false ? VOICE_TYPE_SOUND : VOICE_TYPE_SPEACH;

    strcpy(cmd.name, name);
    BaseType_t rc = xQueueSend(m_cmdQueue, &cmd, pdMS_TO_TICKS(nWaitTime));
    if(rc!=pdTRUE) {
        ESP_LOGE(TAG, "Failed to send sound play command: %s.", name);
        return ESP_FAIL;
    }
    return ESP_OK;
}

int SoundPlayer::playSpeach(uint16_t cmd_id)
{
    char name[32];
    sprintf(name, "asr/%u", cmd_id); 
    play(name, SOUND_PLAY_PRI_NORMAL, true);
    return 0;
}

int SoundPlayer::playSpeach(const char *cmd_str, uint16_t nWaitTime)
{
    char name[32];
    sprintf(name, "asr/%s", cmd_str);
    play(name, SOUND_PLAY_PRI_NORMAL, true, nWaitTime);
    return 0;
}

int SoundPlayer::buildNum(char *buff, int num)
{
    uint8_t len = 0;
    if(num==100) {
        sprintf(buff, ":asr/EB-NUMMX");
        len += 13;
        return len;
    }

    if(num/10 > 1) {
        sprintf(buff, ":asr/EB-NUM%d", num/10);
        len += 12;
    }

    if(num/10 >= 1) {
        sprintf(buff+len, ":asr/EB-NUM10");
        len += 13;
    }
    
    if(num%10 != 0 || num==0) {
        sprintf(buff+len, ":asr/EB-NUM%d", num%10);
        len += 12;
    }
    
    return len;
}

int SoundPlayer::playSpeachWithNum(const char *name1, int num, const char *name2)
{
    voice_cmd_t cmd;
    uint8_t len = 0;
    BaseType_t rc = ESP_OK;

    cmd.voice_type = VOICE_TYPE_SPEACH;
    cmd.name[0] = '\0';

    sprintf(cmd.name, "asr/%s", name1);
    len = strlen(cmd.name);

    rc = buildNum(cmd.name+len, num);
    if(rc == ESP_FAIL) return rc;
    len += rc;

    if(name2!=NULL) {
        sprintf(cmd.name+len, ":asr/%s", name2);        
    }

    ESP_LOGI(TAG, "playSpeachWithNum: name = %s", cmd.name);

    rc = xQueueSend(m_cmdQueue, &cmd, pdMS_TO_TICKS(5000));
    if(rc!=pdTRUE) {
        ESP_LOGE(TAG, "Failed to send sound play command: %s.", cmd.name);
        return ESP_FAIL;
    }
    return ESP_OK;
}


SoundPlayer* SoundPlayer::getInstance()
{
    return &g_soundPlayer;
}