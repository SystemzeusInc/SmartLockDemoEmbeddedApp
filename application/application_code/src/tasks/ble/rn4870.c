/**
 * @file rn4870.c
 * @author Systemzeus Inc.
 * @copyright Copyright © 2023 Systemzeus Inc. All rights reserved.
 */

// --------------------------------------------------
// システムヘッダの取り込み
// --------------------------------------------------
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#include "definitions.h"
#include "FreeRTOS.h"

// --------------------------------------------------
// ユーザ作成ヘッダの取り込み
// --------------------------------------------------
#include "include/application_define.h"
#include "tasks/ble/include/rn4870.h"

// --------------------------------------------------
// 自ファイル内でのみ使用する#defineマクロ
// --------------------------------------------------
#define MAX_CMD_BUF_SIZE 256 /**< 共通で使用する送信用コマンドのバッファサイズ */

#define MAX_READ_CMD_BUF_SIZE   256 /**< BLEモジュールから受信するコマンドのバッファサイズ */
#define MAX_READ_EVENT_BUF_SIZE 256 /**< BLEモジュールから受信するイベントのバッファサイズ */

#define RN4870_RESET_DELAY   3   /**< リセット後の待機時間[ms] */
#define RN4870_STARTUP_DELAY 300 /**< 起動までの待機時間[ms] */

#define RN4870_ENTER_CMD_MODE_DELAY 1300 /**< CMDモードの遅延時間[ms] */ // NOTE: 1秒以内に"$$$"すべてを送信するとRN4870はこの操作を無視するため

#define EVENT_QUEUE_SIZE   3 /**< イベントキューサイズ */
#define SEND_QUEUE_SIZE    1 /**< 送信キューサイズ */
#define RECEIVE_QUEUE_SIZE 1 /**< 受信キューサイズ */

/* -------------------------------------------------- */

#define RN4870_CMD_END                    "\r" /**< コマンド、結果の終了文字 */
#define RN4870_CMD_DEFAULT_SUCCEED_STRING "AOK" /**< 標準の成功時に返却される文字列 */
#define RN4870_CMD_DEFAULT_FAILED_STRING  "ERR" /**< 標準の失敗時に返却される文字列 */

// --------------------------------------------------
// 自ファイル内でのみ使用する#define関数マクロ
// --------------------------------------------------

// --------------------------------------------------
// 自ファイル内でのみ使用するtypedef定義
// --------------------------------------------------
typedef BLEResult_t (*BLE_JUDGE_RESULT_CB)(uint8_t *pucMessage); /**< 成否判定を行うコールバック関数型 */

// --------------------------------------------------
// 自ファイル内でのみ使用するenumタグ定義（typedefを同時に行う）
// --------------------------------------------------

// --------------------------------------------------
// 自ファイル内でのみ使用するstruct/unionタグ定義（typedefを同時に行う）
// --------------------------------------------------
/**
 * @brief 受信キュー
 */
typedef struct
{
    uint8_t uxIndex;     /**< 一致した期待文字列のインデックス */
    uint8_t uxData[256]; /**< 受信文字 */
} RN4870ReceiveQueueData_t;

/**
 * @brief 送信キュー
 */
typedef struct
{
    uint8_t uxCmd[256]; /**< コマンド文字列 */
    uint16_t usDelay;   /**< 遅延時間 */
    uint8_t uxEnd[2];   /**< 終了文字 */
} RN4870SendQueueData_t;

// --------------------------------------------------
// ファイル内で共有するstatic変数宣言
// --------------------------------------------------
static uint8_t gucCmd[MAX_CMD_BUF_SIZE] = {0}; /**< BLEモジュールに送信するコマンドの共通バッファ */

static BLEInterface_t *gpxInterfaceRN4870;
static BLEEventCallback_t *gpxEventCbRN4870;
static uint8_t guxExpectEndStr[2][256]; /**< 期待する終了文字 */

WvCbList_t *gpxTopWvCbList = NULL; /**< 書き込みコールバックリストの先頭アドレス */
WvCbList_t *gpxEndWvCbList = NULL; /**< 書き込みコールバックリストの終端アドレス */

static QueueHandle_t gxReceiveQueueHandle = NULL; /**< 受信キューハンドル */
static QueueHandle_t gxSendQueueHandle = NULL;    /**< 送信キューハンドル */
static QueueHandle_t gxEventQueueHandle = NULL;   /**< イベントキューハンドル */

static BLECharacteristic_t gxCharacteristicInfo[MAX_CHARACTERISTIC_NUM]; /**< 各UUIDのハンドル値を保持 */

// --------------------------------------------------
// static関数プロトタイプ宣言
// --------------------------------------------------
/**
 * @brief コマンド送信
 *
 * @details コマンドに終了文字の追加や送信の遅延などを行う.
 *          遅延時間などは一定時間内にコマンドを送信しきるとモジュール側に無視されるような場合に使用する.
 *
 * @param [in] puxCmd  コマンド文字列
 * @param [in] usDelay 遅延時間
 * @param [in] puxEnd  終了文字
 *
 * @return BLEResult_t 結果
 */
static BLEResult_t prvSendCMD(uint8_t *puxCmd, uint16_t usDelay, uint8_t *puxEnd);

/**
 * @brief メッセージ受信
 *
 * @param [out]     pucMessage     メッセージバッファ
 * @param [in, out] xSize          メッセージサイズ
 * @param [in]      ulTimeout      受信タイムアウト ms
 * @param [in]      xJudgeResultCb 結果判定コールバック関数
 *
 * @return BLEResult_t 結果
 */
static BLEResult_t prvReceiveMessage(uint8_t *pucMessage, size_t *pxSize, uint32_t ulTimeout, BLE_JUDGE_RESULT_CB xJudgeResultCb);

/* -------------------------------------------------- */

/**
 * @brief メッセージ送受信
 *
 * @param [in]      pucCmd         コマンド文字列
 * @param [in]      usSendDelay    遅延時間
 * @param [in]      pucCmdEnd      終了文字
 * @param [in]      pucExpectStr   期待する文字列
 * @param [out]     pucMessage     受信メッセージ
 * @param [in, out] pxMessageSize  受信メッセージサイズ
 * @param [in]      ulTimeout      受信タイムアウト ms
 * @param [in]      xJudgeResultCb 結果判定コールバック関数
 *
 * @return BLEResult_t 結果
 */
static BLEResult_t prvSendAndReceive(uint8_t *pucCmd, uint16_t usSendDelay, uint8_t *pucCmdEnd, uint8_t *pucExpectStr,
                                     uint8_t *pucMessage, size_t *pxMessageSize, uint32_t ulTimeout,
                                     BLE_JUDGE_RESULT_CB xJudgeResultCb);

/* -------------------------------------------------- */

/**
 * @brief 通常の成否判定コールバック関数
 *
 * @param [in] pucMessage メッセージ
 *
 * @return BLEResult_t 成否
 */
static BLEResult_t prvNormalJudgeResultCb(uint8_t *pucMessage);

/* -------------------------------------------------- */

/**
 * @brief UUIDv4をハイフン無に変換
 *
 * @param [in]  pucUUID    ハイフンありUUID
 * @param [out] pucNewUUID ハイフンなしUUID
 * @param [in]  xSize      バッファサイズ
 *
 * @return bool 結果
 */
