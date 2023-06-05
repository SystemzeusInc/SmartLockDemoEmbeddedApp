/**
 * @file device_shadow_config.h
 * @author Systemzeus Inc.
 * @copyright Copyright © 2023 Systemzeus Inc. All rights reserved.
 */
#ifndef DEVICE_SHADOW_CONFIG_H_
#define DEVICE_SHADOW_CONFIG_H_

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
/**
 * @brief Shadowタスクをシャットダウンするときに待機する最大時間
 */
#define SHADOW_TASK_SHUTDOWN_WAIT_MS (MQTT_PUB_SUB_TIMEOUT_MS + 1000U)

/**
 * @brief /get/acceptedトピックを受信するためのバッファサイズ
 */
#define GET_DEVICE_SHADOW_PAYLOAD_BUFFER_SIZE 1024

/**
 * @brief /update/acceptedトピックを受信するためのバッファサイズ
 */
#define UPDATE_DEVICE_SHADOW_PAYLOAD_BUFFER_SIZE 1024

/**
 * @brief MQTT通信のタイムアウト時間
 *
 * mqtt_config.hに定義されているPub/Subタイムアウト時間（MQTT_PUB_SUB_TIMEOUT_MS）より少し多い時間にする
 */
#define SHADOW_MQTT_TIMEOUT_MS (MQTT_PUB_SUB_TIMEOUT_MS + 1000U)

#ifdef __cplusplus
}
#endif

#endif /* end DEVICE_SHADOW_CONFIG_H_ */
