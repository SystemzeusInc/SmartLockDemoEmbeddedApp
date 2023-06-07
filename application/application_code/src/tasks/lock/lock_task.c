/**
 * @file lock_task.c
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
#include "config/task_config.h"
#include "config/queue_config.h"
#include "config/lock_task_config.h"

#include "common/include/application_define.h"
#include "board/include/board_util.h"

#include "common/include/device_state.h"

#include "include/lock_task.h"
#include "include/motor.h"
#if (MOTOR_TYPE == 1)
#    include "include/adc_sensor.h"
#endif

#include "tasks/shadow/include/device_shadow_task.h"

// --------------------------------------------------
// 自ファイル内でのみ使用する#defineマクロ
// --------------------------------------------------

// --------------------------------------------------
// 自ファイル内でのみ使用する#define関数マクロ
// --------------------------------------------------

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
static LockTaskData_t gxTaskData = {0}; /**< タスク関連データ */

static MotorInterface_t gxInterfaceMotor = {0}; /**< Motorのインターフェース */

// --------------------------------------------------
// static関数プロトタイプ宣言
// --------------------------------------------------
/**
 * @brief LockTask
 *
 * @param [in] pvParameters パラメータ
 */
static void prvLockTask(void *pvParameters);

/**
 * @brief 施錠状態をクラウドに通知(Update shadow)
 *
 * @param[in] 操作主体
 *
 * @return LockTaskResult_t 通知結果
 */
LockTaskResult_t prvUpdateShadow(const UnlockingOperatorType_t eOperator);

/**
 * @brief LockTaskに命令
 *
 * @param [in] pxData    命令内容
 * @param [in] ulTimeout タイムアウトms 0を指定した場合は実行結果を待たず終了,portMAX_DELAYで無限待機
 *
 * @return LockTaskResult_t 開施錠成否
 */
LockTaskResult_t prvLockTaskOp(LockTaskQueueData_t *pxData, uint32_t ulTimeout);

/**
 * @brief 開錠動作
 */
static void prvUnlock(void);

/**
 * @brief 施錠動作
 */
static void prvLock(void);

/**
 * @brief ロックの現在の状態を取得
 *
 * @note 現在角度から施錠、開錠位置に最も近い方を選択
 *
 * @retval DOOR_LOCKED   施錠中
 * @retval DOOR_UNLOCKED 開錠中
 */
LockState_t prvGetLockStateDoor();

/**
 * @brief オートロックタイマーが発火した際に呼ばれるコールバック関数。自動施錠を行う
 *
 * @param[in] xTimer タイマーのハンドル
 */
void prvOnAutoLockTimer(TimerHandle_t xTimer);

// --------------------------------------------------
// 変数定義（staticを除く）
// --------------------------------------------------

// --------------------------------------------------
// 関数定義（staticを除く）
// --------------------------------------------------
LockTaskResult_t eLockTaskInitialize(void)
{
    if (gxTaskData.xTaskHandle != NULL) // すでに初期化済み
    {
        APP_PRINTFDebug("Already initialize lock task.");
        return LOCK_TASK_RESULT_SUCCEED;
    }

    gxTaskData.eState = LOCK_TASK_STATE_INIT;
    gxTaskData.xQueueHandle = NULL;
    gxTaskData.xTaskHandle = NULL;

    gxInterfaceMotor.delay = vTaskDelay;

    // タスクの作成
    BaseType_t xResult = xTaskCreate(prvLockTask,
                                     "Lock Task",
                                     LOCK_TASK_SIZE,
                                     NULL,
                                     LOCK_TASK_PRIORITY,
                                     &gxTaskData.xTaskHandle);
    if (xResult != pdTRUE)
    {
        APP_PRINTFError("Lock task create failed.");
        return LOCK_TASK_RESULT_FAILED;
    }

    // オートロックタイマーを起動
    gxTaskData.xAutoLockTimerHandle = xTimerCreate(
        "AutoLockTimer",
        pdMS_TO_TICKS(AUTO_LOCK_TIME_MS),
        pdFALSE,
        NULL,
        &prvOnAutoLockTimer);

    if (gxTaskData.xAutoLockTimerHandle == NULL)
    {
        APP_PRINTFError("Auto lock timer create failed.");
        return LOCK_TASK_RESULT_FAILED;
    }

    return LOCK_TASK_RESULT_SUCCEED;
}