static bool prvUUIDWithoutHyphen(uint8_t *pucUUID, uint8_t *pucNewUUID, size_t xSize);

/**
 * @brief 16進文字列から数値に変換
 *
 * @param [in] pcHex 16進文字列
 *
 * @return uint32_t 変換後数値
 */
static uint32_t ulHexToInt(uint8_t *pcHex);

/**
 * @brief 文字列を大文字に変換
 *
 * @param [in, out] pcString 文字列
 */
static void vUpper(uint8_t *pcString);

/**
 * @brief 文字列を小文字に変換
 *
 * @param [in, out] pcString 文字列
 */
static void vLower(uint8_t *pcString);

/* -------------------------------------------------- */

/**
 * @brief 書き込み時コールバックリストに追加
 *
 * @param [in] pxData コールバック関数など
 *
 * @return BLEResult_t 結果
 */
static BLEResult_t prvPushWvCbList(WvCbList_t *pxData);

/**
 * @brief 書き込み時コールバックリストから指定のCharacteristicを削除
 *
 * @param [in] puxUUID 削除するCharacteristic UUID
 *
 * @return BLEResult_t 結果
 */
static BLEResult_t prvDeleteWvCbList(uint8_t *puxUUID);

/* -------------------------------------------------- */

/**
 * @brief 文字列(16進数)からバイト列に変換
 *
 * @param [out] puxData   バイト列
 * @param [in]  puxString 16進数文字列
 *
 * @return int 変換バイト数
 */
static int prvBytesFromHexString(uint8_t *puxData, uint8_t *puxString);

/**
 * @brief コマンド実行時に期待する終了文字を登録
 *
 * @note RN4870はコマンド実行後に"CMD>"という文字列が返される.
 *       この文字列を受信したをもってコマンドが実行されたことを確認する.
 *
 * @param [in] puxStr  期待する終了文字
 * @param [in] uxIndex インデックス
 */
static void prvRegisterExpectEndStr(uint8_t *puxStr, uint8_t uxIndex);

/**
 * @brief 登録している期待する終了文字をすべて削除
 */
static void prvAllDeleteExpectEndStr();

/**
 * @brief イベントコールバック
 *
 * @param [in] eType      受信イベントタイプ
 * @param [in] puxMessage 受信メッセージ
 */
static void prvEventCb(BLEEventType_t eType, uint8_t *puxMessage);

/**
 * @brief 接続パラメーターイベントのデコード
 *
 * @param [in] puxMessage   受信メッセージ
 * @param [out] puxInterval インターバル
 * @param [out] puxLatency  レイテンシー
 * @param [out] puxTimeout  タイムアウト
 *
 * @return BLEResult_t 結果
 */
static BLEResult_t prvPraseEventCONN_PARAM(uint8_t *puxMessage, uint16_t *puxInterval, uint16_t *puxLatency, uint16_t *puxTimeout);

/**
 * @brief 接続イベントのデコード
 *
 * @param puxMessage   受信メッセージ
 * @param puxNum       接続数
 * @param puxAddress   アドレス
 * @param xAddressSize アドレスサイズ
 *
 * @return BLEResult_t 結果
 */
static BLEResult_t prvParseEventCONNECT(uint8_t *puxMessage, uint8_t *puxNum, uint8_t *puxAddress, size_t xAddressSize);

/**
 * @brief 書き込みイベントのデコード
 *
 * @param puxMessage 受信メッセージ
 * @param puxHandle  ハンドル
 * @param puxData    データ
 * @param pxDataSize データサイズ
 *
 * @return BLEResult_t 結果
 */
static BLEResult_t prvPraseEventWV(uint8_t *puxMessage, uint16_t *puxHandle, uint8_t *puxData, size_t *pxDataSize);

// --------------------------------------------------
// 変数定義（staticを除く）
// --------------------------------------------------

// --------------------------------------------------
// 関数定義（staticを除く）
// --------------------------------------------------
void vInitializeBLE(BLEInterface_t *pxInterface, BLEEventCallback_t *pxEventCb)
{
    gpxInterfaceRN4870 = pxInterface;
    gpxEventCbRN4870 = pxEventCb;
}

BLEResult_t eRegisterBLEEventCb(BLE_EVENT_CB xCbFunc, BLEEventType_t eType, uint8_t *puxCharaUUID)
{
    WvCbList_t xTmpWvCb;
    memset(&xTmpWvCb, 0x00, sizeof(xTmpWvCb));

    if (eType == BLE_EVENT_CB_TYPE_WV && puxCharaUUID == NULL)
    {
        return BLE_RESULT_BAD_PARAMETER;
    }

    switch (eType)
    {
    case BLE_EVENT_CB_TYPE_CONN_PARAM:
        gpxEventCbRN4870->conn_param = xCbFunc;
        break;
    case BLE_EVENT_CB_TYPE_CONNECT:
        gpxEventCbRN4870->connect = xCbFunc;
        break;
    case BLE_EVENT_CB_TYPE_DISCONNECT:
        gpxEventCbRN4870->disconnect = xCbFunc;
        break;
    case BLE_EVENT_CB_TYPE_SECURED:
        gpxEventCbRN4870->secured = xCbFunc;
        break;
    case BLE_EVENT_CB_TYPE_WV:
        xTmpWvCb.cb = xCbFunc;
        memcpy(xTmpWvCb.uxUUID, puxCharaUUID, strlen((const char *)puxCharaUUID));
        if (prvPushWvCbList(&xTmpWvCb) != BLE_RESULT_SUCCEED)
        {
            return BLE_RESULT_FAILED;
        }

        break;
    default:
        break;
    }
    return BLE_RESULT_SUCCEED;
}

BLEResult_t eDeleteBLEEventCb(BLEEventType_t eType, uint8_t *puxCharaUUID)
{
    if (eType == BLE_EVENT_CB_TYPE_WV && puxCharaUUID == NULL)
    {
        return BLE_RESULT_BAD_PARAMETER;
    }

    switch (eType)
    {
    case BLE_EVENT_CB_TYPE_CONN_PARAM:
        gpxEventCbRN4870->conn_param = NULL;
        break;
    case BLE_EVENT_CB_TYPE_CONNECT:
        gpxEventCbRN4870->connect = NULL;
        break;
    case BLE_EVENT_CB_TYPE_DISCONNECT:
        gpxEventCbRN4870->disconnect = NULL;
        break;
    case BLE_EVENT_CB_TYPE_SECURED:
        gpxEventCbRN4870->secured = NULL;
        break;
    case BLE_EVENT_CB_TYPE_WV:
        prvDeleteWvCbList(puxCharaUUID);
        break;
    default:
        break;
    }
    return BLE_RESULT_SUCCEED;
}

void vHardResetBLE()
{
    gpxInterfaceRN4870->gpio_off();
    gpxInterfaceRN4870->delay(RN4870_RESET_DELAY);
    gpxInterfaceRN4870->gpio_on();
    gpxInterfaceRN4870->delay(RN4870_STARTUP_DELAY);
}

