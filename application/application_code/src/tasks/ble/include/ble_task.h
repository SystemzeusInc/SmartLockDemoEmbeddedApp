/**
 * @file ble_task.h
 * @author Systemzeus Inc.
 * @copyright Copyright © 2023 Systemzeus Inc. All rights reserved.
 */
#ifndef BLE_TASK_H_
#define BLE_TASK_H_

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
#include "queue.h"

// --------------------------------------------------
// ユーザ作成ヘッダの取り込み
// --------------------------------------------------
#include "tasks/ble/include/rn4870.h"

// --------------------------------------------------
// #defineマクロ
// --------------------------------------------------
#define SERVICE_UUID "4fafc2011fb5459e8fccc5c9c331914b" /**< サービスUUID */

#define CHARACTERISTIC_UUID_WIFI_INFO        "beb5483e36e14688b7f5ea07361b26a8" /**< プロビジョニング情報の書き込み */
#define CHARACTERISTIC_UUID_LINKING_INFO     "c41c8a42f4e745c7afa78284ecae2c51" /**< リンキング情報の読み込み */
#define CHARACTERISTIC_UUID_PROVISIONING     "7b842730a65c457b8b855dce1fa2ead1" /**< モード変更リクエスト*/
#define CHARACTERISTIC_UUID_WIFI_INFO_CHANGE "6cd0f24ec1d84dc4902436c4fa17e4d8" /**< Wi-Fi接続先情報の書き込み*/

    /* -------------------------------------------------- */

#define MAX_CHARACTERISTIC_DATA_SIZE 151 /**< characteristicに格納できる最大オクテット(文字) 151まで可能？*/

    // clang-format off
// --------------------------------------------------
// #define関数マクロ
// --------------------------------------------------
    // clang-format on

    // clang-format off
// --------------------------------------------------
// typedef定義
// --------------------------------------------------
    // clang-format on

    // clang-format off
// --------------------------------------------------
// enumタグ定義（typedefを同時に行う）
// --------------------------------------------------
    // clang-format on

    /**
     * @brief BLETaskの結果
     */
    typedef enum
    {
        BLE_TASK_RESULT_SUCCEED = 0x0,  /**< 成功 */
        BLE_TASK_RESULT_FAILED,         /**< 失敗 */
        BLE_TASK_RESULT_BAD_PARAMETER,  /**< 不正な引数 */
        BLE_TASK_RESULT_TIMEOUT,        /**< タイムアウト */
        BLE_TASK_RESULT_NOT_IMPLEMENTED /**< 未実装 */
    } BLETaskResult_t;

    /**
     * @brief BLETaskの状態
     */
    typedef enum
    {
        BLE_APP_STATE_INIT = 0x0,  /**< 初期 */
        BLE_APP_STATE_SERVICE_TASK /**< タスク中 */
    } BLETaskState_t;

    /**
     * @brief BLE Taskに対する命令
     */
    typedef enum
    {
        BLE_OP_WRITE = 0x0, /**< 指定したCharacteristic UUIDに値を書き込む */
        BLE_OP_READ,        /**< 指定したCharacteristic UUIDの値を読み込む */
        BLE_OP_BONDING      /**< 暗号化(Bonding)要求 */
    } BLETaskOp_t;

    /**
     * @brief BLETask命令イベントビット
     */
    typedef enum
    {
        BLE_OP_EVENT_SUCCEED = 0x1 << 0, /**< 成功 */
        BLE_OP_EVENT_FAILED = 0x1 << 1   /**< 失敗 */
    } BLETaskOpEvent_t;

    // clang-format off
