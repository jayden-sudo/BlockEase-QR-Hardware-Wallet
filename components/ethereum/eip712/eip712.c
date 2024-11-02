/*
    fork from https://github.com/keepkey/keepkey-firmware/blob/master/lib/firmware/eip712.c
*/

/*
 * Copyright (c) 2022 markrypto  (cryptoakorn@gmail.com)
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
    Produces hashes based on the metamask v4 rules. This is different from the EIP-712 spec
    in how arrays of structs are hashed but is compatable with metamask.
    See https://github.com/MetaMask/eth-sig-util/pull/107

    eip712 data rules:
    Parser wants to see C strings, not javascript strings:
        requires all complete json message strings to be enclosed by braces, i.e., { ... }
        Cannot have entire json string quoted, i.e., "{ ... }" will not work.
        Remove all quote escape chars, e.g., {"types":  not  {\"types\":
    ints: Strings representing ints must fit into a long size (64-bits).
        Note: Do not prefix ints or uints with 0x
    All hex and byte strings must be big-endian
    Byte strings and address should be prefixed by 0x
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "eip712.h"
#include "tiny-json.h"
#include "utility/trezor/sha3.h"
#include "utility/trezor/memzero.h"
#include "tiny-json.h"
#include <esp_log.h>

#define TAG "eip712"

#define USE_KECCAK 1
#define ADDRESS_SIZE 42
#define JSON_OBJ_POOL_SIZE 100
#define STRBUFSIZE 511
#define MAX_USERDEF_TYPES 10 // This is max number of user defined type allowed
#define MAX_TYPESTRING 33    // maximum size for a type string
#define MAX_ENCBYTEN_SIZE 66

typedef enum
{
    NOT_ENCODABLE = 0,
    ADDRESS,
    STRING,
    UINT,
    INT,
    BYTES,
    BYTES_N,
    BOOL,
    UDEF_TYPE,
    PREV_USERDEF,
    TOO_MANY_UDEFS
} basicType;

typedef enum
{
    DOMAIN = 1,
    MESSAGE
} dm;

// error list status
#define SUCCESS 1
#define NULL_MSG_HASH 2 // this is legal, not an error
#define GENERAL_ERROR 3
#define UDEF_NAME_ERROR 4
#define UDEFS_OVERFLOW 5
#define UDEF_ARRAY_NAME_ERR 6
#define ADDR_STRING_VFLOW 7
#define BYTESN_STRING_ERROR 8
#define BYTESN_SIZE_ERROR 9
#define INT_ARRAY_ERROR 10
#define BYTESN_ARRAY_ERROR 11
#define BOOL_ARRAY_ERROR 12
// #define STACK_TOO_SMALL     13        // reserved - defined in memory.h

#define JSON_PTYPENAMEERR 14
#define JSON_PTYPEVALERR 15
#define JSON_TYPESPROPERR 16
#define JSON_TYPE_SPROPERR 17
#define JSON_DPROPERR 18
#define MSG_NO_DS 19
#define JSON_MPROPERR 20
#define JSON_PTYPESOBJERR 21
#define JSON_TYPE_S_ERR 22
#define JSON_TYPE_S_NAMEERR 23
#define UNUSED_ERR_2 24 // available for re-use
#define JSON_NO_PAIRS 25
#define JSON_PAIRS_NOTEXT 26
#define JSON_NO_PAIRS_SIB 27
#define TYPE_NOT_ENCODABLE 28
#define JSON_NOPAIRVAL 29
#define JSON_NOPAIRNAME 30
#define JSON_TYPE_T_NOVAL 31
#define ADDR_STRING_NULL 32
#define JSON_TYPE_WNOVAL 33

#define LAST_ERROR JSON_TYPE_WNOVAL

typedef struct _TokenType
{
    const char *const address;
    const char *const ticker;
    uint8_t chain_id;
    uint8_t decimals;
} TokenType;

typedef struct
{
    char *eip712types;
    char *eip712primetype;
    char *eip712data;
    int eip712typevals;
} Ethereum712TypesValues;

typedef struct
{
    uint8_t domain_separator_hash[32];
    bool has_domain_separator_hash;
    bool has_msg_hash;
    bool has_message_hash;
    uint8_t message_hash[32];
} EthereumTypedDataSignature;

static const char *udefList[MAX_USERDEF_TYPES] = {0};
static dm confirmProp;

static const char *nameForValue;

void failMessage(int err)
{
    ESP_LOGE(TAG, "EIP-712 error: %d\n", err);
}

int encodableType(const char *typeStr)
{
    int ctr;

    if (0 == strncmp(typeStr, "address", sizeof("address") - 1))
    {
        return ADDRESS;
    }
    if (0 == strncmp(typeStr, "string", sizeof("string") - 1))
    {
        return STRING;
    }
    if (0 == strncmp(typeStr, "int", sizeof("int") - 1))
    {
        // This could be 'int8', 'int16', ..., 'int256'
        return INT;
    }
    if (0 == strncmp(typeStr, "uint", sizeof("uint") - 1))
    {
        // This could be 'uint8', 'uint16', ..., 'uint256'
        return UINT;
    }
    if (0 == strncmp(typeStr, "bytes", sizeof("bytes") - 1))
    {
        // This could be 'bytes', 'bytes1', ..., 'bytes32'
        if (0 == strcmp(typeStr, "bytes"))
        {
            return BYTES;
        }
        else
        {
            // parse out the length val
            uint8_t byteTypeSize = (uint8_t)(strtol((typeStr + 5), NULL, 10));
            if (byteTypeSize > 32)
            {
                return NOT_ENCODABLE;
            }
            else
            {
                return BYTES_N;
            }
        }
    }
    if (0 == strcmp(typeStr, "bool"))
    {
        return BOOL;
    }

    // See if type already defined. If so, skip, otherwise add it to list
    for (ctr = 0; ctr < MAX_USERDEF_TYPES; ctr++)
    {
        char typeNoArrTok[MAX_TYPESTRING] = {0};

        strncpy(typeNoArrTok, typeStr, sizeof(typeNoArrTok) - 1);
        strtok(typeNoArrTok, "["); // eliminate the array tokens if there

        if (udefList[ctr] != 0)
        {
            if (0 == strncmp(udefList[ctr], typeNoArrTok, strlen(udefList[ctr]) - strlen(typeNoArrTok)))
            {
                return PREV_USERDEF;
            }
            else
            {
            }
        }
        else
        {
            udefList[ctr] = typeStr;
            return UDEF_TYPE;
        }
    }
    if (ctr == MAX_USERDEF_TYPES)
    {
        return TOO_MANY_UDEFS;
    }

    return NOT_ENCODABLE; // not encodable
}

/*
    Entry:
            eip712Types points to eip712 json type structure to parse
            typeS points to the type to parse from jType
            typeStr points to caller allocated, zeroized string buffer of size STRBUFSIZE+1
    Exit:
            typeStr points to hashable type string
            returns error list status

    NOTE: reentrant!
*/
int parseType(const json_t *eip712Types, const char *typeS, char *typeStr)
{
    json_t const *tarray, *pairs;
    const json_t *jType;
    char append[STRBUFSIZE + 1] = {0};
    int encTest;
    const char *typeType = NULL;
    int errRet = SUCCESS;
    const json_t *obTest;
    const char *nameTest;
    const char *pVal;

    if (NULL == (jType = json_getProperty(eip712Types, typeS)))
    {
        errRet = JSON_TYPE_S_ERR;
        return errRet;
    }

    if (NULL == (nameTest = json_getName(jType)))
    {
        errRet = JSON_TYPE_S_NAMEERR;
        return errRet;
    }

    strncat(typeStr, nameTest, STRBUFSIZE - strlen((const char *)typeStr));
    strncat(typeStr, "(", STRBUFSIZE - strlen((const char *)typeStr));

    tarray = json_getChild(jType);
    while (tarray != 0)
    {
        if (NULL == (pairs = json_getChild(tarray)))
        {
            errRet = JSON_NO_PAIRS;
            return errRet;
        }
        // should be type JSON_TEXT
        if (pairs->type != JSON_TEXT)
        {
            errRet = JSON_PAIRS_NOTEXT;
            return errRet;
        }
        else
        {
            if (NULL == (obTest = json_getSibling(pairs)))
            {
                errRet = JSON_NO_PAIRS_SIB;
                return errRet;
            }
            typeType = json_getValue(obTest);
            encTest = encodableType(typeType);
            if (encTest == UDEF_TYPE)
            {
                // This is a user-defined type, parse it and append later
                if (']' == typeType[strlen(typeType) - 1])
                {
                    // array of structs. To parse name, remove array tokens.
                    char typeNoArrTok[MAX_TYPESTRING] = {0};
                    strncpy(typeNoArrTok, typeType, sizeof(typeNoArrTok) - 1);
                    if (strlen(typeNoArrTok) < strlen(typeType))
                    {
                        return UDEF_NAME_ERROR;
                    }

                    strtok(typeNoArrTok, "[");
                    // if (STACK_GOOD != (errRet = memcheck(STACK_SIZE_GUARD)))
                    // {
                    //     return errRet;
                    // }
                    if (SUCCESS != (errRet = parseType(eip712Types, typeNoArrTok, append)))
                    {
                        return errRet;
                    }
                }
                else
                {
                    // if (STACK_GOOD != (errRet = memcheck(STACK_SIZE_GUARD)))
                    // {
                    //     return errRet;
                    // }
                    if (SUCCESS != (errRet = parseType(eip712Types, typeType, append)))
                    {
                        return errRet;
                    }
                }
            }
            else if (encTest == TOO_MANY_UDEFS)
            {
                return UDEFS_OVERFLOW;
            }
            else if (encTest == NOT_ENCODABLE)
            {
                return TYPE_NOT_ENCODABLE;
            }

            if (NULL == (pVal = json_getValue(pairs)))
            {
                errRet = JSON_NOPAIRVAL;
                return errRet;
            }
            strncat(typeStr, typeType, STRBUFSIZE - strlen((const char *)typeStr));
            strncat(typeStr, " ", STRBUFSIZE - strlen((const char *)typeStr));
            strncat(typeStr, pVal, STRBUFSIZE - strlen((const char *)typeStr));
            strncat(typeStr, ",", STRBUFSIZE - strlen((const char *)typeStr));
        }
        tarray = json_getSibling(tarray);
    }
    // typeStr ends with a ',' unless there are no parameters to the type.
    if (typeStr[strlen(typeStr) - 1] == ',')
    {
        // replace last comma with a paren
        typeStr[strlen(typeStr) - 1] = ')';
    }
    else
    {
        // append paren, there are no parameters
        strncat(typeStr, ")", STRBUFSIZE - 1);
    }
    if (strlen(append) > 0)
    {
        strncat(typeStr, append, STRBUFSIZE - strlen((const char *)append));
    }

    return SUCCESS;
}

