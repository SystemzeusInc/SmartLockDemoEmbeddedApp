/**
 * @file motor.h
 * @author Systemzeus Inc.
 * @copyright Copyright © 2023 Systemzeus Inc. All rights reserved.
 */
#ifndef MOTOR_H_
#define MOTOR_H_

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

    // --------------------------------------------------
    // #define関数マクロ
    // --------------------------------------------------

    // --------------------------------------------------
    // typedef定義
    // --------------------------------------------------
    typedef void (*MOTOR_DELAY)(const uint32_t ms); /**< 遅延関数インターフェース */

    // --------------------------------------------------
    // enumタグ定義（typedefを同時に行う）
    // --------------------------------------------------
    /**
     * @brief モーターライブラリの結果
     */
    typedef enum
    {
        MOTOR_RESULT_SUCCEED = 0x0,  /**< 成功 */
        MOTOR_RESULT_FAILED,         /**< 失敗 */
        MOTOR_RESULT_BAD_PARAMETER,  /**< 不正な引数 */
        MOTOR_RESULT_TIMEOUT,        /**< タイムアウト */
        MOTOR_RESULT_NOT_IMPLEMENTED /**< 未実装 */
    } MotorResult_t;

    // --------------------------------------------------
    // struct/unionタグ定義（typedefを同時に行う）
    // --------------------------------------------------
    /**
     * @brief モーターライブラリのインターフェース
     */
    typedef struct
    {
        MOTOR_DELAY delay; /**< 遅延関数 */
    } MotorInterface_t;

    // --------------------------------------------------
    // extern変数宣言
    // --------------------------------------------------

    // --------------------------------------------------
    // 関数プロトタイプ宣言
    // --------------------------------------------------
    /**
     * @brief モーターライブラリの初期化
     *
     * @param [in] pxInterface インターフェース
     *
     * @return MotorResult_t 結果
     */
    MotorResult_t eInitializeMotor(MotorInterface_t *pxInterface);

    /**
     * @brief モーターの制御を開始(PWM開始)
     */
    void vStartControlMotor(void);

    /**
     * @brief モーターの制御を停止(PWM停止)
     */
    void vStopControlMotor(void);

    /**
     * @brief 現在の角度を取得
     *
     * @param [out] puxElapsedTime 取得にかかった時間 ms
     *
     * @return uint16_t 角度[°]
     */
    uint16_t usGetAngle(uint8_t *puxElapsedTime);

    /**
     * @brief 指定回数角度を測定しその平均値を取得
     *
     * @param [in]  uxN            平均をとる数
     * @param [out] puxElapsedTime 取得にかかった時間 ms
     *
     * @return uint16_t 角度[°]
     */
    uint16_t usGetAverageAngle(uint8_t uxN, uint8_t *puxElapsedTime);

    /**
     * @brief 最短角度で指定角度(絶対角度)に回転
     *
     * @param [in] usDestAngle 指定角度
     *
     * @return bool 成否
     */
    bool bRotateToAngleByShortest(uint16_t usDestAngle);

    /**
     * @brief 指定角度分だけ回転
     *
     * @param [in] sDeltaAngle 移動角度(-360~360°) 時計回り<0°, 反時計周り>0°
     * @param [in] ulTimeout   タイムアウト ms
     *
     * @return bool 成否
     */
    bool bRotateAtAngle(int16_t sDeltaAngle, uint32_t ulTimeout);

    // --------------------------------------------------
    // インライン関数
    // --------------------------------------------------

#ifdef __cplusplus
}
#endif

#endif /* end MOTOR_H_ */
