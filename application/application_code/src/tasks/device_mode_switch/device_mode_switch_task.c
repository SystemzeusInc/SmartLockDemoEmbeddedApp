/**
 * @file device_mode_switch_task.c
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
#include "queue.h"

// --------------------------------------------------
// ユーザ作成ヘッダの取り込み
// --------------------------------------------------
#include "config/task_config.h"
#include "config/queue_config.h"
#include "config/debug_config.h"
#include "config/device_mode_switch_config.h"

#include "common/include/application_define.h"
#include "common/include/network_operation.h"
#include "common/include/shadow_state_change_callback.h"

#include "tasks/provisioning/include/provisioning_task.h"
#include "tasks/wifi_info_cahnge/include/wifi_info_change_task.h"
#include "tasks/ota/include/ota_agent_task.h"
#include "tasks/mqtt/include/mqtt_operation_task.h"
#include "tasks/flash/include/flash_data.h"
#include "tasks/flash/include/flash_task.h"
#include "tasks/shadow/include/device_shadow_task.h"
#include "tasks/device_mode_switch/include/device_mode_switch_event.h"
#include "tasks/device_mode_switch/include/device_mode_switch_task.h"
#include "tasks/lock/include/lock_task.h"
#include "tasks/ble/include/ble_task.h"

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

//! 受信キューのハンドル
static QueueHandle_t gxDeviceModeSwitchQueueHandle = NULL;

//! 自身のタスク
static TaskHandle_t gxTaskHandle = NULL;

// --------------------------------------------------
// static関数プロトタイプ宣言
// --------------------------------------------------

/**
 * @brief DeviceModeSwitchTaskのエントリーポイント
 *
 * @param[in] pvParameters 使用しない。Task作成関数を使用するために定義した引数。
 */
static void vprvDeviceModeSwitchTask(void *pvParameters);

/**
 * @brief 通常モードへ移行する
 *
 * @param[in] pxEvent 移行通知を受け取った際のイベント
 *
 * @retval true  成功
 * @retval false 失敗
 */
static bool bprvSwitchToMainMode(const DeviceModeSwitchData_t *pxEvent);

/**
 * @brief プロビジョニングモードへ移行する
 *
 * @param[in] pxEvent 移行通知を受け取った際のイベント

 * @retval true  成功
 * @retval false 失敗
 */
static bool bprvSwitchToProvisioningMode(const DeviceModeSwitchData_t *pxEvent);

/**
 * @brief Wi-Fi接続先変更モードへ移行する
 *
 * @param[in] pxEvent 移行通知を受け取った際のイベント
 *
 * @retval true  成功
 * @retval false 失敗
 */
static bool bprvSwitchToWiFiInfoChange(const DeviceModeSwitchData_t *pxEvent);

/**
 * @brief ネットワークを閉じ、さらにモード移行が成功したことをBLE経由で通信相手（スマホ）に伝える
 *
 * @retval true  成功
 * @retval false 失敗
 */
static bool bprvNetworkDisconnectAndSendModeSwitchDoneToBLE(void);

/**
 * @brief ネットワークリトライを続けるか確認する
 *
 * @details
 * DeviceModeSwitchQueueに状態移行命令がス1つ以上溜まっているか確認し
 * 溜まっている場合はネットワークリトライを中断するためfalseを返却する
 *
 * @param[in] uxRetryCount リトライ回数。本関数では使用しない。
 *
 * @retval true  ネットワークリトライを継続する
 * @retval false ネットワークリトライを中断する
 */
static bool bCheckNetworkRetryAcceptable(const uint32_t uxRetryCount);

// --------------------------------------------------
// 変数定義（staticを除く）
// --------------------------------------------------

// --------------------------------------------------
// 関数定義（staticを除く）
// --------------------------------------------------

