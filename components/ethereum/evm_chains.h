#ifndef EVM_CHAINS_H
#define EVM_CHAINS_H

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
    char *chain_name;
    char *symbol;
} evm_chain_info_t;

#ifdef __cplusplus
extern "C"
{
#endif
    /**********************
     * GLOBAL PROTOTYPES
     **********************/
    evm_chain_info_t *evm_chain_name_from_int(long long chain_id);
    evm_chain_info_t *evm_chain_name_from_str(char *chain_id_str);

#ifdef __cplusplus
}
#endif

#endif // EVM_CHAINS_H
