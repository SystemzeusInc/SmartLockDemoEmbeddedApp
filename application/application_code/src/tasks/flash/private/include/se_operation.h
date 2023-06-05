/**
 * @file se_operation.h
 * @author Systemzeus Inc.
 * @copyright Copyright © 2023 Systemzeus Inc. All rights reserved.
 */
#ifndef SE_OPERATION_H_
#define SE_OPERATION_H_

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

#include "definitions.h"
#include "FreeRTOS.h"

#include "iot_wifi.h"
#include "aws_wifi_config.h"

    // --------------------------------------------------
    // ユーザ作成ヘッダの取り込み
    // --------------------------------------------------

#include "tasks/flash/include/flash_data.h"

// --------------------------------------------------
// #defineマクロ
// --------------------------------------------------

/**
 * @brief プロビジョニングフラグのスタートアドレス
 *
 */
#define SE_PROVISIONING_FLAG_START_ADDRESS 0x0000

/**
 * @brief プロビジョニングフラグの長さ
 *
 */
#define SE_PROVISIONING_FLAG_LENGTH 4U

/**
 * @brief SSIDのスタートアドレス
 *
 */
#define SE_SSID_START_ADDRESS 0x0004

/**
 * @brief SSIDの長さ
 *
 */
#define SE_SSID_LENGTH 32U

/**
 * @brief パスワードのスタートアドレス
 *
 */
#define SE_PASSWORD_START_ADDRESS 0x0024

/**
 * @brief パスワードの長さ
 *
 */
#define SE_PASSWORD_LENGTH 64U

/**
 * @brief Wi-FIのセキュリティタイプのスタートアドレス
 *
 * WEP: 0x01(非対応)
 * WPA: 0x02
 * WPA2: 0x03
 * WPA3: 0x04
 */
#define SE_SECURITY_TYPE_START_ADDRESS 0x0064

/**
 * @brief セキュリティタイプの長さ
 *
 */
#define SE_SECURITY_TYPE_LENGTH 4U

/**
 * @brief AWS IoTのエンドポイントのスタートアドレス
 *
 */
#define SE_IOT_ENDPOINT_START_ADDRESS 0x0068

/**
 * @brief AWS IoTのエンドポイントの長さ
 *
 */
#define SE_IOT_ENDPOINT_LENGTH 128U

/**
 * @brief ThingNameのスタートアドレス
 *
 */
#define SE_THING_NAME_START_ADDRESS 0x00E8

/**
 * @brief ThingNameの長さ
 *
 */
#define SE_THING_NAME_LENGTH 128U

/**
 * @brief DeviceIDなどを保存するECC608のスロットID
 *
 */
#define SAVE_SLOT_ID 8

// SE内で保存するWi-Fiのセキュリティタイプ

//! WEP
#define SE_SECURITY_TYPE_WEP 0x00000001
//! WPA
#define SE_SECURITY_TYPE_WPA 0x00000002
//! WPA2
#define SE_SECURITY_TYPE_WPA2 0x00000003
//! WPA3
#define SE_SECURITY_TYPE_WPA3 0x00000004

// プロビジョニングフラグ

//! プロビジョニング未実施
#define PROVISIONING_FLAG_NOT_IMPLEMENTED 0x00000000
//! プロビジョニング実施済み
#define PROVISIONING_FLAG_ALREADY_FINISHED 0x00000001

/**
 * @brief I2Cがビジー状態から回復するまで待機する時間
 *
 * @warning
 * 1秒刻みで宣言する
 *
 */
#define SE_OPERATION_FLASH_RW_TIMEOUT_MS (5U * 1000U)

/**
 * @brief FlashにR/Wするまでのリトライ回数
 *
 */