DeviceModeSwitchTaskResult_t eDeviceModeSwitchTaskInit(void)
{
    APP_PRINTFDebug("Device mode switch task init start");

    // Queueの作成
    if (gxDeviceModeSwitchQueueHandle == NULL)
    {
        gxDeviceModeSwitchQueueHandle = xQueueCreate(DEVICE_MODE_SWITCH_QUEUE_LENGTH, sizeof(DeviceModeSwitchData_t));

        if (gxDeviceModeSwitchQueueHandle == NULL)
        {
            APP_PRINTFFatal("Failed to initialize Device Mode Switch Task. [Queue Create Failed]");
            return DEVICE_MODE_SWITCH_TASK_RESULT_FAILED;
        }
    }

    // タスク作成
    if (gxTaskHandle == NULL)
    {
        BaseType_t xResult = xTaskCreate(vprvDeviceModeSwitchTask,
                                         "DeviceModeSwitchTask",
                                         DEVICE_MODE_SWITCH_TASK_STACK_SIZE,
                                         NULL,
                                         DEVICE_MODE_SWITCH_TASK_PRIORITY,
                                         &gxTaskHandle);
        if (xResult != pdTRUE)
        {
            APP_PRINTFFatal("Failed to initialize Device Mode Switch Task. [Task Create Failed]");
            return DEVICE_MODE_SWITCH_TASK_RESULT_FAILED;
        }
    }

    return DEVICE_MODE_SWITCH_TASK_RESULT_SUCCESS;
}

DeviceModeSwitchTaskResult_t eDeviceModeSwitch(const DeviceModeSwitchData_t *pxDeviceModeSwitchData)
{

    // DeviceModeSwitchQueueに渡すデータを設定
    DeviceModeSwitchData_t xQueueData;
    memset(&xQueueData, 0, sizeof(xQueueData));
    memcpy(&xQueueData, pxDeviceModeSwitchData, sizeof(xQueueData));

    // DeviceModeSwitchQueueにデータを入れる
    BaseType_t xResult = xQueueSend(gxDeviceModeSwitchQueueHandle, &xQueueData, 0);

    return xResult == pdTRUE ? DEVICE_MODE_SWITCH_TASK_RESULT_SUCCESS : DEVICE_MODE_SWITCH_TASK_RESULT_FAILED;
}

// --------------------------------------------------
// static関数定義
// --------------------------------------------------

static void vprvDeviceModeSwitchTask(void *pvParameters)
{
    // この引数は使用しない
    (void)pvParameters;

    // 通知の受け取りバッファを定義
    DeviceModeSwitchData_t xQueueData;

    while (true)
    {
        // メモリの初期化
        memset(&xQueueData, 0x00, sizeof(xQueueData));

        // DeviceModeSwitchQueueからデータ受け取り
        if (xQueueReceive(gxDeviceModeSwitchQueueHandle, &xQueueData, portMAX_DELAY))
        {
            APP_PRINTFDebug("Received mode switch notification.");

            switch (xQueueData.eDeviceModeSwitchEvent)
            {
            case DEVICE_MODE_SWITCH_MAIN_WAKE_UP:      // Wake up task終了
            case DEVICE_MODE_SWITCH_PROVISIONING_DONE: // task終了
            case DEVICE_MODE_SWITCH_WIFI_INFO_DONE:    // WifiInfoChange終了
            case DEVICE_MODE_SWITCH_WIFI_INFO_TIMEOUT: // WifiInfoChangeタイムアウト
                PRINT_PROCESS_MEASUREMENT_POINT_START("Main mode switch");
                if (bprvSwitchToMainMode(&xQueueData) == false)
                {
                    APP_PRINTFError("Main mode switch failed.");
                }
                PRINT_PROCESS_MEASUREMENT_POINT_END("Main mode switch");
                break;
            case DEVICE_MODE_SWITCH_PROVISIONING: // Main -> Provisioning
                PRINT_PROCESS_MEASUREMENT_POINT_START("Provisioning mode switch");
                if (bprvSwitchToProvisioningMode(&xQueueData) == false)
                {
                    APP_PRINTFError("Provisioning mode switch failed.");
                }
                PRINT_PROCESS_MEASUREMENT_POINT_END("Provisioning mode switch");
                break;
            case DEVICE_MODE_SWITCH_WIFI_INFO_SWITCH: // Main -> WifiInfoChange
                PRINT_PROCESS_MEASUREMENT_POINT_START("WifiInfoChange mode switch");
                if (bprvSwitchToWiFiInfoChange(&xQueueData) == false)
                {
                    APP_PRINTFError("WiFi info change mode switch failed.");
                }
                PRINT_PROCESS_MEASUREMENT_POINT_END("WifiInfoChange mode switch");
                break;
            default:
                break;
            }
        }

        PRINT_TASK_REMAINING_STACK_SIZE();
    }
}

