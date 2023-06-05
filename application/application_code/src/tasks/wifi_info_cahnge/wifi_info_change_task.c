/**
 * @file wifi_info_change_task.c
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

#include "common/include/application_define.h"

#include "tasks/ble/include/ble_task.h"
#include "tasks/ble/include/rn4870.h"
#include "tasks/flash/include/flash_data.h"
#include "tasks/flash/include/flash_task.h"
#include "tasks/device_mode_switch/include/device_mode_switch_event.h"
#include "tasks/device_mode_switch/include/device_mode_switch_task.h"
#include "tasks/wifi_info_cahnge/include/wifi_info_change_task.h"

// --------------------------------------------------
// 自ファイル内でのみ使用する#defineマクロ
// --------------------------------------------------

// --------------------------------------------------
// 自ファイル内でのみ使用する#define関数マクロ
// --------------------------------------------------

// --------------------------------------------------
// 自ファイル内でのみ使用するtypedef定義
// --------------------------------------------------

/**
 * @brief WiFi接続先変更タスクとやり取りするQueueのデータ構造
 */
typedef struct
{

    /**
     * @brief Wi-Fi情報のパースに成功したか
     */
    bool bIsWiFiInfoPurseSucceeded;

    /**
     * @brief WI-Fi情報
     */
    WiFiInfo_t xWiFiInfo;

} WiFIInfoChangeQueueData_t;

// --------------------------------------------------
// 自ファイル内でのみ使用するenumタグ定義（typedefを同時に行う）
// --------------------------------------------------

// --------------------------------------------------
// 自ファイル内でのみ使用するstruct/unionタグ定義（typedefを同時に行う）
// --------------------------------------------------

// --------------------------------------------------
// ファイル内で共有するstatic変数宣言
// --------------------------------------------------

/**
 * @brief Wi-Fi接続先変更タスクのタスクハンドル
 *
 */
static TaskHandle_t gxWiFiInfoChangeTaskHandle = NULL;

/**
 * @brief Wi-Fi接続先変更タスクとやり取りするQueueのハンドル
 *
 */
static QueueHandle_t gxWiFiInfoChangeTaskQueueHandle = NULL;

// --------------------------------------------------
// static関数プロトタイプ宣言
// --------------------------------------------------

/**
 * @brief Wi-Fi接続先変更タスクのエントリポイント
 *
 * @param[in] pvParam 本パラメータは使用しない。
 */
static void vprvWiFiInfoChangeTask(void *pvParam);

/**
 * @brief Wi-Fi接続先変更情報が書き込まれたら呼び出されるコールバック関数
 *
 * @param[in] pvValue 書き込まれた情報
 */
static void vprvWiFiInfoChangeBLEEventCallback(void *pvValue);

/**
 * @brief BLETaskから得たデータを解析して #WiFiInfo_t に変換する
 *
 * @param[in]  pucData    BLEから得たデータ
 * @param[in]  uxDataSize データサイズ
 * @param[out] pxWiFIInfo 解析したWiFi接続先情報
 *
 * @retval true  解析成功
 * @retval false 解析失敗
 */
static bool bprvParseWiFiInfo(const uint8_t *pucData, const uint32_t uxDataSize, WiFiInfo_t *pxWiFIInfo);

// --------------------------------------------------
// 変数定義（staticを除く）
// --------------------------------------------------

// --------------------------------------------------
// 関数定義（staticを除く）
// --------------------------------------------------

WIFIInfoChangeResult_t eWiFiInfoChangeTaskInit(void)
{

    // Queueが作成されてない場合は作成
    if (gxWiFiInfoChangeTaskQueueHandle == NULL)
    {
        gxWiFiInfoChangeTaskQueueHandle = xQueueCreate(WIFI_INFO_CHANGE_TASK_QUEUE_LENGTH, sizeof(WiFIInfoChangeQueueData_t));

        if (gxWiFiInfoChangeTaskQueueHandle == NULL)
        {
            APP_PRINTFError("WiFiInfoChangeTask queue create failed.");
            return WIFI_INFO_CHANGE_RESULT_FAILED;
        }

        APP_PRINTFDebug("Successfully created WiFiInfoChangeQueue.");
    }

    // タスクが作成されたいない場合は作成
    if (gxWiFiInfoChangeTaskHandle == NULL)
    {
        if (xTaskCreate(&vprvWiFiInfoChangeTask,
                        "WiFiInfoChangeTask",
                        WIFI_INFO_CHANGE_TASK_SIZE,
                        NULL,
                        WIFI_INFO_CHANGE_TASK_PRIORITY,
                        &gxWiFiInfoChangeTaskHandle) != pdPASS)
        {
            APP_PRINTFError("WiFiInfoChangeTask create failed.");
            return WIFI_INFO_CHANGE_RESULT_FAILED;
        }
    }
    else
    {
        APP_PRINTFDebug("WiFiInfoChangeTask is already initialized.");
    }

    return WIFI_INFO_CHANGE_RESULT_SUCCESS;
}

