#include "esp_log.h"
#include "esp_random.h"
#include "LedStrip.h"
#include "TargetServo.h"
#include "LaserReceiver_IS0803.h"
#include "SegDisplay.h"
#include "SoundPlayer.h"
#include "EpicBase.h"
#include "EpicMgr.h"
#include "ASRCOmmandProc.h"
#include "nvs_flash.h"
#include "esp_timer.h"
#include "AutoTurret.h"

#define ENCOURAGE_WORDS_COUNT   5
#define NVS_NS_GAME "app_game"

const char *encorageWords[] = {"EA-1", "EA-2", "EA-3", "EA-4", "EA-5"};
static const char *TAG = "EPIC_BASE";

EpicBase::EpicBase()
{
    m_bIsRunning = false;
    m_bStopFlag  = false;
    m_nWordsIdx  = 0;
    m_taskHandle = NULL;

    //m_cmdQueue = xQueueCreate(3, sizeof(epic_cmd_t));
}

EpicBase::~EpicBase()
{
    stop();
}

int EpicBase::stop()
{
    m_bStopFlag = true;
    while(m_bIsRunning!=false) vTaskDelay(20 / portTICK_PERIOD_MS);
    if(m_taskHandle!=NULL) {
        vTaskDelete(m_taskHandle);
        m_taskHandle = NULL;
    }
    return ESP_OK;
}

void EpicBase::playEncourageWords()
{
    SoundPlayer *sndPlayer = SoundPlayer::getInstance();
    sndPlayer->playSpeach(encorageWords[m_nWordsIdx]);
    m_nWordsIdx++;
    if(m_nWordsIdx >= ENCOURAGE_WORDS_COUNT) m_nWordsIdx = 0;
}

int EpicBase::readNVSPair(const char *key, char *val, size_t len)
{
    int rc = ESP_OK;
    nvs_handle_t handle;
    rc = nvs_open(NVS_NS_GAME, NVS_READONLY, &handle);
    if(rc!=ESP_OK) {
        ESP_LOGE(TAG, "Failed to open namespace %s from NVS. (%d)", NVS_NS_GAME, rc);
        return ESP_FAIL;
    }

    rc = nvs_get_str(handle, key, val, &len); 
    if(rc!=ESP_OK) {
        ESP_LOGE(TAG, "Failed to read %s from NVS. (0x%x)", key, rc);
        nvs_close(handle);
        return ESP_FAIL;
    }

    nvs_close(handle);
    return ESP_OK;
}

int EpicBase::readNVSPair(const char *key)
{
    char val[8];
    int rc = 0;
    rc = readNVSPair(key, val, sizeof(val));
    if(rc!=ESP_OK) return ESP_FAIL;
    return atoi(val);
}

int EpicBase::saveNVSPair(const char *key, const char *val)
{
    int rc = ESP_OK;
    nvs_handle_t handle;
    rc = nvs_open(NVS_NS_GAME, NVS_READWRITE, &handle);
    if(rc!=ESP_OK) {
        ESP_LOGE(TAG, "Failed to open namespace %s from NVS. (0x%x)", NVS_NS_GAME, rc);
        return ESP_FAIL;
    }
    rc = nvs_set_str(handle, key, val);
    if(rc!=ESP_OK) {
        ESP_LOGE(TAG, "Failed to save key %s to NVS. (0x%x)", key, rc); 
        nvs_close(handle);
        return ESP_FAIL;
    }
    rc = nvs_commit(handle);
    if(rc!=ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit namespace %s from NVS. (0x%x)", NVS_NS_GAME, rc);
        nvs_close(handle);
        return ESP_FAIL;
    }
    nvs_close(handle);
    return ESP_OK;
}

int EpicBase::saveNVSPair(const char *key, int val)
{
    char str[8];
    sprintf(str, "%d", val);
    return saveNVSPair(key, str);
}


///////////////////////////////////////////////////////////////////////////////////////////


EpicSingleShot::EpicSingleShot():EpicBase()
{
    m_nEpicID = EPIC_SINGLESHOT;
}