int encAddress(const char *string, uint8_t *encoded)
{
    unsigned ctr;
    char byteStrBuf[3] = {0};

    if (string == NULL)
    {
        return ADDR_STRING_NULL;
    }
    if (ADDRESS_SIZE < strlen(string))
    {
        return ADDR_STRING_VFLOW;
    }

    for (ctr = 0; ctr < 12; ctr++)
    {
        encoded[ctr] = '\0';
    }
    for (ctr = 12; ctr < 32; ctr++)
    {
        strncpy(byteStrBuf, &string[2 * ((ctr - 12)) + 2], 2);
        encoded[ctr] = (uint8_t)(strtol(byteStrBuf, NULL, 16));
    }
    return SUCCESS;
}

int encString(const char *string, uint8_t *encoded)
{
    struct SHA3_CTX strCtx;

    sha3_256_Init(&strCtx);
    sha3_Update(&strCtx, (const unsigned char *)string, (size_t)strlen(string));
    keccak_Final(&strCtx, encoded);
    return SUCCESS;
}

int encodeBytes(const char *string, uint8_t *encoded)
{
    struct SHA3_CTX byteCtx;
    const char *valStrPtr = string + 2;
    uint8_t valByte[1];
    char byteStrBuf[3] = {0};

    sha3_256_Init(&byteCtx);
    while (*valStrPtr != '\0')
    {
        strncpy(byteStrBuf, valStrPtr, 2);
        valByte[0] = (uint8_t)(strtol(byteStrBuf, NULL, 16));
        sha3_Update(&byteCtx,
                    (const unsigned char *)valByte,
                    (size_t)sizeof(uint8_t));
        valStrPtr += 2;
    }
    keccak_Final(&byteCtx, encoded);
    return SUCCESS;
}