// --------------------------------------------------
// static関数定義
// --------------------------------------------------

// ---------------------- TASK ------------------------

static void vprvWiFiInfoChangeTask(void *pvParam)
{

    APP_PRINTFDebug("Start WiFiInfoChangeTask.");
    bool bIsTimeout = false; // タイムアウトしたか

    do
    {
        // BLEタスクにキャラクタリスティックIDを登録
        if (eRegisterBLEEventCb(&vprvWiFiInfoChangeBLEEventCallback,
                                BLE_EVENT_CB_TYPE_WV,
                                (uint8_t *)CHARACTERISTIC_UUID_WIFI_INFO_CHANGE) != BLE_RESULT_SUCCEED)
        {
            APP_PRINTFError("Failed to register event ble callback.");
            break;
        }

        // Wi-Fiの情報を得らえられるまで待機
        WiFIInfoChangeQueueData_t xChangeWiFiInfo = {0x00};
        if (xQueueReceive(gxWiFiInfoChangeTaskQueueHandle, &xChangeWiFiInfo, pdMS_TO_TICKS(WIFI_INFO_TO_BE_SENT_WAIT_TIME_MS)) != pdPASS)
        {
            APP_PRINTFError("Timed out waiting for receive WiFi info.");
            bIsTimeout = true;
            break;
        }

        // Wi-Fi情報のパースが成功したか確認
        if (xChangeWiFiInfo.bIsWiFiInfoPurseSucceeded == false)
        {
            APP_PRINTFError("There is an error in the Wi-Fi information retrieved.");
            break;
        }

        APP_PRINTFDebug("RRRR: INFO: ssid: %s, pw: %s ", xChangeWiFiInfo.xWiFiInfo.cWifiSSID, xChangeWiFiInfo.xWiFiInfo.cWiFiPassword);

        // FlashにWi-Fi情報を登録
        if (eWriteFlashInfo(WRITE_FLASH_TYPE_WIFI_INFO, &(xChangeWiFiInfo.xWiFiInfo)) != FLASH_TASK_RESULT_SUCCESS)
        {
            APP_PRINTFError("Fail to write flash wifi info");
            break;
        }

        // BLEのWi-Fi接続情報Characteristic初期化(適当な値に書き換える)
        APP_PRINTFDebug("Initialize Wi-Fi info in BLE characteristic.");
        vInitBLEWiFiInfo();
    } while (false);

    // コールバック関数の登録解除
    eDeleteBLEEventCb(BLE_EVENT_CB_TYPE_WV, (uint8_t *)CHARACTERISTIC_UUID_WIFI_INFO_CHANGE);

    // Queueの削除
    vQueueDelete(gxWiFiInfoChangeTaskQueueHandle);
    gxWiFiInfoChangeTaskQueueHandle = NULL;

    APP_PRINTFDebug("WiFi info change task done.");

    // DeviceModeSwitchTaskに送信
    DeviceModeSwitchData_t xDeviceModeSwitchData = {0x00};
    xDeviceModeSwitchData.eDeviceModeSwitchEvent = bIsTimeout == true ? DEVICE_MODE_SWITCH_WIFI_INFO_TIMEOUT : DEVICE_MODE_SWITCH_WIFI_INFO_DONE;
    if (eDeviceModeSwitch(&xDeviceModeSwitchData) != DEVICE_MODE_SWITCH_TASK_RESULT_SUCCESS)
    {
        APP_PRINTFDebug("Device mode switch task send data failed.");
    }

    APP_PRINTFDebug("WIFInfoChangeTask delete.");

    PRINT_TASK_REMAINING_STACK_SIZE();

    // タスクの削除を行うためタスクハンドルをNULLに
    gxWiFiInfoChangeTaskHandle = NULL;

    // 自身のタスクを削除
    vTaskDelete(NULL);
}