void EpicSingleShot::proc(void *ctx)
{
    LR_IS0803 *laserRecv = LR_IS0803::getInstance();
    SoundPlayer *sndPlayer = SoundPlayer::getInstance();
    SegDisplay *segDisplay = SegDisplay::getInstance();
    TargetServo *tgtServo = TargetServo::getInstance();
    LEDStrip *ledStrip = LEDStrip::getInstance();

    ESP_LOGI(TAG, "EpicSingleShot task has been started.");
    EpicSingleShot *handle = (EpicSingleShot*)ctx;

    int shotCount = 0;
    handle->m_bIsRunning = true;
    laserRecv->resetQueue();
    ledStrip->sendCmd(LED_CMD_OFF_ALL);
    segDisplay->displayNum(0);
    while (handle->m_bStopFlag==false) {
        if (laserRecv->getCmd(200)>0) {
            ESP_LOGI(TAG, "Received shoot trigger.");

            shotCount++;
            if(shotCount>=100) shotCount = 1;
        
            segDisplay->displayNum(shotCount); //show number on LED display
            sndPlayer->play(SE_GUN_M1, SOUND_PLAY_PRI_HIGH_2);        //play gun shoot sound
            handle->playEncourageWords();
            ledStrip->sendCmd(LED_CMD_BLINK_RANDOM);   //blink LED strip

            tgtServo->setDown();               //put down target
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            tgtServo->setUp();                 //put up target
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }

    handle->m_bIsRunning = false;
    ESP_LOGI(TAG, "EpicSingleShot exiting...");
    vTaskDelete(NULL);
}

void EpicSingleShot::start()
{
    if(m_bIsRunning==false) {
        m_bIsRunning = true;
        xTaskCreate(this->proc, "epic_task", 1024*3, (void*)this, 10, NULL);
    }
}


/////////////////////////////////////////////////////////////////////////////////////////


EpicDoubleShot::EpicDoubleShot():EpicBase()
{
    m_nEpicID = EPIC_DOUBLESHOT;
}


void EpicDoubleShot::proc(void *ctx)
{
    LR_IS0803 *laserRecv = LR_IS0803::getInstance();
    SoundPlayer *sndPlayer = SoundPlayer::getInstance();
    SegDisplay *segDisplay = SegDisplay::getInstance();
    TargetServo *tgtServo = TargetServo::getInstance();
    LEDStrip *ledStrip = LEDStrip::getInstance();

    ESP_LOGI(TAG, "EpicDoubleShot task has been started.");
    EpicDoubleShot *handle = (EpicDoubleShot*)ctx;

    int shotCount = 0;
    handle->m_bIsRunning = true;
    laserRecv->resetQueue();
    ledStrip->sendCmd(LED_CMD_ON_ALL);
    segDisplay->displayNum(0);
    while (handle->m_bStopFlag==false) {
        if (laserRecv->getCmd(200)>0) {
            ESP_LOGI(TAG, "Received shoot trigger.");

            shotCount++;
            if(shotCount==100) shotCount = 1;
            segDisplay->displayNum(shotCount); //show number on LED display

            sndPlayer->play(SE_GUN_M1, SOUND_PLAY_PRI_HIGH_2);        //play gun shoot sound

            if(shotCount%2 ==0) {
                handle->playEncourageWords();
                ledStrip->sendCmd(LED_CMD_BLINK_RANDOM);   //blink LED strip
                tgtServo->setDown();               //pull down target
                vTaskDelay(1500 / portTICK_PERIOD_MS);
                tgtServo->setUp();                 //pull up target
                ledStrip->sendCmd(LED_CMD_ON_ALL);
            }else{
                ledStrip->sendCmd(LED_CMD_OFF_HALF);   //turn off half of all LEDs
            }
        } else {

        }
    }

    handle->m_bIsRunning = false;
    tgtServo->setUp();
    tgtServo->stop();
    vTaskDelete(NULL);
}

void EpicDoubleShot::start()
{
    if(m_bIsRunning==false) {
        m_bIsRunning = true;
        xTaskCreate(this->proc, "epic_task", 1024*3, (void*)this, 10, NULL);
    }
}


/////////////////////////////////////////////////////////////////////////////////////////


EpicEightShot::EpicEightShot():EpicBase()
{
    m_nEpicID = EPIC_EIGHTSHOT;
}


void EpicEightShot::proc(void *ctx)
{
    LR_IS0803 *laserRecv = LR_IS0803::getInstance();
    SoundPlayer *sndPlayer = SoundPlayer::getInstance();
    SegDisplay *segDisplay = SegDisplay::getInstance();
    //TargetServo *tgtServo = TargetServo::getInstance();
    LEDStrip *ledStrip = LEDStrip::getInstance();

    ESP_LOGI(TAG, "EpicEightShot task has been started.");
    EpicEightShot *handle = (EpicEightShot*)ctx;

    int shootCount = 0;
    int64_t nStartTime = esp_timer_get_time();
    handle->m_bIsRunning = true;
    laserRecv->resetQueue();
    segDisplay->displayNum(0);
    ledStrip->sendCmd(LED_CMD_OFF_ALL);
    sndPlayer->playSpeach(VOICE_8SHOOT_START); 
    sndPlayer->playSpeach(VOICE_COUNT_BACK);
    vTaskDelay(8500 / portTICK_PERIOD_MS);
    ledStrip->sendCmd(LED_CMD_ON_ALL);

    while (handle->m_bStopFlag==false && shootCount<8) {
        if (laserRecv->getCmd(1000)>0) {   
            shootCount++;
            sndPlayer->play(SE_GUN_M1);        //play gun shoot sound
            ledStrip->sendCmd(LED_CMD_OFF_ONE_MORE);
        }
        int nTime = (esp_timer_get_time() - nStartTime)/(1000*1000);
        if(nTime>99) nTime = 99;
        segDisplay->displayNum(nTime);
    }

    int nTime = (esp_timer_get_time() - nStartTime)/(1000*1000);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    sndPlayer->play(VOICE_SUCCESS_2, SOUND_PLAY_PRI_HIGH_1);  //true means overwrite any existing voice in the queue
    segDisplay->blinkNum(nTime);
    ledStrip->sendCmd(LED_CMD_BLINK_RANDOM);
    laserRecv->resetQueue();
    //tgtServo->setUp();
    sndPlayer->playSpeachWithNum("EB-8STOK", nTime, "EB-UNIT1"); 
    handle->checkHistoryRecord(nTime);

    ESP_LOGI(TAG, "EpicEightShot exiting...");
    handle->m_bIsRunning = false;
    handle->m_bStopFlag  = true;
    vTaskDelete(NULL);

}

void EpicEightShot::checkHistoryRecord(int nTime)
{
    int rc = 0;
    const char *key = "game.8shoot.top"; //NVS_KEY_NAME_MAX_SIZE=16
    rc = readNVSPair(key);
    ESP_LOGI(TAG, "History record: %ds.", rc);
    if(rc<0 || (rc>=0 && rc > nTime)) {      //no existing record, or there is existing records, but it is slower than the current one
        SoundPlayer::getInstance()->playSpeach("EB-NFREC", 5000);
        rc = saveNVSPair(key, nTime);
        if(rc!=ESP_OK) ESP_LOGE(TAG, "Failed to save history record.");
    }else if(nTime > rc){ //there is existing record, faster than the current one
        SoundPlayer::getInstance()->playSpeachWithNum("EB-STREC", nTime-rc, "EB-UNIT1");
    }
}


void EpicEightShot::start()
{
    if(m_bIsRunning==false) {
        m_bIsRunning = true;
        xTaskCreate(this->proc, "epic_task", 1024*4, (void*)this, 10, NULL);
    }
}



/////////////////////////////////////////////////////////////////////////////////////////


EpicTimeLimit::EpicTimeLimit(uint8_t tm):EpicBase()
{
    m_nEpicID = EPIC_TMLIMIT;
    m_nInterval = tm;
}


void EpicTimeLimit::proc(void *ctx)
{
    LR_IS0803 *laserRecv = LR_IS0803::getInstance();
    SoundPlayer *sndPlayer = SoundPlayer::getInstance();
    SegDisplay *segDisplay = SegDisplay::getInstance();
    //TargetServo *tgtServo = TargetServo::getInstance();
    LEDStrip *ledStrip = LEDStrip::getInstance();

    ESP_LOGI(TAG, "EpicTimeLimit task has been started.");
    EpicTimeLimit *handle = (EpicTimeLimit*)ctx;

    int shotCount = 0;
    int tmRemain = 15;
    handle->m_bIsRunning = true;
    ledStrip->sendCmd(LED_CMD_OFF_ALL);
    segDisplay->displayNum(tmRemain);
    laserRecv->resetQueue();

    sndPlayer->playSpeach(VOICE_TMLIMIT_START); 
    sndPlayer->playSpeach(VOICE_COUNT_BACK);
    vTaskDelay(6000 / portTICK_PERIOD_MS);

    while (handle->m_bStopFlag==false) {
        if (laserRecv->getCmd(1000)>0) {                    //wait 1s, make use of this to count remaining time
            ESP_LOGI(TAG, "Received shoot trigger.");

            shotCount++;
            if(shotCount==100) shotCount = 1;

            sndPlayer->play(SE_GUN_M1);        //play gun shoot sound

            if(shotCount%2 ==0) {
                ledStrip->sendCmd(LED_CMD_ON_ONE_MORE); 
            }
        } 

        tmRemain--;
        segDisplay->displayNum(tmRemain); //show time in second
        if(tmRemain<=0) break;
    }

    sndPlayer->play(VOICE_SUCCESS_1, SOUND_PLAY_PRI_HIGH_2);  //true means overwrite any existing voice in the queue
    segDisplay->blinkNum(shotCount);
    laserRecv->resetQueue();

    sndPlayer->playSpeachWithNum("EB-TLOK", shotCount, "EB-UNIT2");
    handle->checkHistoryRecord(shotCount);

    ESP_LOGI(TAG, "EpicTimeLimit exiting...");
    handle->m_bIsRunning = false;
    handle->m_bStopFlag  = true;
    vTaskDelete(NULL);
}

void EpicTimeLimit::checkHistoryRecord(int shotCount)
{
    int rc = 0;
    const char *key = "game.timelt.top"; //NVS_KEY_NAME_MAX_SIZE=16
    rc = readNVSPair(key);
    ESP_LOGI(TAG, "History record: %ds.", rc);
    if(rc<0 || (rc>=0 && rc < shotCount)) {      //no existing record, or there is existing records, but it is slower than the current one
        SoundPlayer::getInstance()->playSpeach("EB-NFREC", 5000);
        rc = saveNVSPair(key, shotCount);
        if(rc!=ESP_OK) ESP_LOGE(TAG, "Failed to save history record.");
    }else if(rc > shotCount){ //there is existing record, faster than the current one
        SoundPlayer::getInstance()->playSpeachWithNum("EB-TLREC", rc-shotCount, "EB-UNIT2");
    }
}

void EpicTimeLimit::start()
{
    if(m_bIsRunning==false) {
        m_bIsRunning = true;
        xTaskCreate(this->proc, "epic_task", 1024*3, (void*)this, 10, NULL);
    }
}


/////////////////////////////////////////////////////////////////////////////////////////

#define EPIC_RANDOM_CASE_NUM    10

EpicRandomShoot::EpicRandomShoot():EpicBase()
{
    m_nEpicID = EPIC_RANDOMSHOOT;
}

void EpicRandomShoot::start()
{
    if(m_bIsRunning==false) {
        m_bIsRunning = true;
        xTaskCreate(this->proc, "epic_task", 1024*3, (void*)this, 10, NULL);
    }
}

void EpicRandomShoot::proc(void *ctx)
{
    LR_IS0803 *laserRecv = LR_IS0803::getInstance();
    SoundPlayer *sndPlayer = SoundPlayer::getInstance();
    SegDisplay *segDisplay = SegDisplay::getInstance();
    TargetServo *tgtServo = TargetServo::getInstance();
    LEDStrip *ledStrip = LEDStrip::getInstance();

    ESP_LOGI(TAG, "EpicRandomShoot task has been started.");
    EpicRandomShoot *handle = (EpicRandomShoot*)ctx;

    
    handle->m_bIsRunning = true;
    ledStrip->sendCmd(LED_CMD_OFF_ALL);
    segDisplay->displayNum(0);
    laserRecv->resetQueue();
    ledStrip->stop();

    sndPlayer->playSpeach(VOICE_RNDSHOOT_START); 
    sndPlayer->playSpeach(VOICE_COUNT_BACK);
    vTaskDelay(9000 / portTICK_PERIOD_MS);

    int shotCount = 0;
    uint32_t tmCount = 0;
    bool bInShootPeriod = false;
    uint8_t caseSlot[EPIC_RANDOM_CASE_NUM] = {0};
    while (handle->m_bStopFlag==false && tmCount < 10 * 6 * EPIC_RANDOM_CASE_NUM) {

        handle->makeRandomCase(tmCount++, &bInShootPeriod);

        if (laserRecv->getCmd(100)>0) {   
            uint8_t n = tmCount/60; 
            if(bInShootPeriod==true){
                if(caseSlot[n]==0) {
                    caseSlot[n] = 1;
                    sndPlayer->play(SE_GUN_M1, true);        //play gun shoot sound
                    shotCount++;
                    segDisplay->displayNum(shotCount);
                }
            }else{
                if(caseSlot[n]==0) {
                    caseSlot[n] = 2;
                    sndPlayer->playSpeach(VOICE_SHOOT_WRONG);        //play gun shoot sound
                }
            }
            tmCount = (n+1) * 60;  //trigger new case
        }

    }

    int wrongCount = 0;
    int missCount = 0;
    shotCount = 0;
    for(int i=0; i< EPIC_RANDOM_CASE_NUM; i++) {
        if(caseSlot[i]==1) shotCount++;
        else if(caseSlot[i]==2) { wrongCount++; missCount++; }
        else missCount++;
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    sndPlayer->play(VOICE_SUCCESS_1, SOUND_PLAY_PRI_HIGH_1);  //true means overwrite any existing voice in the queue
    segDisplay->blinkNum(shotCount);
    laserRecv->resetQueue();
    tgtServo->setUp();
    ledStrip->stop();
    sndPlayer->playSpeachWithNum("EB-SUHT", shotCount, "EB-UNIT2");
    sndPlayer->playSpeachWithNum("EB-WRHT", wrongCount, "EB-UNIT2");
    sndPlayer->playSpeachWithNum("EB-UNHT", missCount, "EB-UNIT2");
    sndPlayer->playSpeachWithNum("EB-SUPT", (shotCount * 100)/(shotCount+missCount), NULL);

    ESP_LOGI(TAG, "EpicRandomShoot exiting...");
    tgtServo->stop();
    handle->m_bIsRunning = false;
    handle->m_bStopFlag  = true;
    vTaskDelete(NULL);
}

//make a case every 6 seconds
void EpicRandomShoot::makeRandomCase(uint32_t nTime, bool *pInRecvShootPeriod)
{
    TargetServo *tgtServo = TargetServo::getInstance();
    LEDStrip *ledStrip = LEDStrip::getInstance();

    static bool bTargetDown = false;
    static bool bRecvShoot  = false;
    static uint16_t nRecvShootStartTime = 0;
    static uint16_t nTargetUpStartTime  = 0;
    static uint16_t nRecvShootLastTime = 0;

    if(nTime%60==0) {   //begining of a case, decide if we want to put down the target
        bTargetDown = false;
        bRecvShoot  = false;
        *pInRecvShootPeriod = false;
        nRecvShootLastTime  = 0;
        nRecvShootStartTime = esp_random()%40;
        if(nRecvShootStartTime<10) nRecvShootStartTime += 10;  //start no ealier than 1s
        nTargetUpStartTime  = esp_random()%40;
        if(nTargetUpStartTime<10) nTargetUpStartTime += 10;    //start no ealier than 1s

        if(esp_random()%2==0){
            bTargetDown = true;
            tgtServo->setDown();
        }
        ledStrip->stop();
        ESP_LOGI(TAG, "EpicRandomShoot case start. nRecvShootStartTime=%u, nTargetUpStartTime=%u", nRecvShootStartTime, nTargetUpStartTime);
    }

    if(nTime%60==nRecvShootStartTime) { //decide if we want to receive shoot
        bRecvShoot = true;
        ledStrip->sendCmd(LED_CMD_BLINK_FLOW);
    }

    if(nTime%60==nTargetUpStartTime && bTargetDown == true) { //if target was put down, it is time to pull it up
        bTargetDown = false;
        tgtServo->setUp();
    }

    if(bRecvShoot==true && bTargetDown==false) {    //count lasting time when target is in UP state
        nRecvShootLastTime++;  
        *pInRecvShootPeriod = true;
    }
    if(bRecvShoot==true && nRecvShootLastTime > 20) {       //shoot receiving lasts for 2s
        bRecvShoot = false;
        nRecvShootLastTime = 0;
        *pInRecvShootPeriod = false;
        ledStrip->stop();

        ESP_LOGI(TAG, "Stop shoot receiving state.");
    }
}


/////////////////////////////////////////////////////////////////////////////////////////
EpicAdvAttack::EpicAdvAttack():EpicBase()
{
    m_nEpicID = EPIC_ADVATTACK;
}

void EpicAdvAttack::start()
{
    if(m_bIsRunning==false) {
        m_bIsRunning = true;
        xTaskCreate(this->proc, "epic_task", 1024*3, (void*)this, 10, NULL);
    }
}

void EpicAdvAttack::proc(void *ctx)
{
    LR_IS0803 *laserRecv = LR_IS0803::getInstance();
    SoundPlayer *sndPlayer = SoundPlayer::getInstance();
    SegDisplay *segDisplay = SegDisplay::getInstance();
    //TargetServo *tgtServo = TargetServo::getInstance();
    LEDStrip *ledStrip = LEDStrip::getInstance();

    ESP_LOGI(TAG, "EpicAdvAttack task has been started.");
    EpicAdvAttack *handle = (EpicAdvAttack*)ctx;

    
    handle->m_bIsRunning = true;
    ledStrip->sendCmd(LED_CMD_OFF_ALL);
    segDisplay->displayNum(0);
    laserRecv->resetQueue();
    ledStrip->stop();

    sndPlayer->playSpeach("EB-ADVCK"); 
    sndPlayer->playSpeach(VOICE_COUNT_BACK);
    vTaskDelay(10000 / portTICK_PERIOD_MS);
    AutoTurret::getInstance()->setIRMCode(EPICADVATTACK_IRM_CODE);
    AutoTurret::getInstance()->start();

    int shotCount = 0;
    uint16_t tmSeconds = 60;
    int64_t tmStart = esp_timer_get_time();
    while (handle->m_bStopFlag==false) {
        if (laserRecv->getCmd(200)>0) {
            ESP_LOGI(TAG, "Received shoot trigger.");

            shotCount++;
            if(shotCount>=100) shotCount = 1;

            sndPlayer->play(SE_GUN_M1, SOUND_PLAY_PRI_HIGH_2);        //play gun shoot sound
            //sndPlayer->playSpeach("EB-YUHIT"); 
            ledStrip->sendCmd(LED_CMD_BLINK_RANDOM);   //blink LED strip
        }

        uint16_t tmLastSeconds = ((esp_timer_get_time() - tmStart)/(1000*1000));
        if(tmSeconds-tmLastSeconds<=0) break;
        segDisplay->displayNum(tmSeconds-tmLastSeconds); //show remaining time on LED display
    }

    AutoTurret::getInstance()->stop();
    int hitCount = EpicMgr::getInstance()->getHitCount();
    segDisplay->displayNum(shotCount);
    ledStrip->stop();
    sndPlayer->play(VOICE_SUCCESS_1, SOUND_PLAY_PRI_HIGH_2);  //true means overwrite any existing voice in the queue
    sndPlayer->playSpeachWithNum("EB-HITLT", shotCount, "EB-UNIT2"); 
    sndPlayer->playSpeachWithNum("EB-LTHIT", hitCount, "EB-UNIT2"); 

    handle->m_bIsRunning = false;
    handle->m_bStopFlag  = true;
    ESP_LOGI(TAG, "EpicAdvAttack exiting...");
    vTaskDelete(NULL);
}
