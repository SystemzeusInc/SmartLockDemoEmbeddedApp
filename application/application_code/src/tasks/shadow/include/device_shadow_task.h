/**
 * @file device_shadow_task.h
 * @author Systemzeus Inc.
 * @copyright Copyright © 2023 Systemzeus Inc. All rights reserved.
 */
#ifndef DEVICE_SHADOW_H_
#define DEVICE_SHADOW_H_

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
#include "config/device_shadow_config.h"
#include "config/mqtt_config.h"

#include "common/include/application_define.h"
#include "common/include/device_state.h"

// --------------------------------------------------
// #defineマクロ
// --------------------------------------------------

/**
 * @brief Shadowの施錠状態を特定するJSONキー
 */
#define SHADOW_STATE_JSON_KEY_LOCK_STATE "lockState"

/**
 * @brief Shadowの施錠状態を特定するJSONキーの長さ
 */
#define SHADOW_STATE_JSON_KEY_LOCK_STATE_LENGTH (sizeof(SHADOW_STATE_JSON_KEY_LOCK_STATE) - 1U)

/**
 * @brief Shadowの操作主体を特定するJSONキー
 */
#define SHADOW_STATE_JSON_KEY_OPERATOR "operator"

/**
 * @brief Shadowの操作主体を特定するJSONキーの長さ
 */
#define SHADOW_STATE_JSON_KEY_OPERATOR_LENGTH (sizeof(SHADOW_STATE_JSON_KEY_OPERATOR) - 1U)

/**
 * @brief Shadowのdesiredキーを検索するためのパス
 */
#define SHADOW_DESIRED_PATH "state.desired"

/**
 * @brief Shadowのstateキーを検索するためのパス
 */
#define SHADOW_STATE_PATH "state"

/**
 * @brief Shadowのdesiredキーを検索するためのパスの長さ
 */
#define SHADOW_MAX_PATH_LENGTH (sizeof(SHADOW_DESIRED_PATH) - 1U)

/**
 * @brief ShadowのclientTokenキーを検索するためのパス
 */
#define CLIENT_TOKEN_PATH "clientToken"

/**
 * @brief ShadowのclientTokenキーを検索するためのパスの長さ
 */
#define CLIENT_TOKEN_PATH_LENGTH (sizeof(CLIENT_TOKEN_PATH) - 1U)

    // ------------------------------- JSON テンプレート --------------------------------

/**
 * Shadowの各状態のJSON文字列
 */
#define SHADOW_JSON_LOCK_STATE          "\"" SHADOW_STATE_JSON_KEY_LOCK_STATE "\":\"%s\""
#define SHADOW_JSON_OPERATOR            "\"" SHADOW_STATE_JSON_KEY_OPERATOR "\":\"%s\""
#define SHADOW_JSON_CONTROL_CHAR_LENGTH (5U)

/**
 * JSON文字列の長さ
 */
#define JSON_LOCK_STATE_MAX_LENGTH        (SHADOW_STATE_JSON_KEY_LOCK_STATE_LENGTH + SHADOW_JSON_CONTROL_CHAR_LENGTH + LOCK_STATE_STRING_MAX_LENGTH)
#define JSON_OPERATOR_MAX_LENGTH          (SHADOW_STATE_JSON_KEY_OPERATOR_LENGTH + SHADOW_JSON_CONTROL_CHAR_LENGTH + UNLOCKING_OPERATOR_TYPE_STRING_MAX_LENGTH)
#define SHADOW_JSON_STATE_PART_MAX_LENGTH (JSON_LOCK_STATE_MAX_LENGTH + JSON_OPERATOR_MAX_LENGTH + 2 /* , × 2*/)

/**
 * @brief ShadowUpdateを行うときのJsonテンプレート、reportedとdesired、clientTokenを格納することで完成する
 */
#define SHADOW_UPDATE_TEMPLATE "{\"state\":{\"desired\":{%s},\"reported\":{%s}},\"clientToken\":\"%s\"}"

/**
 * @brief ShadowUpdateを行うペイロードの最大長
 */
#define SHADOW_UPDATE_MAX_LENGTH (sizeof(SHADOW_UPDATE_TEMPLATE) - 4 /* %s × 2 */ + (SHADOW_JSON_STATE_PART_MAX_LENGTH * 2) + CREATE_CLIENT_TOKEN_MAX_LENGTH + 1 /* \0 */)

/**
 * @brief ClientTokenの最大長
 */
#define CREATE_CLIENT_TOKEN_MAX_LENGTH (sizeof(TickType_t) * 2)
    // --------------------------------------------------
    // #define関数マクロ
    // --------------------------------------------------

/**
 * @brief ShadowのUpdateペイロードを作成する
 *
 * @param[in] buffer      ペイロードを格納するバッファ
 * @param[in] bufferSize  バッファサイズ
 * @param[in] data        Shadowのreportedとdesiredに格納されるデータ
 * @param[in] clientToken ClientToken
 */
#define CREATE_SHADOW(buffer, bufferSize, data, clientToken) snprintf(buffer, bufferSize, SHADOW_UPDATE_TEMPLATE, data, data, clientToken)

/**
 * @brief ClientTokenを生成する
 */