BLEResult_t eEnterCMDMode()
{
    return prvSendAndReceive((uint8_t *)ENTER_CMD, RN4870_ENTER_CMD_MODE_DELAY, NULL, (uint8_t *)"CMD>", NULL, 0, 0, NULL);
}

BLEResult_t eExitCMDMode()
{
    return prvSendAndReceive((uint8_t *)EXIT_CMD, 0, (uint8_t *)RN4870_CMD_END, (uint8_t *)"END", NULL, 0, 0, NULL);
}

BLEResult_t eStartAdvertisement(uint16_t uxInterval, uint16_t uxTotal)
{
    memset(gucCmd, 0x00, sizeof(gucCmd));
    snprintf((char *)gucCmd, sizeof(gucCmd), "%s,%04X,%04X", START_ADVERTISEMENT, uxInterval, uxTotal);

    return prvSendAndReceive(gucCmd, 0, (uint8_t *)RN4870_CMD_END, (uint8_t *)"CMD>", NULL, 0, 0, prvNormalJudgeResultCb);
}

BLEResult_t eStartBondingProcess()
{
    return prvSendAndReceive((uint8_t *)START_BONDING_PROCESS, 0, (uint8_t *)RN4870_CMD_END, (uint8_t *)"CMD>", NULL, 0, 0, prvNormalJudgeResultCb);
}

BLEResult_t eGetDeviceInfo()
{
    uint8_t ucDeviceInfo[256] = {0};
    size_t xDeviceInfoLength = sizeof(ucDeviceInfo);
    memset(ucDeviceInfo, 0x00, sizeof(ucDeviceInfo));

    prvSendAndReceive((uint8_t *)DISPLAY_DEVICE_INFO_CMD, 0, (uint8_t *)RN4870_CMD_END, (uint8_t *)"CMD>", ucDeviceInfo, &xDeviceInfoLength, 0, NULL);
    APP_PRINTFInfo("Device info:\r\n%s", ucDeviceInfo);
    return BLE_RESULT_SUCCEED;
}

BLEResult_t eReboot()
{
    return prvSendAndReceive((uint8_t *)REBOOT, 0, (uint8_t *)RN4870_CMD_END, (uint8_t *)"Rebooting", NULL, 0, 0, NULL);
}

BLEResult_t eRemoveBonding(uint8_t uxIndex)
{
    memset(gucCmd, 0x00, sizeof(gucCmd));
    snprintf((char *)gucCmd, sizeof(gucCmd) - 1, "%s,%d", UNBOND, uxIndex);

    return prvSendAndReceive(gucCmd, 0, (uint8_t *)RN4870_CMD_END, (uint8_t *)"CMD>", NULL, 0, 0, prvNormalJudgeResultCb);
}

BLEResult_t eGetFWVersion()
{
    uint8_t ucFWVersion[64] = {0};
    size_t xFWVersionLength = sizeof(ucFWVersion);
    memset(ucFWVersion, 0x00, sizeof(ucFWVersion));

    prvSendAndReceive((uint8_t *)DISPLAY_FW_VERSION, 0, (uint8_t *)RN4870_CMD_END, (uint8_t *)"CMD>", ucFWVersion, &xFWVersionLength, 0, NULL);
    APP_PRINTFInfo("FW version:\r\n%s", ucFWVersion);
    return BLE_RESULT_SUCCEED;
}

BLEResult_t eStopAdvertisement()
{
    return prvSendAndReceive((uint8_t *)STOP_ADVERTISEMENT, 0, (uint8_t *)RN4870_CMD_END, (uint8_t *)"CMD>", NULL, 0, 0, prvNormalJudgeResultCb);
}

BLEResult_t eSetSerializedBluetoothName(uint8_t *pucName)
{
    if (strlen((const char *)pucName) > 16)
    {
        APP_PRINTFError("under 15");
        return BLE_RESULT_FAILED;
    }

    memset(gucCmd, 0x00, sizeof(gucCmd));
    snprintf((char *)gucCmd, sizeof(gucCmd), "%s,%s", SET_SERIALIZED_BLUETOOTH_NAME, pucName);

    return prvSendAndReceive(gucCmd, 0, (uint8_t *)RN4870_CMD_END, (uint8_t *)"CMD>", NULL, 0, 0, prvNormalJudgeResultCb);
}

BLEResult_t eSetPairingMode(uint8_t uxMode)
{
    memset(gucCmd, 0x00, sizeof(gucCmd));
    snprintf((char *)gucCmd, sizeof(gucCmd), "%s,%d", SET_PAIRING_MODE, uxMode);

    return prvSendAndReceive(gucCmd, 0, (uint8_t *)RN4870_CMD_END, (uint8_t *)"CMD>", NULL, 0, 0, prvNormalJudgeResultCb);
}

BLEResult_t eSetDeviceName(uint8_t *pucName)
{
    if (strlen((const char *)pucName) > 21)
    {
        APP_PRINTFError("under 20");
        return BLE_RESULT_BAD_PARAMETER;
    }

    memset(gucCmd, 0x00, sizeof(gucCmd));
    snprintf((char *)gucCmd, sizeof(gucCmd), "%s,%s", SET_DEVICE_NAME, pucName);

    return prvSendAndReceive(gucCmd, 0, (uint8_t *)RN4870_CMD_END, (uint8_t *)"CMD>", NULL, 0, 0, prvNormalJudgeResultCb);
}

BLEResult_t eSetFixPIN(uint8_t *puxPIN)
{
    if (strlen((const char *)puxPIN) > 6)
    {
        return BLE_RESULT_BAD_PARAMETER;
    }

    memset(gucCmd, 0x00, sizeof(gucCmd));
    snprintf((char *)gucCmd, sizeof(gucCmd), "%s,%s", SET_FIX_PIN, puxPIN);

    return prvSendAndReceive(gucCmd, 0, (uint8_t *)RN4870_CMD_END, (uint8_t *)"CMD>", NULL, 0, 0, prvNormalJudgeResultCb);
}

#if 0
BLEResult_t sSetFeature(uint16_t usFeature)
{
    return BLE_RESULT_NOT_IMPLEMENTED;
    memset(gucCmd, 0x00, sizeof(gucCmd));
    snprintf((char *)gucCmd, sizeof(gucCmd), "%s,%04X", SET_FEATURE, usFeature);

    return prvSendAndReceive(gucCmd, 0, (uint8_t *)RN4870_CMD_END, (uint8_t *)"CMD>", NULL, 0, 0, prvNormalJudgeResultCb);
}
#endif

BLEResult_t eListBondedDevices(uint8_t *pucMessage, size_t xSize)
{
    // ex) 1,20C19B81CD05,0
    // index, address ,address type
    uint8_t ucList[256] = {0};
    size_t xListLength = sizeof(ucList);
    memset(ucList, 0x00, sizeof(ucList));

    prvSendAndReceive((uint8_t *)LIST_BONDED_DEVICE, 0, (uint8_t *)RN4870_CMD_END, (uint8_t *)"CMD>", ucList, &xListLength, 0, NULL);
    if (strlen((const char *)ucList) + 1 > xSize)
    {
        return BLE_RESULT_BAD_PARAMETER;
    }
    memcpy(pucMessage, ucList, strlen((const char *)ucList));
    return BLE_RESULT_SUCCEED;
}

