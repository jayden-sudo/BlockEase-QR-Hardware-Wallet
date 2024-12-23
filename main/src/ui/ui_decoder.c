/*********************
 *      INCLUDES
 *********************/
#include "ui/ui_decoder.h"
#include "ui/ui_style.h"
#include "esp_log.h"
#include "ui/ui_master_page.h"
#include "alloc_utils.h"
#include "ui/ui_events.h"
#include "wallet_db.h"
#include "ui/ui_panic.h"
#include "ui/ui_pin.h"

/*********************
 *      DEFINES
 *********************/
#define TAG "UI_DECODER"

/**********************
 *  STATIC VARIABLES
 **********************/
static alloc_utils_memory_struct *alloc_utils_memory_struct_pointer;
static Wallet wallet;
static lv_obj_t *event_target = NULL;
static ui_master_page_t *master_page = NULL;
static lv_obj_t *container = NULL;
static int32_t container_width = 0;
static int32_t container_height = 0;
static lv_obj_t *sign_btn = NULL;
static lv_obj_t *transaction_detail_container = NULL;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void ui_event_handler(lv_event_t *e);
static char *verify_pin(char *pin_str);
static void transaction_decoder(qrcode_protocol_bc_ur_data_t *_qrcode_protocol_bc_ur_data);

/**********************
 * GLOBAL PROTOTYPES
 **********************/
void ui_decoder_init(Wallet _wallet, qrcode_protocol_bc_ur_data_t *_qrcode_protocol_bc_ur_data, lv_obj_t *event_target);
void ui_decoder_destroy(void);

/**********************
 *   STATIC FUNCTIONS
 **********************/
static void ui_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED)
    {
        // sign transaction

        wallet_data_version_1_t walletData;
        if (wallet_db_load_wallet_data(&walletData) == false)
        {
            ui_panic("Can't load wallet data", PANIC_REBOOT);
            return;
        }
        if (walletData.signPinRequired)
        {
            ui_pin_verify(container, container_width, container_height, NULL, verify_pin);
        }
        else
        {
            lv_async_call(ui_decoder_destroy, NULL);
            lv_obj_send_event(event_target, UI_EVENT_DECODER_CONFIRM, NULL);
        }
    }
    else if (code == UI_EVENT_MASTER_PAGE_CLOSE_BUTTON_CLICKED)
    {
        ui_master_page_set_close_button_visibility(false, master_page);
        lv_async_call(ui_decoder_destroy, NULL);
        lv_obj_send_event(event_target, UI_EVENT_DECODER_CANCEL, NULL);
    }
}
static char *verify_pin(char *pin_str)
{
    char *ret = wallet_db_verify_pin(pin_str);
    if (ret == NULL)
    {
        free(wallet_db_pop_private_key());
        lv_async_call(ui_decoder_destroy, NULL);
        lv_obj_send_event(event_target, UI_EVENT_DECODER_CONFIRM, NULL);
    }
    return ret;
}
static void transaction_decoder(qrcode_protocol_bc_ur_data_t *_qrcode_protocol_bc_ur_data)
{
    lv_obj_t *label = lv_label_create(transaction_detail_container);
    lv_label_set_text(label, "Transaction decode not implemented yet");
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_size(label, LV_PCT(100), LV_SIZE_CONTENT);

    lv_obj_clear_state(sign_btn, LV_STATE_DISABLED);
}

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
void ui_decoder_init(Wallet _wallet, qrcode_protocol_bc_ur_data_t *_qrcode_protocol_bc_ur_data, lv_obj_t *_event_target)
{
    ui_init_events();
    event_target = _event_target;
    wallet = _wallet;

    ALLOC_UTILS_INIT_MEMORY_STRUCT(alloc_utils_memory_struct_pointer);

    if (lvgl_port_lock(0))
    {
        lv_obj_add_event_cb(event_target, ui_event_handler, UI_EVENT_MASTER_PAGE_CLOSE_BUTTON_CLICKED, NULL);
        ALLOC_UTILS_MALLOC_MEMORY(alloc_utils_memory_struct_pointer, master_page, sizeof(ui_master_page_t));
        ui_master_page_init(NULL, event_target, false, true, "Transaction", master_page);
        lv_obj_t *_container = ui_master_page_get_container(master_page);
        ui_master_page_get_container_size(master_page, &container_width, &container_height);

        container = lv_obj_create(_container);
        NO_BODER_PADDING_STYLE(container);
        lv_obj_set_size(container, container_width, container_height);
        lv_obj_set_scrollbar_mode(container, LV_SCROLLBAR_MODE_OFF);
        int32_t width_col = container_width * 0.9;
        lv_obj_t *cont_col = lv_obj_create(container);
        NO_BODER_PADDING_STYLE(cont_col);
        lv_obj_set_size(cont_col, width_col, LV_SIZE_CONTENT);
        lv_obj_center(cont_col);
        lv_obj_set_flex_flow(cont_col, LV_FLEX_FLOW_COLUMN);

        int footer_height = 60;
        transaction_detail_container = lv_obj_create(cont_col);
        NO_BODER_PADDING_STYLE(transaction_detail_container);
        lv_obj_set_size(transaction_detail_container, LV_PCT(100), container_height - footer_height);

        sign_btn = lv_button_create(cont_col);
        lv_obj_set_size(sign_btn, LV_PCT(100), footer_height * 0.8);
        lv_obj_t *label = lv_label_create(sign_btn);
        lv_label_set_text(label, "Sign");
        lv_obj_center(label);
        lv_obj_add_state(sign_btn, LV_STATE_DISABLED);
        lv_obj_add_event_cb(sign_btn, ui_event_handler, LV_EVENT_CLICKED, NULL);
        transaction_decoder(_qrcode_protocol_bc_ur_data);
    }
    lvgl_port_unlock();
}
void ui_decoder_destroy()
{
    wallet = NULL;

    ui_pin_destroy();

    if (lvgl_port_lock(0))
    {
        if (container != NULL)
        {
            lv_obj_del(container);
            container = NULL;
        }
        lvgl_port_unlock();
    }

    ui_master_page_destroy(master_page);

    ALLOC_UTILS_FREE_MEMORY(alloc_utils_memory_struct_pointer);

    master_page = NULL;
}