int encodeBytesN(const char *typeT, const char *string, uint8_t *encoded)
{
    char byteStrBuf[3] = {0};
    unsigned ctr;

    if (MAX_ENCBYTEN_SIZE < strlen(string))
    {
        return BYTESN_STRING_ERROR;
    }

    // parse out the length val
    uint8_t byteTypeSize = (uint8_t)(strtol((typeT + 5), NULL, 10));
    if (32 < byteTypeSize)
    {
        return BYTESN_SIZE_ERROR;
    }
    for (ctr = 0; ctr < 32; ctr++)
    {
        // zero padding
        encoded[ctr] = 0;
    }
    unsigned zeroFillLen = 32 - ((strlen(string) - 2 /* skip '0x' */) / 2);
    // bytesN are zero padded on the right
    for (ctr = zeroFillLen; ctr < 32; ctr++)
    {
        strncpy(byteStrBuf, &string[2 + 2 * (ctr - zeroFillLen)], 2);
        encoded[ctr - zeroFillLen] = (uint8_t)(strtol(byteStrBuf, NULL, 16));
    }
    return SUCCESS;
}

int confirmName(const char *name, bool valAvailable)
{
    if (valAvailable)
    {
        nameForValue = name;
    }
    else
    {
        //(void)review(ButtonRequestType_ButtonRequest_Other, "MESSAGE DATA", "Press button to continue for\n\"%s\" values", name);
    }
    return SUCCESS;
}

