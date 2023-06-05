/**
 * @file rn4870.h
 * @author Systemzeus Inc.
 * @copyright Copyright © 2023 Systemzeus Inc. All rights reserved.
 */
#ifndef RN4870_H_
#define RN4870_H_

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

#include "definitions.h"
#include "FreeRTOS.h"

// --------------------------------------------------
// ユーザ作成ヘッダの取り込み
// --------------------------------------------------

// --------------------------------------------------
// #defineマクロ
// --------------------------------------------------
#define MAX_CHARACTERISTIC_NUM 4 /**< 1Serviceで扱う最大のCharacteristic数 (プログラム(便宜)上の最大値であり、仕様上の最大値ではない) */
#define MAX_BONDING_NUM        8 /**< Bondingする最大数 */

/* -------------------------------------------------- */

// Set Commands
#define SET_SERIALIZED_BLUETOOTH_NAME "S-"
#define SET_PAIRING_MODE              "SA"
#define SET_DEVICE_NAME               "SN"
#define SET_FIX_PIN                   "SP"

// Action Commands
#define ENTER_CMD               "$$$"
#define EXIT_CMD                "---"
#define START_ADVERTISEMENT     "A"
#define START_BONDING_PROCESS   "B"
#define DISPLAY_DEVICE_INFO_CMD "D"
#define REBOOT                  "R,1"
#define UNBOND                  "U" // remove existing bonding
#define DISPLAY_FW_VERSION      "V"
#define STOP_ADVERTISEMENT      "Y"

// List Commands
#define LIST_BONDED_DEVICE          "LB"
#define LIST_SERVICE_CHARACTERISTIC "LS"

// Service Configuration Commands
#define SET_UUID_CHARACTERISTIC "PC"
#define SET_UUID_SERVICE        "PS"
#define CLEAR_ALL_SERVICE       "PZ"

// Characteristic Access Commands
#define READ_LOCAL_CHARACTERISTIC_VALUE  "SHR"
#define WRITE_LOCAL_CHARACTERISTIC_VALUE "SHW"

/* -------------------------------------------------- */

/**
 *  Set I/O capability
 *
 * ・Numeric Comparison: RN4870が出力する値とスマホ側で表示されている値が一致するか確認
 * ・Just Works        : 自動的に認証
 * ・Passkey Entry     : パスキーを入力することで認証
 * ・(Out of Band(OBB) : NFC等別の手段で鍵を交換)
 */
#define DISPLAY_YES_NO     1 /**< Numeric Comparison */
#define NO_INPUT_NO_OUTPUT 2 /**< Just Works 入出力無(自動) */
#define KEYBOARD_ONLY      3 /**< Passkey Entry: スマホ側で数字が表示されるのでPeripheral(デバイス)側でパスキーを入力*/
#define DISPLAY_ONLY       4 /**< Passkey Entry: Central(スマホ)側でパスキーを入力 */

/**
 *  Characteristic properties
 */
// #define EXTENDED_PROPERTY 0b10000000 // Not supported
// #define AUTHENTICATED_WRITE 0b0100000 // Not supported
#define INDICATE               0b00100000
#define NOTIFY                 0b00010000
#define WRITE                  0b00001000
#define WRITE_WITHOUT_RESPONSE 0b00000100
#define READ                   0b00000010
    // #define BROADCAST 0b00000001 // Not supported

    /* -------------------------------------------------- */

#define STATUS_MESSAGE_BONDED     "BONDED"
#define STATUS_MESSAGE_CONN_PARAM "CONN_PARAM"
#define STATUS_MESSAGE_CONNECT    "CONNECT"
#define STATUS_MESSAGE_DISCONNECT "DISCONNECT"
#define STATUS_MESSAGE_REBOOT     "REBOOT"
#define STATUS_MESSAGE_SECURED    "SECURED"
#define STATUS_MESSAGE_WV         "WV"

    /* -------------------------------------------------- */

#define BLE_MAC_ADDRESS_SIZE 6  /**< BLE MACアドレスバイトサイズ */
#define BLE_UUID_STR_LENGTH 32 /**< ハイフン無UUID文字列長 */

// --------------------------------------------------
// #define関数マクロ
// --------------------------------------------------

    // --------------------------------------------------
    // typedef定義
    // --------------------------------------------------
    // clang-format off
typedef size_t (*BLE_UART_TX)(uint8_t *puxBuffer, const size_t xSize); /**< UART TXインターフェース */
typedef size_t (*BLE_UART_RX)(uint8_t *puxBuffer, const size_t xSize); /**< UART RXインターフェース */
typedef void (*BLE_GPIO_ON)(void);                                     /**< GPIO ONインターフェース */
typedef void (*BLE_GPIO_OFF)(void);                                    /**< GPIO OFFインターフェース */
typedef void (*BLE_DELAY)(const uint32_t ms);                          /**< 遅延関数*/

