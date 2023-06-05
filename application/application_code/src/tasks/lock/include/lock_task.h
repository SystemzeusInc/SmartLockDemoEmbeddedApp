/**
 * @file lock_task.h
 * @author Systemzeus Inc.
 * @copyright Copyright © 2023 Systemzeus Inc. All rights reserved.
 */
#ifndef LOCK_TASK_H_
#define LOCK_TASK_H_

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
#include "queue.h"
#include "timers.h"

    // --------------------------------------------------
    // ユーザ作成ヘッダの取り込み
    // --------------------------------------------------
#include "common/include/device_state.h"

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
     * @brief LockTaskの結果
     */
    typedef enum
    {
        LOCK_TASK_RESULT_SUCCEED = 0x0,  /**< 成功 */
        LOCK_TASK_RESULT_FAILED,         /**< 失敗 */
        LOCK_TASK_RESULT_BAD_PARAMETER,  /**< 不正な引数 */
        LOCK_TASK_RESULT_TIMEOUT,        /**< タイムアウト */
        LOCK_TASK_RESULT_NOT_IMPLEMENTED /**< 未実装 */
    } LockTaskResult_t;

    /**
     * @brief LockTaskの状態
     */
    typedef enum
    {
        LOCK_TASK_STATE_INIT = 0x0, /**< 初期 */
        LOCK_TASK_STATE_TASK        /**< タスク中 */
    } LockTaskState_t;

    /**
     * @brief LockTaskに対する命令
     */
    typedef enum
    {
        LOCK_OP_UNLOCK = 0x0,  /**< 開錠 */
        LOCK_OP_LOCK,          /**< 施錠 */
        LOCK_OP_GET_LOCK_STATE /**< 開施錠状態取得 */
    } LockTaskOp_t;

    /**
     * @brief 開施錠完了イベントビット
     */
    typedef enum
    {
        COMPLETE_LOCKED_UNLOCKED_EVENT = 0x1 << 0, /**< 開施錠完了 */
    } CompleteLockEvent_t;

    // --------------------------------------------------
    // struct/unionタグ定義（typedefを同時に行う）
    // --------------------------------------------------
    /**
     * @brief LockTaskのデータ
     */
    typedef struct
    {
        LockTaskState_t eState;     /**< LockTask状態 */
        QueueHandle_t xQueueHandle; /**< Queueハンドル */
        TaskHandle_t xTaskHandle;   /**< Taskハンドル */
        TimerHandle_t xAutoLockTimerHandle; /**< 自動施錠を行うためのタイマーハンドル */
    } LockTaskData_t;

    /**
     * @brief  LockTaskキューデータ構造
     */
    typedef struct
    {
        TaskHandle_t xTaskHandle;          /**< 開施錠が完了したことを通知するTaskハンドル */
        LockTaskOp_t eOp;                  /**< 開施錠指示 */
        UnlockingOperatorType_t eOperator; /**< 操作主体*/
        union
        {
            struct
            {
                LockState_t *peState; /**< 開施錠状態を格納するポインタ */
            } get_lock_state;
        } u;
    } LockTaskQueueData_t;

    // --------------------------------------------------
    // extern変数宣言
    // --------------------------------------------------

    // --------------------------------------------------
    // 関数プロトタイプ宣言
    // --------------------------------------------------
    /**
     * @brief LockTaskの初期化、生成
     *
     * @return LockTaskResult_t 結果
     */
    LockTaskResult_t eLockTaskInitialize(void);

    /**
     * @brief LockTaskの終了処理
     *
     * @return LockTaskResult_t 結果
     */
    LockTaskResult_t eShutdownLockTask(void);

    /**
     * @brief 解錠指示をLockTaskに送信する
     *
     * @param[in] eUnlockOperator 操作主体。例えば、解施錠する主体がアプリの場合は #UNLOCKING_OPERATOR_TYPE_APP を選択する。
     *
     */
    void vUnlockOpLockTask(const UnlockingOperatorType_t eUnlockOperator);

    /**
     * @brief 施錠指示をLockTaskに送信する
     *
     * @param[in] eUnlockOperator 操作主体。例えば、解施錠する主体がアプリの場合は #UNLOCKING_OPERATOR_TYPE_APP を選択する。
     *
     */
    void vLockOpLockTask(const UnlockingOperatorType_t eUnlockOperator);

    /**
     * @brief 現在の開施錠状態を取得
     *
     * @retval LOCK_STATE_UNLOCKED 開錠中
     * @retval LOCK_STATE_LOCKED 施錠中
     */
    LockState_t eGetLockStateOpLockTask();

    // --------------------------------------------------
    // インライン関数
    // --------------------------------------------------

#ifdef __cplusplus
}
#endif

#endif /* end LOCK_TASK_H_ */
