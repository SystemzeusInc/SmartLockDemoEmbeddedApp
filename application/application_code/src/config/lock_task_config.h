/**
 * @file lock_task_config.h
 * @author Systemzeus Inc.
 * @copyright Copyright © 2023 Systemzeus Inc. All rights reserved.
 */
#ifndef LOCK_TASK_CONFIG_H_
#define LOCK_TASK_CONFIG_H_

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
 * @brief 自動施錠を行うまでの時間（ミリ秒）
 */
#define AUTO_LOCK_TIME_MS (15000U)

#define ANGLE_TO_LOCK   190 /**< 施錠時の角度 */
#define ANGLE_TO_UNLOCK 100 /**< 開錠時の角度 */

#ifdef __cplusplus
}
#endif

#endif /* end LOCK_TASK_CONFIG_H_ */