BLEResult_t eListServiceCharacteristic(uint8_t *pucServiceUUID, uint8_t *pucMessage, size_t *pxSize)
{
    // ex) BEB5483E36E14688B7F5EA07361B26A8,0072,02
    // uuid, handle, property
    // handle: 全attributeに与えられる固有の16bit識別子
    uint8_t ucList[256] = {0};
    size_t xListLength = sizeof(ucList);
    memset(ucList, 0x00, sizeof(ucList));
    if (pucServiceUUID != NULL)
        snprintf((char *)gucCmd, sizeof(gucCmd), "%s,%s", LIST_SERVICE_CHARACTERISTIC, pucServiceUUID);
    else
        snprintf((char *)gucCmd, sizeof(gucCmd), "%s", LIST_SERVICE_CHARACTERISTIC);

    prvSendAndReceive(gucCmd, 0, (uint8_t *)RN4870_CMD_END, (uint8_t *)"CMD>", ucList, &xListLength, 0, NULL);
    if (strlen((const char *)ucList) + 1 > *pxSize)
    {
        return BLE_RESULT_BAD_PARAMETER;
    }
    memcpy(pucMessage, ucList, strlen((const char *)ucList));
    return BLE_RESULT_SUCCEED;
}

BLEResult_t eSetUUIDCharacteristic(uint8_t *pucUUID, uint8_t uxProperty, uint8_t uxDataSize)
{
    memset(gucCmd, 0x00, sizeof(gucCmd));
    snprintf((char *)gucCmd, sizeof(gucCmd), "%s,%s,%02X,%02X", SET_UUID_CHARACTERISTIC, pucUUID, uxProperty, uxDataSize);

    return prvSendAndReceive(gucCmd, 0, (uint8_t *)RN4870_CMD_END, (uint8_t *)"CMD>", NULL, 0, 0, prvNormalJudgeResultCb);
}

BLEResult_t eSetUUIDService(uint8_t *pucUUID)
{
    memset(gucCmd, 0x00, sizeof(gucCmd));
    snprintf((char *)gucCmd, sizeof(gucCmd), "%s,%s", SET_UUID_SERVICE, pucUUID);

    return prvSendAndReceive(gucCmd, 0, (uint8_t *)RN4870_CMD_END, (uint8_t *)"CMD>", NULL, 0, 0, prvNormalJudgeResultCb);
}

BLEResult_t eClearAllService()
{
    memset(gucCmd, 0x00, sizeof(gucCmd));
    snprintf((char *)gucCmd, sizeof(gucCmd), "%s", CLEAR_ALL_SERVICE);

    return prvSendAndReceive(gucCmd, 0, (uint8_t *)RN4870_CMD_END, (uint8_t *)"CMD>", NULL, 0, 0, prvNormalJudgeResultCb);
}

BLEResult_t eReadLocalCharacteristicValue(uint16_t usHandle, uint8_t *puxValue, size_t *pxSize)
{
    memset(gucCmd, 0x00, sizeof(gucCmd));
    snprintf((char *)gucCmd, sizeof(gucCmd), "%s,%04X", READ_LOCAL_CHARACTERISTIC_VALUE, usHandle);

    BLEResult_t xResult = prvSendAndReceive(gucCmd, 0, (uint8_t *)RN4870_CMD_END, (uint8_t *)"CMD>", puxValue, pxSize, 0, NULL);
    APP_PRINTFDebug("%s", puxValue);
    return xResult;
}

BLEResult_t eWriteLocalCharacteristicValue(uint16_t usHandle, uint8_t *puxValue, size_t xSize)
{
    size_t xCmdSize = xSize * 2 + strlen((const char *)WRITE_LOCAL_CHARACTERISTIC_VALUE) + 4 + 2 + 1;
    uint8_t *pucCmd = pvPortMalloc(xCmdSize);
    if (pucCmd == NULL)
    {
        return BLE_RESULT_FAILED;
    }
    memset(pucCmd, 0x00, xCmdSize);
    snprintf((char *)pucCmd, xCmdSize, "%s,%04X,", WRITE_LOCAL_CHARACTERISTIC_VALUE, usHandle);
    for (int i = 0; i < xSize; i++)
    {
        snprintf((char *)pucCmd + strlen((const char *)WRITE_LOCAL_CHARACTERISTIC_VALUE) + 4 + 2 + (i * 2),
                 xCmdSize - (strlen((const char *)WRITE_LOCAL_CHARACTERISTIC_VALUE) + 4 + 2 - (i * 2)),
                 "%02X", puxValue[i]);
    }

    BLEResult_t xResult = prvSendAndReceive(pucCmd, 0, (uint8_t *)RN4870_CMD_END, (uint8_t *)"CMD>", NULL, 0, 0, prvNormalJudgeResultCb);
    vPortFree(pucCmd);
    return xResult;
}

/* -------------------------------------------------- */

uint16_t usGetHandleByUUID(uint8_t *pucServiceUUID, uint8_t *pucCharaUUID) // TODO: サービスを指定しない場合も対応させる
{
    uint16_t usHandle = 0;
    uint8_t uxCharaUUID[BLE_UUID_STR_LENGTH + 1] = {0};
    memset(uxCharaUUID, 0x00, sizeof(uxCharaUUID));

    // 指定されたCharacteristic UUIDと一致するHandle値を探す
    memcpy(uxCharaUUID, pucCharaUUID, strlen((const char *)pucCharaUUID));
    vUpper(uxCharaUUID);
    for (uint8_t c = 0; c < MAX_CHARACTERISTIC_NUM; c++)
    {
        if (strcmp((const char *)gxCharacteristicInfo[c].uxUUID, (const char *)uxCharaUUID) == 0)
        {
            usHandle = gxCharacteristicInfo[c].usHandle;
            return usHandle;
        }
    }
    return usHandle;
}

BLEResult_t eUpdateHandleInfo(uint8_t *pucServiceUUID) // TODO: サービスを指定しない場合も対応させる
{
    uint8_t uxListString[256] = {0};
    size_t xListStringLength = sizeof(uxListString);
    eListServiceCharacteristic(NULL, uxListString, &xListStringLength);

    memset(gxCharacteristicInfo, 0x00, sizeof(gxCharacteristicInfo));

    // 取得したCharacteristic情報文字列を解析
    eParseLs(uxListString, gxCharacteristicInfo);

    return BLE_RESULT_SUCCEED;
}