typedef void (*BLE_EVENT_CB)(void *pxValue); /**< イベントコールバック関数インターフェース */

// --------------------------------------------------
// enumタグ定義（typedefを同時に行う）
// --------------------------------------------------
/**
 * @brief BLEライブラリの結果
 */
typedef enum
{
    BLE_RESULT_SUCCEED = 0x0,  /**< 成功 */
    BLE_RESULT_FAILED,         /**< 失敗 */
    BLE_RESULT_BAD_PARAMETER,  /**< 不正な引数 */
    BLE_RESULT_TIMEOUT,        /**< タイムアウト */
    BLE_RESULT_NOT_IMPLEMENTED /**< 未実装 */
} BLEResult_t;

/**
 * @brief イベントタスクの状態
 */
typedef enum
{
    BLE_EVENT_LOOP_STATE_INIT = 0x0, /**< 初期 */
    BLE_EVENT_LOOP_STATE_LOOP        /**< タスク中 */
} BLEEventLoopState_t;

/**
 * @brief UART送信タスクの状態
 */
typedef enum
{
    BLE_SEND_LOOP_STATE_INIT = 0x0, /**< 初期 */
    BLE_SEND_LOOP_STATE_LOOP        /**< タスク中 */
} BLESendLoopState_t;

/**
 * @brief UART受信タスクの状態
 */
typedef enum
{
    BLE_IF_LOOP_STATE_INIT = 0x0,      /**< 初期 */
    BLE_IF_LOOP_STATE_CMD_RECEIVING,   /**< コマンド受信中 */
    BLE_IF_LOOP_STATE_EVENT_RECEIVING, /**< イベント受信中 */
    BLE_IF_LOOP_STATE_EVENT_PARSE      /**< イベント解析中 */
} BLEInterfaceLoopState_t;

/**
 * @brief イベントコールバックの種類
 */
typedef enum
{
    BLE_EVENT_CB_TYPE_CONN_PARAM = 0x0, /**< 接続パラメータ */
    BLE_EVENT_CB_TYPE_CONNECT,          /**< 接続時 */
    BLE_EVENT_CB_TYPE_DISCONNECT,       /**< 切断時 */
    BLE_EVENT_CB_TYPE_REBOOT,           /**< 再起動 */
    BLE_EVENT_CB_TYPE_SECURED,          /**< ペアリング時(暗号化時) */
    BLE_EVENT_CB_TYPE_WV,               /**< 書き込み時 */
    BLE_EVENT_CB_TYPE_UNKNOWN           /**< 不明なイベント */
} BLEEventType_t;

// --------------------------------------------------
// struct/unionタグ定義（typedefを同時に行う）
// --------------------------------------------------
/**
 * @brief BLEライブラリのインターフェース
 */
typedef struct
{
    BLE_UART_TX uart_tx;   /**< UART送信 */
    BLE_UART_RX uart_rx;   /**< UART受信 */
    BLE_GPIO_ON gpio_on;   /**< GPIO High */
    BLE_GPIO_OFF gpio_off; /**< GPIO Low */
    BLE_DELAY delay;       /**< 遅延関数 */
} BLEInterface_t;

/**
 * @brief BLE Characteristicデータ構造
 */
typedef struct
{
    uint8_t uxUUID[BLE_UUID_STR_LENGTH + 1]; /**< Characteristic UUID */
    uint16_t usHandle;                       /**< Characteristic handle */
    uint8_t uxProperty;                      /**< Characteristic property */
} BLECharacteristic_t;

/**
 * @brief BLE Bondingデータ構造
 */
typedef struct
{
    uint8_t uxIndex;                         /**< Bondingのインデックス */
    uint8_t uxAddress[BLE_MAC_ADDRESS_SIZE]; /**< Bonding先のMACアドレス */
    uint8_t uxAddressType;                   /**< アドレスタイプ */
} BLEBonding_t;

/**
 * @brief 接続パラメータイベント
 */
typedef struct
{
    uint16_t uxInterval; /**< インターバル */
    uint16_t uxLatency;  /**< レイテンシ */
    uint16_t uxTimeout;  /**< タイムアウト */
} BLEEventConnParamValue_t;

/**
 * @brief 接続イベント
 */
typedef struct
{
    uint8_t uxNum;                          /**< 接続数 */
    uint8_t uxAddress[BLE_MAC_ADDRESS_SIZE]; /**< 接続先MACアドレス */
} BLEEventConnectValue_t;

/**
 * @brief 書き込みイベント
 */
typedef struct
{
    uint16_t uxHandle;   /**< 書き込みハンドル値 */
    uint8_t uxData[256]; /**< 書き込みデータ */
    size_t xDataSize;    /**< 書き込みデータサイズ */
} BLEEventWVValue_t;

