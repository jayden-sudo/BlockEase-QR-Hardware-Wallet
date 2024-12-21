#ifndef ERC20_H
#define ERC20_H

/*********************
 *      INCLUDES
 *********************/
#include <string.h>

/**********************
 *      TYPEDEFS
 **********************/
typedef struct
{
    long long chain_id;
    // char *name;
    char *symbol;
    char *address;
    int8_t decimal;
} erc20_info_t;

#ifdef __cplusplus
extern "C"
{
#endif
    /**********************
     * GLOBAL PROTOTYPES
     **********************/
    erc20_info_t *get_erc20_info_from_int_chain_id(char *address, long long chain_id);
    erc20_info_t *get_erc20_info_from_str_chain_id(char *address, char *chain_id_str);

#ifdef __cplusplus
}
#endif

#endif // ERC20_H