int confirmValue(const char *value)
{
    //(void)review(ButtonRequestType_ButtonRequest_Other, "MESSAGE DATA", "%s %s", nameForValue, value);
    return SUCCESS;
}

static const char *dsname = NULL, *dsversion = NULL, *dschainId = NULL, *dsverifyingContract = NULL;
void marshallDsVals(const char *value)
{

    if (0 == strncmp(nameForValue, "name", sizeof("name")))
    {
        dsname = value;
    }
    if (0 == strncmp(nameForValue, "version", sizeof("version")))
    {
        dsversion = value;
    }
    if (0 == strncmp(nameForValue, "chainId", sizeof("chainId")))
    {
        dschainId = value;
    }
    if (0 == strncmp(nameForValue, "verifyingContract", sizeof("verifyingContract")))
    {
        dsverifyingContract = value;
    }
    return;
}

/*
    Entry:
            eip712Types points to the eip712 types structure
            jType points to eip712 json type structure to parse
            nextVal points to the next value to encode
            msgCtx points to caller allocated hash context to hash encoded values into.
    Exit:
            msgCtx points to current final hash context
            returns error status

    NOTE: reentrant!
*/
int parseVals(const json_t *eip712Types, const json_t *jType, const json_t *nextVal, struct SHA3_CTX *msgCtx)
{
    json_t const *tarray, *pairs, *walkVals, *obTest;
    int ctr;
    const char *typeName = NULL, *typeType = NULL;
    uint8_t encBytes[32] = {0}; // holds the encrypted bytes for the message
    const char *valStr = NULL;
    struct SHA3_CTX valCtx = {0}; // local hash context
    bool hasValue = 0;
    bool ds_vals = 0; // domain sep values are confirmed on a single screen
    int errRet = SUCCESS;

    if (0 == strncmp(json_getName(jType), "EIP712Domain", sizeof("EIP712Domain")))
    {
        ds_vals = true;
    }

    tarray = json_getChild(jType);

    while (tarray != 0)
    {
        if (NULL == (pairs = json_getChild(tarray)))
        {
            errRet = JSON_NO_PAIRS;
            return errRet;
        }
        // should be type JSON_TEXT
        if (pairs->type != JSON_TEXT)
        {
            errRet = JSON_PAIRS_NOTEXT;
            return errRet;
        }
        else
        {
            if (NULL == (typeName = json_getValue(pairs)))
            {
                errRet = JSON_NOPAIRNAME;
                return errRet;
            }
            if (NULL == (obTest = json_getSibling(pairs)))
            {
                errRet = JSON_NO_PAIRS_SIB;
                return errRet;
            }
            if (NULL == (typeType = json_getValue(obTest)))
            {
                errRet = JSON_TYPE_T_NOVAL;
                return errRet;
            }
            walkVals = nextVal;
            while (0 != walkVals)
            {
                if (0 == strcmp(json_getName(walkVals), typeName))
                {
                    valStr = json_getValue(walkVals);
                    break;
                }
                else
                {
                    // keep looking for val
                    walkVals = json_getSibling(walkVals);
                }
            }

            if (JSON_TEXT == json_getType(walkVals) || JSON_INTEGER == json_getType(walkVals))
            {
                hasValue = 1;
            }
            else
            {
                hasValue = 0;
            }
            confirmName(typeName, hasValue);

            if (walkVals == 0)
            {
                errRet = JSON_TYPE_WNOVAL;
                return errRet;
            }
            else
            {
                if (0 == strncmp("address", typeType, strlen("address") - 1))
                {
                    if (']' == typeType[strlen(typeType) - 1])
                    {
                        // array of addresses
                        json_t const *addrVals = json_getChild(walkVals);
                        sha3_256_Init(&valCtx); // hash of concatenated encoded strings
                        while (0 != addrVals)
                        {
                            // just walk the string values assuming, for fixed sizes, all values are there.
                            if (ds_vals)
                            {
                                marshallDsVals(json_getValue(addrVals));
                            }
                            else
                            {
                                confirmValue(json_getValue(addrVals));
                            }

                            errRet = encAddress(json_getValue(addrVals), encBytes);
                            if (SUCCESS != errRet)
                            {
                                return errRet;
                            }
                            sha3_Update(&valCtx, (const unsigned char *)encBytes, 32);
                            addrVals = json_getSibling(addrVals);
                        }
                        keccak_Final(&valCtx, encBytes);
                    }
                    else
                    {
                        if (ds_vals)
                        {
                            marshallDsVals(valStr);
                        }
                        else
                        {
                            confirmValue(valStr);
                        }
                        errRet = encAddress(valStr, encBytes);
                        if (SUCCESS != errRet)
                        {
                            return errRet;
                        }
                    }
                }
                else if (0 == strncmp("string", typeType, strlen("string") - 1))
                {
                    if (']' == typeType[strlen(typeType) - 1])
                    {
                        // array of strings
                        json_t const *stringVals = json_getChild(walkVals);
                        uint8_t strEncBytes[32];
                        sha3_256_Init(&valCtx); // hash of concatenated encoded strings
                        while (0 != stringVals)
                        {
                            // just walk the string values assuming, for fixed sizes, all values are there.
                            if (ds_vals)
                            {
                                marshallDsVals(json_getValue(stringVals));
                            }
                            else
                            {
                                confirmValue(json_getValue(stringVals));
                            }
                            errRet = encString(json_getValue(stringVals), strEncBytes);
                            if (SUCCESS != errRet)
                            {
                                return errRet;
                            }
                            sha3_Update(&valCtx, (const unsigned char *)strEncBytes, 32);
                            stringVals = json_getSibling(stringVals);
                        }
                        keccak_Final(&valCtx, encBytes);
                    }
                    else
                    {
                        if (ds_vals)
                        {
                            marshallDsVals(valStr);
                        }
                        else
                        {
                            confirmValue(valStr);
                        }
                        errRet = encString(valStr, encBytes);
                        if (SUCCESS != errRet)
                        {
                            return errRet;
                        }
                    }
                }
                else if ((0 == strncmp("uint", typeType, strlen("uint") - 1)) ||
                         (0 == strncmp("int", typeType, strlen("int") - 1)))
                {

                    if (']' == typeType[strlen(typeType) - 1])
                    {
                        return INT_ARRAY_ERROR;
                    }
                    else
                    {
                        if (ds_vals)
                        {
                            marshallDsVals(valStr);
                        }
                        else
                        {
                            confirmValue(valStr);
                        }
                        uint8_t negInt = 0; // 0 is positive, 1 is negative
                        if (0 == strncmp("int", typeType, strlen("int") - 1))
                        {
                            if (*valStr == '-')
                            {
                                negInt = 1;
                            }
                        }
                        // parse out the length val
                        for (ctr = 0; ctr < 32; ctr++)
                        {
                            if (negInt)
                            {
                                // sign extend negative values
                                encBytes[ctr] = 0xFF;
                            }
                            else
                            {
                                // zero padding for positive
                                encBytes[ctr] = 0;
                            }
                        }
                        // all int strings are assumed to be base 10 and fit into 64 bits
                        long long intVal = strtoll(valStr, NULL, 10);
                        // Needs to be big endian, so add to encBytes appropriately
                        encBytes[24] = (intVal >> 56) & 0xff;
                        encBytes[25] = (intVal >> 48) & 0xff;
                        encBytes[26] = (intVal >> 40) & 0xff;
                        encBytes[27] = (intVal >> 32) & 0xff;
                        encBytes[28] = (intVal >> 24) & 0xff;
                        encBytes[29] = (intVal >> 16) & 0xff;
                        encBytes[30] = (intVal >> 8) & 0xff;
                        encBytes[31] = (intVal) & 0xff;
                    }
                }
                else if (0 == strncmp("bytes", typeType, strlen("bytes")))
                {
                    if (']' == typeType[strlen(typeType) - 1])
                    {
                        return BYTESN_ARRAY_ERROR;
                    }
                    else
                    {
                        // This could be 'bytes', 'bytes1', ..., 'bytes32'
                        if (ds_vals)
                        {
                            marshallDsVals(valStr);
                        }
                        else
                        {
                            confirmValue(valStr);
                        }
                        if (0 == strcmp(typeType, "bytes"))
                        {
                            errRet = encodeBytes(valStr, encBytes);
                            if (SUCCESS != errRet)
                            {
                                return errRet;
                            }
                        }
                        else
                        {
                            errRet = encodeBytesN(typeType, valStr, encBytes);
                            if (SUCCESS != errRet)
                            {
                                return errRet;
                            }
                        }
                    }
                }
                else if (0 == strncmp("bool", typeType, strlen(typeType)))
                {
                    if (']' == typeType[strlen(typeType) - 1])
                    {
                        return BOOL_ARRAY_ERROR;
                    }
                    else
                    {
                        if (ds_vals)
                        {
                            marshallDsVals(valStr);
                        }
                        else
                        {
                            confirmValue(valStr);
                        }
                        for (ctr = 0; ctr < 32; ctr++)
                        {
                            // leading zeros in bool
                            encBytes[ctr] = 0;
                        }
                        if (0 == strncmp(valStr, "true", sizeof("true")))
                        {
                            encBytes[31] = 0x01;
                        }
                    }
                }
                else
                {
                    // encode user defined type
                    char encSubTypeStr[STRBUFSIZE + 1] = {0};
                    // clear out the user-defined types list
                    for (ctr = 0; ctr < MAX_USERDEF_TYPES; ctr++)
                    {
                        udefList[ctr] = NULL;
                    }

                    char typeNoArrTok[MAX_TYPESTRING] = {0};
                    // need to get typehash of type first
                    if (']' == typeType[strlen(typeType) - 1])
                    {
                        // array of structs. To parse name, remove array tokens.
                        strncpy(typeNoArrTok, typeType, sizeof(typeNoArrTok) - 1);
                        if (strlen(typeNoArrTok) < strlen(typeType))
                        {
                            return UDEF_ARRAY_NAME_ERR;
                        }
                        strtok(typeNoArrTok, "[");
                        // if (STACK_GOOD != (errRet = memcheck(STACK_SIZE_GUARD)))
                        // {
                        //     return errRet;
                        // }
                        if (SUCCESS != (errRet = parseType(eip712Types, typeNoArrTok, encSubTypeStr)))
                        {
                            return errRet;
                        }
                    }
                    else
                    {
                        // if (STACK_GOOD != (errRet = memcheck(STACK_SIZE_GUARD)))
                        // {
                        //     return errRet;
                        // }
                        if (SUCCESS != (errRet = parseType(eip712Types, typeType, encSubTypeStr)))
                        {
                            return errRet;
                        }
                    }
                    sha3_256_Init(&valCtx);
                    sha3_Update(&valCtx, (const unsigned char *)encSubTypeStr, (size_t)strlen(encSubTypeStr));
                    keccak_Final(&valCtx, encBytes);

                    if (']' == typeType[strlen(typeType) - 1])
                    {
                        // array of udefs
                        struct SHA3_CTX eleCtx = {0}; // local hash context
                        struct SHA3_CTX arrCtx = {0}; // array elements hash context
                        uint8_t eleHashBytes[32];

                        sha3_256_Init(&arrCtx);

                        json_t const *udefVals = json_getChild(walkVals);
                        while (0 != udefVals)
                        {
                            sha3_256_Init(&eleCtx);
                            sha3_Update(&eleCtx, (const unsigned char *)encBytes, 32);
                            // if (STACK_GOOD != (errRet = memcheck(STACK_SIZE_GUARD)))
                            // {
                            //     return errRet;
                            // }
                            if (SUCCESS != (errRet =
                                                parseVals(
                                                    eip712Types,
                                                    json_getProperty(eip712Types, strtok(typeNoArrTok, "]")),
                                                    json_getChild(udefVals), // where to get the values
                                                    &eleCtx                  // encode hash happens in parse, this is the return
                                                    )))
                            {
                                return errRet;
                            }
                            keccak_Final(&eleCtx, eleHashBytes);
                            sha3_Update(&arrCtx, (const unsigned char *)eleHashBytes, 32);
                            // just walk the udef values assuming, for fixed sizes, all values are there.
                            udefVals = json_getSibling(udefVals);
                        }
                        keccak_Final(&arrCtx, encBytes);
                    }
                    else
                    {
                        sha3_256_Init(&valCtx);
                        sha3_Update(&valCtx, (const unsigned char *)encBytes, (size_t)sizeof(encBytes));
                        // if (STACK_GOOD != (errRet = memcheck(STACK_SIZE_GUARD)))
                        // {
                        //     return errRet;
                        // }
                        if (SUCCESS != (errRet =
                                            parseVals(
                                                eip712Types,
                                                json_getProperty(eip712Types, typeType),
                                                json_getChild(walkVals), // where to get the values
                                                &valCtx                  // val hash happens in parse, this is the return
                                                )))
                        {
                            return errRet;
                        }
                        keccak_Final(&valCtx, encBytes);
                    }
                }
            }

            // hash encoded bytes to final context
            sha3_Update(msgCtx, (const unsigned char *)encBytes, 32);
        }
        tarray = json_getSibling(tarray);
    }

    return SUCCESS;
}

