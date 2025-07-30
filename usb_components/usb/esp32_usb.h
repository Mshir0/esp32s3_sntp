#ifndef __ESP32_USB__
#define __ESP32_USB__

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <stdlib.h>
#include "sdkconfig.h"
#include "esp_console.h"
#include "esp_check.h"
#include "esp_partition.h"
#include "driver/gpio.h"
#include "tinyusb.h"
#include "tusb_msc_storage.h"


#define MAX_LINE_LENGTH 256
#define MAX_VALUE_LENGTH 128


#ifdef CONFIG_EXAMPLE_STORAGE_MEDIA_SDMMC
#include "sdmmc_cmd.h"
#include "diskio_impl.h"
#include "diskio_sdmmc.h"
#if CONFIG_EXAMPLE_SD_PWR_CTRL_LDO_INTERNAL_IO
#include "sd_pwr_ctrl_by_on_chip_ldo.h"
#endif // CONFIG_EXAMPLE_SD_PWR_CTRL_LDO_INTERNAL_IO
#endif

/*
 * We warn if a secondary serial console is enabled. A secondary serial console is always output-only and
 * hence not very useful for interactive console applications. If you encounter this warning, consider disabling
 * the secondary serial console in menuconfig unless you know what you are doing.
 */
#if SOC_USB_SERIAL_JTAG_SUPPORTED
#if !CONFIG_ESP_CONSOLE_SECONDARY_NONE
#warning "A secondary serial console is not useful when using the console component. Please disable it in menuconfig."
#endif
#endif

#define TAG  "usb"

/* TinyUSB descriptors
   ********************************************************************* */
#define EPNUM_MSC       1
#define TUSB_DESC_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_MSC_DESC_LEN)

enum {
    ITF_NUM_MSC = 0,
    ITF_NUM_TOTAL
};

enum {
    EDPT_CTRL_OUT = 0x00,
    EDPT_CTRL_IN  = 0x80,

    EDPT_MSC_OUT  = 0x01,
    EDPT_MSC_IN   = 0x81,
};

static tusb_desc_device_t descriptor_config = {
    .bLength = sizeof(descriptor_config),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = 0x303A, // This is Espressif VID. This needs to be changed according to Users / Customers
    .idProduct = 0x4002,
    .bcdDevice = 0x100,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01
};

static uint8_t const msc_fs_configuration_desc[] = {
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, TUSB_DESC_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

    // Interface number, string index, EP Out & EP In address, EP size
    TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, 0, EDPT_MSC_OUT, EDPT_MSC_IN, 64),
};

#if (TUD_OPT_HIGH_SPEED)
static const tusb_desc_device_qualifier_t device_qualifier = {
    .bLength = sizeof(tusb_desc_device_qualifier_t),
    .bDescriptorType = TUSB_DESC_DEVICE_QUALIFIER,
    .bcdUSB = 0x0200,
    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .bNumConfigurations = 0x01,
    .bReserved = 0
};

static uint8_t const msc_hs_configuration_desc[] = {
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, TUSB_DESC_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

    // Interface number, string index, EP Out & EP In address, EP size
    TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, 0, EDPT_MSC_OUT, EDPT_MSC_IN, 512),
};
#endif // TUD_OPT_HIGH_SPEED

static char const *string_desc_arr[] = {
    (const char[]) { 0x09, 0x04 },  // 0: is supported language is English (0x0409)
    "TinyUSB",                      // 1: Manufacturer
    "TinyUSB Device",               // 2: Product
    "123456",                       // 3: Serials
    "Example MSC",                  // 4. MSC
};

/*********************************************************************** TinyUSB descriptors*/

#define BASE_PATH "/data" // base path to mount the partition

#define PROMPT_STR CONFIG_IDF_TARGET

// mount the partition and show all the files in BASE_PATH
void mount(void)
{
    ESP_LOGI(TAG, "Mount storage...");
    ESP_ERROR_CHECK(tinyusb_msc_storage_mount(BASE_PATH));

    // List all the files in this directory
    ESP_LOGI(TAG, "\nls command output:");
    struct dirent *d;
    DIR *dh = opendir(BASE_PATH);
    if (!dh) {
        if (errno == ENOENT) {
            //If the directory is not found
            ESP_LOGE(TAG, "Directory doesn't exist %s", BASE_PATH);
        } else {
            //If the directory is not readable then throw error and exit
            ESP_LOGE(TAG, "Unable to read directory %s", BASE_PATH);
        }
        return;
    }
    //While the next entry is not readable we will print directory files
    while ((d = readdir(dh)) != NULL) {
        printf("%s\n", d->d_name);
    }
    return;
}

// unmount storage
static int console_unmount(int argc, char **argv)
{
    if (tinyusb_msc_storage_in_use_by_usb_host()) {
        ESP_LOGE(TAG, "storage is already exposed");
        return -1;
    }
    ESP_LOGI(TAG, "Unmount storage...");
    ESP_ERROR_CHECK(tinyusb_msc_storage_unmount());
    return 0;
}

