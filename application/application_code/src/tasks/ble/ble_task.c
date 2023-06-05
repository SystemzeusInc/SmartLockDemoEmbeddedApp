/**
 * @file ble_task.c
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
#include "atca_basic.h"

// --------------------------------------------------
// ユーザ作成ヘッダの取り込み
// --------------------------------------------------
#include "config/task_config.h"
#include "config/queue_config.h"

#include "common/include/application_define.h"

#include "tasks/ble/include/ble_task.h"
#include "tasks/ble/include/rn4870.h"
#include "tasks/flash/include/flash_data.h"
#include "tasks/flash/include/flash_task.h"
#include "tasks/provisioning/include/provisioning.h"

// --------------------------------------------------
// 自ファイル内でのみ使用する#defineマクロ
// --------------------------------------------------
#define BLE_DEVICE_NAME_PREFIX "SMARTLOCK_" /**< デバイス名の接頭辞 */
#define BLE_PAIRING_PIN        "123456"     /**< ペアリング時のPIN(6桁) */

#define EVENT_LOOP_TASK_STACK_SIZE     (configMINIMAL_STACK_SIZE * 1) /**< イベントループタスクのスタックサイズ */
#define INTERFACE_LOOP_TASK_STACK_SIZE (configMINIMAL_STACK_SIZE * 1) /**< インターフェースループタスクのスタックサイズ */
#define SEND_LOOP_TASK_STACK_SIZE      (configMINIMAL_STACK_SIZE * 1) /**< 送信ループタスクのスタックサイズ */

#define EVENT_LOOP_TASK_PRIORITY     1 /**< イベントループタスクの優先度 */
#define INTERFACE_LOOP_TASK_PRIORITY 1 /**< インターフェースループタスクの優先度 */
#define SEND_LOOP_TASK_PRIORITY      1 /**< 送信ループタスクの優先度 */

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
static BLETaskData_t gxAppData = {0};         /**< タスク関連データ */
static BLEInterface_t gxBLEInterface = {0};   /**< BLEモジュールのインターフェース */
static BLEEventCallback_t gxBLEEventCb = {0}; /**< BLEのイベントコールバック */

// --------------------------------------------------
// static関数プロトタイプ宣言
// --------------------------------------------------
/**
 * @brief BLETask
 *
 * @param [in] pvParameters パラメータ
 */
static void prvBLETask(void *pvParameters);

/**
 * @brief BLETaskに命令
 *
 * @param [in] pxData    命令内容
 * @param [in] ulTimeout タイムアウトms 0を指定した場合は実行結果を待たず終了,portMAX_DELAYで無限待機
 *
 * @return BLETaskResult 結果
 */
BLETaskResult_t eprvBLETaskOp(BLETaskQueueData_t *pxData, uint32_t ulTimeout);

/**
 * GPIOのH/Lを行う関数
 */
static void prvGpioOn();
static void prvGpioOff();

/**
 * @brief 接続時に得られるパラメータを取得するコールバック関数
 *
 * @param [in] pvValue パラメータ
 */
static void prvDefaultCbConnParam(void *pvValue);

/**
 * @brief 接続時に呼ばれるコールバック関数
 *
 * @param [in] pvValue パラメータ
 */
static void prvDefaultCbConnect(void *pvValue);

/**
 * @brief 切断時に呼ばれるコールバック関数
 *
 * @param [in] pvValue パラメータ
 */
static void prvDefaultCbDisconnect(void *pvValue);

/**
 * @brief 再起動時に呼ばれるコールバック関数
 *
 * @param [in] pvValue パラメータ
 */
static void prvDefaultCbReboot(void *pvValue);

/**
 * @brief ペアリング完了時に呼ばれるコールバック関数
 *
 * @param [in] pvValue パラメータ
 */
static void prvDefaultCbSecuredBLE(void *pvValue);

/**
 * @brief 指定Characteristicにデータを書き込む
 *
 * @param [in] uxUUID    Characteristic UUID
 * @param [in] uxData    書き込むデータ
 * @param [in] xDataSize データサイズ
 */
static void prvWriteCharacteristicValue(uint8_t *uxUUID, uint8_t *uxData, size_t xDataSize);

