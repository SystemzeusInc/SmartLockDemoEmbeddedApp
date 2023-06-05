/**
 * @file task_config.h
 * @author Systemzeus Inc.
 * @copyright Copyright © 2023 Systemzeus Inc. All rights reserved.
 */
#ifndef TASK_CONFIG_H_
#define TASK_CONFIG_H_

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

/**
 * @brief WakeUpTaskのスタックサイズ
 */
#define WAKE_UP_TASK_STACK_SIZE (configMINIMAL_STACK_SIZE * 1)

/**
 * @brief WakeUpTaskの優先度
 */
#define WAKE_UP_TASK_PRIORITY (tskIDLE_PRIORITY)


/**
 * @brief DeviceModeSwitchTaskのスタックサイズ
 */
#define DEVICE_MODE_SWITCH_TASK_STACK_SIZE (configMINIMAL_STACK_SIZE * 2)

/**
 * @brief DeviceModeSwitchTaskの優先度
 */
#define DEVICE_MODE_SWITCH_TASK_PRIORITY (tskIDLE_PRIORITY + 3)

/**
 * @brief MQTTTaskのスタックサイズ
 *
 */
#define MQTT_TASK_STACK_SIZE (configMINIMAL_STACK_SIZE * 1)

/**
 * @brief MQTTTaskの優先度
 *
 */
#define MQTT_TASK_PRIORITY (tskIDLE_PRIORITY + 1)

/**
 * @brief FlashTaskのタスクサイズ
 *
 */
#define FLASH_TASK_SIZE (configMINIMAL_STACK_SIZE * 1)

/**
 * @brief FlashTaskの優先度
 *
 */
#define FLASH_TASK_PRIORITY (tskIDLE_PRIORITY + 1)

/**
 * @brief ShadowTaskのスタックサイズ
 *
 */
#define SHADOW_TASK_SIZE (configMINIMAL_STACK_SIZE * 2)

/**
 * @brief ShadowTaskの優先度
 *
 */
#define SHADOW_TASK_PRIORITY (tskIDLE_PRIORITY + 1)

/**
 * @brief Wi-Fi接続先変更タスクスのスタックサイズ
 *
 */
#define WIFI_INFO_CHANGE_TASK_SIZE (configMINIMAL_STACK_SIZE * 1)

/**
 * @brief Wi-Fi接続先変更タスクの優先度
 *
 */
#define WIFI_INFO_CHANGE_TASK_PRIORITY (tskIDLE_PRIORITY + 1)

/**
 * @brief BLETaskのスタックサイズ
 */
#define BLE_TASK_SIZE (configMINIMAL_STACK_SIZE * 2)

/**
 * @brief BLETaskの優先度
 */
#define BLE_TASK_PRIORITY (tskIDLE_PRIORITY + 1)

/**
 * @brief ProvisioningTaskのスタックサイズ
 */
#define PROVISIONING_TASK_SIZE (configMINIMAL_STACK_SIZE * 3)

/**
 * @brief ProvisioningTaskの優先度
 */
#define PROVISIONING_TASK_PRIORITY (tskIDLE_PRIORITY + 1)

/**
 * @brief LockTaskのスタックサイズ
 *
 */
#define LOCK_TASK_SIZE (configMINIMAL_STACK_SIZE * 1)

/**
 * @brief LockTaskの優先度
 */
#define LOCK_TASK_PRIORITY (tskIDLE_PRIORITY + 1)

/**
 * @brief OTAAgentTaskのスタックサイズ
 *
 */
#define OTA_AGENT_STACK_SIZE (configMINIMAL_STACK_SIZE * 3)

/**
 * @brief OTAAgentTaskの優先度
 */
#define OTA_AGENT_TASK_PRIORITY (tskIDLE_PRIORITY + 1)

#ifdef __cplusplus
}
#endif

#endif /* end TASK_CONFIG_H_ */