#define SE_OPERATION_FLASH_RETRY_COUNT (10U)

    // #define関数マクロ
    // --------------------------------------------------

    // --------------------------------------------------
    // typedef定義
    // --------------------------------------------------

    // --------------------------------------------------
    // enumタグ定義（typedefを同時に行う）
    // --------------------------------------------------

    /**
     * @brief WiFiUtil関数を使用した時の結果
     */
    typedef enum
    {
        /**
         * @brief 成功
         */
        SE_OPERATION_RESULT_SUCCESS = 0,

        /**
         * @brief 失敗
         */
        SE_OPERATION_RESULT_FAILURE = 1

    } SEOperation_t;

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
     * @brief このライブラリを初期化する
     *
     *  @retval #SE_OPERATION_RESULT_SUCCESS 成功
     *  @retval #SE_OPERATION_RESULT_FAILURE 失敗
     */
    SEOperation_t eSEOperationInit(void);

    /**
     * ##################################
     * # 読み込み用関数
     * ##################################
     */

    /**
     * @brief WiFiの情報をセキュアエレメントから取得する
     *
     * @param[out] pxWiFiInfo WiFi情報を格納するデータ
     *
     *
     * @retval #SE_OPERATION_RESULT_SUCCESS 成功
     * @retval #SE_OPERATION_RESULT_FAILURE 失敗
     */
    SEOperation_t eGetWiFiInfoFromSE(WiFiInfo_t *pxWiFiInfo);

    /**
     * @brief プロビジョニングをセキュアエレメントから取得する
     *
     * @param[out] pxProvisioningFlag プロビジョニングフラグを格納するためのデータ
     *
     * @retval #SE_OPERATION_RESULT_SUCCESS 成功
     * @retval #SE_OPERATION_RESULT_FAILURE 失敗
     */
    SEOperation_t eGetProvisioningFlag(ProvisioningFlag_t *pxProvisioningFlag);

    /**
     * @brief IoT Endpointをセキュアエレメントから取得する
     *
     * @param[out] pxEndpoint endpointを格納するデータ
     *
     * @retval #SE_OPERATION_RESULT_SUCCESS 成功
     * @retval #SE_OPERATION_RESULT_FAILURE 失敗
     */
    SEOperation_t eGetIoTEndpoint(AWSIoTEndpoint_t *pxEndpoint);

    /**
     * @brief ThingName(普段使い用)をセキュアエレメントから取得する
     *
     * @param[out] pxName ThingNameを格納するデータ
     *
     * @retval #SE_OPERATION_RESULT_SUCCESS 成功
     * @retval #SE_OPERATION_RESULT_FAILURE 失敗
     */
    SEOperation_t eGetThingName(ThingName_t *pxName);

    /**
     * @brief 工場出荷ThingNameをセキュアエレメントから取得する
     *
     * @param[in] pxName ThingNameを格納するデータ
     *
     * @retval #SE_OPERATION_RESULT_SUCCESS 成功
     * @retval #SE_OPERATION_RESULT_FAILURE 失敗
     */
    SEOperation_t eGetFactoryThingName(FactoryThingName_t *pxName);

    /**
     * ##################################
     * # 書き込み用関数
     * ##################################
     */

    /**
     * @brief WiFiの情報セキュリティタイプを書き込む
     *
     * @param[in] pxWiFiInfo WiFi情報
     *
     * @retval #SE_OPERATION_RESULT_SUCCESS 成功
     * @retval #SE_OPERATION_RESULT_FAILURE 失敗
     */
    SEOperation_t eSetWiFiInfoToSE(const WiFiInfo_t *pxWiFiInfo);

    /**
     * @brief プロビジョニングをセキュアエレメントへ書き込む
     *
     * @param[in] xProvisioningFlag セットしたいプロビジョニングフラグデータ
     *
     * @retval #SE_OPERATION_RESULT_SUCCESS 成功
     * @retval #SE_OPERATION_RESULT_FAILURE 失敗
     */
    SEOperation_t eSetProvisioningFlag(const ProvisioningFlag_t xProvisioningFlag);

    /**
     * @brief IoT Endpointをセキュアエレメントへ書き込む
     *
     * @param[in] pxEndpoint 格納したいendpointのデータ
     *
     * @retval #SE_OPERATION_RESULT_SUCCESS 成功
     * @retval #SE_OPERATION_RESULT_FAILURE 失敗
     */
    SEOperation_t eSetIoTEndpoint(const AWSIoTEndpoint_t *pxEndpoint);

    /**
     * @brief ThingName(普段使い用)をセキュアエレメントへ書き込む
     *
     * @param[in] pxName 格納したいThingNameのデータ
     *
     * @retval #SE_OPERATION_RESULT_SUCCESS 成功
     * @retval #SE_OPERATION_RESULT_FAILURE 失敗
     */
    SEOperation_t eSetThingName(const ThingName_t *pxName);

    // --------------------------------------------------
    // インライン関数
    // --------------------------------------------------

#ifdef __cplusplus
}
#endif

#endif /* end SE_OPERATION_H_ */
