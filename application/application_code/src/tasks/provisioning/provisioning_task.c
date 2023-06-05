/**
 * @file provisioning_task.c
 * @author Systemzeus Inc.
 * @copyright Copyright © 2023 Systemzeus Inc. All rights reserved.
 */

// --------------------------------------------------
// システムヘッダの取り込み
// --------------------------------------------------
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "definitions.h"
#include "FreeRTOS.h"

// --------------------------------------------------
// ユーザ作成ヘッダの取り込み
// --------------------------------------------------
#include "include/application_define.h"
#include "config/task_config.h"
#include "config/queue_config.h"

#include "common/randutil/include/randutil.h"
#include "common/include/network_operation.h"

#include "tasks/ble/include/rn4870.h"
#include "tasks/ble/include/ble_task.h"
#include "tasks/provisioning/include/provisioning.h"
#include "tasks/provisioning/include/provisioning_task.h"
#include "tasks/mqtt/include/mqtt_operation_task.h"
#include "tasks/device_mode_switch/include/device_mode_switch_task.h"
#include "tasks/device_mode_switch/include/device_mode_switch_event.h"
#include "tasks/flash/include/flash_task.h"
#include "tasks/flash/private/include/se_operation.h"

#include "tasks/provisioning/private/include/device_register.h"

// --------------------------------------------------
// 自ファイル内でのみ使用する#defineマクロ
// --------------------------------------------------
#define OTT_SIZE        8  /**< OTTのバイトサイズ */
#define OTT_BASE64_SIZE 12 /**< OTTをbase64に変換後のサイズ */

#define WIFI_CONNECT_RETRY_MAX_NUM 3 /**< Wi-Fiへの最大接続試行回数 */

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
/**
 * @brief プロビジョニングタスクキューデータ構造
 */
typedef struct
{
    uint8_t uxSSID[WIFI_SSID_MAX_LENGTH + 1];            /**< SSID */
    uint8_t uxPassword[WIFI_PASSWORD_MAX_LENGTH + 1];    /**< Password */
    WIFISecurity_t eWiFiSecurity;                        /**< Security */
    uint8_t uxEndpoint[AWS_IOT_ENDPOINT_MAX_LENGTH + 1]; /**< Endpoint */
} ProvisioningTaskQueueData_t;

// --------------------------------------------------
// ファイル内で共有するstatic変数宣言
// --------------------------------------------------
static ProvisioningTaskData_t gxAppData;

// --------------------------------------------------
// static関数プロトタイプ宣言
// --------------------------------------------------
/**
 * @brief プロビジョニングタスク
 *
 * @param [in] pvParameters パラメータ
 */
static void prvProvisioningTask(void *pvParameters);
static void vprvCbReceiveBLE(void *pvValue);

// --------------------------------------------------
// 変数定義（staticを除く）
// --------------------------------------------------

// --------------------------------------------------
// 関数定義（staticを除く）
// --------------------------------------------------
ProvisioningTaskResult_t eProvisioningTaskInitialize(void)
{
    if (gxAppData.xTaskHandle != NULL) // すでに初期化済み
    {
        APP_PRINTFDebug("Already initialized provisioning task.");
        return PROVISIONING_RESULT_SUCCESS;
    }

    gxAppData.eState = PROVISIONING_APP_STATE_INIT;
    gxAppData.xQueue = NULL;
    gxAppData.xTaskHandle = NULL;

    BaseType_t xResult = xTaskCreate(prvProvisioningTask,
                                     "Provisioning Task",
                                     PROVISIONING_TASK_SIZE,
                                     NULL,
                                     PROVISIONING_TASK_PRIORITY,
                                     &gxAppData.xTaskHandle);
    if (xResult != pdTRUE)
    {
        return PROVISIONING_TASK_RESULT_FAILED;
    }

    return PROVISIONING_TASK_RESULT_SUCCEED;
}

ProvisioningTaskResult_t eShutdownProvisioningTask(void)
{
    if (gxAppData.xQueue != NULL)
    {
        vQueueDelete(gxAppData.xQueue);
        gxAppData.xQueue = NULL;
    }

    if (gxAppData.xTaskHandle != NULL)
    {
        vTaskDelete(gxAppData.xTaskHandle);
        gxAppData.xTaskHandle = NULL;
    }

    return PROVISIONING_TASK_RESULT_SUCCEED;
}