int encode(const json_t *jsonTypes, const json_t *jsonVals, const char *typeS, uint8_t *hashRet)
{
    int ctr;
    char encTypeStr[STRBUFSIZE + 1] = {0};
    uint8_t typeHash[32];
    struct SHA3_CTX finalCtx = {0};
    int errRet;
    json_t const *typesProp;
    json_t const *typeSprop;
    json_t const *domainOrMessageProp;
    json_t const *valsProp;
    char *domOrMsgStr = NULL;

    // clear out the user-defined types list
    for (ctr = 0; ctr < MAX_USERDEF_TYPES; ctr++)
    {
        udefList[ctr] = NULL;
    }
    if (NULL == (typesProp = json_getProperty(jsonTypes, "types")))
    {
        errRet = JSON_TYPESPROPERR;
        return errRet;
    }
    if (SUCCESS != (errRet =
                        parseType(typesProp, typeS, encTypeStr)))
    {
        return errRet;
    }

    sha3_256_Init(&finalCtx);
    sha3_Update(&finalCtx, (const unsigned char *)encTypeStr, (size_t)strlen(encTypeStr));
    keccak_Final(&finalCtx, typeHash);

    // They typehash must be the first message of the final hash, this is the start
    sha3_256_Init(&finalCtx);
    sha3_Update(&finalCtx, (const unsigned char *)typeHash, (size_t)sizeof(typeHash));

    if (NULL == (typeSprop = json_getProperty(typesProp, typeS)))
    { // e.g., typeS = "EIP712Domain"
        errRet = JSON_TYPESPROPERR;
        return errRet;
    }

    if (0 == strncmp(typeS, "EIP712Domain", sizeof("EIP712Domain")))
    {
        confirmProp = DOMAIN;
        domOrMsgStr = "domain";
    }
    else
    {
        // This is the message value encoding
        confirmProp = MESSAGE;
        domOrMsgStr = "message";
    }
    if (NULL == (domainOrMessageProp = json_getProperty(jsonVals, domOrMsgStr)))
    { // "message" or "domain" property
        if (confirmProp == DOMAIN)
        {
            errRet = JSON_DPROPERR;
        }
        else
        {
            errRet = JSON_MPROPERR;
        }
        return errRet;
    }
    if (NULL == (valsProp = json_getChild(domainOrMessageProp)))
    { // "message" or "domain" property values
        if (confirmProp == MESSAGE)
        {
            errRet = NULL_MSG_HASH; // this is legal, not an error.
            return errRet;
        }
    }

    if (SUCCESS != (errRet = parseVals(typesProp, typeSprop, valsProp, &finalCtx)))
    {
        return errRet;
    }

    keccak_Final(&finalCtx, hashRet);
    // clear typeStr
    memzero(encTypeStr, sizeof(encTypeStr));

    return SUCCESS;
}

