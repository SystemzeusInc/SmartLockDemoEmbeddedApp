/**
 * @file queue_config.h
 * @author Systemzeus Inc.
 * @copyright Copyright © 2023 Systemzeus Inc. All rights reserved.
 */
#ifndef QUEUE_CONFIG_H /* QUEUE_CONFIG_Hが定義されていないとき */
#define QUEUE_CONFIG_H

#ifdef __cplusplus // Provide Cplusplus Compatibility

extern "C"
{
#endif /* end Provide Cplusplus Compatibility */

//! DeviceModeSwitchキューのサイズ
#define DEVICE_MODE_SWITCH_QUEUE_LENGTH (5U)

//! MQTT Communication Task内で使用するMQTT Agentが受け取るコマンドの最大数
#define MQTT_AGENT_COMMAND_QUEUE_LENGTH (5U)

//! Flashタスクが同時に受付できるコマンドの最大数
#define FLASH_TASK_COMMAND_QUEUE_LENGTH (5U)

//! ShadowのUpdateやGetができる最大のコマンド数
#define SHADOW_TASK_QUEUE_LENGTH (10U)

//! Wi-Fi接続先変更タスクが受け付ける最大コマンド数
#define WIFI_INFO_CHANGE_TASK_QUEUE_LENGTH (1U)

//! BLETaskのキューサイズ
#define BLE_TASK_QUEUE_LENGTH (3U)

//! ProvisioningTaskのキューサイズ
#define PROVISIONING_TASK_QUEUE_LENGTH (1U)

//! LockTaskのキューサイズ
#define LOCK_TASK_QUEUE_LENGTH (5U)

#ifdef __cplusplus
}
#endif

#endif /* end QUEUE_CONFIG_H */