// --------------------------------------------------
// static関数定義
// --------------------------------------------------
static void prvProvisioningTask(void *pvParameters)
{
    BaseType_t xResult;
    ProvisioningTaskQueueData_t xReceiveQueueData;

    while (1)
    {
        switch (gxAppData.eState)
        {
        case PROVISIONING_APP_STATE_INIT:
            APP_PRINTFDebug("Initialize provisioning task...");

            // キュー作成
            gxAppData.xQueue = xQueueCreate(PROVISIONING_TASK_QUEUE_LENGTH, sizeof(ProvisioningTaskQueueData_t));
            if (gxAppData.xQueue == NULL)
            {
                APP_PRINTFError("Failed to create provisioning queue.");
            }

            // BLEのイベントコールバック設定
            if (eRegisterBLEEventCb(vprvCbReceiveBLE, BLE_EVENT_CB_TYPE_WV, (uint8_t *)CHARACTERISTIC_UUID_WIFI_INFO) != BLE_RESULT_SUCCEED)
            {
                APP_PRINTFError("Failed to register BLE event cb.");
            }

            gxAppData.eState = PROVISIONING_APP_STATE_TASK;
            break;
        case PROVISIONING_APP_STATE_TASK:
            // BLEからの受信待機
            APP_PRINTFDebug("Waiting to receive wifi info...");
            xResult = xQueueReceive(gxAppData.xQueue, &xReceiveQueueData, portMAX_DELAY);
            if (xResult != pdPASS)
            {
                APP_PRINTFError("Failed to receive queue.");
            }
            APP_PRINTFDebug("ssid    : %s", xReceiveQueueData.uxSSID);
            APP_PRINTFDebug("password: %s", xReceiveQueueData.uxPassword);
            APP_PRINTFDebug("endpoint: %s", xReceiveQueueData.uxEndpoint);

            // SSIDなどのフラッシュへの保存
#if 1
            WiFiInfo_t xWiFiInfo;
            memset(&xWiFiInfo, 0x00, sizeof(xWiFiInfo));
            memcpy(xWiFiInfo.cWifiSSID, xReceiveQueueData.uxSSID, strlen((const char *)xReceiveQueueData.uxSSID));
            memcpy(xWiFiInfo.cWiFiPassword, xReceiveQueueData.uxPassword, strlen((const char *)xReceiveQueueData.uxPassword));
            xWiFiInfo.xWiFiSecurity = xReceiveQueueData.eWiFiSecurity;

            AWSIoTEndpoint_t xEndpoint;
            memset(&xEndpoint, 0x00, sizeof(xEndpoint));
            memcpy(xEndpoint.ucEndpoint, xReceiveQueueData.uxEndpoint, strlen((const char *)xReceiveQueueData.uxEndpoint));

            if (eWriteFlashInfo(WRITE_FLASH_TYPE_WIFI_INFO, &xWiFiInfo) != FLASH_TASK_RESULT_SUCCESS)
            {
                APP_PRINTFError("Failed to write flash Wi-Fi info.");
            }
            if (eWriteFlashInfo(WRITE_FLASH_TYPE_AWS_IOT_ENDPOINT, &xEndpoint) != FLASH_TASK_RESULT_SUCCESS)
            {
                APP_PRINTFError("Failed to write flash iot endpoint.");
            }

            // BLEのWi-Fi接続情報Characteristic初期化(適当な値に書き換える)
            APP_PRINTFDebug("Initialize Provisioning info in BLE characteristic.");
            vInitBLEProvisioningInfo();
#endif

            // OTT(ワンタイムトークン)作成
            uint8_t uxTmpOTT[OTT_SIZE] = {0};
            xGetRandomBytes(uxTmpOTT, sizeof(uxTmpOTT));

            uint8_t uxOTTBase64[OTT_BASE64_SIZE + 1] = {0};
            memset(uxOTTBase64, 0x00, sizeof(uxOTTBase64));
            size_t size_uxOTTBase64 = sizeof(uxOTTBase64);
            atcab_base64encode_(uxTmpOTT, sizeof(uxTmpOTT), (char *)uxOTTBase64, &size_uxOTTBase64, atcab_b64rules_default);

            // 工場ThingName取得
            FactoryThingName_t xECC608Sn;
            memset(&xECC608Sn, 0x00, sizeof(xECC608Sn));
            eGetFactoryThingName(&xECC608Sn);

            // リンキング情報送信
            uint8_t ucLinkingInfo[36] = {0};
            memset(ucLinkingInfo, 0x00, sizeof(ucLinkingInfo));
            snprintf((char *)ucLinkingInfo, sizeof(ucLinkingInfo), "%s\n%s\n", xECC608Sn.ucName, uxOTTBase64);
            vWriteOpBLE((uint8_t *)CHARACTERISTIC_UUID_LINKING_INFO, ucLinkingInfo, strlen((const char *)ucLinkingInfo));

            // デバイス登録
#if 1
            if (eWiFiConnectToRouter(WIFI_CONNECT_RETRY_MAX_NUM, NULL) != NETWORK_OPERATION_RESULT_SUCCESS)
            {
                APP_PRINTFError("Wi-Fi Connect failed.");
                gxAppData.eState = PROVISIONING_APP_STATE_DEINIT;
                break;
            }

            LinkingOneTimeToken_t xLOTT;
            memset(&xLOTT, 0x00, sizeof(xLOTT));
            memcpy(xLOTT.uxLOTT, uxOTTBase64, strlen((const char *)uxOTTBase64));
            eRunDeviceRegisterProcess(&xLOTT);
#endif

            gxAppData.eState = PROVISIONING_APP_STATE_DEINIT;
            break;
        case PROVISIONING_APP_STATE_DEINIT:
            // BLETaskから受信コールバック削除
            eDeleteBLEEventCb(BLE_EVENT_CB_TYPE_WV, (uint8_t *)CHARACTERISTIC_UUID_WIFI_INFO);

            // OTTを初期値に戻す
            vInitLinkingInfo();

            // キュー削除
            vQueueDelete(gxAppData.xQueue);
            gxAppData.xQueue = NULL;

            // プロビジョニング終了通知
            DeviceModeSwitchData_t xSwitchData;
            memset(&xSwitchData, 0x00, sizeof(xSwitchData));
            xSwitchData.eDeviceModeSwitchEvent = DEVICE_MODE_SWITCH_PROVISIONING_DONE;
            xSwitchData.param.eProvisioningResult = PROVISIONING_RESULT_SUCCESS;
            eDeviceModeSwitch((const DeviceModeSwitchData_t *)&xSwitchData);

            // タスク終了(削除)
            APP_PRINTFDebug("Finish provisioning task!!!");

            PRINT_TASK_REMAINING_STACK_SIZE();

            gxAppData.xTaskHandle = NULL;
            vTaskDelete(NULL);
            return;
        default:
            break;
        }
    }
}