BLEResult_t eParseLs(uint8_t *pucMessage, BLECharacteristic_t *pxCharaList)
{
    uint8_t *puxLinePos = (uint8_t *)strchr((const char *)pucMessage, (int)'\r') + 1;
    for (uint8_t c = 0; c < MAX_CHARACTERISTIC_NUM; c++)
    {
        uint8_t *puxNextLinePos = (uint8_t *)strchr((const char *)puxLinePos, (int)'\r') + 1;

        uint8_t uxLine[2 + BLE_UUID_STR_LENGTH + 2 + 6 + 2 + 1] = {0}; // space + UUID +  comma + handle + property + \r\n
        memset(uxLine, 0x00, sizeof(uxLine));
        memcpy(uxLine, puxLinePos, puxNextLinePos - puxLinePos);

        puxLinePos = puxNextLinePos;

        // ENDのとき(終了)
        if (strstr((const char *)uxLine, "END") != NULL)
        {
            break;
        }

        uint8_t uxIndex = 0;
        uint8_t *puxValuePos = (uint8_t *)strtok((char *)uxLine, (const char *)",");

        // puxValuePos: \r\n  XXX...
        // UUIDの前ににスペースが2個ある
        char *tmp_uuid = strstr((const char *)puxValuePos, "  ");

        memcpy(pxCharaList[c].uxUUID, tmp_uuid + 2, BLE_UUID_STR_LENGTH); // スペース除外
        uxIndex++;

        while (puxValuePos != NULL)
        {
            puxValuePos = (uint8_t *)strtok(NULL, (const char *)",");
            switch (uxIndex)
            {
            case 1: // handle
                pxCharaList[c].usHandle = (uint16_t)ulHexToInt(puxValuePos);
                break;
            case 2: // property
                pxCharaList[c].uxProperty = (uint16_t)ulHexToInt(puxValuePos);
                break;
            default:
                break;
            }
            uxIndex++;
        }
    }
    return BLE_RESULT_SUCCEED;
}

uint8_t uxParseLb(uint8_t *pucMessage, BLEBonding_t *pxBondingList)
{
    uint8_t *puxLinePos = pucMessage;
    uint8_t uxB = 0;
    for (uxB = 0; uxB < MAX_BONDING_NUM; uxB++)
    {
        uint8_t *puxNextLinePos = (uint8_t *)strchr((const char *)puxLinePos, (int)'\r') + 1;

        uint8_t uxLine[1 + 12 + 1 + 2] = {0}; // index + mac address + type + comma
        memset(uxLine, 0x00, sizeof(uxLine));
        memcpy(uxLine, puxLinePos, puxNextLinePos - puxLinePos);

        puxLinePos = puxNextLinePos;

        // ENDのとき
        if (strstr((const char *)uxLine, "END") != NULL)
        {
            break;
        }

        uint8_t uxIndex = 0;
        uint8_t *puxValuePos = (uint8_t *)strtok((char *)uxLine, (const char *)",");

        // index
        pxBondingList[uxB].uxIndex = strtol((const char *)puxValuePos, NULL, 10);

        while (puxValuePos != NULL)
        {
            puxValuePos = (uint8_t *)strtok(NULL, (const char *)",");
            switch (uxIndex)
            {
            case 0: // address
                prvBytesFromHexString(pxBondingList[uxB].uxAddress, puxValuePos);
                break;
            case 1: // address type
                pxBondingList[uxB].uxAddressType = strtol((const char *)puxValuePos, NULL, 10);
                break;
            default:
                break;
            }
            uxIndex++;
        }
    }
    return uxB;
}

/* -------------------------------------------------- */

void vEventLoop(void *pvParameters)
{
    (void)pvParameters;

    BLEEventLoopState_t eState = BLE_EVENT_LOOP_STATE_INIT;

    BaseType_t xResult;
    BLEEventQueue_t xReceiveQueueData;

    while (1)
    {
        switch (eState)
        {
        case BLE_EVENT_LOOP_STATE_INIT:
            gxEventQueueHandle = xQueueCreate(EVENT_QUEUE_SIZE, sizeof(BLEEventQueue_t));
            eState = BLE_EVENT_LOOP_STATE_LOOP;
            break;
        case BLE_EVENT_LOOP_STATE_LOOP:
            memset(&xReceiveQueueData, 0x00, sizeof(xReceiveQueueData));
            xResult = xQueueReceive(gxEventQueueHandle, &xReceiveQueueData, portMAX_DELAY);
            if (xResult != pdTRUE)
            {
                // printf("error!!!\r\n");
                continue;
            }

            // イベントに応じてコールバック関数実行
            prvEventCb(xReceiveQueueData.eCbType, xReceiveQueueData.uxEventString);
            PRINT_TASK_REMAINING_STACK_SIZE();
            break;
        default:
            break;
        }
    }
}

void vSendLoop(void *pvParameters)
{
    (void)pvParameters;

    BaseType_t xResult;
    RN4870SendQueueData_t uxSendTmp;
    static BLESendLoopState_t eState = BLE_SEND_LOOP_STATE_INIT;

    size_t xWriteSize = 0;
    size_t xCmdLength = 0;

    while (1)
    {
        switch (eState)
        {
        case BLE_SEND_LOOP_STATE_INIT:
            // キュー作成
            gxSendQueueHandle = xQueueCreate(SEND_QUEUE_SIZE, sizeof(RN4870SendQueueData_t));
            eState = BLE_SEND_LOOP_STATE_LOOP;
            break;
        case BLE_SEND_LOOP_STATE_LOOP:
            // Queue受信
            memset(&uxSendTmp, 0x00, sizeof(uxSendTmp));
            xResult = xQueueReceive(gxSendQueueHandle, &uxSendTmp, portMAX_DELAY);
            if (xResult != pdTRUE)
                continue;

            // UART送信
            xCmdLength = strlen((const char *)uxSendTmp.uxCmd);
            xWriteSize = gpxInterfaceRN4870->uart_tx(uxSendTmp.uxCmd, xCmdLength - 1);
            if (xWriteSize != xCmdLength - 1)
            {
                continue;
            }
            if (uxSendTmp.usDelay != 0)
                gpxInterfaceRN4870->delay(uxSendTmp.usDelay);
            xWriteSize = gpxInterfaceRN4870->uart_tx(uxSendTmp.uxCmd + xCmdLength - 1, 1);
            if (xWriteSize != 1)
            {
                continue;
            }
            if (uxSendTmp.uxEnd != NULL)
            {
                xWriteSize = gpxInterfaceRN4870->uart_tx(uxSendTmp.uxEnd, strlen((const char *)uxSendTmp.uxEnd));
                if (xWriteSize != strlen((const char *)uxSendTmp.uxEnd))
                {
                    continue;
                }
            }
            PRINT_TASK_REMAINING_STACK_SIZE();
            break;
        default:
            break;
        }
    }
}

