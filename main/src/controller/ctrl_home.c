/*********************
 *      INCLUDES
 *********************/
#include "controller/ctrl_home.h"
#include <esp_system.h>
#include <esp_log.h>
#include <esp_random.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include "kv_fs.h"
#include <string.h>
#include "ui/ui_home.h"
#include "qrcode_protocol.h"
#include "wallet.h"
#include "crc32.h"
#include "app_peripherals.h"
#include "sha256_str.h"
#include "qrcode_protocol.h"
#include "esp_code_scanner.h"
#include "controller/ctrl_sign.h"
#include "controller/ctrl_init.h"
#include "freertos/timers.h"
#include "wallet_db.h"
#include "stack_log.h"

/*********************
 *      DEFINES
 *********************/
/* logo declare */
LV_IMG_DECLARE(logo_bitcoin)
LV_IMG_DECLARE(logo_ethereum)
LV_IMG_DECLARE(wallet_imtoken)
LV_IMG_DECLARE(wallet_metamask)
LV_IMG_DECLARE(wallet_rabby)

/**********************
 *      TYPEDEFS
 **********************/
typedef struct
{
    lv_obj_t *image;
    lv_obj_t *progress_bar;
} ctrl_home_scan_qr_data_t;

/**********************
 *  STATIC VARIABLES
 **********************/
static const char *TAG = "ctrl_home";
static Wallet wallet = 0;
static ctrl_home_network_data_t *network_data = NULL;
static bool scan_task_status_request = false;
static bool scan_task_status = false;
static TimerHandle_t lock_screen_timer;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void qrScannerTask(void *parameters);
static void global_touch_event_handler(lv_event_t *e);
static void lock_screen_timeout_callback(TimerHandle_t xTimer);

/**********************
 * GLOBAL PROTOTYPES
 **********************/
void ctrl_home_init(char *privateKeyStr);
void ctrl_home_destroy(void);
void ctrl_home_lock_screen(void);

/* wallet page */
ctrl_home_network_data_t *ctrl_home_list_networks(void);
char *ctrl_home_get_connect_qrcode(ctrl_home_network_data_t *network, ctrl_home_connect_qr_type qr_type);

/* scanner page */
void ctrl_home_scan_qr_start(lv_obj_t *image, lv_obj_t *progress_bar);
void ctrl_home_scan_qr_stop(void);

/* settings page */
bool ctrl_home_lock(void);
bool ctrl_home_erase_wallet(void);
int ctrl_home_pin_max_attempts_get(void);
bool ctrl_home_pin_max_attempts_set(int max_attempts);

/**********************
 *   STATIC FUNCTIONS
 **********************/
