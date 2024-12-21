/*********************
 *      INCLUDES
 *********************/
#include "controller/ctrl_sign.h"
#include <esp_log.h>
#include <string.h>
#include <stdlib.h>
#include "base64url.h"
#include "transaction_factory.h"
#include "ui/ui_decoder.h"
#include "ui/ui_events.h"
#include "ui/ui_qr_code.h"
#include "ui/ui_loading.h"
#include "ethereum.h"
#include "cJSON.h"
#include "evm_chains.h"
#include <math.h>

/*********************
 *      DEFINES
 *********************/
#define TAG "ctrl_sign"

/**********************
 *  STATIC VARIABLES
 **********************/
static Wallet wallet = 0;
static qrcode_protocol_bc_ur_data_t *qrcode_protocol_bc_ur_data;
static lv_obj_t *event_target = NULL;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void ui_event_handler(lv_event_t *e);
static char *ctrl_sign_get_signature(void);
static char *ctrl_sign_decode_transaction(void);
static void show_qr_signature(char *arg);

/**********************
 * GLOBAL PROTOTYPES
 **********************/
void ctrl_sign_init(Wallet wallet, qrcode_protocol_bc_ur_data_t *qrcode_protocol_bc_ur_data);
void ctrl_sign_destroy();

/**********************
 *   STATIC FUNCTIONS
 **********************/
static void ui_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == UI_EVENT_DECODER_CANCEL)
    {
        lv_async_call(ctrl_sign_destroy, NULL);
    }
    else if (code == UI_EVENT_DECODER_CONFIRM)
    {
        ui_loading_show();
        lv_async_call(show_qr_signature, NULL);
    }
}
static void show_qr_signature(char *arg)
{
    char *signature = ctrl_sign_get_signature();
    ctrl_sign_destroy();
    ui_qr_code_init("Signature", "Scan the QR code to send transaction", signature, NULL);
    free(signature);
    ui_loading_hide();
}