/**
 * @brief 指定Characteristicのデータを読み込み
 *
 * @param [in]      pucUUID      Characteristic UUID
 * @param [out]     puxBuffer    読み込むバッファ
 * @param [in, out] pxBufferSize バッファサイズ
 */
static void prvReadCharacteristicValue(uint8_t *pucUUID, uint8_t *puxBuffer, size_t *pxBufferSize);

/**
 * @brief ボンディング要求
 */
static void prvBonding();

// --------------------------------------------------
// 変数定義（staticを除く）
// --------------------------------------------------
static bool gbRebootFlag = false;  /**< 再起動確認用フラグ */
static bool gbSecuredFlag = false; /**< ペアリング済確認用フラグ */

// --------------------------------------------------
// 関数定義（staticを除く）
// --------------------------------------------------
BLETaskResult_t eBLETaskInitialize(void)
{
    if (gxAppData.xTaskHandle != NULL) // すでに初期化済み
    {
        APP_PRINTFDebug("Already initialized ble task.");
        return BLE_TASK_RESULT_SUCCEED;
    }

    gxAppData.eState = BLE_APP_STATE_INIT;
    gxAppData.xQueue = NULL;
    gxAppData.xTaskHandle = NULL;

    gxBLEInterface.uart_tx = UART2_Write;
    gxBLEInterface.uart_rx = UART2_Read;
    gxBLEInterface.gpio_on = prvGpioOn;
    gxBLEInterface.gpio_off = prvGpioOff;
    gxBLEInterface.delay = vTaskDelay;

    gxBLEEventCb.conn_param = prvDefaultCbConnParam;
    gxBLEEventCb.connect = prvDefaultCbConnect;
    gxBLEEventCb.disconnect = prvDefaultCbDisconnect;
    gxBLEEventCb.reboot = prvDefaultCbReboot;
    gxBLEEventCb.secured = prvDefaultCbSecuredBLE;

    vInitializeBLE(&gxBLEInterface, &gxBLEEventCb);

    BaseType_t xResult = xTaskCreate(prvBLETask,
                                     "BLE Task",
                                     BLE_TASK_SIZE,
                                     NULL,
                                     BLE_TASK_PRIORITY,
                                     &gxAppData.xTaskHandle);
    if (xResult != pdTRUE)
    {
        return BLE_TASK_RESULT_FAILED;
    }
    return BLE_TASK_RESULT_SUCCEED;
}

uint8_t uxBLEGetBondingList(BLEBonding_t *pxBondingList)
{
    eEnterCMDMode();
    uint8_t uxListBondingString[256] = {0};
    memset(uxListBondingString, 0x00, sizeof(uxListBondingString));
    eListBondedDevices(uxListBondingString, sizeof(uxListBondingString));

    uint8_t uxBondingNum = uxParseLb(uxListBondingString, pxBondingList);
    eExitCMDMode();
    return uxBondingNum;
}

void vInitLinkingInfo()
{
    vWriteOpBLE((uint8_t *)CHARACTERISTIC_UUID_LINKING_INFO, (uint8_t *)LINKING_INFO_INIT_VALUE, strlen(LINKING_INFO_INIT_VALUE));
}

void vSetProvisioningMode()
{
    vWriteOpBLE((uint8_t *)CHARACTERISTIC_UUID_PROVISIONING, (uint8_t *)PROVISIONING_MODE_REQ_STRING, strlen(PROVISIONING_MODE_REQ_STRING));
}

void vSetCommandNothingMode()
{
    vWriteOpBLE((uint8_t *)CHARACTERISTIC_UUID_PROVISIONING, (uint8_t *)COMMAND_NOTHING_STRING, strlen(COMMAND_NOTHING_STRING));
}

void vInitBLEProvisioningInfo()
{
    uint8_t uxTmpWiFiInfo[1] = {0};
    vWriteOpBLE((uint8_t *)CHARACTERISTIC_UUID_WIFI_INFO, uxTmpWiFiInfo, sizeof(uxTmpWiFiInfo));
}

void vInitBLEWiFiInfo()
{
    uint8_t uxTmpWiFiInfo[1] = {0};
    vWriteOpBLE((uint8_t *)CHARACTERISTIC_UUID_WIFI_INFO_CHANGE, uxTmpWiFiInfo, sizeof(uxTmpWiFiInfo));
}

