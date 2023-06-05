/**
 * @file device_register.h
 * @author Systemzeus Inc.
 * @copyright Copyright © 2023 Systemzeus Inc. All rights reserved.
 */
#ifndef DEVICE_REGISTER_WITH_MQTT_H_
#define DEVICE_REGISTER_WITH_MQTT_H_

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

#include "tasks/flash/include/flash_data.h"

    // --------------------------------------------------
    // #defineマクロ
    // --------------------------------------------------
/**
 * @brief MQTT接続をリトライする回数。1回+2回リトライ
 */
#define MQTT_CONNECT_RETRY_TIME (3U)

/**
 * @brief デバイス登録結果がPublishされるまで待機する時間
 */
#define DEVICE_REGISTER_RESPONSE_WITE_TIME_MS (20 * 1000)

/**
 * @brief リンキングワントークンの文字列長
 */
#define LINKING_ONE_TIME_TOKEN_LENGTH (12U)

/**
 * @brief デバイス登録を行うトピックテンプレート
 *
 * @details
 * device/register/{factoryThingName}
 */
#define DEVICE_REGISTER_TOPIC_TEMPLATE "device/register/%s"

/**
 * @brief デバイス登録トピックの長さ
 *
 * @details
 * device/register/->16文字 + FACTORY_THING_NAME_LENGTH + 終端文字
 */
#define DEVICE_REGISTER_TOPIC_LENGTH (16U + FACTORY_THING_NAME_LENGTH + 1)

/**
 * @brief デバイス登録を行うときのペイロードテンプレート
 */
#define DEVICE_REGISTER_PAYLOAD_TEMPLATE "{\"lott\":\"%s\"}"

/**
 * @brief デバイス登録のペイロードサイズ
 *
 * @details
 * {"lott":""} -> 11文字 + LINKING_ONE_TIME_TOKEN_LENGTH + バッファ
 */
#define DEVICE_REGISTER_PAYLOAD_SIZE (11U + LINKING_ONE_TIME_TOKEN_LENGTH + 10U)

/**
 * @brief デバイス登録結果のトピックテンプレート
 *
 * @details
 * device/register/{factoryThingName}/res
 */
#define DEVICE_REGISTER_RESPONSE_TOPIC_TEMPLATE "device/register/%s/res"

/**
 * @brief デバイス登録結果トピックの長さ
 *
 * @details
 * device/register/->16文字 + FACTORY_THING_NAME_LENGTH + /res->4文字 + 終端文字
 */
#define DEVICE_REGISTER_RESPONSE_TOPIC_LENGTH (16U + FACTORY_THING_NAME_LENGTH + 4 + 1)

/**
 * @brief デバイス登録結果のペイロードの長さ
 *
 * @details
 * {"thingName"："","result":"alreadyRegistered"}->45文字 + THING_NAME_LENGTH + バッファ
 */
#define DEVICE_REGISTER_RESPONSE_PAYLOAD_SIZE (45 + THING_NAME_LENGTH + 20)

/**
 * @brief デバイス登録結果に格納されている登録成功可否を表すキー
 */
#define DEVICE_REGISTER_RESPONSE_RESULT_JSON_KEY_STRING "result"

/**
 * @brief デバイス登録結果の"result"キーに格納される成功を意味するメッセージ
 */
#define DEVICE_REGISTER_RESPONSE_RESULT_JSON_VALUE_SUCCESS "success"

/**
 * @brief デバイス登録結果の"result"キーに格納される既に登録済みを意味するメッセージ
 */
#define DEVICE_REGISTER_RESPONSE_RESULT_JSON_VALUE_ALREADY_REGISTERED "alreadyRegistered"

/**
 * @brief デバイス登録結果に格納されているThingNameのキー
 */
#define DEVICE_REGISTER_RESPONSE_THING_NAME_JSON_KEY_STRING "thingName"

    // --------------------------------------------------
    // #define関数マクロ
    // --------------------------------------------------

    // --------------------------------------------------
    // typedef定義
    // --------------------------------------------------

    /**
     * @brief リンキングワンタイムトークン
     */
    typedef struct
    {
        /**
         * @brief トークン
         */
        uint8_t uxLOTT[LINKING_ONE_TIME_TOKEN_LENGTH + 1];
    } LinkingOneTimeToken_t;

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
        DEVICE_REGISTER_RESULT_SUCCESS = 0,

        /**
         * @brief 既にデバイス登録されている
         */
        DEVICE_REGISTER_ALREADY_REGISTERED = 1,

        /**
         * @brief 失敗
         */
        DEVICE_REGISTER_RESULT_FAILED = 2,
    } DeviceRegisterResult_t;

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
     * @brief デバイス登録プロセスを実行する
     *
     * @details
     *
     * 以下の処理を行う
     *
     * 1. MQTT接続を行う
     * 2. デバイス登録MQTTトピックに対してデバイス登録要求を行う
     * 3. MQTTの切断を行う
     * 4. 登録結果を確認してThingNameをFlashに登録する
     * 5. プロビジョニングフラグをFlashに登録する
     *
     * @param[in] pxLOTT リンキングワンタイムトークン
     *
     * @retval #DEVICE_REGISTER_RESULT_SUCCESS     成功
     * @retval #DEVICE_REGISTER_ALREADY_REGISTERED デバイス登録済み
     * @retval #DEVICE_REGISTER_RESULT_FAILED      失敗
     */
    DeviceRegisterResult_t eRunDeviceRegisterProcess(const LinkingOneTimeToken_t *pxLOTT);

    // --------------------------------------------------
    // インライン関数
    // --------------------------------------------------

#ifdef __cplusplus
}
#endif

#endif /* end DEVICE_REGISTER_WITH_MQTT_H_ */
