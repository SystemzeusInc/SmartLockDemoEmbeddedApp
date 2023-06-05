/**
 * @file flash_data.h
 * @author Systemzeus Inc.
 * @copyright Copyright © 2023 Systemzeus Inc. All rights reserved.
 */
#ifndef FLASH_DATA_H_
#define FLASH_DATA_H_

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

#include "iot_wifi.h"

// --------------------------------------------------
// ユーザ作成ヘッダの取り込み
// --------------------------------------------------
#include "config/flash_config.h"

    // --------------------------------------------------
    // #defineマクロ
    // --------------------------------------------------

    // --------------------------------------------------
    // #define関数マクロ
    // --------------------------------------------------

    // --------------------------------------------------
    // typedef定義
    // --------------------------------------------------

    /**
     * @brief プロビジョニングフラグ
     *
     * true : プロビジョニング実施済み
     * false: プロビジョニング未実施
     */
    typedef bool ProvisioningFlag_t;
    // --------------------------------------------------
    // enumタグ定義（typedefを同時に行う）
    // --------------------------------------------------

    // --------------------------------------------------
    // struct/unionタグ定義（typedefを同時に行う）
    // --------------------------------------------------

    /**
     * @brief WiFiの情報
     */
    typedef struct
    {
        /**
         * @brief Wi-FiのSSID
         *
         * @warning
         * 必ず\0終端のさせること
         */
        uint8_t cWifiSSID[WIFI_SSID_MAX_LENGTH + 1];

        /**
         * @brief Wi-Fiのパスワード
         *
         * @warning
         * 必ず\0終端のさせること
         */
        uint8_t cWiFiPassword[WIFI_PASSWORD_MAX_LENGTH + 1];

        /**
         * @brief Wi-Fiのセキュリティ
         */
        WIFISecurity_t xWiFiSecurity;

    } WiFiInfo_t;

    /**
     * @brief AWS IoT Endpoint
     */
    typedef struct
    {
        /**
         * @brief エンドポイント
         *
         * @warning
         * 必ず\0終端させること
         */
        uint8_t ucEndpoint[AWS_IOT_ENDPOINT_MAX_LENGTH + 1];
    } AWSIoTEndpoint_t;

    /**
     * @brief 工場出荷ThingName
     */
    typedef struct
    {
        /**
         * @brief 工場出荷ThingName
         */
        uint8_t ucName[FACTORY_THING_NAME_LENGTH + 1];
    } FactoryThingName_t;

    /**
     * @brief 普段使い用ThingName
     */
    typedef struct
    {
        /**
         * @brief ThingName
         */
        uint8_t ucName[THING_NAME_LENGTH + 1];
    } ThingName_t;

    // --------------------------------------------------
    // extern変数宣言
    // --------------------------------------------------

    // --------------------------------------------------
    // 関数プロトタイプ宣言
    // --------------------------------------------------

    // --------------------------------------------------
    // インライン関数
    // --------------------------------------------------

#ifdef __cplusplus
}
#endif

#endif /* end FLASH_DATA_H_ */