static char *ctrl_sign_decode_transaction(void)
{
    /*
            if(decode_transaction==true):
                # if Ethereum
                ChainId: {chainId}
                Chain name: {chainName}

                From: {address}
                Path: {path}
                Interacted With: {address} {(address info)}

                Native token transfer|ERC20 transfer|ERC20 approve|Sign message|Unknown

                - Transfer {amount} ETH to {to}
                - Transfer {amount} {symbol} to {to}
                - Approve {amount} {symbol} to {spender}
                - {message}
                - {Hex data}

                Value: {value} ETH
                Max Gas Fee: {fee} ETH
                Input Data:{input data}
         */

    char *decoded_str = NULL;
    // #TODO
    // const char *type = qrcode_protocol_bc_ur_type(qrcode_protocol_bc_ur_data);
    // ESP_LOGI(TAG, "type: %s", type);
    // if (strcmp(type, METAMASK_ETH_SIGN_REQUEST) == 0)
    // {
    //     metamask_sign_request_t request;
    //     int err = decode_metamask_sign_request(qrcode_protocol_bc_ur_data->ur, &request);
    //     if (err == 0)
    //     {
    //         size_t sign_data_max_len = strlen(request.sign_data_base64url);
    //         uint8_t *sign_data = (uint8_t *)malloc(sign_data_max_len + 1);
    //         size_t sign_data_len = decode_base64url(request.sign_data_base64url, sign_data, sign_data_max_len);
    //         sign_data[sign_data_len] = '\0';
    //         Wallet account = wallet_derive(wallet, request.derivation_path);
    //         char account_address[43];
    //         wallet_get_eth_address(account, account_address);
    //         if (strcmp(account_address, request.address) != 0)
    //         {
    //             decoded_str = malloc(1024);
    //             sprintf(decoded_str, "ERROR: Invalid address");
    //         }
    //         else
    //         {
    //             decoded_str = malloc(sign_data_max_len + 1024);
    //             decoded_str[0] = '\0';
    //             /*
    //                 From: {address}
    //                   Path: {path}
    //              */
    //             sprintf(decoded_str, "From: %s\n\tPath: %s\n", account_address, request.derivation_path);

    //             if (request.data_type == KEY_DATA_TYPE_SIGN_TYPED_TRANSACTION)
    //             {
    //                 ESP_LOGI(TAG, "sign typed transaction");
    //                 transaction_data_t *transaction_data = (transaction_data_t *)malloc(sizeof(transaction_data_t));
    //                 transaction_factory_init(transaction_data, sign_data, sign_data_len);

    //                 if (transaction_data->error == 0)
    //                 {
    //                     /*
    //                         Interacted With: {address}
    //                      */
    //                     strcat(decoded_str, "Interacted With: ");
    //                     strcat(decoded_str, (char *)transaction_data->to);
    //                     strcat(decoded_str, "\n");

    //                     /*
    //                         ChainId: {chainId}
    //                         Chain name: {chainName}
    //                      */
    //                     strcat(decoded_str, "ChainId: ");
    //                     strcat(decoded_str, (char *)transaction_data->chainId);
    //                     strcat(decoded_str, "\n");
    //                     strcat(decoded_str, "Chain name: ");
    //                     evm_chain_info_t *chain_info = evm_chain_name_from_str((char *)transaction_data->chainId);
    //                     strcat(decoded_str, chain_info->chain_name);
    //                     strcat(decoded_str, "\n");
    //                 }
    //                 else
    //                 {
    //                     sprintf(decoded_str, "ERROR: Failed to create transaction factory");
    //                 }
    //                 transaction_factory_free(transaction_data);
    //                 free(transaction_data);
    //             }
    //             else if (request.data_type == KEY_DATA_TYPE_SIGN_PERSONAL_MESSAGE)
    //             {
    //                 ESP_LOGI(TAG, "sign personal message");
    //                 uint8_t signature[65];
    //                 uint8_t digest[32];
    //                 const char *sign_data_str = (const char *)sign_data;
    //                 ethereum_keccak256_eip191(sign_data_str, strlen(sign_data_str), digest);
    //                 wallet_eth_sign(account, digest, signature);
    //                 size_t uuid_max_len = strlen(request.uuid_base64url);
    //                 uint8_t *uuid = (uint8_t *)malloc(uuid_max_len);
    //                 decode_base64url(request.uuid_base64url, uuid, uuid_max_len);
    //                 free(uuid);
    //             }
    //             else if (request.data_type == KEY_DATA_TYPE_SIGN_TYPED_DATA)
    //             {
    //                 ESP_LOGE(TAG, "sign typed data");
    //                 cJSON *json = cJSON_Parse((char *)sign_data);
    //                 // get domain
    //                 char *chainId_str = NULL;
    //                 char *name_str = NULL;
    //                 char *verifyingContract_str = NULL;

    //                 char *primaryType_str = NULL;
    //                 char *message_str = NULL;
    //                 char *types_str = NULL;
    //                 char *domain_str = NULL;

    //                 cJSON *primaryType = cJSON_GetObjectItemCaseSensitive(json, "primaryType");
    //                 cJSON *types = cJSON_GetObjectItemCaseSensitive(json, "types");
    //                 cJSON *domain = cJSON_GetObjectItemCaseSensitive(json, "domain");
    //                 cJSON *message = cJSON_GetObjectItemCaseSensitive(json, "message");
    //                 if (primaryType != NULL && types != NULL && domain != NULL && message != NULL)
    //                 {
    //                     // get chainid
    //                     cJSON *chainId = cJSON_GetObjectItemCaseSensitive(domain, "chainId");
    //                     cJSON *name = cJSON_GetObjectItemCaseSensitive(domain, "name");
    //                     cJSON *verifyingContract = cJSON_GetObjectItemCaseSensitive(domain, "verifyingContract");
    //                     if (
    //                         (cJSON_GetStringValue(chainId) != NULL || !isnan(cJSON_GetNumberValue(chainId))) &&
    //                         cJSON_GetStringValue(name) != NULL && cJSON_GetStringValue(verifyingContract) != NULL)
    //                     {
    //                         if (cJSON_GetStringValue(chainId) != NULL)
    //                         {
    //                             chainId_str = malloc(strlen(chainId->valuestring) + 1);
    //                             strcpy(chainId_str, chainId->valuestring);
    //                         }
    //                         else
    //                         {
    //                             chainId_str = malloc(33);
    //                             sprintf(chainId_str, "%f", cJSON_GetNumberValue(chainId));
    //                         }

    //                         char *_name = cJSON_GetStringValue(name);
    //                         name_str = malloc(strlen(_name) + 1);
    //                         strcpy(name_str, _name);

    //                         char *_verifyingContract = cJSON_GetStringValue(verifyingContract);
    //                         verifyingContract_str = malloc(strlen(_verifyingContract) + 1);
    //                         strcpy(verifyingContract_str, _verifyingContract);

    //                         cJSON *primaryTypeObject = cJSON_CreateObject();
    //                         cJSON_AddItemToObject(primaryTypeObject, "primaryType", cJSON_Duplicate(primaryType, 1));
    //                         primaryType_str = cJSON_Print(primaryTypeObject);
    //                         cJSON_Delete(primaryTypeObject);

    //                         cJSON *typesObject = cJSON_CreateObject();
    //                         cJSON_AddItemToObject(typesObject, "types", cJSON_Duplicate(types, 1));
    //                         types_str = cJSON_Print(typesObject);
    //                         cJSON_Delete(typesObject);

    //                         cJSON *domainObject = cJSON_CreateObject();
    //                         cJSON_AddItemToObject(domainObject, "domain", cJSON_Duplicate(domain, 1));
    //                         // if domainObject.chainId is not a string, change it to string
    //                         if (cJSON_GetStringValue(chainId) == NULL)
    //                         {
    //                             cJSON_DeleteItemFromObject(domainObject, "chainId");
    //                             cJSON_AddStringToObject(domainObject, "chainId", chainId_str);
    //                         }
    //                         domain_str = cJSON_Print(domainObject);
    //                         cJSON_Delete(domainObject);

    //                         cJSON *messageObject = cJSON_CreateObject();
    //                         cJSON_AddItemToObject(messageObject, "message", cJSON_Duplicate(message, 1));
    //                         message_str = cJSON_Print(messageObject);
    //                         cJSON_Delete(messageObject);
    //                     }
    //                     else
    //                     {
    //                         ESP_LOGE(TAG, "Failed to get chainId, name, verifyingContract");
    //                     }
    //                 }
    //                 else
    //                 {
    //                     ESP_LOGE(TAG, "Failed to get primaryType, types, domain, message");
    //                 }

    //                 if (chainId_str)
    //                 {
    //                     uint8_t typed_data_hash[32];
    //                     ethereum_typed_data_hash_v4(primaryType_str, types_str, domain_str, message_str, typed_data_hash);
    //                     // ESP_LOG_BUFFER_HEXDUMP(TAG, typed_data_hash, 32, ESP_LOG_INFO);
    //                     uint8_t signature[65];
    //                     wallet_eth_sign(account, typed_data_hash, signature);
    //                     size_t uuid_max_len = strlen(request.uuid_base64url);
    //                     uint8_t *uuid = (uint8_t *)malloc(uuid_max_len);
    //                     decode_base64url(request.uuid_base64url, uuid, uuid_max_len);
    //                     generate_metamask_eth_signature(uuid, signature, &qr_code_str);
    //                     free(uuid);
    //                 }

    //                 free(chainId_str);
    //                 free(name_str);
    //                 free(verifyingContract_str);
    //                 free(primaryType_str);
    //                 free(message_str);
    //                 free(types_str);
    //                 free(domain_str);

    //                 chainId_str = NULL;
    //                 name_str = NULL;
    //                 verifyingContract_str = NULL;
    //                 primaryType_str = NULL;
    //                 message_str = NULL;
    //                 types_str = NULL;
    //                 domain_str = NULL;

    //                 cJSON_Delete(json);
    //                 json = NULL;
    //             }
    //             else
    //             {
    //                 sprintf(decoded_str, "ERROR: Invalid data type: %ld", request.data_type);
    //             }
    //         }
    //         wallet_free(account);
    //         free(sign_data);
    //     }
    //     else
    //     {
    //         decoded_str = malloc(1024);
    //         sprintf(decoded_str, "ERROR: decode_metamask_sign_typed_transaction_request error: %d", err);
    //     }
    // }
    // else
    // {
    //     decoded_str = malloc(1024);
    //     sprintf(decoded_str, "ERROR: Unsupported type: %s", type);
    // }
    return decoded_str;
}
static char *ctrl_sign_get_signature()
{
    char *qr_code_str = NULL;
    const char *type = qrcode_protocol_bc_ur_type(qrcode_protocol_bc_ur_data);
    ESP_LOGI(TAG, "type: %s", type);
    if (strcmp(type, METAMASK_ETH_SIGN_REQUEST) == 0)
    {
        metamask_sign_request_t request;
        int err = decode_metamask_sign_request(qrcode_protocol_bc_ur_data->ur, &request);
        if (err == 0)
        {
            size_t sign_data_max_len = strlen(request.sign_data_base64url);
            uint8_t *sign_data = (uint8_t *)malloc(sign_data_max_len + 1);
            size_t sign_data_len = decode_base64url(request.sign_data_base64url, sign_data, sign_data_max_len);
            sign_data[sign_data_len] = '\0';
            Wallet account = wallet_derive(wallet, request.derivation_path);
            char account_address[43];
            wallet_get_eth_address(account, account_address);
            if (strcmp(account_address, request.address) != 0)
            {
                ESP_LOGE(TAG, "Invalid address");
            }
            else
            {
                if (request.data_type == KEY_DATA_TYPE_SIGN_TYPED_TRANSACTION)
                {
                    ESP_LOGI(TAG, "sign typed transaction");
                    transaction_data_t *transaction_data = (transaction_data_t *)malloc(sizeof(transaction_data_t));
                    transaction_factory_init(transaction_data, sign_data, sign_data_len);

                    if (transaction_data->error == 0)
                    {
                        ESP_LOGI(TAG, "Signing transaction...");
                        uint8_t signature[65];
                        uint8_t digest[32];
                        ethereum_keccak256(sign_data, sign_data_len, digest);
                        wallet_eth_sign(account, digest, signature);
                        size_t uuid_max_len = strlen(request.uuid_base64url);
                        uint8_t *uuid = (uint8_t *)malloc(uuid_max_len);
                        decode_base64url(request.uuid_base64url, uuid, uuid_max_len);
                        generate_metamask_eth_signature(uuid, signature, &qr_code_str);
                        free(uuid);
                    }
                    else
                    {
                        ESP_LOGE(TAG, "Failed to create transaction factory");
                    }
                    transaction_factory_free(transaction_data);
                    free(transaction_data);
                }
                else if (request.data_type == KEY_DATA_TYPE_SIGN_PERSONAL_MESSAGE)
                {
                    ESP_LOGI(TAG, "sign personal message");
                    uint8_t signature[65];
                    uint8_t digest[32];
                    const char *sign_data_str = (const char *)sign_data;
                    ethereum_keccak256_eip191(sign_data_str, strlen(sign_data_str), digest);
                    wallet_eth_sign(account, digest, signature);
                    size_t uuid_max_len = strlen(request.uuid_base64url);
                    uint8_t *uuid = (uint8_t *)malloc(uuid_max_len);
                    decode_base64url(request.uuid_base64url, uuid, uuid_max_len);
                    generate_metamask_eth_signature(uuid, signature, &qr_code_str);
                    free(uuid);
                }
                else if (request.data_type == KEY_DATA_TYPE_SIGN_TYPED_DATA)
                {
                    ESP_LOGE(TAG, "sign typed data");
                    cJSON *json = cJSON_Parse((char *)sign_data);
                    // get domain
                    char *chainId_str = NULL;
                    char *name_str = NULL;
                    char *verifyingContract_str = NULL;

                    char *primaryType_str = NULL;
                    char *message_str = NULL;
                    char *types_str = NULL;
                    char *domain_str = NULL;

                    cJSON *primaryType = cJSON_GetObjectItemCaseSensitive(json, "primaryType");
                    cJSON *types = cJSON_GetObjectItemCaseSensitive(json, "types");
                    cJSON *domain = cJSON_GetObjectItemCaseSensitive(json, "domain");
                    cJSON *message = cJSON_GetObjectItemCaseSensitive(json, "message");
                    if (primaryType != NULL && types != NULL && domain != NULL && message != NULL)
                    {
                        // get chainid
                        cJSON *chainId = cJSON_GetObjectItemCaseSensitive(domain, "chainId");
                        cJSON *name = cJSON_GetObjectItemCaseSensitive(domain, "name");
                        cJSON *verifyingContract = cJSON_GetObjectItemCaseSensitive(domain, "verifyingContract");
                        if (
                            (cJSON_GetStringValue(chainId) != NULL || !isnan(cJSON_GetNumberValue(chainId))) &&
                            cJSON_GetStringValue(name) != NULL && cJSON_GetStringValue(verifyingContract) != NULL)
                        {
                            if (cJSON_GetStringValue(chainId) != NULL)
                            {
                                chainId_str = malloc(strlen(chainId->valuestring) + 1);
                                strcpy(chainId_str, chainId->valuestring);
                            }
                            else
                            {
                                chainId_str = malloc(33);
                                sprintf(chainId_str, "%f", cJSON_GetNumberValue(chainId));
                            }

                            char *_name = cJSON_GetStringValue(name);
                            name_str = malloc(strlen(_name) + 1);
                            strcpy(name_str, _name);

                            char *_verifyingContract = cJSON_GetStringValue(verifyingContract);
                            verifyingContract_str = malloc(strlen(_verifyingContract) + 1);
                            strcpy(verifyingContract_str, _verifyingContract);

                            cJSON *primaryTypeObject = cJSON_CreateObject();
                            cJSON_AddItemToObject(primaryTypeObject, "primaryType", cJSON_Duplicate(primaryType, 1));
                            primaryType_str = cJSON_Print(primaryTypeObject);
                            cJSON_Delete(primaryTypeObject);

                            cJSON *typesObject = cJSON_CreateObject();
                            cJSON_AddItemToObject(typesObject, "types", cJSON_Duplicate(types, 1));
                            types_str = cJSON_Print(typesObject);
                            cJSON_Delete(typesObject);

                            cJSON *domainObject = cJSON_CreateObject();
                            cJSON_AddItemToObject(domainObject, "domain", cJSON_Duplicate(domain, 1));
                            // if domainObject.chainId is not a string, change it to string
                            if (cJSON_GetStringValue(chainId) == NULL)
                            {
                                cJSON_DeleteItemFromObject(domainObject, "chainId");
                                cJSON_AddStringToObject(domainObject, "chainId", chainId_str);
                            }
                            domain_str = cJSON_Print(domainObject);
                            cJSON_Delete(domainObject);

                            cJSON *messageObject = cJSON_CreateObject();
                            cJSON_AddItemToObject(messageObject, "message", cJSON_Duplicate(message, 1));
                            message_str = cJSON_Print(messageObject);
                            cJSON_Delete(messageObject);
                        }
                        else
                        {
                            ESP_LOGE(TAG, "Failed to get chainId, name, verifyingContract");
                        }
                    }
                    else
                    {
                        ESP_LOGE(TAG, "Failed to get primaryType, types, domain, message");
                    }

                    if (chainId_str)
                    {
                        uint8_t typed_data_hash[32];
                        ethereum_typed_data_hash_v4(primaryType_str, types_str, domain_str, message_str, typed_data_hash);
                        // ESP_LOG_BUFFER_HEXDUMP(TAG, typed_data_hash, 32, ESP_LOG_INFO);
                        uint8_t signature[65];
                        wallet_eth_sign(account, typed_data_hash, signature);
                        size_t uuid_max_len = strlen(request.uuid_base64url);
                        uint8_t *uuid = (uint8_t *)malloc(uuid_max_len);
                        decode_base64url(request.uuid_base64url, uuid, uuid_max_len);
                        generate_metamask_eth_signature(uuid, signature, &qr_code_str);
                        free(uuid);
                    }

                    free(chainId_str);
                    free(name_str);
                    free(verifyingContract_str);
                    free(primaryType_str);
                    free(message_str);
                    free(types_str);
                    free(domain_str);

                    chainId_str = NULL;
                    name_str = NULL;
                    verifyingContract_str = NULL;
                    primaryType_str = NULL;
                    message_str = NULL;
                    types_str = NULL;
                    domain_str = NULL;

                    cJSON_Delete(json);
                    json = NULL;
                }
                else
                {
                    ESP_LOGE(TAG, "Invalid data type: %ld", request.data_type);
                }
            }
            wallet_free(account);
            free(sign_data);
        }
        else
        {
            ESP_LOGE(TAG, "decode_metamask_sign_typed_transaction_request error: %d", err);
        }
    }
    else
    {
        ESP_LOGI(TAG, "Unsupported type: %s", type);
    }
    return qr_code_str;
}

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
void ctrl_sign_init(Wallet _wallet, qrcode_protocol_bc_ur_data_t *_qrcode_protocol_bc_ur_data)
{
    ui_init_events();

    wallet = _wallet;
    qrcode_protocol_bc_ur_data = _qrcode_protocol_bc_ur_data;

    if (lvgl_port_lock(0))
    {
        event_target = lv_obj_create(lv_scr_act());
        lv_obj_add_flag(event_target, LV_OBJ_FLAG_HIDDEN);
        lvgl_port_unlock();
    }

    lv_obj_add_event_cb(event_target, ui_event_handler, UI_EVENT_DECODER_CANCEL, NULL);
    lv_obj_add_event_cb(event_target, ui_event_handler, UI_EVENT_DECODER_CONFIRM, NULL);

    ui_decoder_init(wallet, qrcode_protocol_bc_ur_data, event_target);
}
void ctrl_sign_destroy()
{
    if (qrcode_protocol_bc_ur_data != NULL)
    {
        qrcode_protocol_bc_ur_free(qrcode_protocol_bc_ur_data);
        free(qrcode_protocol_bc_ur_data);
        qrcode_protocol_bc_ur_data = NULL;
    }
    if (event_target != NULL)
    {
        if (lvgl_port_lock(0))
        {
            lv_obj_del(event_target);
            lvgl_port_unlock();
            event_target = NULL;
        }
    }
    ui_qr_code_destroy();
    ui_loading_hide();
}