/**
 * @file motor.c
 * @author Systemzeus Inc.
 * @copyright Copyright © 2023 Systemzeus Inc. All rights reserved.
 */

// --------------------------------------------------
// システムヘッダの取り込み
// --------------------------------------------------
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "definitions.h"
#include "FreeRTOS.h"

// --------------------------------------------------
// ユーザ作成ヘッダの取り込み
// --------------------------------------------------
#include "config/motor_config.h"

#include "common/include/application_define.h"
#include "include/motor.h"
#if (MOTOR_TYPE == 1)
#    include "include/adc_sensor.h"
#endif

// --------------------------------------------------
// 自ファイル内でのみ使用する#defineマクロ
// --------------------------------------------------
#if (MOTOR_TYPE == 1)
// FR5311M関連の値↓
#    define PWM_CYCLE_MS              20   /**< PWMの周期 */
#    define STOP_DUTY_VALUE_US        1500 /**< サーボを停止させるDuty値(1480~1520) */
#    define MAXIMUM_CW_DUTY_VALUE_US  2000 /**< 時計回りさせるときの最大Duty値 */
#    define MAXIMUM_CCW_DUTY_VALUE_US 1000 /**< 反時計回りさせるときの最大Duty値 */
#    define MINIMUM_CW_DUTY_VALUE_US  1520 /**< 時計回りさせるときの最小Duty値 */
#    define MINIMUM_CCW_DUTY_VALUE_US 1480 /**< 反時計回りさせるときの最小Duty値 */

#    define ADC_RESOLUTION 4095 /**< ADC分解能 12bitADC */
#elif (MOTOR_TYPE == 2)
// Parallax Feedback 360° High-Speed Servo関連の値↓
#    define PWM_CYCLE_MS              20   /**< PWMの周期 */
#    define STOP_DUTY_VALUE_US        1500 /**< サーボを停止させるDuty値(1480~1520) */
#    define MAXIMUM_CW_DUTY_VALUE_US  1720 /**< 時計回りさせるときの最大Duty値 */
#    define MAXIMUM_CCW_DUTY_VALUE_US 1280 /**< 反時計回りさせるときの最大Duty値 */
#    define MINIMUM_CW_DUTY_VALUE_US  1520 /**< 時計回りさせるときの最小Duty値 */
#    define MINIMUM_CCW_DUTY_VALUE_US 1480 /**< 反時計回りさせるときの最小Duty値 */
#else
#    error Please select the type of motor
#endif

// --------------------------------------------------
// 自ファイル内でのみ使用する#define関数マクロ
// --------------------------------------------------
#define ALLOWABLE_RANGE_SMALLER(x) (x - ALLOWABLE_ERROR_ANGLE)
#define ALLOWABLE_RANGE_BIGGER(x)  (x + ALLOWABLE_ERROR_ANGLE)

#if (MOTOR_TYPE == 1)
#    define CONVERT_ADC_TO_DEGREE(adc) ((uint16_t)((float)360 * adc / ADC_RESOLUTION)) /**< ADC値から角度算出 */
#endif

// --------------------------------------------------
// 自ファイル内でのみ使用するtypedef定義
// --------------------------------------------------

// --------------------------------------------------
// 自ファイル内でのみ使用するenumタグ定義（typedefを同時に行う）
// --------------------------------------------------

// --------------------------------------------------
// 自ファイル内でのみ使用するstruct/unionタグ定義（typedefを同時に行う）
// --------------------------------------------------

// --------------------------------------------------
// ファイル内で共有するstatic変数宣言
// --------------------------------------------------
static MotorInterface_t *gpxInterfaceMotor = NULL; /**< Motorインターフェース */
#if (MOTOR_TYPE == 1)
static ADCSensorInterface_t gxInterfaceADC = {0};  /**< ADCSensorインターフェース */
#endif

// PID関連↓
static float fPrevErr = 0; /**< 1step前の目標値との誤差 */
static float fErr = 0;     /**< 目標値との誤差 */
static float fD = 0;       /**< 微分項 */
static float fI = 0;       /**< 積分項 */

#if (MOTOR_TYPE == 2)
static float gfAngle = 0; /** Parallax 角度 */
#endif

// --------------------------------------------------
// static関数プロトタイプ宣言
// --------------------------------------------------
#if (MOTOR_TYPE == 2)
/**
 * @brief ICAPの割り込み時に呼ばれるコールバック関数
 *
 * @param [in] pxContext コンテキスト(未使用)
 */
static void vICAP_Cb(uintptr_t pxContext);
#endif

