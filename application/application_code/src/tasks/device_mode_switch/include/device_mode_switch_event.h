/**
 * @file device_mode_switch_event.h
 * @author Systemzeus Inc.
 * @copyright Copyright © 2023 Systemzeus Inc. All rights reserved.
 */
#ifndef DEVICE_MODE_SWITCH_EVENT_H_
#define DEVICE_MODE_SWITCH_EVENT_H_

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
     * @brief DeviceModeSwitchTaskへの状態移行を指示する際に使用する列挙型
     */
    typedef enum
    {
        /**
         * @brief 通常モードへ移行。電源起動以外の場面では使用しない。
         */
        DEVICE_MODE_SWITCH_MAIN_WAKE_UP = 0x01,

        /**
         * @brief 通常モードへ移行(電源起動以外)
         */
        DEVICE_MODE_SWITCH_MAIN = 0x02,

        /**
         * @brief プロビジョニングモードへ移行
         */
        DEVICE_MODE_SWITCH_PROVISIONING = 0x03,

        /**
         * @brief プロビジョニングモードが終了したため通常モードへ移行
         */
        DEVICE_MODE_SWITCH_PROVISIONING_DONE = 0x04,

        /**
         * @brief Wi-Fi接続先変更モードへの移行
         */
        DEVICE_MODE_SWITCH_WIFI_INFO_SWITCH = 0x05,

        /**
         * @brief スマホからWi-Fi接続先を受け取り、Wi-Fi接続先変更モードを終了してメインモードへ移行する
         */
        DEVICE_MODE_SWITCH_WIFI_INFO_DONE = 0x06,

        /**
         * @brief スマホからWi-Fi接続先を受け取れず、タイムアウトした場合のメインモード移行
         */
        DEVICE_MODE_SWITCH_WIFI_INFO_TIMEOUT = 0x07,

    } DeviceModeSwitchEvent_t;

    /**
     * @brief ロックアプリの状態
     */
    typedef enum
    {
        /**
         * @brief メインモード
         */
        LOCK_DEVICE_MODE_MAIN = 0x00,

        /**
         * @brief プロビジョニングモード
         */
        LOCK_DEVICE_MODE_PROVISIONING = 0x01,

        /**
         * @brief Wi-Fi接続先変更
         */
        LOCK_DEVICE_MODE_WIFI_INFO_CHANGE = 0x02

    } LockDeviceModeState_t;

    /**
     * @brief プロビジョニング結果
     */
    typedef enum
    {
        /**
         * @brief 成功
         */
        PROVISIONING_RESULT_SUCCESS = 0,

        /**
         * @brief 失敗
         */
        PROVISIONING_RESULT_FAILED = 1
    } ProvisioningResult_t;

    // --------------------------------------------------
    // struct/unionタグ定義（typedefを同時に行う）
    // --------------------------------------------------

    /**
     * @brief DeviceModeSwitchTaskとの通信時に使用するデータ
     */
    typedef struct
    {
        /**
         * @brief 状態移行指示
         */
        DeviceModeSwitchEvent_t eDeviceModeSwitchEvent;

        /**
         * @brief モード移行の際にDeviceModeSwitchTaskに渡すパラメータ
         *
         */
        union
        {
            /**
             * @brief リンキング結果
             */
            ProvisioningResult_t eProvisioningResult;
        } param;

    } DeviceModeSwitchData_t;

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

#endif /* end DEVICE_MODE_SWITCH_EVENT_H_ */
