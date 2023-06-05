/**
 * @file mqtt_config.h
 * @author Systemzeus Inc.
 * @copyright Copyright © 2023 Systemzeus Inc. All rights reserved.
 */
#ifndef MQTT_CONFIG_H_
#define MQTT_CONFIG_H_

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
 * @brief MQTT 通信で使用するバッファサイズ
 *
 * @details
 * 2048で足りなくなったら変更する
 */
#define MQTT_BUFFER_SIZE (2048U)

/**
 * @brief MQTTのKeepAliveの間隔
 */
#define MQTT_KEEP_ALIVE_INTERVAL_SECONDS (60U)

/**
 * @brief MQTTConnect時のタイムアウト時間
 *
 * @details
 * AWS東京リージョンとのやり取りになるため、2秒もあれば十分と思われる
 */
#define MQTT_CONNECT_TIMEOUT_MS (2000U)

/**
 * @brief MQTTタスクにコマンドを送信するときのタイムアウト
 *
 * @warning
 * MQTTのPub/Subのタイムアウトではなく、MQTT Taskにコマンドをエンキューする際のタイムアウトである。
 */
#define MQTT_TASK_COMMAND_ENQUEUE_TIMEOUT_MS (2U * 1000U)

/**
 * @brief ソケット送信/受信時のタイムアウト設定
 */
#define SOCKET_SEND_RECV_TIMEOUT_MS (750)

/**
 * @brief 同時にSubscribeできる最大数
 */
#define MQTT_MAX_SUBSCRIBE_NUM (10U)

/**
 * @brief MQTTPub/Sub時のタイムアウト時間
 *
 * @details
 * AWS東京リージョンとのやり取りになるため、30秒もあれば十分と思われる
 */
#define MQTT_PUB_SUB_TIMEOUT_MS (30U * 1000U)

/**
 * @brief AWS IoTへ接続するためのポート番号
 */
#define AWS_IOT_MQTT_PORT (8883U)

#ifdef __cplusplus
}
#endif

#endif /* end MQTT_CONFIG_H_ */