#define CREATE_CLIENT_TOKEN(buffer, buffersize) snprintf(buffer, buffersize, "%X", (xTaskGetTickCount() % 1000000));

    // --------------------------------------------------
    // typedef定義
    // --------------------------------------------------

    /**
     * @brief Shadow内のステータス
     */
    typedef struct
    {
        /**
         * @brief 解施錠状態
         */
        LockState_t xLockState;

        /**
         * @brief 操作主体
         */
        UnlockingOperatorType_t xUnlockingOperator;

    } ShadowState_t;

    // --------------------------------------------------
    // enumタグ定義（typedefを同時に行う）
    // --------------------------------------------------

    /**
     * @brief 本ライブラリの実行結果
     */
    typedef enum
    {
        /**
         * @brief 成功
         */
        DEVICE_SHADOW_RESULT_SUCCESS = 0x00,

        /**
         * @brief 失敗
         */
        DEVICE_SHADOW_RESULT_FAILED = 0x01
    } DeviceShadowResult_t;

    /**
     * @brief どのShadowStateを更新するか決定するタイプ
     */
    typedef enum
    {
        /**
         * @brief 解施錠状態
         */
        SHADOW_UPDATE_TYPE_LOCK_STATE = (1U << 0),
    } ShadowUpdateType_t;

// NOTE: ShadowUpdateType_tの値を使用するためここでdefine定義する
#define SHADOW_UPDATE_TYPE_ALL (SHADOW_UPDATE_TYPE_LOCK_STATE)

    /**
     * @brief ShadowのDeltaが発火した際のコールバック関数
     *
     * @param[in] xUpdateShadowType どのタイプのステータスが変わったか.例えば、解錠状態が変更された場合は #SHADOW_UPDATE_TYPE_LOCK_STATE となる。
     * @param[in] pxShadowSate       変更されたステータス
     *
     * @warning
     * 1.本関数は、MQTTTaskのコンテキストで実行される。
     *
     * 2.xUpdateShadowTypeで指定されたステートタイプ以外のxShadowSateデータは参照しないこと。
     * 例えば、 xUpdateShadowType が SHADOW_UPDATE_TYPE_LOCK_STATE の場合は、 xShadowSate.xLockState しか参照できない
     */
    typedef void (*ShadowChangeCallback_t)(const uint32_t xUpdateShadowType, const ShadowState_t *pxShadowSate);

    // --------------------------------------------------
    // struct/unionタグ定義（typedefを同時に行う）
    // --------------------------------------------------

    // --------------------------------------------------
    // extern変数宣言
    // --------------------------------------------------

    // --------------------------------------------------
    // 関数プロトタイプ宣言
    // --------------------------------------------------

    /**
     * @brief ShadowTaskをイニシャライズする
     *
     * @param[in] xCallbackFuction クラウドのShadowステータス変化に応じて呼び出されるコールバック関数
     *
     * @retval DEVICE_SHADOW_RESULT_SUCCESS 成功
     * @retval DEVICE_SHADOW_RESULT_FAILED  失敗
     */
    DeviceShadowResult_t eDeviceShadowTaskInit(const ShadowChangeCallback_t xCallbackFuction);

    /**
     * @brief ShadowTaskを終了してメモリを開放する
     *
     * @retval DEVICE_SHADOW_RESULT_SUCCESS 成功
     * @retval DEVICE_SHADOW_RESULT_FAILED  失敗
     */
    DeviceShadowResult_t eDeviceShadowTaskShutdown(void);

    /**
     * @brief 現在のShadowの状態を取得する
     *
     * @param[out] pxOutShadowSate クラウドから得たShadowの状態を格納するバッファ
     * @param[in]  xTimeoutMS      Shadowの取得を待機するミリ秒
     *
     * @retval DEVICE_SHADOW_RESULT_SUCCESS 成功
     * @retval DEVICE_SHADOW_RESULT_FAILED  失敗
     */
    DeviceShadowResult_t eGetShadowState(ShadowState_t *pxOutShadowSate, uint32_t xTimeoutMS);

    /**
     * @brief 指定したShadowStateをUpdateする。Updateは非同期で行われる。
     *
     * @details
     * ShadowTaskに対してUpdateコマンドを送信する。
     *
     *
     * @param[in] xUpdateShadowType UpdateするShadowのタイプ。 ShadowUpdateType_t の組み合わせ。
     *                              例えば施錠状態をアップデートする場合は (SHADOW_UPDATE_TYPE_LOCK_STATE) とする
     * @param[in] pxShadowState     更新する情報
     *
     * @warning
     * xUpdateShadowType に SHADOW_UPDATE_TYPE_LOCK_STATE を指定した場合は、解施錠状態（ xLockState ）と操作主体（ xUnlockingOperator ） 両方更新しなければならない。
     *
     *
     * @retval DEVICE_SHADOW_RESULT_SUCCESS 成功
     * @retval DEVICE_SHADOW_RESULT_FAILED  失敗
     */
    DeviceShadowResult_t eUpdateShadowStateAsync(const uint32_t xUpdateShadowType, const ShadowState_t *pxShadowState);

    /**
     * @brief 指定したShadowStateをUpdateする。本APIは #eUpdateShadowStateAsync の同期関数である。Shadowが送信されるまで待機する。
     * 詳細は #eUpdateShadowStateAsync 参照
     *
     * @param[in] xUpdateShadowType UpdateするShadowのタイプ。 ShadowUpdateType_t の組み合わせ。
     *                              例えば施錠状態をアップデートする場合は (SHADOW_UPDATE_TYPE_LOCK_STATE) とする
     * @param[in] pxShadowState     更新する情報
     * @param[in] xTimeoutMS        Shadow更新を待機するミリ秒
     */
    DeviceShadowResult_t eUpdateShadowStateSync(const uint32_t xUpdateShadowType, const ShadowState_t *pxShadowState, uint32_t xTimeoutMS);

    // --------------------------------------------------
    // インライン関数
    // --------------------------------------------------

#ifdef __cplusplus
}
#endif

#endif /* end DEVICE_SHADOW_H_ */