void vInterfaceLoop(void *pvParameters)
{
    (void)pvParameters;

    // イベントメッセージを格納するバッファ
    static uint8_t uxReadEventBuf[MAX_READ_EVENT_BUF_SIZE] = {0};
    memset(uxReadEventBuf, 0x00, sizeof(uxReadEventBuf));
    static uint32_t ulCountEventBuf = 0;

    // コマンド応答メッセージを格納するバッファ
    static uint8_t uxReadCommandBuf[MAX_READ_CMD_BUF_SIZE] = {0};
    memset(uxReadCommandBuf, 0x00, sizeof(uxReadCommandBuf));
    static uint32_t ulCountCommandBuf = 0;

    static BLEInterfaceLoopState_t eState = BLE_IF_LOOP_STATE_INIT;

    uint8_t *pxEventDelimiterPos = NULL;
    uint8_t uxEventTypeBuf[256] = {0};
    memset(uxEventTypeBuf, 0x00, sizeof(uxEventTypeBuf));

    while (1)
    {
        uint8_t uxTmp = 0;

        switch (eState)
        {
        case BLE_IF_LOOP_STATE_INIT:
            memset(guxExpectEndStr, 0x00, sizeof(guxExpectEndStr));

            // キュー作成
            gxReceiveQueueHandle = xQueueCreate(RECEIVE_QUEUE_SIZE, sizeof(RN4870ReceiveQueueData_t));

            eState = BLE_IF_LOOP_STATE_CMD_RECEIVING;
            break;
        case BLE_IF_LOOP_STATE_CMD_RECEIVING:
            if (gpxInterfaceRN4870->uart_rx(&uxTmp, 1) != 1)
            {
                gpxInterfaceRN4870->delay(30);
                break;
            }

            if (uxTmp == '%') // イベントの受信
            {
                eState = BLE_IF_LOOP_STATE_EVENT_RECEIVING;
            }
            else // コマンドの受信
            {
                uxReadCommandBuf[ulCountCommandBuf++] = uxTmp;

                // 期待する終了文字があるか確認(AOK, ERR, END, CMD>...)
                for (int c = 0; c < sizeof(guxExpectEndStr) / sizeof(guxExpectEndStr[0]); c++)
                {
                    size_t xExpectEndStrSize = strlen((const char *)guxExpectEndStr[c]);
                    if (xExpectEndStrSize == 0)
                        continue;

                    if (memcmp(&uxReadCommandBuf[ulCountCommandBuf - xExpectEndStrSize], guxExpectEndStr[c], xExpectEndStrSize) == 0) // 最後の文字が期待する文字と一致するか？
                    {
                        RN4870ReceiveQueueData_t xData;
                        memset(&xData, 0x00, sizeof(xData));
                        xData.uxIndex = c;
                        memcpy(xData.uxData, uxReadCommandBuf, strlen((const char *)uxReadCommandBuf) - xExpectEndStrSize); // 終了文字を除いた結果を送信
                        xQueueSend(gxReceiveQueueHandle, &xData, portMAX_DELAY);

                        memset(uxReadCommandBuf, 0x00, sizeof(uxReadCommandBuf));
                        ulCountCommandBuf = 0;

                        prvAllDeleteExpectEndStr(); // 終了文字削除
                    }
                }
            }
            break;
        case BLE_IF_LOOP_STATE_EVENT_RECEIVING: // イベントメッセージがすべて受信されるまで
            if (gpxInterfaceRN4870->uart_rx(&uxTmp, 1) != 1)
            {
                gpxInterfaceRN4870->delay(30);
                break;
            }
            if (uxTmp == '%')
            {
                eState = BLE_IF_LOOP_STATE_EVENT_PARSE;
            }
            else
            {
                uxReadEventBuf[ulCountEventBuf++] = uxTmp;
            }
            break;
        case BLE_IF_LOOP_STATE_EVENT_PARSE:
            // イベント種別(CONNECT, WV...)を確認
            pxEventDelimiterPos = (uint8_t *)strchr((const char *)uxReadEventBuf, (int)',');

            uint8_t uxEventTypeBuf[24] = {0};
            memset(uxEventTypeBuf, 0x00, sizeof(uxEventTypeBuf));
            if (pxEventDelimiterPos == NULL) // イベントのみ(区切り文字がない)(DISCONNECTなど)) // TODO: KEYなども対応させる
            {
                memcpy(uxEventTypeBuf, uxReadEventBuf, strlen((const char *)uxReadEventBuf));
            }
            else
            {
                memcpy(uxEventTypeBuf, uxReadEventBuf, pxEventDelimiterPos - uxReadEventBuf);
            }

            BLEEventQueue_t xQueueData;
            memset(&xQueueData, 0x00, sizeof(xQueueData));
            memcpy(xQueueData.uxEventString, uxReadEventBuf, strlen((const char *)uxReadEventBuf));

            // コマンド種別毎にEventLoopタスクに通知
            if (memcmp(uxEventTypeBuf, STATUS_MESSAGE_CONN_PARAM, strlen(STATUS_MESSAGE_CONN_PARAM)) == 0)
            {
                xQueueData.eCbType = BLE_EVENT_CB_TYPE_CONN_PARAM;
            }
            else if (memcmp(uxEventTypeBuf, STATUS_MESSAGE_CONNECT, strlen(STATUS_MESSAGE_CONNECT)) == 0)
            {
                xQueueData.eCbType = BLE_EVENT_CB_TYPE_CONNECT;
            }
            else if (memcmp(uxEventTypeBuf, STATUS_MESSAGE_DISCONNECT, strlen(STATUS_MESSAGE_DISCONNECT)) == 0)
            {
                xQueueData.eCbType = BLE_EVENT_CB_TYPE_DISCONNECT;
            }
            else if (memcmp(uxEventTypeBuf, STATUS_MESSAGE_REBOOT, strlen(STATUS_MESSAGE_REBOOT)) == 0)
            {
                xQueueData.eCbType = BLE_EVENT_CB_TYPE_REBOOT;
            }
            else if (memcmp(uxEventTypeBuf, STATUS_MESSAGE_SECURED, strlen(STATUS_MESSAGE_SECURED)) == 0)
            {
                xQueueData.eCbType = BLE_EVENT_CB_TYPE_SECURED;
            }
            else if (memcmp(uxEventTypeBuf, STATUS_MESSAGE_WV, strlen(STATUS_MESSAGE_WV)) == 0)
            {
                xQueueData.eCbType = BLE_EVENT_CB_TYPE_WV;
            }
            else
            {
                xQueueData.eCbType = BLE_EVENT_CB_TYPE_UNKNOWN;
            }

            // キュー送信
            if (xQueueData.eCbType != BLE_EVENT_CB_TYPE_UNKNOWN)
            {
                xQueueSend(gxEventQueueHandle, &xQueueData, portMAX_DELAY);
            }
            /* -------------------------------------------------- */

            memset(uxReadEventBuf, 0x00, sizeof(uxReadEventBuf));
            ulCountEventBuf = 0;
            eState = BLE_IF_LOOP_STATE_CMD_RECEIVING;

            PRINT_TASK_REMAINING_STACK_SIZE();
            break;
        default:
            break;
        }
    }
}

// --------------------------------------------------
// static関数定義
// --------------------------------------------------
static BLEResult_t prvSendCMD(uint8_t *puxCmd, uint16_t usDelay, uint8_t *puxEnd)
{
    BaseType_t xResult;

    RN4870SendQueueData_t xSendQueueData;
    memset(&xSendQueueData, 0x00, sizeof(xSendQueueData));

    if (puxCmd == NULL)
    {
        return BLE_RESULT_BAD_PARAMETER;
    }

    memcpy(xSendQueueData.uxCmd, puxCmd, strlen((const char *)puxCmd));
    xSendQueueData.usDelay = usDelay;
    if (puxEnd != NULL)
        memcpy(xSendQueueData.uxEnd, puxEnd, strlen((const char *)puxEnd));

    xResult = xQueueSend(gxSendQueueHandle, &xSendQueueData, portMAX_DELAY);
    if (xResult != pdTRUE)
    {
        return BLE_RESULT_FAILED;
    }
    return BLE_RESULT_SUCCEED;
}

