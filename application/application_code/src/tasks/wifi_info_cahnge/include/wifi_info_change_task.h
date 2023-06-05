/**
 * @file wifi_info_change_task.h
 * @author Systemzeus Inc.
 * @copyright Copyright © 2023 Systemzeus Inc. All rights reserved.
 */
#ifndef WIFI_INFO_CHANGE_TASK_H_
#define WIFI_INFO_CHANGE_TASK_H_

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

/**
 * @brief Wi-Fiの変更情報が得られるまで待機する時間（３分）
 */
#define WIFI_INFO_TO_BE_SENT_WAIT_TIME_MS (3U * 60U * 1000U)

/**
 * @brief Wi-Fiの情報をパースする際の区切り文字
 */
#define WIFI_INFO_SEPARATE_CHARACTER '\n'

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
        WIFI_INFO_CHANGE_RESULT_SUCCESS = 0x00,

        /**
         * @brief 失敗
         */
        WIFI_INFO_CHANGE_RESULT_FAILED = 0x01
    } WIFIInfoChangeResult_t;

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
     * @brief Wi-Fi接続先変更タスクを初期化、起動する
     *
     * @details
     * スマホアプリからのWi-Fi接続先情報を受け取り、Flashに保存後自身のタスクを削除する
     *
     * @retval WIFI_INFO_CHANGE_RESULT_SUCCESS 成功
     * @retval WIFI_INFO_CHANGE_RESULT_FAILED  失敗
     */
    WIFIInfoChangeResult_t eWiFiInfoChangeTaskInit(void);

    // --------------------------------------------------
    // インライン関数
    // --------------------------------------------------

#ifdef __cplusplus
}
#endif

#endif /* end WIFI_INFO_CHANGE_TASK_H_ */