void vWriteOpBLE(uint8_t *pucUUID, uint8_t *puxData, size_t xDataSize)
{
    BLETaskQueueData_t xQueueData = {
        .eOp = BLE_OP_WRITE,
        .u.write.xDataSize = xDataSize};
    if (xDataSize > sizeof(xQueueData.u.write.uxData))
    {
        return;
    }
    memcpy(xQueueData.u.write.ucUUID, pucUUID, strlen((const char *)pucUUID));
    memcpy(xQueueData.u.write.uxData, puxData, xDataSize);
    eprvBLETaskOp(&xQueueData, 0);
}

void vReadOpBLE(uint8_t *pucUUID, uint8_t *puxBuffer, size_t *pxBufferSize)
{
    BLETaskQueueData_t xQueueData = {
        .eOp = BLE_OP_READ,
        .u.read.puxBuffer = puxBuffer,
        .u.read.pxBufferSize = pxBufferSize};
    memcpy(xQueueData.u.read.ucUUID, pucUUID, strlen((const char *)pucUUID));
    eprvBLETaskOp(&xQueueData, portMAX_DELAY);
}

void vBondingOpBLE()
{
    BLETaskQueueData_t xSendQueueData = {0};
    xSendQueueData.eOp = BLE_OP_BONDING;
    eprvBLETaskOp(&xSendQueueData, 0);
}

bool bCheckSecuredBLE()
{
    return gbSecuredFlag;
}

