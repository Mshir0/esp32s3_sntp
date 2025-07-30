#ifndef __WS2812_H__
#define __WS2812_H__
#include "driver/rmt_tx.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define TAG "WS2812_RMT"

// 配置参数
#define WS2812_GPIO_NUM      48      // 数据引脚
#define LED_NUMBERS          1       // LED数量
#define RMT_RESOLUTION_HZ    10000000 // 10MHz RMT时钟 (0.1us/tick)

// WS2812时序参数 (单位：0.1us)
#define T0H                  4       // 0码高电平时间 (0.4us)
#define T0L                  8       // 0码低电平时间 (0.8us)
#define T1H                  8       // 1码高电平时间 (0.8us)
#define T1L                  4       // 1码低电平时间 (0.4us)
#define RESET_DURATION       500     // 复位时间 (50us)

// 颜色结构体
typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} rgb_color;

// 自定义编码器结构体
typedef struct {
    rmt_encoder_t base;
    rmt_encoder_t *copy_encoder;
    uint32_t reset_duration; // 复位信号持续时间
} rmt_ws2812_encoder_t;

// WS2812编码器初始化函数
static size_t rmt_encode_ws2812(rmt_encoder_t *encoder, rmt_channel_handle_t channel,
                                const void *primary_data, size_t data_size, rmt_encode_state_t *ret_state);

// 初始化WS2812编码器
static esp_err_t rmt_new_ws2812_encoder(rmt_encoder_handle_t *ret_encoder);

// 设置LED颜色
void set_led_color(rgb_color *led_colors, size_t led_num);

void esp32_init_rmt(void);

#endif