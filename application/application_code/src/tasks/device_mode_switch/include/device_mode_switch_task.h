/**
 * @file device_mode_switch_task.h
 * @author Systemzeus Inc.
 * @copyright Copyright © 2023 Systemzeus Inc. All rights reserved.
 */
#ifndef DEVICE_MODE_SWITCH_TASK_H_
#define DEVICE_MODE_SWITCH_TASK_H_

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

#include "tasks/device_mode_switch/include/device_mode_switch_event.h"
#include "tasks/shadow/include/device_shadow_task.h"

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
     * @brief 関数の実行結果
     */
    typedef enum
    {
        /**
         * @brief 成功したとき
         */
        DEVICE_MODE_SWITCH_TASK_RESULT_SUCCESS = 0,
        /**
         * @brief 失敗したとき
         */
        DEVICE_MODE_SWITCH_TASK_RESULT_FAILED = 1
    } DeviceModeSwitchTaskResult_t;

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
     * @brief DeviceModeSwitchTaskを初期化する
     *
     * @retval #DEVICE_MODE_SWITCH_TASK_RESULT_SUCCESS タスクの初期化成功、もしくはタスク初期化済み
     * @retval #DEVICE_MODE_SWITCH_TASK_RESULT_FAILED  タスクの初期化失敗、もしくはキュー作成失敗
     */
    DeviceModeSwitchTaskResult_t eDeviceModeSwitchTaskInit(void);

    /**
     * @brief DeviceModeの変更をタスクに通知する
     *
     * @param[in] pxDeviceModeSwitchData どのようなイベントで状態移行を行うかを示す
     *
     * @retval #DEVICE_MODE_SWITCH_TASK_RESULT_SUCCESS 通知成功
     * @retval #DEVICE_MODE_SWITCH_TASK_RESULT_FAILED  通知失敗
     */
    DeviceModeSwitchTaskResult_t eDeviceModeSwitch(const DeviceModeSwitchData_t *pxDeviceModeSwitchData);

    // --------------------------------------------------
    // インライン関数
    // --------------------------------------------------

#ifdef __cplusplus
}
#endif

#endif /* end DEVICE_MODE_SWITCH_TASK_H_ */