/**
 * @brief 連続で角度を取得し目標角度に対して許容範囲内いるか確認
 *
 * @note センサ値が不安定なため再確認を行う際などに使用する
 *
 * @param [in] sDestAngle     目標角度
 * @param [in] uxN            連続で角度を取得する回数
 * @param [in] puxElapsedTime 経過時間
 *
 * @retval true 目標値に対して許容範囲内
 * @retval false 範囲外
 */
static bool bCheckInRange(int16_t sDestAngle, uint8_t uxN, uint8_t *puxElapsedTime);

/**
 * @brief PID値算出
 *
 * @param [in] fDist      目標値
 * @param [in] fCurrent   現在値
 * @param [in] ulInterval 前回との間隔 ms
 *
 * @return float PID出力値
 */
static float prvCalcPID(const float fDist, const float fCurrent, const uint32_t ulInterval);

/**
 * @brief PID関連の変数を初期化
 */
static void vprvResetPIDValue();

/**
 * @brief パーセントから出力するDuty値を算出(-100~100%)
 *
 * @param [in] fPercent パーセント
 *
 * @return uint16_t
 */
static uint16_t prvCalcDutyValueFromPercent(float fPercent);

/**
 * @brief PWMの周期を設定
 *
 * @param [in] ms 周期
 */
static void prvSetPWMMilliSecondsCycle(const uint32_t ms);

/**
 * @brief PWMのDuty値を設定
 *
 * @param [in] us Duty値
 */
static void prvSetPWMDutyValueMicroSeconds(const uint32_t us);

/**
 * @brief マイクロ秒からOCMPのOCXRC値を算出
 *
 * @param [in] us マイクロ秒
 *
 * @return uint32_t OCXRC値
 */
static uint32_t ulCalcOCXRCFromMicroSeconds(const uint32_t us);

/**
 * @brief ミリ秒からTMRXのPRX値を算出
 *
 * @param [in] us ミリ秒
 *
 * @return uint32_t PR値
 */
static uint32_t ulCalcPRXFromMilliSeconds(const uint32_t ms);

// --------------------------------------------------
// 変数定義（staticを除く）
// --------------------------------------------------

// --------------------------------------------------
// 関数定義（staticを除く）
// --------------------------------------------------
MotorResult_t eInitializeMotor(MotorInterface_t *pxInterface)
{
    // Validation
    if (pxInterface == NULL)
    {
        return MOTOR_RESULT_BAD_PARAMETER;
    }
    gpxInterfaceMotor = pxInterface;

#if (MOTOR_TYPE == 1)
    // Initialize ADC
    gxInterfaceADC.delay = gpxInterfaceMotor->delay;
    eInitializeADCSensor(&gxInterfaceADC);
#elif (MOTOR_TYPE == 2)
    TMR2_Start();
    // Initialize ICAP
    ICAP3_CallbackRegister(vICAP_Cb, NULL);
    ICAP3_Enable();
#endif

    // Start Timer
    prvSetPWMMilliSecondsCycle(PWM_CYCLE_MS);
    TMR3_InterruptEnable();
    TMR3_Start();

    // Enable PWM
    prvSetPWMDutyValueMicroSeconds(STOP_DUTY_VALUE_US);
    OCMP1_Enable();

    return MOTOR_RESULT_SUCCEED;
}

void vStartControlMotor(void)
{
    // Enable PWM
    OCMP1_Enable();
}

void vStopControlMotor(void)
{
    // Disable PWM
    OCMP1_Disable();
}

uint16_t usGetAngle(uint8_t *puxElapsedTime)
{
#if (MOTOR_TYPE == 1)
    uint16_t usADCValue = usGetADCValue(puxElapsedTime);
    return CONVERT_ADC_TO_DEGREE(usADCValue);
#elif (MOTOR_TYPE == 2)
    if (puxElapsedTime != NULL)
    {
        *puxElapsedTime = 0;
    }
    return (uint16_t)gfAngle;
#endif
}

uint16_t usGetAverageAngle(uint8_t uxN, uint8_t *puxElapsedTime)
{
#if (MOTOR_TYPE == 1)
    float fAverageADCValue = 0;
    for (uint8_t i = 0; i < uxN; i++)
    {
        uint8_t uxTmpElapsed = 0;
        fAverageADCValue += (float)usGetADCValue(&uxTmpElapsed);
        if (puxElapsedTime != NULL)
        {
            *puxElapsedTime += uxTmpElapsed;
        }
    }
    fAverageADCValue /= (float)uxN;
    return CONVERT_ADC_TO_DEGREE(fAverageADCValue);
#elif (MOTOR_TYPE == 2)
    if (puxElapsedTime != NULL)
    {
        *puxElapsedTime = 0;
    }
    return (uint16_t)gfAngle;
#endif
}

