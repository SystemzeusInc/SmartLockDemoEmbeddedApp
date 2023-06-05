/**
 * @file ota_agent_task.h
 * @author Systemzeus Inc.
 * @copyright Copyright © 2023 Systemzeus Inc. All rights reserved.
 */
#ifndef OTA_AGENT_TASK_H_
#define OTA_AGENT_TASK_H_

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
     * @brief 本ライブラリの実行結果
     */
    typedef enum
    {
        /**
         * @brief 成功
         */
        OTA_AGENT_TASK_RESULT_SUCCESS = 0,

        /**
         * @brief 失敗
         */
        OTA_AGENT_TASK_RESULT_FAILED = 1,
    } OTAAgentTaskResult_t;

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
     * @brief OTAAgentTaskを初期化・起動する
     *
     * @retval OTA_AGENT_TASK_RESULT_SUCCESS 成功
     * @retval OTA_AGENT_TASK_RESULT_FAILED  失敗
     */
    OTAAgentTaskResult_t eOTAAgentTaskInit(void);

    /**
     * @brief OTAAgentTaskを停止する
     *
     * @retval OTA_AGENT_TASK_RESULT_SUCCESS 成功
     * @retval OTA_AGENT_TASK_RESULT_FAILED  失敗
     */
    OTAAgentTaskResult_t eOTAAgentTaskShutdown(void);

    // --------------------------------------------------
    // インライン関数
    // --------------------------------------------------

#ifdef __cplusplus
}
#endif

#endif /* end OTA_AGENT_TASK_H_ */