static BLEResult_t prvReceiveMessage(uint8_t *pucMessage, size_t *pxSize, uint32_t ulTimeout, BLE_JUDGE_RESULT_CB xJudgeResultCb)
{
    RN4870ReceiveQueueData_t xData;
    memset(&xData, 0x00, sizeof(xData));
    BaseType_t xQueueResult;
    BLEResult_t xResult = BLE_RESULT_FAILED;

    if (pucMessage != NULL && pxSize == 0)
    {
        return BLE_RESULT_BAD_PARAMETER;
    }

    if (ulTimeout == 0)
    {
        ulTimeout = portMAX_DELAY;
    }

    xQueueResult = xQueueReceive(gxReceiveQueueHandle, &xData, (TickType_t)ulTimeout);
    if (xQueueResult != pdTRUE)
    {
        return BLE_RESULT_TIMEOUT;
    }

    if (pucMessage != NULL)
    {
        memcpy(pucMessage, xData.uxData, strlen((const char *)xData.uxData));
    }

    if (xJudgeResultCb != NULL)
    {
        xResult = xJudgeResultCb(xData.uxData); // メッセージ内容から結果判定
    }
    else
    {
        xResult = BLE_RESULT_SUCCEED;
    }

    return xResult;
}

/* -------------------------------------------------- */

static BLEResult_t prvSendAndReceive(uint8_t *pucCmd, uint16_t usSendDelay, uint8_t *pucCmdEnd, uint8_t *pucExpectStr,
                                     uint8_t *pucMessage, size_t *pxMessageSize, uint32_t ulTimeout,
                                     BLE_JUDGE_RESULT_CB xJudgeResultCb)
{
    prvRegisterExpectEndStr(pucExpectStr, 0);
    prvSendCMD(pucCmd, usSendDelay, pucCmdEnd);
    return prvReceiveMessage(pucMessage, pxMessageSize, ulTimeout, xJudgeResultCb);
}

/* -------------------------------------------------- */

static BLEResult_t prvNormalJudgeResultCb(uint8_t *pucMessage)
{
    BLEResult_t xResult = BLE_RESULT_FAILED;

    if (strstr((const char *)pucMessage, RN4870_CMD_DEFAULT_SUCCEED_STRING) != NULL)
    {
        xResult = BLE_RESULT_SUCCEED;
    }
    else if (strstr((const char *)pucMessage, RN4870_CMD_DEFAULT_FAILED_STRING) != NULL)
    {
        xResult = BLE_RESULT_FAILED;
    }
    else
    {
        xResult = BLE_RESULT_FAILED;
    }

    return xResult;
}

/* -------------------------------------------------- */

static bool prvUUIDWithoutHyphen(uint8_t *pucUUID, uint8_t *pucNewUUID, size_t xSize)
{
    if (xSize < BLE_UUID_STR_LENGTH + 1)
    {
        APP_PRINTFError("Not enough size.");
        return false;
    }

    uint32_t index = 0;
    for (int i = 0; i < strlen((const char *)pucUUID); i++)
    {
        if (*(pucUUID + i) == '-')
            continue;
        *(pucNewUUID + index) = *(pucUUID + i);
        index++;
    }
    return true;
}

static uint32_t ulHexToInt(uint8_t *pcHex)
{
    uint16_t r = strtol((const char *)pcHex, NULL, 16);
    return r;
}

static void vUpper(uint8_t *pcString)
{
    for (int i = 0; i < strlen((const char *)pcString); i++)
    {
        if (*(pcString + i) >= 'a' && *(pcString + i) <= 'z')
            *(pcString + i) -= 32;
    }
}

static void vLower(uint8_t *pcString)
{
    for (int i = 0; i < strlen((const char *)pcString); i++)
    {
        if (*(pcString + i) >= 'A' && *(pcString + i) <= 'Z')
            *(pcString + i) += 32;
    }
}

/* -------------------------------------------------- */

static BLEResult_t prvPushWvCbList(WvCbList_t *pxData)
{
    if (pxData == NULL)
    {
        return BLE_RESULT_BAD_PARAMETER;
    }

    if (gpxTopWvCbList == NULL)
    {
        gpxTopWvCbList = pvPortMalloc(sizeof(WvCbList_t));
        if (gpxTopWvCbList == NULL)
        {
            return BLE_RESULT_FAILED;
        }
        gpxEndWvCbList = gpxTopWvCbList;
    }
    else
    {
        gpxEndWvCbList->next = pvPortMalloc(sizeof(WvCbList_t));
        if (gpxEndWvCbList->next == NULL)
        {
            return BLE_RESULT_FAILED;
        }
        gpxEndWvCbList = gpxEndWvCbList->next;
    }

    *gpxEndWvCbList = *pxData;
    gpxEndWvCbList->next = NULL;

    return BLE_RESULT_SUCCEED;
}

static BLEResult_t prvDeleteWvCbList(uint8_t *puxUUID)
{
    WvCbList_t *pxPreList = gpxTopWvCbList;
    for (WvCbList_t *pxCurrentList = gpxTopWvCbList; pxCurrentList != NULL; pxCurrentList = pxCurrentList->next)
    {
        WvCbList_t *pxNextList = pxCurrentList->next;
        if (memcmp(puxUUID, pxCurrentList->uxUUID, strlen((const char *)puxUUID)) == 0)
        {
            if (pxCurrentList == gpxTopWvCbList) // 削除対象が先頭だった場合
            {
                vPortFree(pxCurrentList);
                gpxTopWvCbList = pxNextList;
            }
            else if (pxCurrentList == gpxEndWvCbList) // 削除対象が後尾だった場合
            {
                vPortFree(pxCurrentList);
                pxPreList->next = NULL;
                gpxEndWvCbList = pxPreList;
            }
            else
            {
                pxPreList->next = pxNextList;
                vPortFree(pxCurrentList);
            }
            return BLE_RESULT_SUCCEED;
        }
        pxPreList = pxCurrentList;
    }
    return BLE_RESULT_SUCCEED;
}

/* -------------------------------------------------- */

static int prvBytesFromHexString(uint8_t *puxData, uint8_t *puxString)
{
    int byte = 0;
    for (int i = 0; i < strlen((const char *)puxString); i += 2)
    {
        uint8_t x = 0;
#if 0
        sscanf((const char *)(puxString + i), "%02hhx", &x);
#else
        uint8_t uxTmp[3] = {0};
        memset(uxTmp, 0x00, sizeof(uxTmp));
        memcpy(uxTmp, puxString + i, 2);
        x = strtol((const char *)uxTmp, NULL, 16);
#endif
        puxData[i / 2] = x;
        byte++;

    }
    return byte;
}

static void prvRegisterExpectEndStr(uint8_t *puxStr, uint8_t uxIndex)
{
    if (puxStr != NULL)
        memcpy(guxExpectEndStr[uxIndex], puxStr, strlen((const char *)puxStr));
    else
        memset(guxExpectEndStr[uxIndex], 0x00, sizeof(guxExpectEndStr));
}

static void prvAllDeleteExpectEndStr()
{
    for (int i = 0; i < sizeof(guxExpectEndStr) / sizeof(guxExpectEndStr[0]); i++)
    {
        memset(guxExpectEndStr[i], 0x00, sizeof(guxExpectEndStr));
    }
}