static bool bprvSwitchToMainMode(const DeviceModeSwitchData_t *pxEvent)
{

// デバックとしてプロビジョニングフラグ確認をスキップするかどうか
#if PROVISIONING_FLAG_SKIP_CONFIRMATION == 0

    // プロビジョニングフラグの取得
    ProvisioningFlag_t xProvisioningFlag;
    FlashTaskResult_t eReadFlashResult = eReadFlashInfo(READ_FLASH_TYPE_PROVISIONING_FLAG,
                                                        &xProvisioningFlag,
                                                        sizeof(ProvisioningFlag_t));
    if (eReadFlashResult != FLASH_TASK_RESULT_SUCCESS)
    {
        APP_PRINTFError("Provisioning flag read failed. Reason: %d", eReadFlashResult);
        return false;
    }

    // プロビジョニングフラグが立っていない場合は何もしない
    if (xProvisioningFlag == false)
    {
        APP_PRINTFInfo("Provisioning flag is false. Therefore, skip further processing.");
        return true;
    }
#endif /* PROVISIONING_FLAG_SKIP_CONFIRMATION */

    // Wi-Fiルータへの接続
    WiFiConnectRejectConditionFunction_t xWiFiRejectConditionFunction = {
        .bRejectConditionFunction = &bCheckNetworkRetryAcceptable};

    NetworkOperationResult_t eWiFiResult = eWiFiConnectToRouter(WIFI_CONNECT_RETRY_REPEAT_AD_INFINITUM, &xWiFiRejectConditionFunction);
    if (eWiFiResult != NETWORK_OPERATION_RESULT_SUCCESS)
    {
        APP_PRINTFError("Wi-Fi connect failed. Reason: %d", eWiFiResult);
        return false;
    }

    // AWS IoTへの接続
    MQTTConnectRejectConditionFunction_t xMQTTRejectConditionFunction = {
        .bRejects = &bCheckNetworkRetryAcceptable};

    if (eMQTTConnectToAWSIoT(MQTT_THING_NAME_TYPE_USUAL, MQTT_CONNECT_RETRY_REPEAT_AD_INFINITUM, &xMQTTRejectConditionFunction) != MQTT_OPERATION_TASK_RESULT_SUCCESS)
    {
        APP_PRINTFError("MQTT connect to aws iot failed.");
        return false;
    }

    APP_PRINTFDebug("MQTT connect to aws iot succeeded.");

    // OTAAgent Taskの初期化
    if (eOTAAgentTaskInit() != OTA_AGENT_TASK_RESULT_SUCCESS)
    {
        APP_PRINTFError("OTAAgent task initialization failed.");
        return false;
    }
    APP_PRINTFDebug("OTAAgent task initialization succeeded.");

    ShadowChangeCallback_t xShadowChangeCallback = NULL;
    // Shadowが変更された時のコールバック関数を取得
    vGetShadowStateChangeCallback(&xShadowChangeCallback);
    // Shadow Taskの初期化
    if (eDeviceShadowTaskInit(xShadowChangeCallback) != DEVICE_SHADOW_RESULT_SUCCESS)
    {
        APP_PRINTFError("Shadow task initialization failed.");
        return false;
    }

    // 解錠状態を取得
    LockState_t eLockState = eGetLockStateOpLockTask();

    // Update Device shadow
    ShadowState_t xState = {
        .xLockState = eLockState,
        .xUnlockingOperator = UNLOCKING_OPERATOR_TYPE_NONE /* 主体者がいないためNoneにする */};
    if (eUpdateShadowStateSync(SHADOW_UPDATE_TYPE_ALL, &xState, DEVICE_MODE_SWITCH_SHADOW_UPDATE_WAITE_TIME_MS) != DEVICE_SHADOW_RESULT_SUCCESS)
    {
        APP_PRINTFError("Failed to update shadow state.");
    }

    return true;
}