/**
 * @brief 書き込み時イベントコールバックリスト
 */
typedef struct _WvCbList
{
    BLE_EVENT_CB cb;                         /**< コールバック関数 */
    uint8_t uxUUID[BLE_UUID_STR_LENGTH + 1]; /**< コールバック関数を行うcharacteristics UUID */
    struct _WvCbList *next;                  /**< リスト */
} WvCbList_t;

/**
 * @brief イベントコールバック
 */
typedef struct
{
    BLE_EVENT_CB conn_param; /**< 接続パラメータ */
    BLE_EVENT_CB connect;    /**< 接続 */
    BLE_EVENT_CB disconnect; /**< 切断 */
    BLE_EVENT_CB reboot;     /**< 再起動 */
    BLE_EVENT_CB secured;    /**< ペアリング時 */
} BLEEventCallback_t;

/**
 * @brief イベントキューデータ構造
 */
typedef struct
{
    BLEEventType_t eCbType;     /**< イベントタイプ */
    uint8_t uxEventString[256]; /**< イベント文字列 */
} BLEEventQueue_t;

// --------------------------------------------------
// extern変数宣言
// --------------------------------------------------

// --------------------------------------------------
// 関数プロトタイプ宣言
// --------------------------------------------------
/**
 * @brief BLEライブラリタスクの初期化
 *
 * @param [in] pxInterface インターフェース
 * @param [in] pxEventCb   コールバック
 */
void vInitializeBLE(BLEInterface_t *pxInterface, BLEEventCallback_t *pxEventCb);

/**
 * @brief コールバック関数登録
 *
 * @param [in] xCbFunc      コールバック関数ポインタ
 * @param [in] eType        コールバック種別
 * @param [in] puxCharaUUID Characteristic UUID(書き込みイベントコールバック登録時のみ指定)
 *
 * @retval BLE_RESULT_BAD_PARAMETER 不正な引数
 * @retval BLE_RESULT_SUCCEED 登録成功
 */
BLEResult_t eRegisterBLEEventCb(BLE_EVENT_CB xCbFunc, BLEEventType_t eType, uint8_t *puxCharaUUID);

/**
 * @brief コールバック関数削除
 *
 * @param [in] eType        コールバック種別
 * @param [in] puxCharaUUID Characteristic UUID(書き込みイベントコールバック登録時のみ指定)
 *
 * @retval BLE_RESULT_BAD_PARAMETER 不正な引数
 * @retval BLE_RESULT_SUCCEED 登録成功
 */
BLEResult_t eDeleteBLEEventCb(BLEEventType_t eType, uint8_t *puxCharaUUID);

/**
 * @brief BLEモジュールハードリセット
 */
void vHardResetBLE();

/* -------------------------------------------------- */

/**
 * @brief コマンドモード
 *
 * @return BLEResult_t コマンド成否
 */
BLEResult_t eEnterCMDMode();

/**
 * @brief コマンドモード終了
 *
 * @return BLEResult_t コマンド成否
 */
BLEResult_t eExitCMDMode();

/**
 * @brief アドバタイズ開始
 *
 * @param [in] uxInterval インターバル
 * @param [in] uxTotal トータル
 *
 * @return BLEResult_t コマンド成否
 */
BLEResult_t eStartAdvertisement(uint16_t uxInterval, uint16_t uxTotal);

/**
 * @brief ボンディング開始
 *
 * @return BLEResult_t コマンド成否
 */
BLEResult_t eStartBondingProcess();

/**
 * @brief BLEモジュール情報取得
 *
 * @return BLEResult_t コマンド成否
 */
BLEResult_t eGetDeviceInfo();

/**
 * @brief BLEモジュール再起動
 *
 * @return BLEResult_t 結果
 */
BLEResult_t eReboot();

/**
 * @brief ボンディング情報削除
 *
 * @param [in] uxIndex 削除するボンディング情報のインデックス
 *
 * @return BLEResult_t コマンド成否
 */
BLEResult_t eRemoveBonding(uint8_t uxIndex);

/**
 * @brief BLEモジュールのFWバージョン取得
 *
 * @return BLEResult_t コマンド成否
 */
BLEResult_t eGetFWVersion();

/**
 * @brief アドバタイズ停止
 *
 * @return BLEResult_t コマンド成否
 */
BLEResult_t eStopAdvertisement();

/**
 * @brief シリアライズBLE名設定
 *
 * @param [in] pucName BLE名
 *
 * @return BLEResult_t コマンド成否
 */
BLEResult_t eSetSerializedBluetoothName(uint8_t *pucName); // FIXME: シリアライズNameとは？

/**
 * @brief ペアリングモード設定
 *
 * @param [in] uxMode ペアリングモード
 *
 * @return BLEResult_t コマンド成否
 */
BLEResult_t eSetPairingMode(uint8_t uxMode);