LockTaskResult_t eShutdownLockTask(void)
{
    if (gxTaskData.xQueueHandle != NULL)
    {
        vQueueDelete(gxTaskData.xQueueHandle);
        gxTaskData.xQueueHandle = NULL;
    }

    if (gxTaskData.xTaskHandle != NULL)
    {
        vTaskDelete(gxTaskData.xTaskHandle);
        gxTaskData.xTaskHandle = NULL;
    }

    return LOCK_TASK_RESULT_SUCCEED;
}

void vUnlockOpLockTask(const UnlockingOperatorType_t eUnlockOperator)
{
    // Lock Taskに解錠命令
    LockTaskQueueData_t xQueueData = {
        .eOp = LOCK_OP_UNLOCK,
        .eOperator = eUnlockOperator};
    prvLockTaskOp(&xQueueData, 0);
}

void vLockOpLockTask(const UnlockingOperatorType_t eUnlockOperator)
{
    // Lock Taskに施錠命令
    LockTaskQueueData_t xQueueData = {
        .eOp = LOCK_OP_LOCK,
        .eOperator = eUnlockOperator};
    prvLockTaskOp(&xQueueData, 0);
}

LockState_t eGetLockStateOpLockTask()
{
    LockState_t eState;
    LockTaskQueueData_t xQueueData = {
        .eOp = LOCK_OP_GET_LOCK_STATE,
        .u.get_lock_state.peState = &eState};

    // LockTaskに現在の解施錠状態を問い合わせ
    if (prvLockTaskOp(&xQueueData, portMAX_DELAY) != LOCK_TASK_RESULT_SUCCEED)
    {
        // データが遅れないが、遅れても応答が帰ってこなかったため、UNDEFINEDを返却する
        return LOCK_STATE_UNDEFINED;
    }

    return eState;
}

// --------------------------------------------------
// static関数定義
// --------------------------------------------------
static void prvLockTask(void *pvParameters)
{
    BaseType_t xResult;
    LockTaskQueueData_t xReceiveQueueData = {0};
    bool bShouldUpdateShadow = false;

    while (1)
    {
        switch (gxTaskData.eState)
        {
        case LOCK_TASK_STATE_INIT:
            APP_PRINTFDebug("Initialize Lock task...");

            // キュー作成
            gxTaskData.xQueueHandle = xQueueCreate(LOCK_TASK_QUEUE_LENGTH, sizeof(LockTaskQueueData_t));
            if (gxTaskData.xQueueHandle == NULL)
            {
                APP_PRINTFError("Failed to create lock queue.");
            }

            // PWM, ADCなどの初期化
            eInitializeMotor(&gxInterfaceMotor);

            // FIXME: ICAPの初めの方が値を取得できていないため
            vTaskDelay(300);

            // 施錠状態確認後、ずれていた場合は位置を調整
            if (prvGetLockStateDoor() == LOCK_STATE_LOCKED)
            {
                bRotateToAngle(ANGLE_TO_LOCK);
                vLEDOn(RED);
            }
            else
            {
                bRotateToAngle(ANGLE_TO_UNLOCK);
            }

            gxTaskData.eState = LOCK_TASK_STATE_TASK;
            break;
        case LOCK_TASK_STATE_TASK:
            bShouldUpdateShadow = false;
            xResult = xQueueReceive(gxTaskData.xQueueHandle, &xReceiveQueueData, 1000);
            if (xResult != pdPASS)
            {
                break;
            }

            if (xReceiveQueueData.eOp == LOCK_OP_UNLOCK)
            {
                // オートロックタイマーをスタートさせる
                if (xTimerStart(gxTaskData.xAutoLockTimerHandle, 0) != pdPASS)
                {
                    APP_PRINTFError("Auto lock timer start failed.");
                }

                // モーターを回す
                APP_PRINTFDebug("Unlock.");
                prvUnlock();

                // ShadowをUpdateする必要があるためTrueに設定
                bShouldUpdateShadow = true;
            }
            else if (xReceiveQueueData.eOp == LOCK_OP_LOCK)
            {
                // モーターを回す
                APP_PRINTFDebug("Lock.");
                prvLock();

                // ShadowをUpdateする必要があるためTrueに設定
                bShouldUpdateShadow = true;
            }
            else if (xReceiveQueueData.eOp == LOCK_OP_GET_LOCK_STATE)
            {
                // 解施錠状態の取得
                LockState_t eState = prvGetLockStateDoor();
                APP_PRINTFDebug("Now lock state: 0x%X", eState);
                *(xReceiveQueueData.u.get_lock_state.peState) = eState;
            }
            else
            {
                APP_PRINTFWarn("Unknown operation: 0x%X", xReceiveQueueData.eOp);
                break;
            }

            if (bShouldUpdateShadow == true) // 解施錠の状態変化があった場合
            {
                // 施錠状態の場合はLED点灯,解錠状態の場合はLED消灯
                LockState_t eState = prvGetLockStateDoor(); // 施錠状態確認
                if (eState == LOCK_STATE_LOCKED)
                {
                    vLEDOn(RED);
                }
                else
                {
                    vLEDOff(RED);
                }

                // クラウドに通知
                if (prvUpdateShadow(xReceiveQueueData.eOperator) != LOCK_TASK_RESULT_SUCCEED)
                {
                    APP_PRINTFError("Failed to update shadow.");
                }
            }

            // 完了通知
            if (xReceiveQueueData.xTaskHandle != NULL)
            {
                xTaskNotify(xReceiveQueueData.xTaskHandle, COMPLETE_LOCKED_UNLOCKED_EVENT, eSetBits);
            }

            PRINT_TASK_REMAINING_STACK_SIZE();
            break;
        default:
            APP_PRINTFError("Undefined status.");
            break;
        }
    }
}