static bool bprvSwitchToProvisioningMode(const DeviceModeSwitchData_t *pxEvent)
{

    APP_PRINTFDebug("Switch to provisioning mode.");

    // ネットワークの切断とモード移行をBLEに伝える
    if (bprvNetworkDisconnectAndSendModeSwitchDoneToBLE() == false)
    {
        APP_PRINTFError("Failed to network disconnect or send mode switch to ble.");
        return false;
    }

    // プロビジョニングタスクの初期化
    if (eProvisioningTaskInitialize() != PROVISIONING_TASK_RESULT_SUCCEED)
    {
        APP_PRINTFError("Provisioning task create failed.");
        return false;
    }

    return true;
}

static bool bprvSwitchToWiFiInfoChange(const DeviceModeSwitchData_t *pxEvent)
{
    // ネットワークの切断とモード移行をBLEに伝える
    if (bprvNetworkDisconnectAndSendModeSwitchDoneToBLE() == false)
    {
        APP_PRINTFError("Failed to network disconnect or send mode switch to ble.");
        return false;
    }

    // Wi-Fi情報変更タスクの初期化
    if (eWiFiInfoChangeTaskInit() != WIFI_INFO_CHANGE_RESULT_SUCCESS)
    {
        APP_PRINTFError("Failed to init wifi info change task.");
        return false;
    }

    return true;
}

static bool bprvNetworkDisconnectAndSendModeSwitchDoneToBLE(void)
{
    // Shadow TaskのShutdown
    if (eDeviceShadowTaskShutdown() != DEVICE_SHADOW_RESULT_SUCCESS)
    {
        APP_PRINTFError("Shadow task shutdown failed.");
        return false;
    }

    // OTAAgent TaskのShutdown
    if (eOTAAgentTaskShutdown() != OTA_AGENT_TASK_RESULT_SUCCESS)
    {
        APP_PRINTFError("OTAAgent task shutdown failed.");
        return false;
    }

    // MQTT から切断
    if (eMQTTDisconnectAndTaskShutdown() != MQTT_OPERATION_TASK_RESULT_SUCCESS)
    {
        APP_PRINTFError("MQTT Disconnect and task shutdown failed.");
        return false;
    }

    // Wi-Fiルータからの切断
    if (eWiFiDisconnectFromRouter() != NETWORK_OPERATION_RESULT_SUCCESS)
    {
        APP_PRINTFError("Wi-Fi Disconnect failed.");
        return false;
    }

    // ペアリングされるまで待機
    while (!bCheckSecuredBLE())
    {
        vTaskDelay(300);
    }

    // モード変更リクエスト応答結果の送信
    vSetCommandNothingMode();

    return true;
}

// -----------------CALLBACKS ----------------

static bool bCheckNetworkRetryAcceptable(const uint32_t uxRetryCount)
{
    APP_PRINTFDebug("Network retry count: %u", uxRetryCount);

    // DeviceModeSwitchQueueに溜まっているメッセージの数が1個以上の場合はネットワークリトライを中断する
    if (uxQueueMessagesWaiting(gxDeviceModeSwitchQueueHandle) > 0)
    {
        return false;
    }

    return true;
}

// --------------------------------------------------
// Unit Test用関数定義(関数のプロトタイプ宣言は「自身のファイル名+ "_test.h"」で宣言されていること)
// --------------------------------------------------
#if (BUILD_MODE_TEST == 1) /* BUILD_MODE_TESTが定義されているとき */
#endif                     /* end  BUILD_MODE_TEST */