bool bRotateToAngle(const uint16_t usDestAngle)
{
    if (usDestAngle > 360)
    {
        return false;
    }

    // 現在角度確認
    uint16_t usAngle = usGetAverageAngle(ANGLE_AVERAGE_NUM, NULL);

    int16_t sDeltaAngle = 0;
    sDeltaAngle = (int16_t)(usDestAngle - usAngle);

    // 回転
    bRotateAtAngleByDelta(sDeltaAngle, MOTOR_OUTPUT_TIMEOUT);

    return true;
}

bool bRotateAtAngleByDelta(int16_t sDeltaAngle, uint32_t ulTimeout)
{
    bool bResult = false;
    uint32_t ulTotalElapsedTime = 0; // 経過時間

    static uint32_t debug_count = 0;

    if (sDeltaAngle > 360 || sDeltaAngle < -360)
    {
        return false;
    }

    uint16_t usNowAngle = usGetAngle(NULL);                   // 開始,現在角度
    int16_t sDestAngle = (int16_t)(usNowAngle + sDeltaAngle); // 目標角度

#if 0
    bool bStraddleFlag = false; // 境界(0°, 360°)にまたがって回転するとき

    if (sDestAngle > 360)
    {
        sDestAngle -= 360;
        bStraddleFlag = true;
    }
    else if (sDestAngle < 0)
    {
        sDestAngle = 360 - sDestAngle;
        bStraddleFlag = true;
    }
    uint16_t usAbsoluteDestAngle = sDestAngle;
#endif

    APP_PRINTFDebug("Rotate start: %3d deg -> dest: %3d deg", usNowAngle, sDestAngle);

    vStartControlMotor();
    vprvResetPIDValue();

    while (1)
    {
        uint8_t uxPIDElapsedTime = CONTROL_INTERVAL_MS;
        uint8_t uxGetElapsedTime = 0;

        usNowAngle = usGetAverageAngle(ANGLE_AVERAGE_NUM, &uxGetElapsedTime);
        uxPIDElapsedTime += uxGetElapsedTime;

        // 停止判定
        if (usNowAngle >= ALLOWABLE_RANGE_SMALLER(sDestAngle) && usNowAngle <= ALLOWABLE_RANGE_BIGGER(sDestAngle)) // 範囲内にいる場合は停止
        {
            prvSetPWMDutyValueMicroSeconds(prvCalcDutyValueFromPercent(0));
            gpxInterfaceMotor->delay(50);                                              // 停止するまで少し待機
            if (bCheckInRange(sDestAngle, CHECK_STOP_IN_RANGE_NUM, &uxGetElapsedTime)) // 停止後、再度範囲内にいることを確認
            {
                bResult = true;
                break;
            }
            else // センサ誤差などで範囲外だと判断される場合は、再度移動
            {
                uxPIDElapsedTime += uxGetElapsedTime;

                usNowAngle = usGetAverageAngle(ANGLE_AVERAGE_NUM, &uxGetElapsedTime);
                uxPIDElapsedTime += uxGetElapsedTime;
                vprvResetPIDValue();
            }
        }

        float fPIDOutput = prvCalcPID((float)sDestAngle, (float)usNowAngle, uxPIDElapsedTime + CONTROL_INTERVAL_MS);
        fPIDOutput *= -1; // PIDの出力と実際の値が逆のため
        // if (debug_count % 10 == 0) // 大量に過ぎて正しく表示できないため
        // {
        //     APP_PRINTFDebug("%3d->%3d, pid: %3.3f, us: %4d", usNowAngle, sDestAngle, fPIDOutput, prvCalcDutyValueFromPercent((int8_t)fPIDOutput));
        // }
        prvSetPWMDutyValueMicroSeconds(prvCalcDutyValueFromPercent(fPIDOutput));
        ulTotalElapsedTime += uxPIDElapsedTime;

        if (ulTotalElapsedTime >= ulTimeout) // タイムアウト
        {
            APP_PRINTFWarn("Target value not reached in time. Stop PWM.");
            bResult = false;
            break;
        }

        gpxInterfaceMotor->delay(CONTROL_INTERVAL_MS);
        debug_count++;
    }
    vStopControlMotor();
    APP_PRINTFDebug("stop: %3d deg, dist: %3d deg", usNowAngle, sDestAngle);
    return bResult;
}

