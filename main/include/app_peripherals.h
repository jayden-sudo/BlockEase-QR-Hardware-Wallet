#ifndef APP_PERIPHERALS_H
#define APP_PERIPHERALS_H

/*********************
 *      INCLUDES
 *********************/
#include "esp_err.h"
#include <stdbool.h>

/*********************
 *      DEFINES
 *********************/
#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S3
// dual core: esp32, esp32-s3
#define MCU_CORE0 0
#define MCU_CORE1 1
#else
// single core: esp32-s2, esp32-c3 ...
#define MCU_CORE0 0
#define MCU_CORE1 0
#endif

/**********************
 *      TYPEDEFS
 **********************/
typedef enum
{
    CAMERA_MODULE_WROVER_KIT = 0,
    CAMERA_MODULE_ESP_EYE,
    CAMERA_MODULE_ESP_S2_KALUGA,
    CAMERA_MODULE_ESP_S3_EYE,
    CAMERA_MODULE_ESP32_CAM_BOARD,
    CAMERA_MODULE_M5STACK_PSRAM,
    CAMERA_MODULE_M5STACK_WIDE,
    CAMERA_MODULE_AI_THINKER,
    CAMERA_MODULE_CUSTOM,
} camera_module_t;

typedef struct __attribute__((aligned(4)))
{
    int pin_pwdn;
    int pin_reset;
    int pin_xclk;
    int pin_sioc;
    int pin_siod;
    int pin_d7;
    int pin_d6;
    int pin_d5;
    int pin_d4;
    int pin_d3;
    int pin_d2;
    int pin_d1;
    int pin_d0;
    int pin_vsync;
    int pin_href;
    int pin_pclk;
} camera_module_custom_config_t;

typedef struct __attribute__((aligned(4)))
{
    int xclk_freq_hz;
    int pixelformat;
    int framesize;
    int fb_count;
    int swap_x;
    int swap_y;
} camera_module_config_t;

typedef enum
{
    LCD_MODULE_ST7735 = 0,
    LCD_MODULE_ST7789,
    LCD_MODULE_ST7796,
    LCD_MODULE_ILI9341,
    LCD_MODULE_SSD1306,
    LCD_MODULE_NT35510,
    LCD_MODULE_GC9A01,
} lcd_module_t;

typedef struct __attribute__((aligned(4)))
{
    int pin_sclk;
    int pin_mosi;
    int pin_rst;
    int pin_dc;
    int pin_cs;
    int pin_bl;
} lcd_module_pin_config_t;

typedef struct __attribute__((aligned(4)))
{
    int h_res;
    int v_res;
    bool swap_xy;
    bool mirror_x;
    bool mirror_y;
    bool gpio_bl_pwm;
    int spi_num;
    int pixel_clk_hz;
    int cmd_bits;
    int param_bits;
    int color_space;
    int bits_per_pixel;
    bool draw_buff_double;
    int draw_buff_height;
    int bl_on_level;
} lcd_module_config_t;

typedef enum
{
    TOUCH_MODULE_CST816S = 0,
    TOUCH_MODULE_FT5X06,
    TOUCH_MODULE_FT6X36,
    TOUCH_MODULE_GT1151,
    TOUCH_MODULE_GT911,
    TOUCH_MODULE_TT21100,
} touch_module_t;

typedef struct __attribute__((aligned(4)))
{
    int i2c_scl;
    int i2c_sda;
    int gpio_int;
    int i2c_clk_hz;
    int i2c_num;
    bool swap_xy;
    bool mirror_x;
    bool mirror_y;
} touch_module_config_t;

typedef struct __attribute__((aligned(4)))
{
    camera_module_t camera_module;
    camera_module_custom_config_t camera_module_custom_config;
    camera_module_config_t camera_module_config;
    lcd_module_t lcd_module;
    lcd_module_pin_config_t lcd_module_pin_config;
    lcd_module_config_t lcd_module_config;
    touch_module_t touch_module;
    touch_module_config_t touch_module_config;
    uint32_t check_sum;
} peripherals_config_t;

#ifdef __cplusplus
extern "C"
{
#endif

    /**********************
     * GLOBAL PROTOTYPES
     **********************/
    esp_err_t app_camera_init(void);
    esp_err_t app_lvgl_init(void);

    peripherals_config_t *app_peripherals_read();

#ifdef __cplusplus
}
#endif

#endif // APP_PERIPHERALS_H