static void qrScannerTask(void *parameters)
{
    int width = 240;

    ctrl_home_scan_qr_data_t *scan_qr_data = (ctrl_home_scan_qr_data_t *)parameters;
    lv_obj_t *image = scan_qr_data->image;
    lv_obj_t *progress_bar = scan_qr_data->progress_bar;
    free(scan_qr_data);

    if (ESP_OK != app_camera_init())
    {
        return;
    }

    lv_img_dsc_t img_buffer = {
        .header.w = 0,
        .header.h = 0,
        .data_size = 0,
        .header.cf = LV_COLOR_FORMAT_RGB565,
        .data = NULL,
    };
    camera_fb_t *fb = esp_camera_fb_get();
    if (fb == NULL)
    {
        ESP_LOGE(TAG, "camera get failed");
        vTaskDelete(NULL);
        return;
    }
    if (fb->width != width || fb->height != width)
    {
        ESP_LOGE(TAG, "camera not in 240x240");
        vTaskDelete(NULL);
        return;
    }

    scan_task_status = true;

    qrcode_protocol_bc_ur_data_t *qrcode_protocol_bc_ur_data = (qrcode_protocol_bc_ur_data_t *)malloc(sizeof(qrcode_protocol_bc_ur_data_t));
    qrcode_protocol_bc_ur_init(qrcode_protocol_bc_ur_data);

    uint8_t *swap_buf = NULL;
    uint8_t *line_buf = NULL;
    int line_size = width * 2; // RGB565 2 bytes per pixel
    peripherals_config_t *peripherals_config = app_peripherals_read();
    if (peripherals_config->camera_module_config.swap_x || peripherals_config->camera_module_config.swap_y)
    {
        swap_buf = (uint8_t *)malloc(line_size * width);
        if (!peripherals_config->camera_module_config.swap_y)
        {
            line_buf = (uint8_t *)malloc(line_size);
        }
    }

    img_buffer.header.w = fb->width;
    img_buffer.header.h = fb->height;
    img_buffer.data_size = line_size * width; // fb->len;

    esp_code_scanner_config_t config = {ESP_CODE_SCANNER_MODE_FAST, ESP_CODE_SCANNER_IMAGE_RGB565, fb->width, fb->height};
    esp_camera_fb_return(fb);

    bool scan_success = false;

    wallet_data_version_1_t walletData;
    wallet_db_load_wallet_data(&walletData);
    uint32_t LOCK_SCREEN_TIMEOUT_MS = walletData.lockScreenTimeout;

    TickType_t time_start = xTaskGetTickCount();

    LOG_STACK_USAGE_TASK_INIT(qrScannerTask);

    while (scan_task_status_request && !scan_success)
    {
        fb = esp_camera_fb_get();
        if (fb == NULL)
        {
            ESP_LOGE(TAG, "camera get failed");
            continue;
        }
        if (peripherals_config->camera_module_config.swap_x || peripherals_config->camera_module_config.swap_y)
        {
            int new_x;
            int new_y;
            if (peripherals_config->camera_module_config.swap_x && peripherals_config->camera_module_config.swap_y)
            {
                /*
                 (0,0) -> (239,239)
                 (0,1) -> (239,238)
                 ...
                 (239,238) -> (0,1)
                 (239,239) -> (0,0)
                 */
                for (int x = 0; x < width; x++)
                {
                    for (int y = 0; y < width; y++)
                    {
                        new_x = width - x - 1;
                        new_y = width - y - 1;
                        memcpy(swap_buf + new_x * line_size + new_y, fb->buf + x * line_size + y, 2);
                    }
                }
            }
            else if (peripherals_config->camera_module_config.swap_x)
            {
                /*
                 (0,0) -> (239,0)
                 (0,1) -> (239,1)
                 ...
                 (239,238) -> (0,238)
                 (239,239) -> (0,239)
                 */
                for (int x = 0; x < width; x++)
                {
                    for (int y = 0; y < width; y++)
                    {
                        new_x = width - x - 1;
                        memcpy(swap_buf + new_x * line_size + y, fb->buf + x * line_size + y, 2);
                    }
                }
            }
            else if (peripherals_config->camera_module_config.swap_y)
            {
                /*
               (0,0) -> (0,239)
               (0,1) -> (0,238)
               ...
               (239,238) -> (239,1)
               (239,239) -> (239,0)
               */
                for (int y = 0; y < width; y++)
                {
                    memcpy(swap_buf + y * line_size, fb->buf + (width - y - 1) * line_size, line_size);
                }
            }
            img_buffer.data = swap_buf;
        }
        else
        {
            img_buffer.data = fb->buf;
        }

        ui_home_update_camera_preview(&img_buffer);
        vTaskDelay(pdMS_TO_TICKS(15));

        bool debug_mode = false;
        if (debug_mode)
        {
            char *qr_data = "UR:ETH-SIGN-REQUEST/OLADTPDAGDFDFDOXCPJOLRGTOTNYKIAHSFRTKSJKJTAOHDEYAOWTLSPKENOSASLRHKISDLAELRJKTBIOTPLFGMAYMWNEVOESHLIOINKSENQZNEGMFTGYFPOSYAZMTIATIMLNHTWFBEKNFZAELARTAXAAAACYAEPKENOSAHTAADDYOEADLECSDWYKCSFNYKAEYKAEWKAEWKAOCYWLDAQZPRAMGHNEVOESHLIOINKSENQZNEGMFTGYFPOSYAZMTIATIMLTHNIHIM";
            qrcode_protocol_bc_ur_receive(qrcode_protocol_bc_ur_data, qr_data);
            if (qrcode_protocol_bc_ur_is_success(qrcode_protocol_bc_ur_data))
            {
                // ESP_LOGI(TAG, "scan success");
                scan_success = true;
                ui_home_stop_qr_scan();
                ctrl_sign_init(wallet, qrcode_protocol_bc_ur_data);
            }
        }
        else
        {
            // Decode Progress
            esp_image_scanner_t *esp_scn = esp_code_scanner_create();
            esp_code_scanner_set_config(esp_scn, config);
            int decoded_num = esp_code_scanner_scan_image(esp_scn, img_buffer.data);
            if (decoded_num)
            {
                time_start = xTaskGetTickCount();

                esp_code_scanner_symbol_t result = esp_code_scanner_result(esp_scn);
                if (result.data != NULL && strlen(result.data) > 0)
                {
                    // ESP_LOGI(TAG, "scan result:%s", result.data);
                    // Decode UR
                    qrcode_protocol_bc_ur_receive(qrcode_protocol_bc_ur_data, result.data);
                    size_t progress = qrcode_protocol_bc_ur_progress(qrcode_protocol_bc_ur_data);
                    if (progress > 0)
                    {
                        progress *= 0.9;
                        ui_home_set_qr_scan_progress(progress);
                    }
                    if (qrcode_protocol_bc_ur_is_success(qrcode_protocol_bc_ur_data))
                    {
                        // ESP_LOGI(TAG, "scan success");
                        scan_success = true;
                        ui_home_stop_qr_scan();
                        ctrl_sign_init(wallet, qrcode_protocol_bc_ur_data);
                    }
                }
            }
            /* esp_code_scanner_symbol_t unavailable after esp_code_scanner_destroy */
            esp_code_scanner_destroy(esp_scn);
        }

        esp_camera_fb_return(fb);
        vTaskDelay(pdMS_TO_TICKS(5));

        if ((xTaskGetTickCount() - time_start) * portTICK_PERIOD_MS > LOCK_SCREEN_TIMEOUT_MS)
        {
            // lock screen
            time_start = xTaskGetTickCount();
            scan_task_status_request = false;
            lv_async_call(ctrl_home_lock_screen, NULL);
        }

        LOG_STACK_USAGE_TASK(NULL, qrScannerTask);
    }

    if (!scan_success)
    {
        qrcode_protocol_bc_ur_free(qrcode_protocol_bc_ur_data);
        free(qrcode_protocol_bc_ur_data);
        qrcode_protocol_bc_ur_data = NULL;
    } // if scan success, ctrl_sign_init will free qrcode_protocol_bc_ur_data

    ui_home_update_camera_preview(NULL);
    ui_home_set_qr_scan_progress(0);
    if (swap_buf != NULL)
    {
        free(swap_buf);
        swap_buf = NULL;
    }
    if (line_buf != NULL)
    {
        free(line_buf);
        line_buf = NULL;
    }
    esp_camera_deinit();
    scan_task_status = false;
    vTaskDelete(NULL);
}
static void global_touch_event_handler(lv_event_t *e)
{
    if (scan_task_status)
    {
        return;
    }
    if (lock_screen_timer == NULL)
    {
        ESP_LOGE(TAG, "lock_screen_timer is NULL");
        return;
    }
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_DRAW_POST || code == LV_EVENT_GET_SELF_SIZE)
    {
        if (xTimerReset(lock_screen_timer, 0) != pdPASS)
        {
            ESP_LOGE(TAG, "Failed to reset lock screen timer");
        }
    }
}
static void lock_screen_timeout_callback(TimerHandle_t xTimer)
{
    ESP_LOGI(TAG, "lock_screen_timeout_callback");
    ctrl_home_lock_screen();
}

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
void ctrl_home_init(char *privateKeyStr)
{
    scan_task_status_request = false;
    scan_task_status = false;
    wallet = wallet_init_from_xprv(privateKeyStr);
    ui_home_init();

    wallet_data_version_1_t walletData;
    wallet_db_load_wallet_data(&walletData);
    uint32_t LOCK_SCREEN_TIMEOUT_MS = walletData.lockScreenTimeout;
    lock_screen_timer = xTimerCreate("LockScreenTimer", pdMS_TO_TICKS(LOCK_SCREEN_TIMEOUT_MS), pdFALSE, NULL, lock_screen_timeout_callback);
    if (lock_screen_timer == NULL)
    {
        ESP_LOGE(TAG, "Lock screen timer create failed");
    }
    // global event
    lv_obj_add_event_cb(lv_scr_act(), global_touch_event_handler, LV_EVENT_DRAW_POST, NULL);
    lv_obj_add_event_cb(lv_scr_act(), global_touch_event_handler, LV_EVENT_GET_SELF_SIZE, NULL);
    xTimerStart(lock_screen_timer, 0);
}
void ctrl_home_destroy(void)
{
    scan_task_status_request = false;
    while (scan_task_status)
    {
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    ctrl_sign_destroy();
    ui_home_destroy();
    if (wallet != NULL)
    {
        wallet_free(wallet);
        wallet = NULL;
    }
    if (network_data != NULL)
    {
        ctrl_home_network_data_t *network_current = network_data;
        ctrl_home_network_data_t *network_next;
        while (network_current != NULL)
        {
            network_next = network_current->next;
            // free current
            wallet_free(network_current->wallet_current);
            // free compatible_wallet_group
            {
                ctrl_home_compatible_wallet_group_t *compatible_wallet_group = network_current->compatible_wallet_group;
                // free wallet_info_3rd
                if (compatible_wallet_group != NULL)
                {
                    ctrl_home_3rd_wallet_info_t *wallet_info_3rd_current = compatible_wallet_group->wallet_info_3rd;
                    ctrl_home_3rd_wallet_info_t *wallet_info_3rd_next;
                    while (wallet_info_3rd_current != NULL)
                    {
                        wallet_info_3rd_next = wallet_info_3rd_current->next;
                        // ESP_LOG_BUFFER_HEXDUMP(TAG, wallet_info_3rd_current, sizeof(ctrl_home_3rd_wallet_info_t), ESP_LOG_INFO);
                        free(wallet_info_3rd_current);
                        wallet_info_3rd_current = wallet_info_3rd_next;
                    }
                }
                free(compatible_wallet_group);
            }
            free(network_current);
            network_current = network_next;
        }

        network_data = NULL;
    }
    if (lock_screen_timer != NULL)
    {
        lv_obj_remove_event_cb(lv_scr_act(), global_touch_event_handler); // remove LV_EVENT_GET_SELF_SIZE
        lv_obj_remove_event_cb(lv_scr_act(), global_touch_event_handler); // remove LV_EVENT_DRAW_POST
        xTimerStop(lock_screen_timer, 0);
        xTimerDelete(lock_screen_timer, 0);
        lock_screen_timer = NULL;
    }
}

/* wallet page */
ctrl_home_network_data_t *ctrl_home_list_networks(void)
{
    if (network_data == NULL)
    {
        ctrl_home_network_data_t *network_data_ethereum = NULL;
        {
            ctrl_home_network_data_t *network_data_temp = (ctrl_home_network_data_t *)malloc(sizeof(ctrl_home_network_data_t));
            memset(network_data_temp, 0, sizeof(ctrl_home_network_data_t));
            {
                network_data_temp->type = CTRL_HOME_NETWORK_TYPE_ETH;
                network_data_temp->icon = &logo_ethereum;
                strcpy(network_data_temp->name, "Ethereum");
                Wallet _wallet = wallet_derive_eth(wallet, 0);
                char walletAddress[43];
                wallet_get_eth_address(_wallet, walletAddress);
                strcpy(network_data_temp->address, walletAddress);
                network_data_temp->wallet_main = wallet;
                network_data_temp->wallet_current = _wallet;
                ctrl_home_compatible_wallet_group_t *compatible_wallet_group = (ctrl_home_compatible_wallet_group_t *)malloc(sizeof(ctrl_home_compatible_wallet_group_t));
                {
                    memset(compatible_wallet_group, 0, sizeof(ctrl_home_compatible_wallet_group_t));
                    compatible_wallet_group->qr_type = CTRL_HOME_CONNECT_QR_TYPE_METAMASK;
                    {
                        // MetaMask
                        ctrl_home_3rd_wallet_info_t *wallet_info_3rd_metamask = NULL;
                        {
                            ctrl_home_3rd_wallet_info_t *wallet_info_3rd_tmp = (ctrl_home_3rd_wallet_info_t *)malloc(sizeof(ctrl_home_3rd_wallet_info_t));
                            memset(wallet_info_3rd_tmp, 0, sizeof(ctrl_home_3rd_wallet_info_t));
                            strcpy(wallet_info_3rd_tmp->name, "MetaMask");
                            wallet_info_3rd_tmp->icon = &wallet_metamask;
                            wallet_info_3rd_tmp->next = NULL;
                            wallet_info_3rd_metamask = wallet_info_3rd_tmp;
                        }
                        // imToken
                        ctrl_home_3rd_wallet_info_t *wallet_info_3rd_imtoken = NULL;
                        {
                            ctrl_home_3rd_wallet_info_t *wallet_info_3rd_tmp = (ctrl_home_3rd_wallet_info_t *)malloc(sizeof(ctrl_home_3rd_wallet_info_t));
                            memset(wallet_info_3rd_tmp, 0, sizeof(ctrl_home_3rd_wallet_info_t));
                            strcpy(wallet_info_3rd_tmp->name, "imToken");
                            wallet_info_3rd_tmp->icon = &wallet_imtoken;
                            wallet_info_3rd_tmp->next = NULL;
                            wallet_info_3rd_imtoken = wallet_info_3rd_tmp;
                        }
                        // Rabby
                        ctrl_home_3rd_wallet_info_t *wallet_info_3rd_rabby = NULL;
                        {
                            ctrl_home_3rd_wallet_info_t *wallet_info_3rd_tmp = (ctrl_home_3rd_wallet_info_t *)malloc(sizeof(ctrl_home_3rd_wallet_info_t));
                            memset(wallet_info_3rd_tmp, 0, sizeof(ctrl_home_3rd_wallet_info_t));
                            strcpy(wallet_info_3rd_tmp->name, "Rabby");
                            wallet_info_3rd_tmp->icon = &wallet_rabby;
                            wallet_info_3rd_tmp->next = NULL;
                            wallet_info_3rd_rabby = wallet_info_3rd_tmp;
                        }
                        compatible_wallet_group->wallet_info_3rd = wallet_info_3rd_metamask;
                        wallet_info_3rd_metamask->next = wallet_info_3rd_imtoken;
                        wallet_info_3rd_imtoken->next = wallet_info_3rd_rabby;
                    }
                    compatible_wallet_group->next = NULL;
                }
                network_data_temp->compatible_wallet_group = compatible_wallet_group;
                network_data_temp->next = NULL;
            }
            network_data_ethereum = network_data_temp;
        }

        ctrl_home_network_data_t *network_data_bitcoin_segwit = NULL;
        {
            ctrl_home_network_data_t *network_data_temp = (ctrl_home_network_data_t *)malloc(sizeof(ctrl_home_network_data_t));
            memset(network_data_temp, 0, sizeof(ctrl_home_network_data_t));
            {
                network_data_temp->type = CTRL_HOME_NETWORK_TYPE_ETH;
                network_data_temp->icon = &logo_bitcoin;
                strcpy(network_data_temp->name, "Bitcoin segwit");
                Wallet _wallet = wallet_derive_btc(wallet, 0);
                char walletAddress[43];
                wallet_get_btc_address_segwit(_wallet, walletAddress);
                strcpy(network_data_temp->address, walletAddress);
                network_data_temp->wallet_main = wallet;
                network_data_temp->wallet_current = _wallet;
                network_data_temp->compatible_wallet_group = NULL;
                network_data_temp->next = NULL;
            }
            network_data_bitcoin_segwit = network_data_temp;
        }

        ctrl_home_network_data_t *network_data_bitcoin_legacy = NULL;
        {
            ctrl_home_network_data_t *network_data_temp = (ctrl_home_network_data_t *)malloc(sizeof(ctrl_home_network_data_t));
            memset(network_data_temp, 0, sizeof(ctrl_home_network_data_t));
            {
                network_data_temp->type = CTRL_HOME_NETWORK_TYPE_ETH;
                network_data_temp->icon = &logo_bitcoin;
                strcpy(network_data_temp->name, "Bitcoin legacy");
                Wallet _wallet = wallet_derive_btc(wallet, 0);
                char walletAddress[43];
                wallet_get_btc_address_legacy(_wallet, walletAddress);
                strcpy(network_data_temp->address, walletAddress);
                network_data_temp->wallet_main = wallet;
                network_data_temp->wallet_current = _wallet;
                network_data_temp->compatible_wallet_group = NULL;
                network_data_temp->next = NULL;
            }
            network_data_bitcoin_legacy = network_data_temp;
        }

        network_data_ethereum->next = network_data_bitcoin_segwit;
        network_data_bitcoin_segwit->next = network_data_bitcoin_legacy;
        network_data = network_data_ethereum;
    }
    return network_data;
}
char *ctrl_home_get_connect_qrcode(ctrl_home_network_data_t *network, ctrl_home_connect_qr_type qr_type)
{
    char *hdkey = NULL;
    generate_metamask_crypto_hdkey(network->wallet_main, &hdkey);
    return hdkey;
}

/* scanner page */
void ctrl_home_scan_qr_start(lv_obj_t *image, lv_obj_t *progress_bar)
{
    scan_task_status_request = true;
    ctrl_home_scan_qr_data_t *scan_qr_data = (ctrl_home_scan_qr_data_t *)malloc(sizeof(ctrl_home_scan_qr_data_t));
    scan_qr_data->image = image;
    scan_qr_data->progress_bar = progress_bar;
    xTaskCreatePinnedToCore(qrScannerTask, "qrScannerTask", 4 * 1024, scan_qr_data, 10, NULL, MCU_CORE1);
}
void ctrl_home_scan_qr_stop(void)
{
    scan_task_status_request = false;
}
void ctrl_home_lock_screen(void)
{
    xEventGroupSetBits(event_group_global, EVENT_LOCK_SCREEN);
}