/**
 * @brief デバイス名設定
 *
 * @param [in] pucName デバイス名
 *
 * @return BLEResult_t コマンド成否
 */
BLEResult_t eSetDeviceName(uint8_t *pucName); // FIXME: 反映にはリセットが必要？

/**
 * @brief ペアリング時に使用するPINの設定
 *
 * @param [in] puxPIN PIN
 *
 * @return BLEResult_t コマンド成否
 */
BLEResult_t eSetFixPIN(uint8_t *puxPIN);

/**
 * @brief ボンディングリスト取得
 *
 * @param [out]     pucMessage リスト文字列
 * @param [in, out] xSize      文字列長さ
 *
 * @return BLEResult_t コマンド成否
 */
BLEResult_t eListBondedDevices(uint8_t *pucMessage, size_t xSize);

/**
 * @brief characteristicsリスト取得
 *
 * @param [in]      pucServiceUUID Service UUID
 * @param [out]     pucMessage     リスト文字列
 * @param [in, out] pxSize         文字列長さ
 *
 * @return BLEResult_t コマンド成否
 */
BLEResult_t eListServiceCharacteristic(uint8_t *pucServiceUUID, uint8_t *pucMessage, size_t *pxSize);

/**
 * @brief characteristics設定
 *
 * @param [in] pucUUID    Characteristic UUID
 * @param [in] uxProperty プロパティ
 * @param [in] uxDataSize データサイズ
 *
 * @return BLEResult_t コマンド成否
 */
BLEResult_t eSetUUIDCharacteristic(uint8_t *pucUUID, uint8_t uxProperty, uint8_t uxDataSize);

/**
 * @brief Service設定
 *
 * @param [in] pucUUID Service UUID
 *
 * @return BLEResult_t コマンド成否
 */
BLEResult_t eSetUUIDService(uint8_t *pucUUID);

/**
 * @brief すべてのServiceを削除
 *
 * @return BLEResult_t コマンド成否
 */
BLEResult_t eClearAllService();

/**
 * @brief characteristicsの値を読み取り
 *
 * @param [in]      usHandle characteristicsハンドル値
 * @param [out]     puxValue バッファ
 * @param [in, out] pxSize   バッファサイズ
 *
 * @return BLEResult_t コマンド成否
 */
BLEResult_t eReadLocalCharacteristicValue(uint16_t usHandle, uint8_t *puxValue, size_t *pxSize);

/**
 * @brief characteristicsに値を書き込み
 *
 * @param [in] usHandle characteristicsハンドル値
 * @param [in] puxValue バッファ
 * @param [in] xSize    バッファサイズ
 *
 * @return BLEResult_t コマンド成否
 */
BLEResult_t eWriteLocalCharacteristicValue(uint16_t usHandle, uint8_t *puxValue, size_t xSize);

/* -------------------------------------------------- */

/**
 * @brief characteristics UUIDからHandle値を取得
 *
 * @param [in] pucServiceUUID Service UUID
 * @param [in] pucCharaUUID   characteristics UUID
 *
 * @return uint16_t ハンドル値
 */
uint16_t usGetHandleByUUID(uint8_t *pucServiceUUID, uint8_t *pucCharaUUID);

/**
 * @brief characteristics情報を更新
 *
 * @note BLEモジュールにcharacteristicsを登録した際に呼び出す
 *
 * @param [in] pucServiceUUID Service UUID
 * @return BLEResult_t
 */
BLEResult_t eUpdateHandleInfo(uint8_t *pucServiceUUID);

/**
 * @brief characteristicsリストの解析
 *
 * @param [in]  pucMessage  リスト文字列
 * @param [out] pxCharaList characteristicsリスト
 *
 * @return BLEResult_t 成否
 */
BLEResult_t eParseLs(uint8_t *pucMessage, BLECharacteristic_t *pxCharaList);

/**
 * @brief Bondingリストの解析
 *
 * @param [in]  pucMessage    リスト文字列
 * @param [out] pxBondingList Bondingリスト
 *
 * @return uint8_t Bonding数
 */
uint8_t uxParseLb(uint8_t *pucMessage, BLEBonding_t *pxBondingList);

/* -------------------------------------------------- */

/**
 * @brief イベントループ
 *
 * @param [in] pvParameters パラメータ(未使用)
 */
void vEventLoop(void *pvParameters);

/**
 * @brief UART送信ループ
 *
 * @param [in] pvParameters パラメータ(未使用)
 */
void vSendLoop(void *pvParameters);

/**
 * @brief UART受信ループ
 *
 * @param [in] pvParameters パラメータ(未使用)
 */
void vInterfaceLoop(void *pvParameters);

// --------------------------------------------------
// インライン関数
// --------------------------------------------------

#ifdef __cplusplus
}
#endif

#endif /* end RN4870_H_ */
