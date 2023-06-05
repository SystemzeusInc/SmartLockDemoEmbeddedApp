/**
 * @file wakeup_task.c
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
#include "ota_appversion32.h"

// --------------------------------------------------
// ユーザ作成ヘッダの取り込み
// --------------------------------------------------
#include "config/task_config.h"
#include "config/log_config.h"

#include "common/include/application_define.h"
#include "common/include/network_operation.h"

#include "tasks/lock/include/lock_task.h"
#include "tasks/ble/include/ble_task.h"
#include "tasks/device_mode_switch/include/device_mode_switch_task.h"
#include "tasks/wifi_info_cahnge/include/wifi_info_change_task.h"
#include "tasks/wakeup/include/wakeup_task.h"
#include "tasks/mqtt/include/mqtt_operation_task.h"
#include "tasks/flash/include/flash_data.h"
#include "tasks/flash/include/flash_task.h"
#include "tasks/device_mode_switch/include/device_mode_switch_event.h"
#include "tasks/provisioning/include/provisioning.h"
#include "tasks/provisioning/include/provisioning_task.h"

// --------------------------------------------------
// 自ファイル内でのみ使用する#defineマクロ
// --------------------------------------------------

// --------------------------------------------------
// 自ファイル内でのみ使用する#define関数マクロ
// --------------------------------------------------

// --------------------------------------------------
// 自ファイル内でのみ使用するtypedef定義
// --------------------------------------------------

// --------------------------------------------------
// 自ファイル内でのみ使用するenumタグ定義（typedefを同時に行う）
// --------------------------------------------------

// --------------------------------------------------
// 自ファイル内でのみ使用するstruct/unionタグ定義（typedefを同時に行う）
// --------------------------------------------------

// --------------------------------------------------
// ファイル内で共有するstatic変数宣言
// --------------------------------------------------
//! 自身のタスク
static TaskHandle_t gxTask = NULL;

// --------------------------------------------------
// static関数プロトタイプ宣言
// --------------------------------------------------

/**
 * @brief WakeUpTaskのエントリーポイント
 *
 * @details
 * 共通部の電源ONイベントを処理する。電源ON時に必要なタスクやテーブルを初期化して、DeviceModeSwitchTaskに起動通知を行う。
 *
 * @param[in] pvParameters Task作成関数を使用するために定義した引数
 */
static void vprvWakeUpTask(void *pvParameters);

/**
 * @brief BLEに書き込みイベントがあった場合に呼ばれるコールバック関数
 *
 * @param [in] pvValue コールバック引数
 */
static void vprvCbReceiveBLE(void *pvValue);

#if (CREATE_PRINT_REMAINING_HEAP_SIZE_TASK == 1)
/**
 * @brief 残りのヒープサイズをプリントする（デバック用）
 *
 * @param pvParameters 使用しない
 */
static void vprvPrintRemainingHeapSize(void *pvParameters);
#endif

// --------------------------------------------------
// 変数定義（staticを除く）
// --------------------------------------------------

// --------------------------------------------------
// 関数定義（staticを除く）
// --------------------------------------------------
extern bool bInitializeNewECC608Interface(void);

WakeUpTaskResult_t eWakeupTaskInit(void)
{
    // WakeUpTaskの作成
    BaseType_t xResult = xTaskCreate(
        &vprvWakeUpTask,
        "WakeupTask",
        WAKE_UP_TASK_STACK_SIZE,
        NULL,
        WAKE_UP_TASK_PRIORITY,
        &gxTask);

    if (xResult != pdTRUE)
    {
        APP_PRINTFFatal("Failed to initialize Wake Up Task. [Task Create Failed]");
        return WAKE_UP_TASK_RESULT_FAILED;
    }

    return WAKE_UP_TASK_RESULT_SUCCESS;
}

// --------------------------------------------------
// static関数定義
// --------------------------------------------------

