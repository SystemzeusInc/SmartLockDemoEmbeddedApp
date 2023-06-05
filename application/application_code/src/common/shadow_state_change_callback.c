/**
 * @file shadow_state_change_callback.c
 * @author Systemzeus Inc.
 * @copyright Copyright © 2023 Systemzeus Inc. All rights reserved.
 */

// --------------------------------------------------
// システムヘッダの取り込み
// --------------------------------------------------
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "FreeRTOS.h"

// --------------------------------------------------
// ユーザ作成ヘッダの取り込み
// --------------------------------------------------
#include "config/task_config.h"

#include "common/include/application_define.h"
#include "common/include/device_state.h"
#include "common/include/shadow_state_change_callback.h"

#include "tasks/lock/include/lock_task.h"

#include "tasks/shadow/include/device_shadow_task.h"

// --------------------------------------------------
// static関数プロトタイプ宣言
// --------------------------------------------------
/**
 * @brief Shadowのステータスが変更された際のコールバック関数
 *
 * @details
 * ShadowのLockStateが変更された場合は、LockTaskに通知を出す
 *
 * @param xUpdateShadowType
 * @param pxShadowSate
 */
static void bprvShadowChangeCallback(const uint32_t xUpdateShadowType, const ShadowState_t *pxShadowSate);

// --------------------------------------------------
// 関数定義（staticを除く）
// --------------------------------------------------
void vGetShadowStateChangeCallback(ShadowChangeCallback_t *pxCallback)
{
    if (pxCallback == NULL)
    {
        APP_PRINTFFatal("Callback buffer is NULL");
    }

    // Callback関数を登録
    *pxCallback = &bprvShadowChangeCallback;
}

// --------------------------------------------------
// static関数定義
// --------------------------------------------------
static void bprvShadowChangeCallback(const uint32_t xUpdateShadowType, const ShadowState_t *pxShadowSate)
{
    APP_PRINTFDebug("bprvShadowChangeCallback Incoming.");

    APP_PRINTFDebug("xUpdateShadowType: %u", xUpdateShadowType);

    // 施錠状態が変更された場合は、LockTaskに通知
    if ((xUpdateShadowType & SHADOW_UPDATE_TYPE_LOCK_STATE) == SHADOW_UPDATE_TYPE_LOCK_STATE)
    {
        if (pxShadowSate->xLockState == LOCK_STATE_LOCKED)
        {
            vLockOpLockTask(UNLOCKING_OPERATOR_TYPE_APP);
        }
        else if (pxShadowSate->xLockState == LOCK_STATE_UNLOCKED)
        {
            vUnlockOpLockTask(UNLOCKING_OPERATOR_TYPE_APP);
        }
    }
}

// --------------------------------------------------
// Unit Test用関数定義(関数のプロトタイプ宣言は「自身のファイル名+ "_test.h"」で宣言されていること)
// --------------------------------------------------
#if (BUILD_MODE_TEST == 1) /* BUILD_MODE_TESTが定義されているとき */
#endif                     /* end  BUILD_MODE_TEST */