// --------------------------------------------------
// static関数定義
// --------------------------------------------------
static void prvBLETask(void *pvParameters)
{
    BaseType_t xResult;
    BLETaskQueueData_t xReceiveQueueData;

    while (1)
    {
        switch (gxAppData.eState)
        {
        case BLE_APP_STATE_INIT:
            APP_PRINTFDebug("Initialize BLE task...");

            // キュー作成
            gxAppData.xQueue = xQueueCreate(BLE_TASK_QUEUE_LENGTH, sizeof(BLETaskQueueData_t));
            if (gxAppData.xQueue == NULL)
            {
                APP_PRINTFError("Failed to create ble queue.");
            }

            // 子タスク生成
            xResult = xTaskCreate(vEventLoop,
                                  "BLE Event Loop",
                                  EVENT_LOOP_TASK_STACK_SIZE,
                                  NULL,
                                  EVENT_LOOP_TASK_PRIORITY,
                                  NULL);
            if (xResult != pdTRUE)
            {
                APP_PRINTFError("Failed to create event loop task.");
            }

            xResult = xTaskCreate(vInterfaceLoop,
                                  "BLE Interface Loop",
                                  INTERFACE_LOOP_TASK_STACK_SIZE,
                                  NULL,
                                  INTERFACE_LOOP_TASK_PRIORITY,
                                  NULL);
            if (xResult != pdTRUE)
            {
                APP_PRINTFError("Failed to create interface loop task.");
            }

            xResult = xTaskCreate(vSendLoop,
                                  "BLE Send Loop",
                                  SEND_LOOP_TASK_STACK_SIZE,
                                  NULL,
                                  SEND_LOOP_TASK_PRIORITY,
                                  NULL);
            if (xResult != pdTRUE)
            {
                APP_PRINTFError("Failed to create send loop task.");
            }

            APP_PRINTFDebug("Reset...");
            vHardResetBLE();

            APP_PRINTFDebug("Enter CMD mode...");
            eEnterCMDMode();

#if 0
            APP_PRINTFDebug("Set serialized Bluetooth name...");
            if (eSetSerializedBluetoothName((uint8_t *)"RN4870Test") != BLE_RESULT_SUCCEED)
                APP_PRINTFError("Failed to ser serialize bluetooth name.");
#endif

            // ECC608のSN取得
            FactoryThingName_t xECC608Sn;
            memset(&xECC608Sn, 0x00, sizeof(xECC608Sn));
            eReadFlashInfo(READ_FLASH_TYPE_FACTORY_THING_NAME, &xECC608Sn, sizeof(xECC608Sn));

            uint8_t uxBLEDeviceName[16] = {0};
            memset(uxBLEDeviceName, 0x00, sizeof(uxBLEDeviceName));
            snprintf((char *)uxBLEDeviceName, sizeof(uxBLEDeviceName), "%s%c%c%c%c", BLE_DEVICE_NAME_PREFIX,
                     xECC608Sn.ucName[4], xECC608Sn.ucName[5], xECC608Sn.ucName[6], xECC608Sn.ucName[7]);
            APP_PRINTFDebug("Set device name...(%s)", uxBLEDeviceName);
            if (eSetDeviceName(uxBLEDeviceName) != BLE_RESULT_SUCCEED)
                APP_PRINTFError("Failed to set device name.");

#if 1
            APP_PRINTFDebug("Get device info...");
            eGetDeviceInfo();

            APP_PRINTFDebug("Get FW version...");
            eGetFWVersion();
#endif

            APP_PRINTFDebug("Clear all service...");
            if (eClearAllService() != BLE_RESULT_SUCCEED)
            {
                APP_PRINTFError("Failed to clear all service.");
            }

            APP_PRINTFDebug("Set service UUID %s...", SERVICE_UUID);
            if (eSetUUIDService((uint8_t *)SERVICE_UUID) != BLE_RESULT_SUCCEED)
            {
                APP_PRINTFError("Failed to set service uuid.(%s)", SERVICE_UUID);
            }

            APP_PRINTFDebug("Set characteristic UUID...");
            if (eSetUUIDCharacteristic((uint8_t *)CHARACTERISTIC_UUID_WIFI_INFO, WRITE, MAX_CHARACTERISTIC_DATA_SIZE) != BLE_RESULT_SUCCEED)
            {
                APP_PRINTFError("Failed to set uuid characteristic.(%s)", CHARACTERISTIC_UUID_WIFI_INFO);
            }
            if (eSetUUIDCharacteristic((uint8_t *)CHARACTERISTIC_UUID_LINKING_INFO, READ, MAX_CHARACTERISTIC_DATA_SIZE) != BLE_RESULT_SUCCEED)
            {
                APP_PRINTFError("Failed to set uuid characteristic.(%s)", CHARACTERISTIC_UUID_LINKING_INFO);
            }
            if (eSetUUIDCharacteristic((uint8_t *)CHARACTERISTIC_UUID_PROVISIONING, READ | WRITE, MAX_CHARACTERISTIC_DATA_SIZE) != BLE_RESULT_SUCCEED)
            {
                APP_PRINTFError("Failed to set uuid characteristic.(%s)", CHARACTERISTIC_UUID_PROVISIONING);
            }
            if (eSetUUIDCharacteristic((uint8_t *)CHARACTERISTIC_UUID_WIFI_INFO_CHANGE, WRITE, 151) != BLE_RESULT_SUCCEED)
            {
                APP_PRINTFError("Failed to set uuid characteristic.(%s)", CHARACTERISTIC_UUID_WIFI_INFO_CHANGE);
            }

            eUpdateHandleInfo((uint8_t *)SERVICE_UUID);

            APP_PRINTFDebug("Get characteristic list of %s...", SERVICE_UUID);
            uint8_t uxListCharaString[256] = {0};
            size_t xListCharaStringLength = sizeof(uxListCharaString);
            eListServiceCharacteristic((uint8_t *)SERVICE_UUID, uxListCharaString, &xListCharaStringLength);
            BLECharacteristic_t xChara[MAX_CHARACTERISTIC_NUM];
            memset(xChara, 0x00, sizeof(xChara));
            eParseLs(uxListCharaString, xChara);
            for (int c = 0; c < MAX_CHARACTERISTIC_NUM; c++)
            {
                if (xChara[c].uxUUID[0] == 0x00)
                {
                    continue;
                }
                APP_PRINTFInfo("uuid: %s, handle: 0x%04X, property: 0x%02X", xChara[c].uxUUID, xChara[c].usHandle, xChara[c].uxProperty);
                vTaskDelay(30);
            }

            APP_PRINTFDebug("Set fix PIN...");
            if (eSetFixPIN((uint8_t *)BLE_PAIRING_PIN) != BLE_RESULT_SUCCEED)
            {
                APP_PRINTFError("Failed to set fix PIN.");
            }

            APP_PRINTFDebug("Set pairing mode...");
            if (eSetPairingMode(DISPLAY_ONLY) != BLE_RESULT_SUCCEED)
            {
                APP_PRINTFError("Failed to set pairing mode.");
            }

#if 1
            APP_PRINTFDebug("Get bonded devices...");
            uint8_t uxListBondingString[256] = {0};
            memset(uxListBondingString, 0x00, sizeof(uxListBondingString));
            if (eListBondedDevices(uxListBondingString, sizeof(uxListBondingString)) != BLE_RESULT_SUCCEED)
            {
                APP_PRINTFWarn("Failed to get bonded devices.");
            }
            APP_PRINTFInfo("\r\n%s", uxListBondingString);
#endif

#if 0
            APP_PRINTFDebug("Delete bonding...");
            if (eRemoveBonding(1) != BLE_RESULT_SUCCEED)
            {
                APP_PRINTFWarn("Failed to remove bonding.");
            }
#endif

            APP_PRINTFDebug("Reboot BLE module...");
            gbRebootFlag = false;
            eReboot();

            // 再起動完了まで待機
            while (1)
            {
                if (gbRebootFlag)
                {
                    gbRebootFlag = false;
                    break;
                }
                vTaskDelay(300);
            }

            APP_PRINTFDebug("Enter CMD mode...");
            eEnterCMDMode();

            // キャラクタリスティック初期値の設定
            APP_PRINTFDebug("Write initial value of linking info...");
            vInitLinkingInfo();
            vSetProvisioningMode();

            APP_PRINTFDebug("Finish command mode...");
            eExitCMDMode();

            gxAppData.eState = BLE_APP_STATE_SERVICE_TASK;

            PRINT_TASK_REMAINING_STACK_SIZE();

            break;
        case BLE_APP_STATE_SERVICE_TASK:
            xResult = xQueueReceive(gxAppData.xQueue, &xReceiveQueueData, 1000);
            if (xResult != pdPASS)
            {
                break;
            }

            if (xReceiveQueueData.eOp == BLE_OP_WRITE)
            {
                prvWriteCharacteristicValue(xReceiveQueueData.u.write.ucUUID, xReceiveQueueData.u.write.uxData, xReceiveQueueData.u.write.xDataSize);
            }
            else if (xReceiveQueueData.eOp == BLE_OP_READ)
            {
                prvReadCharacteristicValue(xReceiveQueueData.u.read.ucUUID, xReceiveQueueData.u.read.puxBuffer, xReceiveQueueData.u.read.pxBufferSize);
            }
            else if (xReceiveQueueData.eOp == BLE_OP_BONDING)
            {
                prvBonding();
            }
            else
            {
                APP_PRINTFWarn("Unknown operation: 0x%X", xReceiveQueueData.eOp);
                break;
            }

            // 完了通知
            if (xReceiveQueueData.xTaskHandle != NULL)
            {
                xTaskNotify(xReceiveQueueData.xTaskHandle, BLE_OP_EVENT_SUCCEED, eSetBits);
            }

            PRINT_TASK_REMAINING_STACK_SIZE();

            break;
        default:
            break;
        }
    }
}

