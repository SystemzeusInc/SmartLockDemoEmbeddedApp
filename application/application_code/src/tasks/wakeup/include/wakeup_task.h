/**
 * @file wakeup_task.h
 * @author Systemzeus Inc.
 * @copyright Copyright © 2023 Systemzeus Inc. All rights reserved.
 */
#ifndef WAKEUP_TASK_H_
#define WAKEUP_TASK_H_

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
        WAKE_UP_TASK_RESULT_SUCCESS = 0,
        /**
         * @brief 失敗したとき
         */
        WAKE_UP_TASK_RESULT_FAILED = 1
    } WakeUpTaskResult_t;

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
     * @brief WakeUpTaskの初期化
     *
     * @retval #WAKE_UP_TASK_RESULT_SUCCESS 成功
     * @retval #WAKE_UP_TASK_RESULT_FAILED  失敗
     */
    WakeUpTaskResult_t eWakeupTaskInit(void);

    // --------------------------------------------------
    // インライン関数
    // --------------------------------------------------

#ifdef __cplusplus
}
#endif

#endif /* end WAKEUP_TASK_H_ */
