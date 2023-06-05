/**
 * @file motor_config.h
 * @author Systemzeus Inc.
 * @copyright Copyright © 2023 Systemzeus Inc. All rights reserved.
 */
#ifndef MOTOR_CONFIG_H_
#define MOTOR_CONFIG_H_

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
// Motor type
// 1: FR5311M
// 2: PARALLAX Feedback 360° High-Speed Servo(#900-00360)
#define MOTOR_TYPE 2

// モーター制御に関連する値↓
#define ALLOWABLE_ERROR_ANGLE 2 /**< 目標角度に対する許容誤差(角度°) */
#define CONTROL_INTERVAL_MS   1 /**< 制御周期 */

#define LIMIT_CW_DUTY_VALUE  1800 /**< 時計回りのDuty値の制限 */
#define LIMIT_CCW_DUTY_VALUE 1200 /**< 反時計回りのDuty値の制限 */

#if (MOTOR_TYPE == 1)
#    define MINIMUM_DUTY_OFFSET 60 /**< Duty値が最小付近(1480, 1520)では実際は回らないためとるオフセット */
#elif (MOTOR_TYPE == 2)
#    define MINIMUM_DUTY_OFFSET 0 /**< Duty値が最小付近(1480, 1520)では実際は回らないためとるオフセット */
#endif

#define ANGLE_AVERAGE_NUM       5 /**< 角度取得時に平均値をとる数(センサ値のノイズ対策) */
#define CHECK_STOP_IN_RANGE_NUM 3 /**< 停止後、目標範囲内いるか確認する回数(センサ値のノイズ対策) */

#define MOTOR_OUTPUT_TIMEOUT 2000 /**< モーター制御のタイムアウト ms */

/* -------------------------------------------------- */

// PID関連↓
#if (MOTOR_TYPE == 1)
#    define KP 0.4   /**< Pゲイン */
#    define KI 0.2   /**< Iゲイン */
#    define KD 0.005 /**< Dゲイン */
#elif (MOTOR_TYPE == 2)
#    define KP 0.4   /**< Pゲイン */
#    define KI 0.01  /**< Iゲイン */
#    define KD 0.005 /**< Dゲイン */
#endif

#ifdef __cplusplus
}
#endif

#endif /* end MOTOR_CONFIG_H_ */