BLETaskResult_t eprvBLETaskOp(BLETaskQueueData_t *pxData, uint32_t ulTimeout)
{
    // キューの存在確認
    if (gxAppData.xQueue == NULL)
    {
        return BLE_TASK_RESULT_FAILED;
    }

    // 実行中タスクの確認
    if (ulTimeout == 0) // タイムアウトを設定しない場合はnotifyをさせないためtaskHandleをNULLのままにする
    {
        pxData->xTaskHandle = NULL;
    }
    else
    {
        pxData->xTaskHandle = xTaskGetCurrentTaskHandle();
    }

    // 送信
    if (xQueueSend(gxAppData.xQueue, pxData, portMAX_DELAY) != pdPASS)
    {
        APP_PRINTFError("Failed to send queue.");
        return BLE_TASK_RESULT_FAILED;
    }

    // 待機
    if (pxData->xTaskHandle != NULL)
    {
        uint32_t ulNotifiedValue = 0;
        xTaskNotifyWait(0xffffffff, 0xffffffff, &ulNotifiedValue, ulTimeout);
        if (ulNotifiedValue & BLE_OP_EVENT_SUCCEED)
        {
            return BLE_TASK_RESULT_SUCCEED;
        }
        else
        {
            return BLE_TASK_RESULT_FAILED;
        }
    }
    return BLE_TASK_RESULT_SUCCEED;
}

static void prvGpioOn()
{
    BLE_RST_Set();
}

static void prvGpioOff()
{
    BLE_RST_Clear();
}