// --------------------------------------------------
// static関数定義
// --------------------------------------------------
#if (MOTOR_TYPE == 2)
static void vICAP_Cb(uintptr_t pxContext)
{
    static uint16_t usPreValue = 0;
    static bool bFlag = false;

    uint16_t usValue = ICAP3_CaptureBufferRead(); // 必ず1回目はrisingEdgeになるように設定

    if (usPreValue > usValue) // カウンタがオーバーフローしたとき
    {
        return;
    }

    if (!bFlag) // rising edge
    {
        usPreValue = usValue;
        bFlag = true;
        return;
    }

    uint16_t diff = usValue - usPreValue;

    TMR2 = 0; // カウンタクリア
    bFlag = false;
    usPreValue = 0;

    // 1 / 910[Hz] * 6250k[Hz] * 97.1% = 6669
    // 1 / 910[Hz] * 6250k[Hz] * 2.9%  = 199
    gfAngle = (float)(diff - 199) * 360 / (6669 - 199 + 1);
}
#endif

static bool bCheckInRange(int16_t sDestAngle, uint8_t uxN, uint8_t *puxElapsedTime)
{
    uint8_t uxElapsedTime = 0;
    if (puxElapsedTime != NULL)
    {
        *puxElapsedTime = 0;
    }

    for (uint8_t i = 0; i < uxN; i++) // 連続で範囲内か確認
    {
        uint16_t usNowAngle = usGetAngle(&uxElapsedTime);
        if (puxElapsedTime != NULL)
        {
            *puxElapsedTime += uxElapsedTime;
        }
        if (usNowAngle < ALLOWABLE_RANGE_SMALLER(sDestAngle) ||
            usNowAngle > ALLOWABLE_RANGE_BIGGER(sDestAngle))
        {
            return false;
        }
    }
    return true;
}

static float prvCalcPID(const float fDist, const float fCurrent, const uint32_t ulInterval)
{
    fErr = fDist - fCurrent;
    fI = fI + ((fErr + fPrevErr) * ulInterval / 1000 / 2);
    fD = (fErr - fPrevErr) / ulInterval * 1000;

    fPrevErr = fErr;

    return KP * fErr + KI * fI + KD * fD;
}

static void vprvResetPIDValue()
{
    fPrevErr = fErr = fD = fI = 0;
}

static uint16_t prvCalcDutyValueFromPercent(float fPercent)
{
    uint16_t usDuty;
    if (fPercent > 100)
    {
        fPercent = 100;
    }
    else if (fPercent < -100)
    {
        fPercent = -100;
    }

    if (fPercent < 0)
    {
        usDuty = (uint16_t)(MINIMUM_CCW_DUTY_VALUE_US - (-fPercent * 480 / 100)) - MINIMUM_DUTY_OFFSET;
        if (usDuty < LIMIT_CCW_DUTY_VALUE) // 速度制限
        {
            usDuty = LIMIT_CCW_DUTY_VALUE;
        }
        if (usDuty < MAXIMUM_CCW_DUTY_VALUE_US)
        {
            usDuty = MAXIMUM_CCW_DUTY_VALUE_US;
        }
    }
    else if (fPercent > 0)
    {
        usDuty = (uint16_t)(MINIMUM_CW_DUTY_VALUE_US + (fPercent * 480 / 100)) + MINIMUM_DUTY_OFFSET;
        if (usDuty > LIMIT_CW_DUTY_VALUE)
        {
            usDuty = LIMIT_CW_DUTY_VALUE;
        }
        if (usDuty > MAXIMUM_CW_DUTY_VALUE_US)
        {
            usDuty = MAXIMUM_CW_DUTY_VALUE_US;
        }
    }
    else
    {
        usDuty = STOP_DUTY_VALUE_US;
    }
    return usDuty;
}

static void prvSetPWMMilliSecondsCycle(const uint32_t ms)
{
    TMR3_PeriodSet(ulCalcPRXFromMilliSeconds(ms));
}

static void prvSetPWMDutyValueMicroSeconds(const uint32_t us)
{
    OCMP1_CompareSecondaryValueSet(ulCalcOCXRCFromMicroSeconds(us));
}

static uint32_t ulCalcOCXRCFromMicroSeconds(const uint32_t us)
{
    return (uint32_t)((double)TMR3_FrequencyGet() / 1000000 * us);
}

static uint32_t ulCalcPRXFromMilliSeconds(const uint32_t ms)
{
    return (uint32_t)((double)TMR3_FrequencyGet() / 1000 * ms);
}

// --------------------------------------------------
// Unit Test用関数定義(関数のプロトタイプ宣言は「自身のファイル名+ "_test.h"」で宣言されていること)
// --------------------------------------------------
#if (BUILD_MODE_TEST == 1) /* BUILD_MODE_TESTが定義されているとき */
#endif                     /* end  BUILD_MODE_TEST */
