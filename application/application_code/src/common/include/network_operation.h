/**
 * @file network_operation.h
 * @author Systemzeus Inc.
 * @copyright Copyright © 2023 Systemzeus Inc. All rights reserved.
 */
#ifndef NETWORK_OPERATION_H_
#define NETWORK_OPERATION_H_

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
#include "FreeRTOSIPConfig.h"

// --------------------------------------------------
// ユーザ作成ヘッダの取り込み
// --------------------------------------------------
#include "common/include/application_define.h"

    // --------------------------------------------------
    // #defineマクロ
    // --------------------------------------------------

/**
 * @brief Wi-Fiドライバが準備可能になるまでに待機する時間
 *
 * @warning
 * チェック周期 #WIFI_DRIVER_STATUS_READY_CHECK_INTERVAL_MS 以上の値にする
 *
 * @note
 * おそらく数秒程度待機すれば良いが、ドライバが準備可能でないと処理が進めなくなってしまうので、数十秒単位の十分な時間をとる
 */
#define WIFI_DRIVER_STATUS_READY_TIMEOUT_MS (10 * 1000)

    // --------------------------------------------------
    // #define関数マクロ
    // --------------------------------------------------

/**
 * @brief Wi-Fi接続に失敗してリトライするまでの待ち時間
 */
#define WIFI_CONNECT_WAIT_TIME_MS 500

/**
 * @brief Wi-Fiルータ接続後にDHCPプロセス等、ネットワーク全体の接続が完了するまで待機する時間
 */
#define NETWORK_DONE_WAIT_TIME_MS (ipconfigMAXIMUM_DISCOVER_TX_PERIOD)

/**
 * @brief Wi-Fiの接続リトライを実質無限回繰り返す際に指定するDefine
 */
#define WIFI_CONNECT_RETRY_REPEAT_AD_INFINITUM ((uint32_t)(0xFFFFFFFF))

    // --------------------------------------------------
    // typedef定義
    // --------------------------------------------------

    // --------------------------------------------------
    // enumタグ定義（typedefを同時に行う）
    // --------------------------------------------------

    /**
     * @brief 関数の実行結果
     */
    typedef enum
    {
        /**
         * @brief 成功
         */
        NETWORK_OPERATION_RESULT_SUCCESS = 0,

        /**
         * @brief 失敗
         */
        NETWORK_OPERATION_RESULT_FAILED = 1,

        /**
         * @brief タイムアウト
         */
        NETWORK_OPERATION_RESULT_TIMEOUT = 2,

        /**
         * @brief 既に初期化が済んでいるため、さらに初期化を行う必要がない
         *
         * @note
         * 本エラーは #eNetworkInit のみで発生する
         */
        NETWORK_OPERATION_RESULT_ALREADY_INIT = 3

    } NetworkOperationResult_t;

    // --------------------------------------------------
    // struct/unionタグ定義（typedefを同時に行う）
    // --------------------------------------------------

    /**
     * @brief Wi-Fiルータへの再接続を中断する条件を指定する関数
     *
     */
    typedef struct
    {
        /**
         * @brief Wi-Fiルーター接続失敗時に呼び出される関数。指定しなくても良い。
         *
         * @param[in] uxRetryCount 何回目の試行を行ったか。１オリジン。
         *
         * @retval true  接続リトライを続ける
         * @retval false 接続リトライを中断
         */
        bool (*bRejectConditionFunction)(const uint32_t uxRetryCount);
} WiFiConnectRejectConditionFunction_t;

    // --------------------------------------------------
    // extern変数宣言
    // --------------------------------------------------

    // --------------------------------------------------
    // 関数プロトタイプ宣言
    // --------------------------------------------------

    /**
     * @brief TCP/IPレイヤのライブラリを初期化する
     *
     * @details
     * FreeRTOS_IP.h の #FreeRTOS_IPInit 関数を呼び出す
     *
     * @warning
     * Wi-Fiルータへの接続は行わない。FreeRTOS本来の動作は、#FreeRTOS_IPInit を呼び出すとWi-Fiルータへの
     * 接続が行われるが、Wi-Fi接続に失敗した場合に無限リトライしてしまう点、アプリケーション側でWi-Fi接続のタイミング
     * をコントールできない点から、Wi-Fi接続を行わないように修正。
     *
     * @retval #NETWORK_OPERATION_RESULT_SUCCESS      成功
     * @retval #NETWORK_OPERATION_RESULT_FAILED       失敗
     * @retval #NETWORK_OPERATION_RESULT_TIMEOUT      タイムアウト。時間内にWi-FiドライバのステータスがREADYにならなかった場合。
     * @retval #NETWORK_OPERATION_RESULT_ALREADY_INIT 既に初期化済みである
     *
     */
    NetworkOperationResult_t eNetworkInit(void);

    /**
     * @brief Wi-Fiルータへの接続を行う。
     *
     * @note
     * - 1. FreeRTOSのネットワークインターフェース層が初期化されていない場合は、初期化してからWi-Fi接続を試みる
     * - 2. 接続情報は、 se_operation.h 内の #eGetWiFiInfoFromSE を用いる
     *
     * @param[in] uxMaxConnectNum             最大接続試行回数。必ず1以上を指定する。1を指定すると1回だけ接続を試みてリトライは行わない。
     *                                        リトライを際限なく続けたい場合は #WIFI_CONNECT_RETRY_REPEAT_AD_INFINITUM を使用する
     * @param[in] pxRejectConditionFunction   接続失敗時に呼び出されるコールバック関数。
     *                                        uxConnectNumに到達する前にWi-Fi再接続を中断したい場合に用いる。NULL指定可。
     *
     * @retval #NETWORK_OPERATION_RESULT_SUCCESS  成功
     * @retval #NETWORK_OPERATION_RESULT_FAILED   引数が不正のとき、Wi-Fiモジュールの操作に失敗したとき、
     *                                            フラッシュからのWi-Fi接続情報の取得に失敗したとき、APに接続できなかった
     * @retval #NETWORK_OPERATION_RESULT_TIMEOUT  Wi-Fiルータへの接続には成功したが、DHCPでIPアドレスの取得に失敗した（タイムアウト）
     */
    NetworkOperationResult_t eWiFiConnectToRouter(
        const uint32_t uxMaxConnectNum,
        const WiFiConnectRejectConditionFunction_t *pxRejectConditionFunction);

    /**
     * @brief Wi-Fiルータから切断する
     *
     * @retval #NETWORK_OPERATION_RESULT_SUCCESS  成功
     * @retval #NETWORK_OPERATION_RESULT_FAILED   失敗
     */
    NetworkOperationResult_t eWiFiDisconnectFromRouter(void);

    /**
     * @brief IPタスク起動の通知を行う
     */
    void
    vprvNotifyIPInitTaskUp(void);

/**
 * @brief IPタスク起動したときに１回だけ実行されるフック関数
 *
 * @see FreeRTOS_IP.h
 *
 * @note
 * vprvNotifyIPInitTaskUp のプロトタイプ宣言後でないと定義できないのでここで宣言する。
 *
 */
#define iptraceIP_TASK_STARTING() \
    do                            \
    {                             \
        vprvNotifyIPInitTaskUp(); \
    } while (false)

    // --------------------------------------------------
    // インライン関数
    // --------------------------------------------------

#ifdef __cplusplus
}
#endif

#endif /* end NETWORK_OPERATION_H_ */
