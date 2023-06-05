/**
 * @file device_state.h
 * @author Systemzeus Inc.
 * @copyright Copyright © 2023 Systemzeus Inc. All rights reserved.
 */
#ifndef DEVICE_STATE_H_
#define DEVICE_STATE_H_

#ifdef __cplusplus // Provide Cplusplus Compatibility

extern "C"
{
#endif /* end Provide Cplusplus Compatibility */

// --------------------------------------------------
// ###   システムヘッダの取り込み
// --------------------------------------------------
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "FreeRTOS.h"

// --------------------------------------------------
// ユーザ作成ヘッダの取り込み
// --------------------------------------------------
#include "common/include/application_define.h"

    // --------------------------------------------------
    // #defineマクロ
    // --------------------------------------------------

    // --------------------------------------------------
    // #define関数マクロ
    // --------------------------------------------------

// ------------------------------------------------------------------------

    /**
     * 解施錠に対する文字列
     */

#define LOCK_STATE_STRING_UNLOCK "Unlock" /**< 施錠状態の文字列 */
#define LOCK_STATE_STRING_LOCK   "Lock"   /**< 解錠状態の文字列 */

/**
 * @brief 解施錠状態の各ステータスの中で最長の文字列長
 */
#define LOCK_STATE_STRING_MAX_LENGTH (sizeof(LOCK_STATE_STRING_UNLOCK) - 1U)

// ------------------------------------------------------------------------

#define MAX_UNLOCK_PATTERN_NUM 5 /**< パターンの組み合わせの数 */

    // ------------------------------------------------------------------------

/**
 * 操作主体の種類の文字列
 */
#define UNLOCKING_OPERATOR_TYPE_STRING_NONE          "None"         /**< 主体なし */
#define UNLOCKING_OPERATOR_TYPE_STRING_APP           "App"          /**< アプリ */
#define UNLOCKING_OPERATOR_TYPE_STRING_AUTO_LOCK     "AutoLock"     /**< 自動施錠 */
#define UNLOCKING_OPERATOR_TYPE_STRING_BLE           "BLE"          /**< BLE */
#define UNLOCKING_OPERATOR_TYPE_STRING_NFC           "NFC"          /**< NCF */

/**
 * @brief 操作主体の最大文字列長
 */
#define UNLOCKING_OPERATOR_TYPE_STRING_MAX_LENGTH (sizeof(UNLOCKING_OPERATOR_TYPE_STRING_AUTO_LOCK) - 1U)

    // ------------------------------------------------------------------------

    // --------------------------------------------------
    // typedef定義
    // --------------------------------------------------

    // --------------------------------------------------
    // enumタグ定義（typedefを同時に行う）
    // --------------------------------------------------

    /**
     * @brief デバイスの解施錠状態
     */
    typedef enum
    {
        /**
         * @brief 解錠状態
         */
        LOCK_STATE_UNLOCKED = 0x00,

        /**
         * @brief 施錠状態
         */
        LOCK_STATE_LOCKED = 0x01,

        /**
         * @brief 未定義。使用してはいけない
         */
        LOCK_STATE_UNDEFINED = 0x02,
    } LockState_t;

    /**
     * @brief 解施錠操作を行った主体の種類
     */
    typedef enum
    {
        UNLOCKING_OPERATOR_TYPE_UNDEFINED = 0x0,  /**< 未定義。異常値として使用する。 */
        UNLOCKING_OPERATOR_TYPE_NONE = 0x1,       /**< 主体者なし */
        UNLOCKING_OPERATOR_TYPE_APP = 0x02,       /**< アプリ */
        UNLOCKING_OPERATOR_TYPE_AUTO_LOCK = 0x04, /**< 自動ロック */
        UNLOCKING_OPERATOR_TYPE_BLE = 0x05,       /**< BLE(将来のための予約) */
        UNLOCKING_OPERATOR_TYPE_NFC = 0x06,       /**< NFC(将来のための予約) */
    } UnlockingOperatorType_t;

    // --------------------------------------------------
    // struct/unionタグ定義（typedefを同時に行う）
    // --------------------------------------------------

    // --------------------------------------------------
    // extern変数宣言
    // --------------------------------------------------

    // --------------------------------------------------
    // 関数プロトタイプ宣言
    // --------------------------------------------------
    // -------------------------- String to Enum --------------------------
    /**
     * @brief 解施錠状態をStringからEnumに変換する。変換できない場合は、LOCK_STATE_UNDEFINEDが返却される
     *
     * @param[in]  pxLockStateString 解施錠状態の文字列。必ずNULL終端であること。
     * @param[out] peOutLockState    変換後のEnum
     */
    void vConvertStringToEnumLockState(const uint8_t *pxLockStateString, LockState_t *peOutLockState);

    /**
     * @brief 操作主体をStringからEnumに変換する。変換できない場合は #UNLOCKING_OPERATOR_TYPE_UNDEFINED が返却される
     *
     * @param[in] pxUnlockingOperatorType      操作主体の文字列
     * @param[out] peOutUnlockingOperatorType  変換後のEnum
     */
    void vConvertStringToEnumUnlockingOperatorType(const uint8_t *pxUnlockingOperatorType, UnlockingOperatorType_t *peOutUnlockingOperatorType);

    // -------------------------- Enum to String --------------------------

    /**
     * @brief 解施錠状態をEnumからStringに変換する。変換できない場合は空文字が返却される。
     * また、eLockStateがLOCK_STATE_UNDEFINEDの場合でも空文字を返却する。
     *
     * @param[in]  eLockState               解施錠状態のEnum
     * @param[out] pxLockStateStringBuffer  変換後の文字列。Bufferの中身を0x00で埋めてから本APIをコールすること。
     * @param[in]  uxBufferSize             バッファのサイズ
     */
    void vConvertEnumToStringLockState(const LockState_t eLockState,
                                       uint8_t *pxLockStateStringBuffer,
                                       const uint32_t uxBufferSize);

    /**
     * @brief 操作主体をEnumからStringに変換する。変換できない場合は、空文字。
     *
     * @param[in]  eUnlockingOperatorType               操作主体のEnum
     * @param[out] pxUnlockingOperatorTypeStringBuffer  変換後の文字列。Bufferの中身を0x00で埋めてから本APIをコールすること。
     * @param[in]  uxBufferSize                         バッファサイズ
     */
    void vConvertEnumToStringUnlockingOperatorType(const UnlockingOperatorType_t eUnlockingOperatorType,
                                                   uint8_t *pxUnlockingOperatorTypeStringBuffer,
                                                   const uint32_t uxBufferSize);

    // --------------------------------------------------
    // インライン関数
    // --------------------------------------------------

#ifdef __cplusplus
}
#endif

#endif /* end DEVICE_STATE_H_ */