static void vprvWakeUpTask(void *pvParameters)
{
    (void)pvParameters;

    while (true)
    {

        // アプリのバージョン表示
        APP_PRINTFDebug("APP VERSION :%d.%d.%d", appFirmwareVersion.u.x.major, appFirmwareVersion.u.x.minor, appFirmwareVersion.u.x.build);

#if (CREATE_PRINT_REMAINING_HEAP_SIZE_TASK == 1)
        // 現在のヒープサイズを出力するタスクの作成
        if (xTaskCreate(&vprvPrintRemainingHeapSize, "HeapSizePrintTask", 256, NULL, 0, NULL) != pdPASS)
        {
            APP_PRINTFError("Failed to create heap size print task");
        }
#endif

        // Flashタスクの初期化
        if (eFlashTaskInit() != FLASH_TASK_RESULT_SUCCESS)
        {
            APP_PRINTFFatal("Flash Task Init failed.");
            break;
        }

        // IPタスクなどネットワーク通信を行うために必要なタスクを初期化
        // NOTE: この関数内ではWi-Fiルータへの接続は行わない
        NetworkOperationResult_t eNetworkResult = eNetworkInit();
        if (eNetworkResult != NETWORK_OPERATION_RESULT_SUCCESS)
        {
            APP_PRINTFFatal("Net work init failed. Reason: %d", eNetworkResult);
            break;
        }

        // MQTTの初期化
        if (eMQTTCommunicationInit() != MQTT_OPERATION_TASK_RESULT_SUCCESS)
        {
            APP_PRINTFFatal("MQTT Communication Init failed.");
            break;
        }

        // Device Mode Switchタスクの初期化
        DeviceModeSwitchTaskResult_t eSwitchResult = eDeviceModeSwitchTaskInit();
        if (eSwitchResult != DEVICE_MODE_SWITCH_TASK_RESULT_SUCCESS)
        {
            APP_PRINTFFatal("Device mode switch task init failed. Reason: %d", eSwitchResult);
            break;
        }
        
        // BLEタスクの初期化と、プロビジョニングモード変更リクエスト用のコールバック登録
        BLETaskResult_t eBLEResult = eBLETaskInitialize();
        if (eBLEResult != BLE_TASK_RESULT_SUCCEED)
        {
            APP_PRINTFFatal("BLE task init failed. Reason: %d", eBLEResult);
            break;
        }

        // モード変更リクエストに対する応答用のコールバック関数を登録
        eBLEResult = eRegisterBLEEventCb(vprvCbReceiveBLE, BLE_EVENT_CB_TYPE_WV, (uint8_t *)CHARACTERISTIC_UUID_PROVISIONING);
        if (eBLEResult != BLE_TASK_RESULT_SUCCEED)
        {
            APP_PRINTFFatal("Register BLE event callback failed. Reason: %d", eBLEResult);
            break;
        }

        // Lockタスクの初期化
        if (eLockTaskInitialize() != LOCK_TASK_RESULT_SUCCEED)
        {
            APP_PRINTFFatal("Failed to initialize LockTask.");
            break;
        }

        // 最後にDeviceModeSwitchタスクに対して通常モードになるように通知
        DeviceModeSwitchData_t xQueueData = {
            .eDeviceModeSwitchEvent = DEVICE_MODE_SWITCH_MAIN_WAKE_UP};
        eSwitchResult = eDeviceModeSwitch(&xQueueData);
        if (eSwitchResult != DEVICE_MODE_SWITCH_TASK_RESULT_SUCCESS)
        {
            APP_PRINTFFatal("Device mode switch notify failed. Reason: %d", eSwitchResult);
            break;
        }

        PRINT_TASK_REMAINING_STACK_SIZE();

        // 初期化処理が終わったので、自分自身のタスクを削除する
        APP_PRINTFDebug("The WakeupTask has completed its operation. Therefore, this task is deleted.");
        vTaskDelete(NULL);
    }

    // 以下異常処理

    APP_PRINTFFatal("########################");
    APP_PRINTFFatal("Wake up task failed.");
    APP_PRINTFFatal("########################");

    vTaskDelete(NULL);
}

static void vprvCbReceiveBLE(void *pvValue)
{

    BLEEventWVValue_t *pxValue = (BLEEventWVValue_t *)pvValue;
    APP_PRINTFDebug("CbReceiveBLE incoming data: %*.s, size: %d", pxValue->xDataSize, pxValue->uxData, pxValue->xDataSize);

    // プロビジョニングモード変更要求かチェック
    if (pxValue->xDataSize == sizeof(PROVISIONING_MODE_REQ_STRING) - 1 &&
        memcmp(pxValue->uxData, PROVISIONING_MODE_REQ_STRING, pxValue->xDataSize) == 0)
    {
        // DeviceModeSwitchタスクにプロビジョニングモード変更要求イベントを知らせる
        DeviceModeSwitchData_t xQueueData = {
            .eDeviceModeSwitchEvent = DEVICE_MODE_SWITCH_PROVISIONING};
        DeviceModeSwitchTaskResult_t xResult = eDeviceModeSwitch(&xQueueData);
        if (xResult != DEVICE_MODE_SWITCH_TASK_RESULT_SUCCESS)
        {
            APP_PRINTFError("Failed to send provisioning mode request to DeviceModeSwitchTask; eDeviceModeSwitch returned %d", xResult);
            return;
        }
        APP_PRINTFInfo("Succeeded to send provisioning mode request to DeviceModeSwitchTask.");
    }

    // Wi-Fi接続先変更モード要求かチェック
    if (pxValue->xDataSize == sizeof(WIFI_INFO_CHANGE_MODE_REQ_STRING) - 1 &&
        memcmp(pxValue->uxData, WIFI_INFO_CHANGE_MODE_REQ_STRING, pxValue->xDataSize) == 0)
    {
        // DeviceModeSwitchタスクにWi-Fi接続先変更モード変更要求イベントを知らせる
        DeviceModeSwitchData_t xQueueData = {
            .eDeviceModeSwitchEvent = DEVICE_MODE_SWITCH_WIFI_INFO_SWITCH};
        DeviceModeSwitchTaskResult_t xResult = eDeviceModeSwitch(&xQueueData);
        if (xResult != DEVICE_MODE_SWITCH_TASK_RESULT_SUCCESS)
        {
            APP_PRINTFError("Failed to send Wi-Fi info change mode change request to DeviceModeSwitchTask; eDeviceModeSwitch returned %d", xResult);
            return;
        }
        APP_PRINTFInfo("Succeeded to send Wi-Fi info change mode request to DeviceModeSwitchTask.");
    }
}

#if (CREATE_PRINT_REMAINING_HEAP_SIZE_TASK == 1)
static void vprvPrintRemainingHeapSize(void *pvParameters)
{
    while (1)
    {
        APP_PRINTF("Remaining heap size is %d Byte", xPortGetFreeHeapSize());
        vTaskDelay(pdMS_TO_TICKS(HEAP_SIZE_DISPLAY_INTERVAL_MS));
    }
}
#endif

// --------------------------------------------------
// Unit Test用関数定義(関数のプロトタイプ宣言は「自身のファイル名+ "_test.h"」で宣言されていること)
// --------------------------------------------------
#if (BUILD_MODE_TEST == 1) /* BUILD_MODE_TESTが定義されているとき */
#endif                     /* end  BUILD_MODE_TEST */
