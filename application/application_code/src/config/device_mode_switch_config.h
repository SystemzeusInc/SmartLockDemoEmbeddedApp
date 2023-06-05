/**
 * @file device_mode_switch_config.h
 * @author Systemzeus Inc.
 * @copyright Copyright © 2023 Systemzeus Inc. All rights reserved.
 */
#ifndef DEVICE_MODE_SWITCH_CONFIG_H_
#define DEVICE_MODE_SWITCH_CONFIG_H_

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

    // --------------------------------------------------
    // #defineマクロ
    // --------------------------------------------------
/**
 * @brief デバイスモードスイッチタスクがShadowを変更するまで待機する時間
 *
 * @details
 * ShadowをUpdateする前にOTAの初期化をしており、そのOTA初期化に時間がかかりShadowのUpdateが遅れる可能性があるため10秒程度待機する
 */
#define DEVICE_MODE_SWITCH_SHADOW_UPDATE_WAITE_TIME_MS (SHADOW_TASK_SHUTDOWN_WAIT_MS)

#ifdef __cplusplus
}
#endif

#endif /* end DEVICE_MODE_SWITCH_CONFIG_H_ */