void ethereum_typed_hash(const uint8_t domain_separator_hash[32],
                         const uint8_t message_hash[32],
                         bool has_message_hash, uint8_t hash[32])
{
    struct SHA3_CTX ctx = {0};
    sha3_256_Init(&ctx);
    sha3_Update(&ctx, (const uint8_t *)"\x19\x01", 2);
    sha3_Update(&ctx, domain_separator_hash, 32);
    if (has_message_hash)
    {
        sha3_Update(&ctx, message_hash, 32);
    }
    keccak_Final(&ctx, hash);
}

void e712_types_values(Ethereum712TypesValues *msg, EthereumTypedDataSignature *resp)
{
    int errRet = SUCCESS;
    json_t memTypes[JSON_OBJ_POOL_SIZE] = {0};
    json_t memVals[JSON_OBJ_POOL_SIZE] = {0};
    json_t memPType[4] = {0};
    json_t const *jsonT;
    json_t const *jsonV;
    json_t const *jsonPT;
    char *typesJsonStr;
    char *primaryTypeJsonStr;
    char *valuesJsonStr;
    const char *primeType;
    json_t const *obTest;

    typesJsonStr = msg->eip712types;
    primaryTypeJsonStr = msg->eip712primetype;
    valuesJsonStr = msg->eip712data;

    jsonT = json_create(typesJsonStr, memTypes, sizeof memTypes / sizeof *memTypes);
    jsonPT = json_create(primaryTypeJsonStr, memPType, sizeof memPType / sizeof *memPType);
    jsonV = json_create(valuesJsonStr, memVals, sizeof memVals / sizeof *memVals);

    if (!jsonT)
    {
        ESP_LOGE(TAG, "EIP-712 type property data error");
        return;
    }
    if (!jsonPT)
    {
        ESP_LOGE(TAG, "EIP-712 primaryType property data error");
        return;
    }
    if (!jsonV)
    {
        ESP_LOGE(TAG, "EIP-712 values data error");
        return;
    }

    if (msg->eip712typevals == 1)
    {
        // Compute domain seperator hash
        if ((int)SUCCESS != (errRet =
                                 encode(jsonT, jsonV, "EIP712Domain", resp->domain_separator_hash)))
        {
            failMessage(errRet);
            return;
        }
        resp->has_domain_separator_hash = true;
        resp->has_msg_hash = false;
    }
    else
    {
        if (!resp->has_domain_separator_hash)
        {
            failMessage(MSG_NO_DS);
            return;
        }
        if (NULL == (obTest = json_getProperty(jsonPT, "primaryType")))
        {
            failMessage(JSON_PTYPENAMEERR);
            return;
        }
        if (0 == (primeType = json_getValue(obTest)))
        {
            failMessage(JSON_PTYPEVALERR);
            return;
        }
        if (0 != strncmp(primeType, "EIP712Domain", strlen(primeType)))
        { // if primaryType is "EIP712Domain", message hash is NULL
            errRet = encode(jsonT, jsonV, primeType, resp->message_hash);
            if (!(SUCCESS == errRet || NULL_MSG_HASH == errRet))
            {
                failMessage(errRet);
                return;
            }
        }
        else
        {
            errRet = NULL_MSG_HASH;
            return;
        }

        resp->has_message_hash = true;
        resp->has_msg_hash = true;
    }
}