static void prvDefaultCbConnParam(void *pvValue)
{
    BLEEventConnParamValue_t *pxParamValue = pvValue;
#if 0 // デバッグ用
    printf("conn param event\r\n");
    printf("    interval: 0x%04X\r\n", pxParamValue->uxInterval);
    printf("    latency : 0x%04X\r\n", pxParamValue->uxLatency);
    printf("    timeout : 0x%04X\r\n", pxParamValue->uxTimeout);
#else
    (void)pxParamValue;
#endif
}

static void prvDefaultCbConnect(void *pvValue)
{
    BLEEventConnectValue_t *pxConnectValue = pvValue;
#if 1 // デバッグ用
    uint8_t message[13] = {0};
    memset(message, 0x00, sizeof(message));
    for (int i = 0; i < 6; i++)
    {
        snprintf((char *)(message + i * 2), 3, "%02X", pxConnectValue->uxAddress[i]);
    }
    APP_PRINTFDebug("<<<Connect BLE address: %s, num: %d", message, pxConnectValue->uxNum);
#endif

    gbSecuredFlag = false;

#if 0 // ペアリング(ボンディング)強制
    BLEBonding_t xBondingList[8];
    memset(xBondingList, 0x00, sizeof(xBondingList));
    uint8_t uxBondingNum = uxBLEGetBondingList(xBondingList);
    APP_PRINTFDebug("bonding num: %d", uxBondingNum);
    bool bBondingFlag = false;
    for (int i = 0; i < uxBondingNum; i++)
    {
        if (memcmp(xBondingList[i].uxAddress, pxConnectValue->uxAddress, strlen((const char *)xBondingList[i].uxAddress)) == 0)
            bBondingFlag = true;
    }

    if (bBondingFlag) // 既にペアリング(ボンディング)済
    {
        return;
    }

    APP_PRINTFDebug("Request bonding...");
    vBondingOpBLE();
#endif
}

static void prvDefaultCbDisconnect(void *pvValue)
{
#if 1 // デバッグ用
    APP_PRINTFDebug("Disconnect event.");
#endif
    gbSecuredFlag = false;
}

static void prvDefaultCbReboot(void *pvValue)
{
    gbRebootFlag = true;
}

static void prvDefaultCbSecuredBLE(void *pvValue)
{
#if 1 // デバッグ用
    APP_PRINTFDebug("secured!!!");
#endif
    gbSecuredFlag = true;
}

/* -------------------------------------------------- */

static void prvWriteCharacteristicValue(uint8_t *uxUUID, uint8_t *uxData, size_t xDataSize)
{
    eEnterCMDMode();
    uint16_t usHandle = usGetHandleByUUID((uint8_t *)SERVICE_UUID, uxUUID);
    APP_PRINTFDebug("Write local chara\r\nuuid: %s(handle: 0x%04X)\r\ndata: %s",
                    uxUUID,
                    usHandle,
                    uxData);
    if (eWriteLocalCharacteristicValue(usHandle, uxData, xDataSize) != BLE_RESULT_SUCCEED)
    {
        APP_PRINTFError("Failed to write characteristic value.");
    }
    eExitCMDMode();
}

static void prvReadCharacteristicValue(uint8_t *pucUUID, uint8_t *puxBuffer, size_t *pxBufferSize)
{
    eEnterCMDMode();
    uint16_t usHandle = usGetHandleByUUID((uint8_t *)SERVICE_UUID, pucUUID);
    if (eReadLocalCharacteristicValue(usHandle, puxBuffer, pxBufferSize) != BLE_RESULT_SUCCEED)
    {
        APP_PRINTFError("Failed to read characteristic value.");
    }
    eExitCMDMode();
}

static void prvBonding()
{
    APP_PRINTFDebug("Start Bonding...");
    eEnterCMDMode();
    if (eStartBondingProcess() != BLE_RESULT_SUCCEED)
    {
        APP_PRINTFError("Failed to start bonding process.");
    }
    eExitCMDMode();
}

// --------------------------------------------------
// Unit Test用関数定義(関数のプロトタイプ宣言は「自身のファイル名+ "_test.h"」で宣言されていること)
// --------------------------------------------------
#if (BUILD_MODE_TEST == 1) /* BUILD_MODE_TESTが定義されているとき */
#endif                     /* end  BUILD_MODE_TEST */
