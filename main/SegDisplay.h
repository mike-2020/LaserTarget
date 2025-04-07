#pragma once
#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"


typedef struct{
	uint8_t cmd;
	uint8_t val;
}seg_display_cmd_t;

class SegDisplay{
private:
	QueueHandle_t m_cmdQueue;
	uint8_t m_buff[9];

	void _displayNum(uint16_t num);
	void _blinkNum(uint16_t num);
	static void proc(void *ctx);
public:
	int init();
	void clear();
	int displayNum(uint16_t num);
	int blinkNum(uint16_t num);
	static SegDisplay* getInstance();
};

