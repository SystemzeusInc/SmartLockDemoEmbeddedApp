/**
 * @file device_state.c
 * @author Systemzeus Inc.
 * @copyright Copyright © 2023 Systemzeus Inc. All rights reserved.
 */

// --------------------------------------------------
// システムヘッダの取り込み
// --------------------------------------------------
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "FreeRTOS.h"

// --------------------------------------------------
// ユーザ作成ヘッダの取り込み
// --------------------------------------------------
#include "common/include/application_define.h"
#include "config/task_config.h"

#include "include/device_state.h"

// --------------------------------------------------
// 自ファイル内でのみ使用する#defineマクロ
// --------------------------------------------------

// --------------------------------------------------
// 自ファイル内でのみ使用する#define関数マクロ
// --------------------------------------------------

// --------------------------------------------------
// 自ファイル内でのみ使用するtypedef定義
// --------------------------------------------------

// --------------------------------------------------
// 自ファイル内でのみ使用するenumタグ定義（typedefを同時に行う）
// --------------------------------------------------

// --------------------------------------------------
// 自ファイル内でのみ使用するstruct/unionタグ定義（typedefを同時に行う）
// --------------------------------------------------
/**
 * @brief 操作主体の文字列と列挙型の対応表の型
 */
typedef struct
{
    UnlockingOperatorType_t eOperatorType; /**< 列挙型 */
    uint8_t *pucString;                    /**< 文字列 */
} RelationshipUnlockingOperatorTypeEnumAndString_t;

// --------------------------------------------------
// ファイル内で共有するstatic変数宣言
// --------------------------------------------------
// clang-format off
/**
 * @brief 操作主体の文字列と列挙型の対応表
 */
static RelationshipUnlockingOperatorTypeEnumAndString_t gxMatchTableUnlockingOperatorTypeEnumAndString[] = {
    {UNLOCKING_OPERATOR_TYPE_NONE,           (uint8_t *)UNLOCKING_OPERATOR_TYPE_STRING_NONE },
    {UNLOCKING_OPERATOR_TYPE_APP,            (uint8_t *)UNLOCKING_OPERATOR_TYPE_STRING_APP },
    {UNLOCKING_OPERATOR_TYPE_AUTO_LOCK,      (uint8_t *)UNLOCKING_OPERATOR_TYPE_STRING_AUTO_LOCK },
    {UNLOCKING_OPERATOR_TYPE_BLE,            (uint8_t *)UNLOCKING_OPERATOR_TYPE_STRING_BLE },
    {UNLOCKING_OPERATOR_TYPE_NFC,            (uint8_t *)UNLOCKING_OPERATOR_TYPE_STRING_NFC }
};
// clang-format on

// --------------------------------------------------
// static関数プロトタイプ宣言
// --------------------------------------------------

// --------------------------------------------------
// 変数定義（staticを除く）
// --------------------------------------------------

// --------------------------------------------------
// 関数定義（staticを除く）
// --------------------------------------------------
void vConvertStringToEnumLockState(const uint8_t *pxLockStateString, LockState_t *peOutLockState)
{
    if (strncmp((const char *)pxLockStateString, (const char *)LOCK_STATE_STRING_UNLOCK, sizeof(LOCK_STATE_STRING_UNLOCK) - 1) == 0)
    {
        *peOutLockState = LOCK_STATE_UNLOCKED;
        return;
    }

    if (strncmp((const char *)pxLockStateString, (const char *)LOCK_STATE_STRING_LOCK, sizeof(LOCK_STATE_STRING_LOCK) - 1) == 0)
    {
        *peOutLockState = LOCK_STATE_LOCKED;
        return;
    }

    *peOutLockState = LOCK_STATE_UNDEFINED;
    return;
}

void vConvertStringToEnumUnlockingOperatorType(const uint8_t *pxUnlockingOperatorType, UnlockingOperatorType_t *peOutUnlockingOperatorType)
{
    for (uint8_t i = 0; i < sizeof(gxMatchTableUnlockingOperatorTypeEnumAndString) / sizeof(gxMatchTableUnlockingOperatorTypeEnumAndString[0]); i++)
    {
        if (strncmp((const char *)gxMatchTableUnlockingOperatorTypeEnumAndString[i].pucString, (const char *)pxUnlockingOperatorType, strlen((const char *)gxMatchTableUnlockingOperatorTypeEnumAndString[i].pucString)) == 0)
        {
            *peOutUnlockingOperatorType = gxMatchTableUnlockingOperatorTypeEnumAndString[i].eOperatorType;
            return;
        }
    }
    *peOutUnlockingOperatorType = UNLOCKING_OPERATOR_TYPE_UNDEFINED;
    return;
}

void vConvertEnumToStringLockState(const LockState_t eLockState,
                                   uint8_t *pxLockStateStringBuffer,
                                   const uint32_t uxBufferSize)
{
    switch (eLockState)
    {
    case LOCK_STATE_UNLOCKED:
        strncpy((char *)pxLockStateStringBuffer, (const char *)LOCK_STATE_STRING_UNLOCK, uxBufferSize);
        return;
    case LOCK_STATE_LOCKED:
        strncpy((char *)pxLockStateStringBuffer, (const char *)LOCK_STATE_STRING_LOCK, uxBufferSize);
        return;
    default:
        memset(pxLockStateStringBuffer, 0x00, uxBufferSize);
        break;
    }
    return;
}

void vConvertEnumToStringUnlockingOperatorType(const UnlockingOperatorType_t eUnlockingOperatorType,
                                               uint8_t *pxUnlockingOperatorTypeStringBuffer,
                                               const uint32_t uxBufferSize)
{

    for (uint8_t i = 0; i < sizeof(gxMatchTableUnlockingOperatorTypeEnumAndString) / sizeof(gxMatchTableUnlockingOperatorTypeEnumAndString[0]); i++)
    {
        if (gxMatchTableUnlockingOperatorTypeEnumAndString[i].eOperatorType == eUnlockingOperatorType)
        {
            strncpy((char *)pxUnlockingOperatorTypeStringBuffer, (const char *)gxMatchTableUnlockingOperatorTypeEnumAndString[i].pucString, uxBufferSize);
            return;
        }
    }
    memset(pxUnlockingOperatorTypeStringBuffer, 0x00, uxBufferSize);
    return;
}

// --------------------------------------------------
// static関数定義
// --------------------------------------------------

// --------------------------------------------------
// Unit Test用関数定義(関数のプロトタイプ宣言は「自身のファイル名+ "_test.h"」で宣言されていること)
// --------------------------------------------------
#if (BUILD_MODE_TEST == 1) /* BUILD_MODE_TESTが定義されているとき */
#endif                     /* end  BUILD_MODE_TEST */
