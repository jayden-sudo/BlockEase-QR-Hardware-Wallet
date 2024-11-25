/*********************
 *      INCLUDES
 *********************/
#include "app_backlight.h"
#include "app_peripherals.h"

/**********************
 * GLOBAL PROTOTYPES
 **********************/
bool app_backlight_support(void);

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
bool app_backlight_support(void)
{
    peripherals_config_t *config = app_peripherals_read();
    return config->lcd_module_config.gpio_bl_pwm != 0;
}
