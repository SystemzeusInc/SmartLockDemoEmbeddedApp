/**
 * @file flash_task.h
 * @author Systemzeus Inc.
 * @copyright Copyright © 2023 Systemzeus Inc. All rights reserved.
 */
#ifndef FLASH_TASK_H_
#define FLASH_TASK_H_

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

    // --------------------------------------------------
    // typedef定義
    // --------------------------------------------------

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
    FLASH_TASK_RESULT_SUCCESS = 0,

    /**
     * @brief 引数間違い
     */
    FLASH_TASK_RESULT_BAD_RESULT = 1,

    /**
     * @brief タイムアウト
     */
    FLASH_TASK_RESULT_TIMEOUT = 2,

    /**
     * @brief 失敗
     */
    FLASH_TASK_RESULT_FAILED = 3
} FlashTaskResult_t;

/**
 * @brief Flashから取得したい情報タイプ
 */
typedef enum
{
    /**
     * @brief Wi-FiのSSID、PW、セキュリティ
     *
     * @details
     * 本パラメータを使用した時のバッファの型は flash_data.h の @ref WiFiInfo_t を使用すること
     *
     * @note
     * WriteFlashType_tと間違えても気づけるように、あえて0xFFからデクリメントする。
     */
    READ_FLASH_TYPE_WIFI_INFO = 0xFF,

    /**
     * @brief AWS IoT Endpoint
     *
     * @details
     * 本パラメータを使用した時のバッファの型は flash_data.h の @ref AWSIoTEndpoint_t を使用すること
     */
    READ_FLASH_TYPE_AWS_IOT_ENDPOINT = 0xFE,

    /**
     * @brief プロビジョニングフラグ
     *
     * @details
     * 本パラメータを使用した時のバッファの型は flash_data.h の @ref ProvisioningFlag_t を使用すること
     */
    READ_FLASH_TYPE_PROVISIONING_FLAG = 0xFD,

    /**
     * @brief 工場出荷ThingName
     *
     * @details
     * 本パラメータを使用した時のバッファの型は flash_data.h の @ref FactoryThingName_t を使用すること
     */
    READ_FLASH_TYPE_FACTORY_THING_NAME = 0xFC,

    /**
     * @brief 普段使用するThingName
     *
     * @details
     * 本パラメータを使用した時のバッファの型は flash_data.h の @ref ThingName_t を使用すること
     */
    READ_FLASH_TYPE_USUAL_THING_NAME = 0xFB,

    /**
     * @brief 解施錠ジェスチャーパターン
     *
     * @details
     * 本パラメータを使用した時のバッファの型は flash_data.h の @ref FlashDataGesturePattern_t を使用すること
     */
    READ_FLASH_TYPE_USUAL_GESTURE_PATTERN = 0xFA
} ReadFlashType_t;

/**
 * @brief Flashに書き込みたい情報タイプ
 */
typedef enum
{
    /**
     * @brief Wi-FiのSSID、PW、セキュリティ、AWSIoTのエンドポイント
     *
     * @details
     * 本パラメータを使用した時の書き込みデータ型は flash_data.h の @ref WiFiInfo_t を使用すること
     */
    WRITE_FLASH_TYPE_WIFI_INFO = 0x01,

    /**
     * @brief プロビジョニングフラグ
     *
     * @details
     * 本パラメータを使用した時の書き込みデータ型は flash_data.h の @ref ProvisioningFlag_t を使用すること
     */
    WRITE_FLASH_TYPE_PROVISIONING_FLAG = 0x02,

    /**
     * @brief AWS IoT Endpoint
     *
     * @details
     * 本パラメータを使用した時の書き込みデータ型は flash_data.h の @ref AWSIoTEndpoint_t を使用すること
     */
    WRITE_FLASH_TYPE_AWS_IOT_ENDPOINT = 0x03,

    /**
     * @brief 普段使用するThingName
     *
     * @details
     * 本パラメータを使用した時の書き込みデータ型は flash_data.h の @ref ThingName_t を使用すること
     */
    WRITE_FLASH_TYPE_USUAL_THING_NAME = 0x04,

    /**
     * @brief ジェスチャーパターン
     *
     * @details
     * 本パラメータを使用した時の書き込みデータ型は flash_data.h の @ref FlashDataGesturePattern_t を使用すること
     */
    WRITE_FLASH_TYPE_USUAL_GESTURE_PATTERN = 0x5

} WriteFlashType_t;

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
 * @brief Flashタスクをイニシャライズする
 *
 * @details
 * 本ライブラリで必要なタスクやキューを作成する
 *
 * @retval FLASH_TASK_RESULT_SUCCESS 成功
 * @retval FLASH_TASK_RESULT_FAILED  失敗
 */
FlashTaskResult_t eFlashTaskInit(void);

/**
 * @brief Flashから情報を取得する
 *
 * @note
 * 本ライブラリはスレッドセーフである
 *
 * @param[in]     eReadFlashType 取得したい情報のタイプ @ref ReadFlashType_t
 * @param[out]    pvBuffer       取得情報を格納するバッファ。FlashTypeに応じて flash_data.h にあるデータ型から適切な型にキャストする。
 * @param[in]     uxBufferSize   バッファのサイズ
 *
 * @retval FLASH_TASK_RESULT_SUCCESS    成功
 * @retval FLASH_TASK_RESULT_BAD_RESULT パラメータエラー
 * @retval FLASH_TASK_RESULT_TIMEOUT    タイムアウト。頻発するなら #FLASH_RESPONSE_TIMEOUT_MS の時間を延ばす。
 * @retval FLASH_TASK_RESULT_FAILED     その他エラー
 */
FlashTaskResult_t eReadFlashInfo(const ReadFlashType_t eReadFlashType,
                                 void *pvBuffer,
                                 const uint32_t uxBufferSize);

/**
 * @brief Flashへ情報を書き込む
 *
 * @note
 * 本ライブラリはスレッドセーフである
 *
 * @param[in] eWriteFlashType 書き込みたい情報のタイプ @ref WriteFlashType_t
 * @param[in] pvWriteData     取得情報のデータ。本API内で書き込みたい情報のタイプに対応する型を　flash_data.h にあるデータ型にキャストしてから使用する。
 *
 * @retval FLASH_TASK_RESULT_SUCCESS    成功
 * @retval FLASH_TASK_RESULT_BAD_RESULT パラメータエラー
 * @retval FLASH_TASK_RESULT_TIMEOUT    タイムアウト。頻発するなら #FLASH_RESPONSE_TIMEOUT_MS の時間を延ばす。
 * @retval FLASH_TASK_RESULT_FAILED     その他エラー
 */
FlashTaskResult_t eWriteFlashInfo(const WriteFlashType_t eWriteFlashType,
                                  const void *pvWriteData);

// --------------------------------------------------
// インライン関数
// --------------------------------------------------

#ifdef __cplusplus
}
#endif

#endif /* end FLASH_TASK_H_ */