// --------------------------------------------------
// struct/unionタグ定義（typedefを同時に行う）
// --------------------------------------------------
    // clang-format on
    /**
     * @brief BLETaskのデータ
     */
    typedef struct
    {
        BLETaskState_t eState;    /**< BLETask状態 */
        QueueHandle_t xQueue;     /**< Queueハンドル */
        TaskHandle_t xTaskHandle; /**< Taskハンドル */
    } BLETaskData_t;

    /**
     * @brief BLETaskキューデータ構造
     */
    typedef struct
    {
        BLETaskOp_t eOp;          /**< BLETaskに対する命令 */
        TaskHandle_t xTaskHandle; /**< Taskハンドル */
        union
        {
            struct
            {
                uint8_t ucUUID[32 + 1]; /**< ハイフン無UUID */
                uint8_t uxData[256];    /**< 書き込むデータ */
                size_t xDataSize;       /**< 書き込むデータサイズ */
            } write;
            struct
            {
                uint8_t ucUUID[32 + 1]; /**< ハイフン無UUID */
                uint8_t *puxBuffer;     /**< 読み込みバッファ */
                size_t *pxBufferSize;   /**< バッファサイズ */
            } read;
        } u;
    } BLETaskQueueData_t;

    // --------------------------------------------------
    // extern変数宣言
    // --------------------------------------------------

    // --------------------------------------------------
    // 関数プロトタイプ宣言
    // --------------------------------------------------
    /**
     * @brief BLETaskの初期化、生成
     *
     * @return BLETaskResult_t 結果
     */
    BLETaskResult_t eBLETaskInitialize(void);

    /**
     * @brief ボンディングリスト取得
     *
     * @param [out] pxBondingList ボンディングリスト
     *
     * @return uint8_t ボンディング数
     */
    uint8_t uxBLEGetBondingList(BLEBonding_t *pxBondingList);

    /**
     * @brief リンキング情報のcharacteristicsを初期化
     */
    void vInitLinkingInfo();

    /**
     * @brief プロビジョニングcharacteristicsをプロビジョニングモード状態に設定
     */
    void vSetProvisioningMode();

    /**
     * @brief プロビジョニングcharacteristicsを通常モード状態に設定
     */
    void vSetCommandNothingMode();

    /**
     * @brief プロビジョニング情報characteristicの値を初期化(適当な値で上書き)
     *
     * @note RN4870ではCharacteristicにREAD権限しかついていなくても、読み側が無視してREADできてしまうため
     *       Wi-Fiのパスワードなどをフラッシュ保存後すぐに別の値で上書きする
     *
     */
    void vInitBLEProvisioningInfo();

    /**
     * @brief Wi-Fi接続先情報characteristicの値を初期化(適当な値で上書き)
     *
     * @note RN4870ではCharacteristicにREAD権限しかついていなくても、読み側が無視してREADできてしまうため
     *       Wi-Fiのパスワードなどをフラッシュ保存後すぐに別の値で上書きする
     */
    void vInitBLEWiFiInfo();

    /**
     * @brief BLETaskの書き込み指示
     *
     * @param [in] pucUUID   characteristicsのハイフン無UUID
     * @param [in] puxData   格納するデータ
     * @param [in] xDataSize データサイズ
     */
    void vWriteOpBLE(uint8_t *pucUUID, uint8_t *puxData, size_t xDataSize);

    /**
     * @brief BLETaskに読み込み指示
     *
     * @param [in]      pucUUID      Characteristicのハイフン無UUID
     * @param [out]     puxBuffer    読み込みデータを格納するバッファ
     * @param [in, out] pxBufferSize バッファサイズ
     */
    void vReadOpBLE(uint8_t *pucUUID, uint8_t *puxBuffer, size_t *pxBufferSize);

    /**
     * @brief BLETaskのペアリング(ボンディング)指示
     */
    void vBondingOpBLE();

    /**
     * @brief ペアリング済か確認
     *
     * @return true  ペアリング済
     * @return false 未ペアリング
     */
    bool bCheckSecuredBLE();

    // --------------------------------------------------
    // インライン関数
    // --------------------------------------------------

#ifdef __cplusplus
}
#endif

#endif /* end BLE_TASK_H_ */