static bool bprvParseWiFiInfo(const uint8_t *pucData, const uint32_t uxDataSize, WiFiInfo_t *pxWiFIInfo)
{
    const uint8_t *pucCopyData;
    uint8_t uxDataLength = 0;
    uint8_t uxTotalDataLength = 0;
    uint8_t *ucTopOfCopy = (uint8_t *)pucData;
    uint8_t *ucCopyIndex = (uint8_t *)pucData;

    const uint8_t xWiFiInfoCount = 2; // SSIDとPWの2回パースする必要があるため2とする
    for (uint8_t uxCount = 0; uxCount < xWiFiInfoCount; uxCount++)
    {
        // どのデータを書き込むか決定。スマホ（BLE）からのWi-Fi情報はSSID -> PWという順番で格納されている。
        pucCopyData = uxCount == 0 ? pxWiFIInfo->cWifiSSID : pxWiFIInfo->cWiFiPassword;

        // 解析
        while (*ucCopyIndex != WIFI_INFO_SEPARATE_CHARACTER)
        {
            if (*ucCopyIndex == '\0' || uxTotalDataLength > uxDataSize)
            {
                APP_PRINTFError("Wifi info parsing failed.");
                return false;
            }
            ucCopyIndex++;       // ポインタを進める
            uxDataLength++;      // 文字数をインクリメント
            uxTotalDataLength++; // トータル文字数をインクリメント
        }

        // データ長の検査。SSIDの場合は WIFI_SSID_MAX_LENGTH, PWの場合は WIFI_PASSWORD_MAX_LENGTH 以上になっていないかチェック
        if (uxCount == 0)
        {
            if (uxDataLength == 0 || uxDataLength > WIFI_SSID_MAX_LENGTH)
            {
                APP_PRINTFError("Wifi info parsing failed. SSID length exceeded.");
                return false;
            }
        }
        else
        {
            if (uxDataLength == 0 || uxDataLength > WIFI_SSID_MAX_LENGTH)
            {
                APP_PRINTFError("Wifi info parsing failed. PW length exceeded.");
                return false;
            }
        }

        // データをコピー
        memcpy((void *)pucCopyData, (const void *)ucTopOfCopy, uxDataLength);

        // SSIDが検索できたため、PWが格納されている先頭ポインタに位置をずらす
        ucTopOfCopy = ucCopyIndex + 1; // +1 は ucCopyIndexの最後が\nで終わっているため１文字ずらす必要がある。
        ucCopyIndex = ucCopyIndex + 1;
        uxDataLength = 0;
    }

    return true;
}

// ------------------------ CallBacks ------------------------

static void vprvWiFiInfoChangeBLEEventCallback(void *pvValue)
{

    // 変換
    BLEEventWVValue_t *pxValue = (BLEEventWVValue_t *)pvValue;

    APP_PRINTFDebug("Received WiFiInfoChangeBLEEventCallback.");
    // Queueのデータを作成
    WiFIInfoChangeQueueData_t xWiFiInfo = {0x00};

    bool bResult = bprvParseWiFiInfo(pxValue->uxData, pxValue->xDataSize, &(xWiFiInfo.xWiFiInfo));

    // WPA3を指定するとWPA3対応のWi-FiルータならWPA3に、WPA3に対応していないルータはWPA2で接続される。
    xWiFiInfo.xWiFiInfo.xWiFiSecurity = eWiFiSecurityWPA3;
    xWiFiInfo.bIsWiFiInfoPurseSucceeded = bResult;

    APP_PRINTFDebug("Purse result is %u. INFO: ssid: %s, pw: %s ", bResult, xWiFiInfo.xWiFiInfo.cWifiSSID, xWiFiInfo.xWiFiInfo.cWiFiPassword);

    // Queueに送信
    // 他のタスクがQueueに送信することはないため、待ち時間なしでQueueに格納できる。
    if (xQueueSend(gxWiFiInfoChangeTaskQueueHandle, &xWiFiInfo, 0) != pdPASS)
    {
        APP_PRINTFError("WiFiInfoChangeTaskHandleQueue send failed.");
    }
}

// --------------------------------------------------
// Unit Test用関数定義(関数のプロトタイプ宣言は「自身のファイル名+ "_test.h"」で宣言されていること)
// --------------------------------------------------
#if (BUILD_MODE_TEST == 1) /* BUILD_MODE_TESTが定義されているとき */
#endif                     /* end  BUILD_MODE_TEST */
