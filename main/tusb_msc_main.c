#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

#include "esp_sntp.h"
#include "esp_netif_sntp.h"

#include "ws2812.h"
#include "oled.h"
#include "esp32_wifi.h"
#include "esp32_usb.h"

#define TAG "main"

#define TASK_RMT_WS2812_STACK_SIZE 4096
#define TASK_I2C_OLED_SIZE         4096
#define TASK_WIFI_SIZE             4096

#define TASK_RMT_WS2812_PRIORITY 4
#define TASK_I2C_OLED_PRIORITY   3
#define TASK_WIFI_PRIORITY       5

TaskHandle_t create_task_handle     = NULL;
TaskHandle_t task_rmt_ws2812_handle = NULL;
TaskHandle_t task_i2c_oled_handle   = NULL;
TaskHandle_t wifi_connect           = NULL;
TaskHandle_t sntp_get_time          = NULL;

BaseType_t xReturn;

// 定义消息结构
typedef struct {
    int command;
    int value;
    char text[20];
} TaskMessage_t;

typedef struct {
    int hour;
    int min;
    int sec;
} Time;


// 创建全局队列句柄
QueueHandle_t xQueue = NULL;

void Create_TASK(void *pvParam);
void RMT_WS2812_TASK(void *pvParam);
void WIFI_CONNECT(void *pvParam);
void I2C_OLED_TASK(void *pvParam);
void SNTP_GET_TIME(void *pvParam);

void app_main(void)
{
    ESP_LOGI(TAG, "Initializing storage...");

    static wl_handle_t wl_handle = WL_INVALID_HANDLE;
    ESP_ERROR_CHECK(storage_init_spiflash(&wl_handle));

    const tinyusb_msc_spiflash_config_t config_spi = {
        .wl_handle = wl_handle,
        .callback_mount_changed = storage_mount_changed_cb,  /* First way to register the callback. This is while initializing the storage. */
        .mount_config.max_files = 5,
    };
    ESP_ERROR_CHECK(tinyusb_msc_storage_init_spiflash(&config_spi));
    ESP_ERROR_CHECK(tinyusb_msc_register_callback(TINYUSB_MSC_EVENT_MOUNT_CHANGED, storage_mount_changed_cb)); /* Other way to register the callback i.e. registering using separate API. If the callback had been already registered, it will be overwritten. */

    //mounted in the app by default
    mount();

    ESP_LOGI(TAG, "USB MSC initialization");
    const tinyusb_config_t tusb_cfg = {
        .device_descriptor = &descriptor_config,
        .string_descriptor = string_desc_arr,
        .string_descriptor_count = sizeof(string_desc_arr) / sizeof(string_desc_arr[0]),
        .external_phy = false,
        .configuration_descriptor = msc_fs_configuration_desc,
    };
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));
    ESP_LOGI(TAG, "USB MSC initialization DONE");

    WIFI WIFI_LOG;
    WIFI_LOG = init_usb_device();

     xReturn = xTaskCreate(Create_TASK,
                 "Create_TASK",
                 4096,
                 (void *)& WIFI_LOG,
                 1,
                 &create_task_handle
             );
    if(xReturn != pdPASS)
        ESP_LOGI(TAG, "任务队列创建失败...");
    while(1){
        vTaskDelay(pdMS_TO_TICKS(1000));
    }


}

void Create_TASK(void *pvParam){
    WIFI* WIFI_LOG = (WIFI *)pvParam;
    xQueue = xQueueCreate(10, sizeof(TaskMessage_t));
     xReturn = xTaskCreate(RMT_WS2812_TASK,
                 "RMT_WS2812_TASK",
                 TASK_RMT_WS2812_STACK_SIZE,
                 NULL,
                 TASK_RMT_WS2812_PRIORITY,
                 &task_rmt_ws2812_handle
             );
    if(xReturn != pdPASS)
        ESP_LOGI(TAG, "WS2812创建失败...");         
    
     xReturn = xTaskCreate(I2C_OLED_TASK,
                 "I2C_OLED_TASK",
                 TASK_I2C_OLED_SIZE,
                 NULL,
                 TASK_I2C_OLED_PRIORITY,
                 &task_i2c_oled_handle
             );
    if(xReturn != pdPASS)
        ESP_LOGI(TAG, "OLED创建失败...");

    xReturn = xTaskCreate(WIFI_CONNECT,
                 "WIFI_CONNECT",
                 TASK_WIFI_SIZE,
                 (void *)WIFI_LOG,
                 TASK_WIFI_PRIORITY,
                 &wifi_connect
             );
    if(xReturn != pdPASS)
        ESP_LOGI(TAG, "WIFI链接失败...");

     ESP_LOGI(TAG, "任务队列创建结束...");
     vTaskDelete(NULL);
     
 }