bool eip712_typed_data_hash_v4(
    char *json_str_primary_type,
    char *json_str_types,
    char *json_str_domain,
    char *json_str_message,
    uint8_t typed_data_hash[32])
{
    Ethereum712TypesValues *msg = malloc(sizeof(Ethereum712TypesValues));
    memset(msg, 0, sizeof(Ethereum712TypesValues));
    EthereumTypedDataSignature *resp = malloc(sizeof(EthereumTypedDataSignature));
    memset(resp, 0, sizeof(EthereumTypedDataSignature));

    // domain hash calculation

    msg->eip712types = malloc(strlen(json_str_types) + 1);
    strcpy(msg->eip712types, json_str_types);
    msg->eip712primetype = malloc(strlen(json_str_primary_type) + 1);
    strcpy(msg->eip712primetype, json_str_primary_type);
    msg->eip712data = malloc(strlen(json_str_domain) + 1);
    strcpy(msg->eip712data, json_str_domain);
    msg->eip712typevals = 1;
    e712_types_values(msg, resp);
    free(msg->eip712types);
    free(msg->eip712primetype);
    free(msg->eip712data);
    if (resp->has_domain_separator_hash == false)
    {
        free(resp);
        free(msg);
        return false;
    }

    // message hash calculation
    msg->eip712types = malloc(strlen(json_str_types) + 1);
    strcpy(msg->eip712types, json_str_types);
    msg->eip712primetype = malloc(strlen(json_str_primary_type) + 1);
    strcpy(msg->eip712primetype, json_str_primary_type);
    msg->eip712data = malloc(strlen(json_str_message) + 1);
    strcpy(msg->eip712data, json_str_message);
    msg->eip712typevals = 2;
    e712_types_values(msg, resp);
    free(msg->eip712types);
    free(msg->eip712primetype);
    free(msg->eip712data);
    if (resp->has_msg_hash == false)
    {
        free(resp);
        free(msg);
        return false;
    }
    free(msg);
    ethereum_typed_hash(resp->domain_separator_hash, resp->message_hash, true, typed_data_hash);
    free(resp);
    return true;
}
