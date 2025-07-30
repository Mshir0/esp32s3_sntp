#include "ws2812.h"

// 全局变量
static rmt_channel_handle_t tx_channel = NULL;
static rmt_encoder_handle_t ws2812_encoder = NULL;


// WS2812编码器初始化函数
static size_t rmt_encode_ws2812(rmt_encoder_t *encoder, rmt_channel_handle_t channel,
                                const void *primary_data, size_t data_size, rmt_encode_state_t *ret_state) {
    rmt_ws2812_encoder_t *ws2812_encoder = __containerof(encoder, rmt_ws2812_encoder_t, base);
    rmt_encoder_handle_t copy_encoder = ws2812_encoder->copy_encoder;
    rmt_encode_state_t session_state = RMT_ENCODING_RESET;
    rmt_encode_state_t state = RMT_ENCODING_RESET;
    size_t encoded_symbols = 0;
    
    // 首先使用复制编码器处理LED数据
    encoded_symbols += copy_encoder->encode(copy_encoder, channel, primary_data, data_size, &session_state);
    
    if (session_state & RMT_ENCODING_COMPLETE) {
        // 添加复位信号
        rmt_symbol_word_t reset_symbol = {
            .level0 = 0,
            .duration0 = ws2812_encoder->reset_duration,
            .level1 = 0,
            .duration1 = 0,
        };
        encoded_symbols += copy_encoder->encode(copy_encoder, channel, &reset_symbol, 
                                                sizeof(reset_symbol), &session_state);
        state |= RMT_ENCODING_COMPLETE;
    }
    
    if (session_state & RMT_ENCODING_MEM_FULL) {
        state |= RMT_ENCODING_MEM_FULL;
    }
    
    *ret_state = state;
    return encoded_symbols;
}


// 初始化WS2812编码器
static esp_err_t rmt_new_ws2812_encoder(rmt_encoder_handle_t *ret_encoder) {
    rmt_ws2812_encoder_t *ws2812_encoder = calloc(1, sizeof(rmt_ws2812_encoder_t));
    if (!ws2812_encoder) {
        return ESP_ERR_NO_MEM;
    }
    
    // 初始化基础编码器
    ws2812_encoder->base.encode = rmt_encode_ws2812;
    ws2812_encoder->base.del = NULL;
    ws2812_encoder->base.reset = NULL;
    
    // 创建字节编码器
    rmt_bytes_encoder_config_t bytes_encoder_config = {
        .bit0 = {
            .duration0 = T0H,
            .level0 = 1,
            .duration1 = T0L,
            .level1 = 0,
        },
        .bit1 = {
            .duration0 = T1H,
            .level0 = 1,
            .duration1 = T1L,
            .level1 = 0,
        },
        .flags.msb_first = 1, // WS2812使用MSB first
    };
    ESP_ERROR_CHECK(rmt_new_bytes_encoder(&bytes_encoder_config, &ws2812_encoder->copy_encoder));
    
    ws2812_encoder->reset_duration = RESET_DURATION;
    *ret_encoder = &ws2812_encoder->base;
    return ESP_OK;
}

// 设置LED颜色
void set_led_color(rgb_color *led_colors, size_t led_num) {
    // 为每个LED分配24位数据 (GRB格式)
    uint8_t *buffer = malloc(led_num * 3);
    if (!buffer) {
        ESP_LOGE(TAG, "内存分配失败");
        return;
    }
    
    // 将RGB颜色转换为GRB格式
    for (int i = 0; i < led_num; i++) {
        buffer[i * 3 + 0] = led_colors[i].green;
        buffer[i * 3 + 1] = led_colors[i].red;
        buffer[i * 3 + 2] = led_colors[i].blue;
    }
    
    // 发送数据
    rmt_transmit_config_t tx_config = {
        .loop_count = 0, // 不循环
        .flags.eot_level = 0, // 传输结束时的电平为0
    };
    
    ESP_ERROR_CHECK(rmt_transmit(tx_channel, ws2812_encoder, buffer, led_num * 3, &tx_config));
    free(buffer);
}

// 初始化RMT通道
void esp32_init_rmt(void) {
    // 配置RMT发送通道
    rmt_tx_channel_config_t tx_chan_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT, // 默认时钟源
        .gpio_num = WS2812_GPIO_NUM,
        .mem_block_symbols = 64, // 内存块大小
        .resolution_hz = RMT_RESOLUTION_HZ,
        .trans_queue_depth = 4, // 传输队列深度
        .flags.invert_out = 0, // 不反转输出
        .flags.with_dma = 0, // 不使用DMA
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &tx_channel));
    
    // 创建WS2812编码器
    ESP_ERROR_CHECK(rmt_new_ws2812_encoder(&ws2812_encoder));
    
    // 启用RMT通道
    ESP_ERROR_CHECK(rmt_enable(tx_channel));
}