// read BASE_PATH/README.MD and print its contents
static int console_read(int argc, char **argv)
{
    if (tinyusb_msc_storage_in_use_by_usb_host()) {
        ESP_LOGE(TAG, "storage exposed over USB. Application can't read from storage.");
        return -1;
    }
    ESP_LOGD(TAG, "read from storage:");
    const char *filename = BASE_PATH "/README.MD";
    FILE *ptr = fopen(filename, "r");
    if (ptr == NULL) {
        ESP_LOGE(TAG, "Filename not present - %s", filename);
        return -1;
    }
    char buf[1024];
    while (fgets(buf, 1000, ptr) != NULL) {
        printf("%s", buf);
    }
    fclose(ptr);
    return 0;
}

// create file BASE_PATH/README.MD if it does not exist
static int console_write(const char *filename)
{
    if (tinyusb_msc_storage_in_use_by_usb_host()) {
        ESP_LOGE(TAG, "storage exposed over USB. Application can't write to storage.");
        return -1;
    }
    ESP_LOGD(TAG, "write to storage:");
    FILE *fd = fopen(filename, "r");
    if(strcmp(filename, "/data/README.MD") == 0){
        if (!fd) {
        ESP_LOGW(TAG, "README.MD doesn't exist yet, creating");
        fd = fopen(filename, "w");
        fprintf(fd, "Mass Storage Devices are one of the most common USB devices. It use Mass Storage Class (MSC) that allow access to their internal data storage.\n");
        fprintf(fd, "In this example, ESP chip will be recognised by host (PC) as Mass Storage Device.\n");
        fprintf(fd, "Upon connection to USB host (PC), the example application will initialize the storage module and then the storage will be seen as removable device on PC.\n");
        fclose(fd);
    }
    }

    if(strcmp(filename, "/data/config.txt") == 0){
    if (!fd) {
        ESP_LOGW(TAG, "config.txt doesn't exist yet, creating");
        fd = fopen(filename, "w");
        fprintf(fd, "wifi ssid:\n");
        fprintf(fd, "wifi passport:\n");
        fclose(fd);
    }    
    }
    return 0;
}

// Show storage size and sector size
static int console_size(int argc, char **argv)
{
    if (tinyusb_msc_storage_in_use_by_usb_host()) {
        ESP_LOGE(TAG, "storage exposed over USB. Application can't access storage");
        return -1;
    }
    uint32_t sec_count = tinyusb_msc_storage_get_sector_count();
    uint32_t sec_size = tinyusb_msc_storage_get_sector_size();
    printf("Storage Capacity %lluMB\n", ((uint64_t) sec_count) * sec_size / (1024 * 1024));
    return 0;
}

esp_err_t storage_init_spiflash(wl_handle_t *wl_handle)
{
    ESP_LOGI(TAG, "Initializing wear levelling");

    const esp_partition_t *data_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_FAT, NULL);
    if (data_partition == NULL) {
        ESP_LOGE(TAG, "Failed to find FATFS partition. Check the partition table.");
        return ESP_ERR_NOT_FOUND;
    }

    return wl_mount(data_partition, wl_handle);
}

// callback that is delivered when storage is mounted/unmounted by application.
void storage_mount_changed_cb(tinyusb_msc_event_t *event)
{
    ESP_LOGI(TAG, "Storage mounted to application: %s", event->mount_changed_data.is_mounted ? "Yes" : "No");
}

void trim_whitespace(char *str) {
    if (!str || !*str) return;
    
    // 去除尾部空白
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    *(end + 1) = '\0';
    
    // 去除首部空白
    char *start = str;
    while (*start && isspace((unsigned char)*start)) start++;
    
    // 移动字符串内容
    if (start != str) {
        memmove(str, start, strlen(start) + 1);
    }
}

static WIFI config_read(const char *filename){
    WIFI read_wifi;
    char line[MAX_LINE_LENGTH];
    char ssid[MAX_VALUE_LENGTH] = {0};
    char password[MAX_VALUE_LENGTH] = {0};

    FILE *fd = fopen(filename, "r");
    
    while (fgets(line, sizeof(line), fd)) {
        // 查找冒号位置
        char *colon = strchr(line, ':');
        if (!colon) continue;  // 跳过没有冒号的行
        
        // 跳过冒号并移动到值的位置
        char *value_start = colon + 1;
        
        // 去除值前后的空白字符
        trim_whitespace(value_start);
        
        // 检查是否是SSID行
        if (strstr(line, "wifi ssid") != NULL) {
            strncpy(ssid, value_start, sizeof(ssid) - 1);
        } 
        // 检查是否是密码行
        else if (strstr(line, "wifi passport") != NULL) {
            strncpy(password, value_start, sizeof(password) - 1);
        }
    }

    fclose(fd);

    // 输出结果
    if (ssid[0] != '\0') {
        printf("提取的WiFi SSID: [%s]\n", ssid);
    } else {
        printf("未找到WiFi SSID\n");
    }
    
    if (password[0] != '\0') {
        printf("提取的WiFi密码: [%s]\n", password);
    } else {
        printf("未找到WiFi密码\n");
    }
    strcpy(read_wifi.ssid , ssid);
    strcpy(read_wifi.passport , password);
    //printf("%s",read_wifi.ssid);
    return read_wifi;

}

WIFI init_usb_device(){
    WIFI wifi_log;
    const char *readme_file = BASE_PATH "/README.MD";
    if (access(readme_file, F_OK) == -1) {
        console_write(readme_file);
    }
    const char *config_file = BASE_PATH "/config.txt";
    if (access(config_file, F_OK) == -1) {
        console_write(config_file);
    }

    wifi_log = config_read(config_file);

    return wifi_log;
}
#endif