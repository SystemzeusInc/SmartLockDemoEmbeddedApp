/**
 * @file shadow_state_change_callback.h
 * @author Systemzeus Inc.
 * @copyright Copyright © 2023 Systemzeus Inc. All rights reserved.
 */
#ifndef SHADOW_STATE_CHANGE_CALLBACK_H_
#define SHADOW_STATE_CHANGE_CALLBACK_H_

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

#include "tasks/shadow/include/device_shadow_task.h"

    // --------------------------------------------------
    // 関数プロトタイプ宣言
    // --------------------------------------------------

    /**
     * @brief Shadowのステータスが変更された際のコールバック関数を取得する
     *
     * @param[out] pxCallback コールバック関数を格納するバッファ
     */
    void vGetShadowStateChangeCallback(ShadowChangeCallback_t *pxCallback);

    // --------------------------------------------------
    // インライン関数
    // --------------------------------------------------

#ifdef __cplusplus
}
#endif

#endif /* end SHADOW_STATE_CHANGE_CALLBACK_H_ */
