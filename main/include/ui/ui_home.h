#ifndef UI_HOME_H
#define UI_HOME_H

/*********************
 *      INCLUDES
 *********************/
#include "esp_lvgl_port.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**********************
     * GLOBAL PROTOTYPES
     **********************/
    void ui_home_init(void);
    void ui_home_destroy(void);
    void ui_home_start_qr_scan(void);
    void ui_home_stop_qr_scan(void);
    void ui_home_update_camera_preview(void *src);
    void ui_home_set_qr_scan_progress(size_t progress);

#ifdef __cplusplus
    extern "C"
}
#endif

#endif /* UI_HOME_H */