static void prvEventCb(BLEEventType_t eType, uint8_t *puxMessage)
{
    BLEEventConnParamValue_t xConnParamValue = {0};
    BLEEventConnectValue_t xConnectValue = {0};
    BLEEventWVValue_t xWVValue = {0};

    switch (eType)
    {
    case BLE_EVENT_CB_TYPE_CONN_PARAM:
        prvPraseEventCONN_PARAM(puxMessage, &(xConnParamValue.uxInterval), &(xConnParamValue.uxLatency), &(xConnParamValue.uxTimeout));

        if (gpxEventCbRN4870->conn_param != NULL)
        {
            gpxEventCbRN4870->conn_param(&xConnParamValue);
        }
        break;
    case BLE_EVENT_CB_TYPE_CONNECT:
        prvParseEventCONNECT(puxMessage, &(xConnectValue.uxNum), xConnectValue.uxAddress, sizeof(xConnectValue.uxAddress));

        if (gpxEventCbRN4870->connect != NULL)
        {
            gpxEventCbRN4870->connect(&xConnectValue);
        }
        break;
    case BLE_EVENT_CB_TYPE_DISCONNECT:
        if (gpxEventCbRN4870->disconnect != NULL)
        {
            gpxEventCbRN4870->disconnect(NULL);
        }
        break;
    case BLE_EVENT_CB_TYPE_REBOOT:
        if (gpxEventCbRN4870->reboot != NULL)
        {
            gpxEventCbRN4870->reboot(NULL);
        }
        break;
    case BLE_EVENT_CB_TYPE_SECURED:
        if (gpxEventCbRN4870->secured != NULL)
        {
            gpxEventCbRN4870->secured(NULL);
        }
        break;
    case BLE_EVENT_CB_TYPE_WV:
        xWVValue.xDataSize = sizeof(xWVValue.uxData);
        prvPraseEventWV(puxMessage, &(xWVValue.uxHandle), xWVValue.uxData, &(xWVValue.xDataSize));

        for (WvCbList_t *pxCurrentList = gpxTopWvCbList; pxCurrentList != NULL; pxCurrentList = pxCurrentList->next)
        {
            // FIXME: 暫定 Service UUIDを指定せずにhandle値を取得できるように修正
            uint16_t usHandle = usGetHandleByUUID((uint8_t *)"4fafc2011fb5459e8fccc5c9c331914b", pxCurrentList->uxUUID);
            if (xWVValue.uxHandle == usHandle && pxCurrentList->cb != NULL)
            {
                pxCurrentList->cb(&xWVValue);
                break;
            }
        }
        break;
    default:
        break;
    }
}

static BLEResult_t prvPraseEventCONN_PARAM(uint8_t *puxMessage, uint16_t *puxInterval, uint16_t *puxLatency, uint16_t *puxTimeout)
{
    uint8_t *puxDelimiterPos = (uint8_t *)strchr((const char *)puxMessage, ',');
    uint8_t *puxNextDelimiterPos = NULL;
    uint8_t tmpString[5] = {0}; // パラメータは <= 4Byteしか想定していない
    memset(tmpString, 0x00, sizeof(tmpString));
    uint8_t uxElement = 0;

    while (1)
    {
        puxNextDelimiterPos = (uint8_t *)strchr((const char *)puxDelimiterPos + 1, ',');

        switch (uxElement)
        {
        case 0: // interval
            memcpy(tmpString, puxDelimiterPos + 1, puxNextDelimiterPos - puxDelimiterPos - 1);
            *puxInterval = (uint16_t)strtol((const char *)tmpString, 0, 16);
            break;
        case 1: // latency
            memcpy(tmpString, puxDelimiterPos + 1, puxNextDelimiterPos - puxDelimiterPos - 1);
            *puxLatency = (uint16_t)strtol((const char *)tmpString, 0, 16);
            break;
        case 2: // timeout
            memcpy(tmpString, puxDelimiterPos + 1, strlen((const char *)puxMessage) - (puxDelimiterPos - puxMessage) - 1);
            *puxTimeout = (uint16_t)strtol((const char *)tmpString, 0, 16);
            break;
        default:
            return BLE_RESULT_FAILED;
            break;
        }

        if (puxNextDelimiterPos == NULL)
            break;
        memset(tmpString, 0x00, sizeof(tmpString));
        puxDelimiterPos = puxNextDelimiterPos;
        uxElement++;
    }
    return BLE_RESULT_SUCCEED;
}

static BLEResult_t prvParseEventCONNECT(uint8_t *puxMessage, uint8_t *puxNum, uint8_t *puxAddress, size_t xAddressSize)
{
    uint8_t *puxDelimiterPos = (uint8_t *)strchr((const char *)puxMessage, ',');
    uint8_t *puxNextDelimiterPos = NULL;
    uint8_t tmpNumString[2] = {0};
    memset(tmpNumString, 0x00, sizeof(tmpNumString));
    uint8_t uxElement = 0;

    while (1)
    {
        puxNextDelimiterPos = (uint8_t *)strchr((const char *)puxDelimiterPos + 1, ',');

        switch (uxElement)
        {
        case 0: // num
            memcpy(tmpNumString, puxDelimiterPos + 1, puxNextDelimiterPos - puxDelimiterPos - 1);
            *puxNum = (uint8_t)strtol((const char *)tmpNumString, 0, 10);
            break;
        case 1: // address
            prvBytesFromHexString(puxAddress, puxDelimiterPos + 1);
            break;
        default:
            return BLE_RESULT_FAILED;
            break;
        }

        if (puxNextDelimiterPos == NULL)
            break;
        puxDelimiterPos = puxNextDelimiterPos;
        uxElement++;
    }
    return BLE_RESULT_SUCCEED;
}

static BLEResult_t prvPraseEventWV(uint8_t *puxMessage, uint16_t *puxHandle, uint8_t *puxData, size_t *pxDataSize)
{
    uint8_t *puxDelimiterPos = (uint8_t *)strchr((const char *)puxMessage, ',');
    uint8_t *puxNextDelimiterPos = NULL;
    uint8_t tmpHandleString[5] = {0};
    memset(tmpHandleString, 0x00, sizeof(tmpHandleString));
    uint8_t uxElement = 0;

    while (1)
    {
        puxNextDelimiterPos = (uint8_t *)strchr((const char *)puxDelimiterPos + 1, ',');

        switch (uxElement)
        {
        case 0: // handle
            memcpy(tmpHandleString, puxDelimiterPos + 1, puxNextDelimiterPos - puxDelimiterPos - 1);
            *puxHandle = (uint16_t)strtol((const char *)tmpHandleString, 0, 16);
            break;
        case 1: // data
            *pxDataSize = prvBytesFromHexString(puxData, puxDelimiterPos + 1);
            break;
        default:
            return BLE_RESULT_FAILED;
            break;
        }

        if (puxNextDelimiterPos == NULL)
            break;
        puxDelimiterPos = puxNextDelimiterPos;
        uxElement++;
    }
    return BLE_RESULT_SUCCEED;
}

// --------------------------------------------------
// Unit Test用関数定義(関数のプロトタイプ宣言は「自身のファイル名+ "_test.h"」で宣言されていること)
// --------------------------------------------------
#if (BUILD_MODE_TEST == 1) /* BUILD_MODE_TESTが定義されているとき */
#endif                     /* end  BUILD_MODE_TEST */
