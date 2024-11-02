/*********************
 *      INCLUDES
 *********************/
#include "app.h"
#include <inttypes.h>
#include "sdkconfig.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_console.h"
#include "esp_timer.h"
#include "linenoise/linenoise.h"
#include <utility/trezor/bip39.h>
#include "crc32.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "cbor.h"
#include "esp_code_scanner.h"
#include "esp_lvgl_port.h"
#include "app_peripherals.h"
#include "esp_heap_caps.h"
#include "controller/ctrl_init.h"
#include "stack_log.h"

/*********************
 *      DEFINES
 *********************/
#define TAG "app"
#define DELAY_MS_MAX 500

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/
void app_main(void);

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
void app_main(void)
{
    printf("QR Hardware Wallet\n");
    /* Print chip information */
    esp_chip_info_t chip_info;
    uint32_t flash_size;
    esp_chip_info(&chip_info);
    printf("This is %s chip with %d CPU core(s), %s%s%s%s, ",
           CONFIG_IDF_TARGET,
           chip_info.cores,
           (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "WiFi/" : "",
           (chip_info.features & CHIP_FEATURE_BT) ? "BT" : "",
           (chip_info.features & CHIP_FEATURE_BLE) ? "BLE" : "",
           (chip_info.features & CHIP_FEATURE_IEEE802154) ? ", 802.15.4 (Zigbee/Thread)" : "");

    unsigned major_rev = chip_info.revision / 100;
    unsigned minor_rev = chip_info.revision % 100;
    printf("silicon revision v%d.%d, ", major_rev, minor_rev);
    if (esp_flash_get_size(NULL, &flash_size) != ESP_OK)
    {
        printf("Get flash size failed");
        return;
    }

    printf("%" PRIu32 "MB %s flash\n", flash_size / (uint32_t)(1024 * 1024),
           (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    printf("Minimum free heap size: %" PRIu32 " bytes\n", esp_get_minimum_free_heap_size());

    fflush(stdout);
    // sleep 0.5s
    vTaskDelay(pdMS_TO_TICKS(500));
    ESP_ERROR_CHECK(app_lvgl_init());
    TaskHandle_t pxCreatedTask;
    BaseType_t ret = xTaskCreatePinnedToCore(ctrl_init, "ctrl_init", 6 * 1024, NULL, 10, &pxCreatedTask, MCU_CORE0);
    if (ret != pdTRUE)
    {
        ESP_LOGE(TAG, "xTaskCreatePinnedToCore failed");
    }

    LOG_STACK_USAGE_TASK_INIT(main_task);
    LOG_STACK_USAGE_TASK_INIT(ctrl_init_task);

    uint32_t task_delay_ms = 0;
    while (1)
    {
        if (lvgl_port_lock(0))
        {
            task_delay_ms = lv_timer_handler();
            lvgl_port_unlock();
        }
        else if (task_delay_ms > DELAY_MS_MAX)
        {
            task_delay_ms = DELAY_MS_MAX;
        }
        vTaskDelay(pdMS_TO_TICKS(task_delay_ms));

        LOG_STACK_USAGE_TASK(NULL, main_task);
        LOG_STACK_USAGE_TASK(pxCreatedTask, ctrl_init_task);
    }
}