LockTaskResult_t prvUpdateShadow(const UnlockingOperatorType_t eOperator)
{
    LockState_t eState = prvGetLockStateDoor(); // 施錠状態確認

    // Shadow Taskに対して解施錠状態を送信
    ShadowState_t xShadowState = {
        .xLockState = eState,
        .xUnlockingOperator = eOperator};
    if (eUpdateShadowStateAsync(SHADOW_UPDATE_TYPE_LOCK_STATE, &xShadowState) != DEVICE_SHADOW_RESULT_SUCCESS)
    {
        return LOCK_TASK_RESULT_FAILED;
    }
    return LOCK_TASK_RESULT_SUCCEED;
}

LockTaskResult_t prvLockTaskOp(LockTaskQueueData_t *pxData, uint32_t ulTimeout)
{
    // 引数チェック
    if (pxData == NULL)
    {
        return LOCK_TASK_RESULT_BAD_PARAMETER;
    }

    // キューが存在するか確認
    if (gxTaskData.xQueueHandle == NULL)
    {
        return LOCK_TASK_RESULT_FAILED;
    }

    // 実行中のタスク確認
    if (ulTimeout == 0) // タイムアウトを設定しない場合はnotifyをさせないためtaskHandleをNULLのままにする
    {
        pxData->xTaskHandle = NULL;
    }
    else
    {
        pxData->xTaskHandle = xTaskGetCurrentTaskHandle();
    }

    // 解施錠、または現在の状態取得指示送信
    if (xQueueSend(gxTaskData.xQueueHandle, pxData, 0) != pdPASS)
    {
        APP_PRINTFError("Failed to send queue.");
        return LOCK_TASK_RESULT_FAILED;
    }

    // 指示が完了するまで待機
    if (pxData->xTaskHandle != NULL)
    {
        uint32_t ulNotifiedValue = 0;
        xTaskNotifyWait(0xffffffff, 0xffffffff, &ulNotifiedValue, ulTimeout);

        // 指示の成否を判定
        if (ulNotifiedValue & COMPLETE_LOCKED_UNLOCKED_EVENT)
        {
            return LOCK_TASK_RESULT_SUCCEED;
        }
        else
        {
            return LOCK_TASK_RESULT_FAILED;
        }
    }
    return LOCK_TASK_RESULT_SUCCEED;
}

static void prvUnlock(void)
{
    bRotateToAngle(ANGLE_TO_UNLOCK);
}

static void prvLock(void)
{
    bRotateToAngle(ANGLE_TO_LOCK);
}

LockState_t prvGetLockStateDoor()
{
    uint16_t usAngle = usGetAngle(NULL); // 現在角度
    if (abs(ANGLE_TO_LOCK - usAngle) < abs(ANGLE_TO_UNLOCK - usAngle))
    {
        return LOCK_STATE_LOCKED;
    }
    return LOCK_STATE_UNLOCKED;
}

void prvOnAutoLockTimer(TimerHandle_t xTimer)
{
    // 解錠状態がUnlockの場合だけ施錠を行う
    LockState_t eState = prvGetLockStateDoor();
    if (eState == LOCK_STATE_UNLOCKED)
    {
        // 施錠を行う
        vLockOpLockTask(UNLOCKING_OPERATOR_TYPE_AUTO_LOCK);
    }
}

// --------------------------------------------------
// Unit Test用関数定義(関数のプロトタイプ宣言は「自身のファイル名+ "_test.h"」で宣言されていること)
// --------------------------------------------------
#if (BUILD_MODE_TEST == 1) /* BUILD_MODE_TESTが定義されているとき */
#endif                     /* end  BUILD_MODE_TEST */
