/**
 * @file mqtt_operation_task.h
 * @author Systemzeus Inc.
 * @copyright Copyright © 2023 Systemzeus Inc. All rights reserved.
 */
#ifndef MQTT_OPERATION_TASK_H_
#define MQTT_OPERATION_TASK_H_

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

#include "transport_interface.h"
#include "transport_secure_sockets.h"
#include "iot_secure_sockets.h"
#include "core_mqtt_agent.h"
#include "freertos_agent_message.h"

#include "mqtt_subscription_manager.h"

// --------------------------------------------------
// ユーザ作成ヘッダの取り込み
// --------------------------------------------------
#include "config/mqtt_config.h"

    // --------------------------------------------------
    // #defineマクロ
    // --------------------------------------------------

    // --------------------------------------------------
    // #define関数マクロ
    // --------------------------------------------------

/**
 * @brief MCUのTicksとミリ秒を変換するために使用する
 */
#define MILLISECONDS_PER_SECOND (1000U)
#define MILLISECONDS_PER_TICK   (MILLISECONDS_PER_SECOND / configTICK_RATE_HZ)

/**
 * @brief MQTTの接続リトライを実質無限回繰り返す際に指定するDefine
 */
#define MQTT_CONNECT_RETRY_REPEAT_AD_INFINITUM ((uint32_t)(0xFFFFFFFF))

    // --------------------------------------------------
    // typedef定義
    // --------------------------------------------------

    // --------------------------------------------------
    // enumタグ定義（typedefを同時に行う）
    // --------------------------------------------------

    /**
     * @brief 本ライブラリの実行結果
     */
    typedef enum
    {
        /**
         * @brief 成功
         */
        MQTT_OPERATION_TASK_RESULT_SUCCESS = 0,

        /**
         * @brief 失敗
         */
        MQTT_OPERATION_TASK_RESULT_FAILED = 1,

        /**
         * @brief MQTT接続されておらず、指定されたオペレーションを実行できない
         */
        MQTT_OPERATION_TASK_RESULT_NOT_MQTT_CONNECTED = 2
    } MQTTOperationTaskResult_t;

    typedef enum
    {
        /**
         * @brief MQTT プロビジョニング用に使用する
         */
        MQTT_THING_NAME_TYPE_PROVISIONING = 0,

        /**
         * @brief 通常用途で使用する
         */
        MQTT_THING_NAME_TYPE_USUAL = 1
    } MQTTThingNameType;

    /**
     * @brief NetworkContextを定義
     */
    struct NetworkContext
    {
        SecureSocketsTransportParams_t *pParams;
    };

    /**
     * @brief MQTTAgentCommandContext_tの定義
     */
    struct MQTTAgentCommandContext
    {
        TaskHandle_t xNotifyTaskHandle;
        void *pxArgs;
    };

    /**
     * @brief MQTTAgentのコマンド終了時に渡す引数
     */
    typedef struct
    {
        /**
         * @brief サブスクライブ時のトピック名等
         *
         * @see MQTTAgentSubscribeArgs_t
         */
        MQTTAgentSubscribeArgs_t *pxMQTTAgentSubscribeArgs;

        /**
         * @brief Subscribeしたトピックを受信した時に発火するCallback関数
         *
         * @see  IncomingPubCallback_t
         */
        IncomingPubCallback_t *pxMQTTSubscribeIncomingPubCallback;

        /**
         * @brief Callback関数がCallされた際に渡される引数
         */
        void *pxIncomingCallbackContext;
    } MQTTCommandDoneArgs_t;

    /**
     * @brief 本ライブラリで使用するMQTTやその下位レイヤのコンテキストをまとめた構造体
     */
    typedef struct
    {
        SecureSocketsTransportParams_t xSecureSocketsTransportParams; /**< ソケット層で利用するコンテキスト*/
        ServerInfo_t xServerInfo;                                     /**< URL等のサーバー接続情報*/
        SocketsConfig_t xSocketsConfig;                               /**< ソケットのコンフィグ情報*/
        NetworkContext_t xNetworkContext;                             /**< ネットワークのコンテキスト */
        TransportInterface_t xTransport;                              /**< トランスポート層へアクセスするためのインターフェース */
        MQTTAgentContext_t xMqttAgentContext;                         /**< MQTTのコンテキスト */
        MQTTAgentMessageContext_t xMQTTAgentMsgContext;               /**< MQTTのメッセージコンテキスト */
        MQTTAgentMessageInterface_t xMsgInterface;                    /**< MQTTのメッセージインターフェース */
        MQTTConnectInfo_t xMQTTConnectInfo;                           /**< MQTT接続に必要な情報 */
        MQTTFixedBuffer_t xFixedBuffer;                               /**< MQTTで利用するバッファ。uxBufferをバッファとする。  */
        uint8_t uxBuffer[MQTT_BUFFER_SIZE];                           /**< MQTT通信で使用するバッファ */

    } MQTTCommunicationContext_t;

    /**
     * @brief Staticで宣言するべきUnsubscribeに必要なバッファ
     */
    typedef struct
    {
        MQTTAgentCommandContext_t xMQTTCommandContext; /**< MQTTコマンドコンテキスト*/
        MQTTCommandDoneArgs_t xMQTTCommandDoneArgs;    /**< MQTTコマンド終了時にコールバック関数に渡されるコンテキスト*/
        MQTTAgentCommandInfo_t xMQTTCommandInfo;       /**< MQTTコマンドを実行する際に使用する情報*/
        MQTTAgentSubscribeArgs_t xSubscribeArgs;       /**< Subscribeの情報*/
    } StaticMQTTUnsubscribeCommandBuffer_t;

    /**
     * @brief Staticで宣言するべきSubscribeに必要なバッファ
     */
    typedef struct
    {
        MQTTAgentCommandContext_t xMQTTCommandContext; /**< MQTTコマンドコンテキスト*/
        MQTTCommandDoneArgs_t xMQTTCommandDoneArgs;    /**< MQTTコマンド終了時にコールバック関数に渡されるコンテキスト*/
        MQTTAgentCommandInfo_t xMQTTCommandInfo;       /**< MQTTコマンドを実行する際に使用する情報*/
        MQTTAgentSubscribeArgs_t xSubscribeArgs;       /**< Subscribeの情報*/
    } StaticMQTTSubscribeCommandBuffer_t;

    /**
     * @brief Staticで宣言するべきPublishに必要なバッファ
     */

    typedef struct
    {
        MQTTAgentCommandInfo_t xMQTTAgentCommandInfo; /**< コマンド情報 */
        MQTTAgentCommandContext_t xMQTTAgentCommand;  /**< コマンドが完了した際のコールバックに使われるコンテキスト */
    } StaticMQTTPublishCommandBuffer_t;

    /**
     * @brief MQTTコマンドに必要なバッファ
     */
    typedef struct
    {
        union
        {
            StaticMQTTUnsubscribeCommandBuffer_t xUnsubscribe; /**< Unsubscribeに必要な情報 */
            StaticMQTTSubscribeCommandBuffer_t xSubscribe;     /**< Subscribeに必要な情報 */
            StaticMQTTPublishCommandBuffer_t xPublish;         /**< Publishに必要な情報 */
        } u;

    } StaticMQTTCommandBuffer_t;

    // --------------------------------------------------
    // struct/unionタグ定義（typedefを同時に行う）
    // --------------------------------------------------

    /**
     * @brief MQTTコネクション実施時のコールバック関数群
     */
    typedef struct
    {

        /**
         * @brief MQTTコネクション失敗時に呼び出され、MQTT接続リトライを継続するか判定する
         *
         * @param[in] uxRetryCount リトライ回数. 0は、初回接続失敗
         *
         * @retval #true  接続リトライを続ける
         * @retval #false 接続リトライを中断
         */
        bool (*bRejects)(const uint32_t uxRetryCount);
    } MQTTConnectRejectConditionFunction_t;

    // --------------------------------------------------
    // extern変数宣言
    // --------------------------------------------------

    // --------------------------------------------------
    // 関数プロトタイプ宣言
    // --------------------------------------------------

    /**
     * @brief 本ライブラリを初期化する
     *
     * @note
     * この関数は、本ライブラリの中で一番最初に呼び出す必要がある
     *
     * @warning
     * 本APIはスレッドセーフでない。wake_up_task以外から呼び出してはいけない。
     *
     * @retval #MQTT_OPERATION_TASK_RESULT_SUCCESS 成功
     * @retval #MQTT_OPERATION_TASK_RESULT_FAILED  失敗
     */
    MQTTOperationTaskResult_t eMQTTCommunicationInit(void);

    /**
     * @brief AWS IoTにMQTT接続を行い、Pub/Subコマンドを処理するMQTT Taskを作成する。
     *
     * @note
     * MQTT接続に必要な情報、例えばAWSIoTのエンドポイントとはSEから取得する
     *
     * @warning
     * 本APIはスレッドセーフでない。 device_mode_switch_task または provisioning_task 以外から呼び出してはいけない。
     *
     * @param[in] eThingNameType           接続する際のMQTT Client ID Type
     * @param[in] uxRetryNum               リトライ回数。1以上を指定する。1を指定した場合は、1回だけ接続を試みてリトライは行わない。
     *                                     リトライを際限なく続けたい場合は #MQTT_CONNECT_RETRY_REPEAT_AD_INFINITUM を使用する
     * @param[in] pxRetryConditionFunction リトライを途中で中断するか判断する関数。NULL可。
     *
     * @retval #MQTT_OPERATION_TASK_RESULT_SUCCESS 成功
     * @retval #MQTT_OPERATION_TASK_RESULT_FAILED  失敗
     */
    MQTTOperationTaskResult_t eMQTTConnectToAWSIoT(const MQTTThingNameType eThingNameType,
                                                   const uint32_t uxRetryNum,
                                                   const MQTTConnectRejectConditionFunction_t *pxRetryConditionFunction);

    /**
     * @brief MQTT Publishを行う
     *
     * @warning この関数は #eMQTTConnectToAWSIoT を呼び出す必要がある
     *
     * @details
     * core_mqtt_agent.h の MQTTAgent_Publish を呼び出す。
     *
     * @param[in] pxPublishInfo   Publishに必要な情報 @ref MQTTPublishInfo_t
     * @param[in] pxContextBuffer コマンド終了時のコンテキストを維持するために使用するバッファ。この変数はSubscribe関数が終了した後も永続化する必要がある。
     *
     *
     * @retval #MQTT_OPERATION_TASK_RESULT_SUCCESS            成功
     * @retval #MQTT_OPERATION_TASK_RESULT_FAILED             失敗
     * @retval #MQTT_OPERATION_TASK_RESULT_NOT_MQTT_CONNECTED MQTT接続が行われていない
     */
    MQTTOperationTaskResult_t eMQTTpublish(const MQTTPublishInfo_t *pxPublishInfo, StaticMQTTCommandBuffer_t *pxContextBuffer);

    /**
     * @brief MQTT Subscribeを行う
     *
     * @note この関数は #eMQTTConnectToAWSIoT を呼び出す必要がある
     *
     * @details
     * core_mqtt_agent.h の MQTTAgent_Subscribe を呼び出す。
     *
     * @param[in] pxSubscribeInfo           Subscribeに必要な情報 @ref MQTTSubscribeInfo_t
     * @param[in] xIncomingCallback         SubscribeしたTopicからデータを受信した場合のコールバック関数
     * @param[in] pxIncomingCallbackContext Topicからデータを受信した場合のコールバック関数に渡される引数
     * @param[in] pxContextBuffer           コマンド終了時のコンテキストを維持するために使用するバッファ。この変数はSubscribe関数が終了した後も永続化する必要がある。
     *
     * @warning
     * - xIncomingCallback
     *   このコールバックはMQTT Taskのコンテキストで実行される。他のSubscribeしたTopicを受信出来なくなってしまうため、
     *   コールバック関数内で時間がかかる処理をしてはいけない。
     * - pxIncomingCallbackContext
     *   この構造体のインスタンスとそれが指す変数は、Callback関数が実行されるまで、参照可能である必要がある。
     *   つまり、この構造体はコールバックがコールされるまでスコープを維持するか、動的または静的領域にメモリ確保する必要がある。
     *
     * @retval #MQTT_OPERATION_TASK_RESULT_SUCCESS            成功
     * @retval #MQTT_OPERATION_TASK_RESULT_FAILED             失敗
     * @retval #MQTT_OPERATION_TASK_RESULT_NOT_MQTT_CONNECTED MQTT接続が行われていない
     */
    MQTTOperationTaskResult_t eMQTTSubscribe(const MQTTSubscribeInfo_t *pxSubscribeInfo,
                                             IncomingPubCallback_t xIncomingCallback,
                                             void *pxIncomingCallbackContext,
                                             StaticMQTTCommandBuffer_t *pxContextBuffer);

    /**
     * @brief SubscribeしたトピックをUnsubscribeする
     *
     * @note この関数は #eMQTTConnectToAWSIoT を呼び出す必要がある
     *
     * @param[in] pxSubscribeInfo #eMQTTSubscribe で使用した eMQTTSubscribe
     * @param[in] pxContextBuffer コマンド終了時のコンテキストを維持するために使用するバッファ。この変数はSubscribe関数が終了した後も永続化する必要がある。
     *
     * @retval #MQTT_OPERATION_TASK_RESULT_SUCCESS            成功
     * @retval #MQTT_OPERATION_TASK_RESULT_FAILED             失敗
     * @retval #MQTT_OPERATION_TASK_RESULT_NOT_MQTT_CONNECTED MQTT接続が行われていない
     */
    MQTTOperationTaskResult_t eMQTTUnsubscribe(const MQTTSubscribeInfo_t *pxSubscribe, StaticMQTTCommandBuffer_t *pxContextBuffer);

    /**
     * @brief MQTT Disconnectを行った後MQTT Taskを終了する
     *
     * @details
     * core_mqtt_agent.h の MQTTAgent_Terminate を呼びだし、その後MQTT Taskを自然に終了するようにする。
     *
     * @note
     * 本APIは #eMQTTConnectToAWSIoT を呼び出したタスクが呼び出す必要がある。
     *
     * @code{c}
     *
     * // Taskの開始
     * eMQTTConnectToAWSIoT(
     *     MQTT_THING_NAME_TYPE_USUAL,
     *     MQTT_CONNECT_RETRY_REPEAT_AD_INFINITUM,
     *     NULL
     * );
     *
     * // Pub/Subなど
     *
     * // 必ず eMQTTConnectToAWSIoT を呼び出したタスク内で呼び出す必要がある
     * eMQTTDisconnectAndTaskShutdown();
     *
     * @endcode
     *
     * @retval #MQTT_OPERATION_TASK_RESULT_SUCCESS 成功
     * @retval #MQTT_OPERATION_TASK_RESULT_FAILED  失敗
     */
    MQTTOperationTaskResult_t eMQTTDisconnectAndTaskShutdown(void);

    // --------------------------------------------------
    // インライン関数
    // --------------------------------------------------

#ifdef __cplusplus
}
#endif

#endif /* end MQTT_OPERATION_TASK_H_ */