// プロビジョニングタスクのキューに送るコールバック関数
static void vprvCbReceiveBLE(void *pvValue)
{
    BLEEventWVValue_t *pxValue = pvValue;
    ProvisioningTaskQueueData_t xData;
    memset(xData.uxSSID, 0x00, sizeof(xData.uxSSID));
    memset(xData.uxPassword, 0x00, sizeof(xData.uxPassword));
    memset(xData.uxEndpoint, 0x00, sizeof(xData.uxEndpoint));

    uint8_t *puxString = (uint8_t *)pvPortMalloc((pxValue->xDataSize) + 1);
    memset(puxString, 0x00, (pxValue->xDataSize) + 1);
    memcpy(puxString, pxValue->uxData, pxValue->xDataSize);

    uint8_t uxElementIndex = 1;
    uint8_t *puxTp = (uint8_t *)strtok((char *)puxString, LINKING_INFO_DELIMITER);
    memcpy(xData.uxSSID, puxTp, strlen((const char *)puxTp));
    while (puxTp != NULL)
    {
        puxTp = (uint8_t *)strtok(NULL, LINKING_INFO_DELIMITER);
        if (puxTp != NULL)
        {
            if (uxElementIndex == 1)
                memcpy(xData.uxPassword, puxTp, strlen((const char *)puxTp));
            else if (uxElementIndex == 2)
                memcpy(xData.uxEndpoint, puxTp, strlen((const char *)puxTp));
        }
        uxElementIndex++;
    }

    // WPA3を指定するとWPA3対応のWi-FiルータならWPA3に、WPA3に対応していないルータはWPA2で接続される。
    xData.eWiFiSecurity = eWiFiSecurityWPA3;

    xQueueSend(gxAppData.xQueue, &xData, 10);
    vPortFree(puxString);
}

// --------------------------------------------------
// Unit Test用関数定義(関数のプロトタイプ宣言は「自身のファイル名+ "_test.h"」で宣言されていること)
// --------------------------------------------------
#if (BUILD_MODE_TEST == 1) /* BUILD_MODE_TESTが定義されているとき */
#endif                     /* end  BUILD_MODE_TEST */