void RMT_WS2812_TASK(void *pvParam){

    TickType_t xLastWakeTime;
    const TickType_t xFrequency = pdMS_TO_TICKS(1000); //1S更新一次
    xLastWakeTime = xTaskGetTickCount();

    ESP_LOGI(TAG, "初始化WS2812 RMT驱动...");

    // 创建LED颜色数组
    static rgb_color leds[LED_NUMBERS];

    // 初始化RMT
    esp32_init_rmt();
    
    ESP_LOGI(TAG, "开始LED颜色循环...");
    while (1) {
        // 红色
        leds[0] = (rgb_color){.red = 255*0.1, .green = 0, .blue = 0};
        set_led_color(leds, LED_NUMBERS);
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        
        // 绿色
        leds[0] = (rgb_color){.red = 0, .green = 255*0.1, .blue = 0};
        set_led_color(leds, LED_NUMBERS);
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        
        // 蓝色
        leds[0] = (rgb_color){.red = 0, .green = 0, .blue = 255*0.1};
        set_led_color(leds, LED_NUMBERS);
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        
        // 黄色
        leds[0] = (rgb_color){.red = 255*0.5, .green = 255*0.1, .blue = 0};
        set_led_color(leds, LED_NUMBERS);
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        
        // 紫色
        leds[0] = (rgb_color){.red = 255*0.5, .green = 0, .blue = 255*0.1};
        set_led_color(leds, LED_NUMBERS);
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        
        // 青色
        leds[0] = (rgb_color){.red = 0, .green = 255*0.1, .blue = 255*0.1};
        set_led_color(leds, LED_NUMBERS);
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        
        // 白色
        leds[0] = (rgb_color){.red = 255*0.1, .green = 255*0.1, .blue = 255*0.1};
        set_led_color(leds, LED_NUMBERS);
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        
        // 关闭LED
        leds[0] = (rgb_color){.red = 0, .green = 0, .blue = 0};
        set_led_color(leds, LED_NUMBERS);
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        
}
}

 void I2C_OLED_TASK(void *pvParam){


    TaskMessage_t receivedMsg;
    Time timerecive;
    char buf[20];
     ESP_LOGI(TAG, "初始化OLED I2C驱动...");
     // 初始化I2C
    esp32_init_i2c();
    vTaskDelay(200);
    OLED_Init();
    OLED_NewFrame();
    OLED_PrintString(0,0,"Hello World!",&font16x16,OLED_COLOR_NORMAL);
    //OLED_PrintString(0,16,"TASK_COUNTER:0",&font16x16,OLED_COLOR_NORMAL);
    OLED_PrintString(0,16,"00:00:00",&font24x12,OLED_COLOR_NORMAL);
    OLED_ShowFrame();

    if(xQueueReceive(xQueue, &timerecive, portMAX_DELAY) == pdPASS) {
        sprintf(buf,"%d:%d:%d",(int)timerecive.hour,(int)timerecive.min,(int)(int)timerecive.sec);
    }

     while (1){

        // timerecive.sec++;
        // if (timerecive.sec >= 60) {
        //     timerecive.sec = 0;
        //     timerecive.min++;
        //     if (timerecive.min >= 60) {
        //         timerecive.min = 0;
        //         timerecive.hour++;
        //         if (timerecive.hour >= 24) {
        //             timerecive.hour = 0;
        //         }
        //     }
        // }

        if(xQueueReceive(xQueue, &timerecive, portMAX_DELAY) == pdPASS) {
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
                 timerecive.hour, timerecive.min, timerecive.sec);
        OLED_PrintString(0,16,buf,&font24x12,OLED_COLOR_NORMAL);
        OLED_ShowFrame();
        //sprintf(buf,"%d:%d:%d",(int)timerecive.hour,(int)timerecive.min,(int)(int)timerecive.sec);
    }
        
     }
    

 }

 void WIFI_CONNECT(void *pvParam){
    WIFI *WIFI_LOG = (WIFI *)pvParam;

    printf("SSID: %s\n",WIFI_LOG->ssid);
     //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    if (CONFIG_LOG_MAXIMUM_LEVEL > CONFIG_LOG_DEFAULT_LEVEL) {
        /* If you only want to open more logs in the wifi module, you need to make the max level greater than the default level,
         * and call esp_log_level_set() before esp_wifi_init() to improve the log level of the wifi module. */
        esp_log_level_set("wifi", CONFIG_LOG_MAXIMUM_LEVEL);
    }

    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta(WIFI_LOG);

    xReturn = xTaskCreate(SNTP_GET_TIME,
                 "SNTP_GET_TIME",
                 TASK_WIFI_SIZE,
                 NULL,
                 TASK_WIFI_PRIORITY,
                 &sntp_get_time
             );
    vTaskDelete(NULL);

 }

 void SNTP_GET_TIME(void *pvParam){

    TickType_t xLastWakeTime;
    const TickType_t xFrequency = pdMS_TO_TICKS(1000); //1S更新一次
    xLastWakeTime = xTaskGetTickCount();

    time_t now;
    struct tm timeinfo;
    char strftime_buf[64];

    //初始化STNP
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    esp_netif_sntp_init(&config);

    //同步
    if (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(15000)) != ESP_OK) {
        ESP_LOGE(TAG, "SNTP 同步失败，继续使用本地时间");
    }

    //设置时区
    setenv("TZ", "CST-8", 1);
    tzset();

    //读取时间
    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%F %T", &timeinfo);
    ESP_LOGI(TAG, "当前时间: %s", strftime_buf);

    Time msg = {
        .hour = timeinfo.tm_hour,
        .min  = timeinfo.tm_min,
        .sec  = timeinfo.tm_sec
    };

    if (xQueueSend(xQueue, &msg, pdMS_TO_TICKS(1000)) != pdPASS) {
        ESP_LOGE(TAG, "时间消息入队失败");
    }

    //esp_netif_sntp_deinit();       // 仅获取一次

    while (1) {
        time(&now);
        localtime_r(&now, &timeinfo);

        Time msg = { .hour = timeinfo.tm_hour,
                     .min  = timeinfo.tm_min,
                     .sec  = timeinfo.tm_sec };
        xQueueSend(xQueue, &msg, portMAX_DELAY);
        //vTaskDelay(pdMS_TO_TICKS(1000));
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }


    vTaskDelete(NULL);             // 只执行一次就退出
}
