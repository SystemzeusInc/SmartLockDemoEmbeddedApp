/**
 * @file provisioning_task.h
 * @author Systemzeus Inc.
 * @copyright Copyright © 2023 Systemzeus Inc. All rights reserved.
 */
#ifndef PROVISIONING_TASK_H_
#define PROVISIONING_TASK_H_

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

#include "definitions.h"
#include "FreeRTOS.h"
#include "queue.h"

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

    // --------------------------------------------------
    // enumタグ定義（typedefを同時に行う）
    // --------------------------------------------------
    /**
     * @brief ProvisioningTaskの結果
     */
    typedef enum
    {
        PROVISIONING_TASK_RESULT_SUCCEED = 0x0,  /**< 成功 */
        PROVISIONING_TASK_RESULT_FAILED,         /**< 失敗 */
        PROVISIONING_TASK_RESULT_BAD_PARAMETER,  /**< 不正な引数 */
        PROVISIONING_TASK_RESULT_TIMEOUT,        /**< タイムアウト */
        PROVISIONING_TASK_RESULT_NOT_IMPLEMENTED /**< 未実装 */
    } ProvisioningTaskResult_t;

    /**
     * @brief ProvisioningTaskの状態
     */
    typedef enum
    {
        PROVISIONING_APP_STATE_INIT = 0x0, /**< 初期 */
        PROVISIONING_APP_STATE_TASK,       /**< タスク中 */
        PROVISIONING_APP_STATE_DEINIT      /**< 終了 */
    } ProvisioningTaskState_t;

    // --------------------------------------------------
    // struct/unionタグ定義（typedefを同時に行う）
    // --------------------------------------------------
    /**
     * @brief ProvisioningTaskのデータ
     */
    typedef struct
    {
        ProvisioningTaskState_t eState; /**< ProvisioningTask状態 */
        QueueHandle_t xQueue;           /**< Queueハンドル */
        TaskHandle_t xTaskHandle;       /**< Taskハンドル */
    } ProvisioningTaskData_t;

    // --------------------------------------------------
    // extern変数宣言
    // --------------------------------------------------

    // --------------------------------------------------
    // 関数プロトタイプ宣言
    // --------------------------------------------------
    /**
     * @brief プロビジョニングタスクの初期化
     *
     * @return ProvisioningTaskResult_t 初期化結果
     */
    ProvisioningTaskResult_t eProvisioningTaskInitialize(void);

    /**
     * @brief プロビジョニングタスク終了処理
     * 
     * @return ProvisioningTaskResult_t 終了処理結果
     */
    ProvisioningTaskResult_t eShutdownProvisioningTask(void);

    // --------------------------------------------------
    // インライン関数
    // --------------------------------------------------

#ifdef __cplusplus
}
#endif

#endif /* end PROVISIONING_TASK_H_ */
