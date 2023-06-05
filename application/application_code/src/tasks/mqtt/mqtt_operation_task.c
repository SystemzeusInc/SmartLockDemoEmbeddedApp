/**
 * @file mqtt_operation_task.c
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

#include "transport_interface.h"
#include "transport_secure_sockets.h"
#include "iot_secure_sockets.h"
#include "core_mqtt_agent.h"
#include "core_mqtt_serializer.h"
#include "freertos_agent_message.h"
#include "freertos_command_pool.h"
#include "freertos_agent_message.h"

#include "mqtt_subscription_manager.h"

#include "aws_clientcredential.h"

// --------------------------------------------------
// ユーザ作成ヘッダの取り込み
// --------------------------------------------------

#include "common/include/application_define.h"
#include "config/task_config.h"
#include "config/queue_config.h"
#include "config/mqtt_config.h"

#include "tasks/mqtt/include/mqtt_operation_task.h"
#include "tasks/flash/include/flash_data.h"
#include "tasks/flash/include/flash_task.h"
// --------------------------------------------------
// 自ファイル内でのみ使用する#defineマクロ
// --------------------------------------------------

// --------------------------------------------------
// 自ファイル内でのみ使用する#define関数マクロ
// --------------------------------------------------

/**
 * @brief SocketConnectをリトライするまでの待機時間
 */
#define SOCKET_CONNECT_WAITE_TIME_MS 100

// --------------------------------------------------
// 自ファイル内でのみ使用するtypedef定義
// --------------------------------------------------

// --------------------------------------------------
// 自ファイル内でのみ使用するenumタグ定義（typedefを同時に行う）
// --------------------------------------------------

/**
 * @brief MQTT Taskに渡すパラメータ
 */
typedef struct
{
    /**
     * @brief MQTT Agentのコンテキスト
     */
    MQTTAgentContext_t *pxMqttAgentContext;

    /**
     * @brief ネットワークレイヤのコンテキスト
     */
    NetworkContext_t *pxNetworkContext;

} MQTTTaskParameters_t;

// --------------------------------------------------
// 自ファイル内でのみ使用するstruct/unionタグ定義（typedefを同時に行う）
// --------------------------------------------------

// --------------------------------------------------
// ファイル内で共有するstatic変数宣言
// --------------------------------------------------

/**
 * @brief MQTTTaskのハンドル
 */
static TaskHandle_t gxMQTTTaskHandle = NULL;

/**
 * @brief eMQTTCommunicationInitを行った時間を記録しておくために使用する
 */
static uint32_t guxGlobalEntryTimeMs = 0;

/**
 * @brief グローバルで使用するMQTTコンテキスト
 */
static MQTTCommunicationContext_t gxMQTTCommunicationContext;

/**
 * @brief MQTTSubscribeを管理する配列
 */
static SubscriptionElement_t gxSubscribeElementList[MQTT_MAX_SUBSCRIBE_NUM];

/**
 * @brief MQTT に接続するために使用するClientID(ThingName)
 */
static uint8_t gxMQTTClientID[THING_NAME_LENGTH + 1] = {0x00};

#if FACTORY_THING_NAME_LENGTH > THING_NAME_LENGTH

/**
 * FACTORY_THING_NAME_LENGTHよりTHING_NAME_LENGTHの方が大きい前提でコーディングしている。もし、FACTORY_THING_NAME_LENGTHの方が大きくなる場合は、
 * gxMQTTClientIDをFACTORY_THING_NAME_LENGTHの長さで定義する
 */
#    error "Coding with the assumption that THING_NAME_LENGTH is smaller than FACTORY_THING_NAME_LENGTH, making it an error"
#endif

/**
 * @brief IoTEndpoint
 */
static AWSIoTEndpoint_t gxIoTEndpoint;

// --------------------------------------------------
// static関数プロトタイプ宣言
// --------------------------------------------------

/**
 * @brief FreeRTOSのタスクスケジューラーが動作を開始してから経過したMSを取得する
 *
 * @return uint32_t 経過時間
 */
static uint32_t prvGetTimeMs(void);

/**
 * @brief Publishが成功した際に呼び出されるCallback関数
 *
 */
static void vprvIncomingPublishCallback(MQTTAgentContext_t *pMqttAgentContext,
                                        uint16_t packetId,
                                        MQTTPublishInfo_t *pxPublishInfo);

/**
 * @brief MQTTタスクのエントリーポイント
 *
 * @details
 * MQTT_ProcessLoopをループする。
 *
 * @param[in] pvParams MQTTとネットワークのコンテキスト。#MQTTTaskParameters_t にキャストして使用する
 */
static void vprvMQTTTask(void *pvParams);

/**
 * @brief TCPの接続を終了する
 *
 * @param[in] pxNetworkContext ネットワークコンテキスト
 *
 * @retval true  切断成功
 * @retval false 切断失敗
 */
static bool bprvSocketDisconnect(const NetworkContext_t *pxNetworkContext);

/**
 * @brief MQTTAgentのコマンド（Subscribeを除く）が終了したとことを検知するCallback関数
 *
 * @param[in] pCmdCallbackContext MQTTAgentコマンドを呼び出した際に指定したコンテキスト
 * @param[in] pReturnInfo         コマンドの実行結果
 */
static void vprvMQTTCommandDoneCallback(MQTTAgentCommandContext_t *pCmdCallbackContext, MQTTAgentReturnInfo_t *pReturnInfo);

/**
 * @brief MQTTAgentのSubscribeコマンドが終了したことを検知するCallback関数
 *
 * @param[in] pCmdCallbackContext MQTTAgent_Subscribe()を呼び出した際に指定したコンテキスト
 * @param[in] pReturnInfo         Subscribeコマンドの実行結果
 */
static void vprvMQTTSubscribeCommandDoneCallback(MQTTAgentCommandContext_t *pCmdCallbackContext, MQTTAgentReturnInfo_t *pReturnInfo);

/**
 * @brief MQTTAgentのUnsubscribeコマンドが終了したことを検知するCallback関数
 *
 * @param[in] pCmdCallbackContext MQTTAgent_Unsubscribe()を呼び出した際に指定したコンテキスト
 * @param[in] pReturnInfo         Subscribeコマンドの実行結果
 */
static void vprvMQTTUnsubscribeCommandDoneCallback(MQTTAgentCommandContext_t *pCmdCallbackContext, MQTTAgentReturnInfo_t *pReturnInfo);

/**
 * @brief TaskNotifyを待機する
 *
 * @param[in] xTimeoutMs タイムアウトする時間
 *
 * @retval true  成功
 * @retval false 失敗。タイムアウトも含む。
 */
static bool bprvWaitTaskNotify(TickType_t xTimeoutMs);

/**
 * @brief リトライありのソケットレイヤのコネクトを実施する
 *
 * @param[in] pNetworkContext           ネットワークコンテキスト
 * @param[in] pServerInfo               接続先情報
 * @param[in] pSocketsConfig            ソケットコンフィグ
 * @param[in] uxMaxRetry                最大リトライ回数
 * @param[in] pxRetryConditionFunction  最大リトライ回数を待たずに中断するための判定関数
 *
 * @retval true  接続成功
 * @retval false 接続失敗
 */
static bool bprvSocketConnectWithRetry(NetworkContext_t *pNetworkContext,
                                       const ServerInfo_t *pServerInfo,
                                       const SocketsConfig_t *pSocketsConfig,
                                       const uint32_t uxMaxRetry,
                                       const MQTTConnectRejectConditionFunction_t *pxRetryConditionFunction);

/**
 * @brief ThingNameとIoTEndpointをFlashから取得する
 *
 * @param[in]  eThingNameType 接続する際のMQTT Client ID Type
 * @param[out] pucThingName   ThingNameを格納するバッファ
 * @param[out] pucIoTEndpoint IoT Endpointを格納するバッファ
 *
 * @return true  成功
 * @return false 失敗
 */
static bool bprvGetMQTTInfoFromFlash(const MQTTThingNameType eThingNameType, uint8_t *pucThingName, uint8_t *pucIoTEndpoint);

// --------------------------------------------------
// 変数定義（staticを除く）
// --------------------------------------------------

// --------------------------------------------------
// 関数定義（staticを除く）
// --------------------------------------------------

MQTTOperationTaskResult_t eMQTTCommunicationInit(void)
{

    // MQTT Subscriptionを初期化
    memset(gxSubscribeElementList, 0x00, sizeof(SubscriptionElement_t) * MQTT_MAX_SUBSCRIBE_NUM);

    // MQTT Agentで使用するQueueの作成
    QueueHandle_t xHandle = xQueueCreate(MQTT_AGENT_COMMAND_QUEUE_LENGTH,
                                         sizeof(MQTTAgentCommand_t *));
    // xMsgInterfaceの初期化
    gxMQTTCommunicationContext.xMQTTAgentMsgContext.queue = xHandle;
    gxMQTTCommunicationContext.xMsgInterface.pMsgCtx = &gxMQTTCommunicationContext.xMQTTAgentMsgContext;
    gxMQTTCommunicationContext.xMsgInterface.send = &Agent_MessageSend;
    gxMQTTCommunicationContext.xMsgInterface.recv = &Agent_MessageReceive;
    gxMQTTCommunicationContext.xMsgInterface.getCommand = &Agent_GetCommand;
    gxMQTTCommunicationContext.xMsgInterface.releaseCommand = &Agent_ReleaseCommand;
    Agent_InitializePool();

    // MQTTが使用するネットワークインターフェース設定
    memset(&gxMQTTCommunicationContext.xMqttAgentContext, 0x00, sizeof(gxMQTTCommunicationContext.xMqttAgentContext));
    memset(&gxMQTTCommunicationContext.xTransport, 0x00, sizeof(gxMQTTCommunicationContext.xTransport));
    gxMQTTCommunicationContext.xTransport.pNetworkContext = &gxMQTTCommunicationContext.xNetworkContext;
    gxMQTTCommunicationContext.xTransport.send = SecureSocketsTransport_Send;
    gxMQTTCommunicationContext.xTransport.recv = SecureSocketsTransport_Recv;

    // ネットワークバッファの初期化
    gxMQTTCommunicationContext.xFixedBuffer.pBuffer = &(gxMQTTCommunicationContext.uxBuffer[0]);
    gxMQTTCommunicationContext.xFixedBuffer.size = sizeof(gxMQTTCommunicationContext.uxBuffer);

    // MQTT Agentの初期化
    MQTTStatus_t eStatus = MQTTAgent_Init(&gxMQTTCommunicationContext.xMqttAgentContext,
                                          &gxMQTTCommunicationContext.xMsgInterface,
                                          &gxMQTTCommunicationContext.xFixedBuffer,
                                          &gxMQTTCommunicationContext.xTransport,
                                          &prvGetTimeMs,
                                          &vprvIncomingPublishCallback,
                                          &gxSubscribeElementList[0]);

    // MQTTを初期化した時間を記録
    guxGlobalEntryTimeMs = prvGetTimeMs();

    if (eStatus != MQTTSuccess)
    {
        APP_PRINTFError("MQTTCommunicationInit failed. Reason: %d", eStatus);
        return MQTT_OPERATION_TASK_RESULT_FAILED;
    }

    APP_PRINTFDebug("MQTTCommunicationInit finished.");
    return MQTT_OPERATION_TASK_RESULT_SUCCESS;
}

MQTTOperationTaskResult_t eMQTTConnectToAWSIoT(const MQTTThingNameType eThingNameType,
                                               const uint32_t uxRetryNum,
                                               const MQTTConnectRejectConditionFunction_t *pxRetryConditionFunction)
{

    // Flashからデータを取得
    memset(gxMQTTClientID, '\0', sizeof(gxMQTTClientID));
    memset(&gxIoTEndpoint, 0x00, sizeof(gxIoTEndpoint));
    if (bprvGetMQTTInfoFromFlash(eThingNameType, gxMQTTClientID, gxIoTEndpoint.ucEndpoint) == false)
    {
        APP_PRINTFError("Read flash error.");
        return MQTT_OPERATION_TASK_RESULT_FAILED;
    }

    // ----- TLS socket connect ----
    memset(&gxMQTTCommunicationContext.xSecureSocketsTransportParams, 0x00, sizeof(gxMQTTCommunicationContext.xSecureSocketsTransportParams));
    gxMQTTCommunicationContext.xNetworkContext.pParams = &gxMQTTCommunicationContext.xSecureSocketsTransportParams;

    // 接続先情報の格納
    gxMQTTCommunicationContext.xServerInfo.pHostName = (const char *)gxIoTEndpoint.ucEndpoint;
    gxMQTTCommunicationContext.xServerInfo.hostNameLength = strlen((const char *)gxIoTEndpoint.ucEndpoint);
    gxMQTTCommunicationContext.xServerInfo.port = AWS_IOT_MQTT_PORT;

    // ソケットの設定
    memset(&gxMQTTCommunicationContext.xSocketsConfig, 0x00, sizeof(gxMQTTCommunicationContext.xSocketsConfig));
    gxMQTTCommunicationContext.xSocketsConfig.enableTls = true;
    gxMQTTCommunicationContext.xSocketsConfig.pAlpnProtos = NULL;
    gxMQTTCommunicationContext.xSocketsConfig.maxFragmentLength = 0;
    gxMQTTCommunicationContext.xSocketsConfig.disableSni = false;
    gxMQTTCommunicationContext.xSocketsConfig.pRootCa = NULL;
    gxMQTTCommunicationContext.xSocketsConfig.rootCaSize = 0;
    gxMQTTCommunicationContext.xSocketsConfig.sendTimeoutMs = SOCKET_SEND_RECV_TIMEOUT_MS;
    gxMQTTCommunicationContext.xSocketsConfig.recvTimeoutMs = SOCKET_SEND_RECV_TIMEOUT_MS;

    // Socket connect
    APP_PRINTFDebug("Connect socket...");
    if (bprvSocketConnectWithRetry(&gxMQTTCommunicationContext.xNetworkContext,
                                   &gxMQTTCommunicationContext.xServerInfo,
                                   &gxMQTTCommunicationContext.xSocketsConfig,
                                   uxRetryNum,
                                   pxRetryConditionFunction) == false)
    {
        APP_PRINTFError("Failed to connect socket.");
        return MQTT_OPERATION_TASK_RESULT_FAILED;
    }

    // ----- MQTT connect ----
    bool bSessionPresent = false;
    memset(&(gxMQTTCommunicationContext.xMQTTConnectInfo), 0x00, sizeof(gxMQTTCommunicationContext.xMQTTConnectInfo));
    gxMQTTCommunicationContext.xMQTTConnectInfo.cleanSession = true;
    gxMQTTCommunicationContext.xMQTTConnectInfo.pClientIdentifier = (const char *)gxMQTTClientID;
    gxMQTTCommunicationContext.xMQTTConnectInfo.clientIdentifierLength = strlen((const char *)gxMQTTClientID);
    gxMQTTCommunicationContext.xMQTTConnectInfo.keepAliveSeconds = MQTT_KEEP_ALIVE_INTERVAL_SECONDS;

    APP_PRINTFDebug("Connect mqtt... Client ID: %s", gxMQTTCommunicationContext.xMQTTConnectInfo.pClientIdentifier);
    // MQTT接続
    // MEMO:
    // 既にソケット接続が終わっているため、この時点でMQTT接続が失敗する可能性は低い。したがってMQTT接続自体のリトライは行わない。
    if (MQTT_Connect((MQTTContext_t *)(&(gxMQTTCommunicationContext.xMqttAgentContext)),
                     (const MQTTConnectInfo_t *)(&(gxMQTTCommunicationContext.xMQTTConnectInfo)),
                     NULL,
                     MQTT_CONNECT_TIMEOUT_MS,
                     &bSessionPresent) != MQTTSuccess)
    {
        APP_PRINTFError("Failed to connect mqtt.");
        return MQTT_OPERATION_TASK_RESULT_FAILED;
    }

    if (gxMQTTTaskHandle == NULL)
    {
        // MQTT Taskの作成
        static MQTTTaskParameters_t xMQTTTaskParam; // タスクに渡すパラメータになるため、static領域に変数を宣言する
        memset(&xMQTTTaskParam, 0x00, sizeof(xMQTTTaskParam));
        xMQTTTaskParam.pxMqttAgentContext = &(gxMQTTCommunicationContext.xMqttAgentContext);
        xMQTTTaskParam.pxNetworkContext = &(gxMQTTCommunicationContext.xNetworkContext);

        if (xTaskCreate(&vprvMQTTTask,
                        "MQTT Task",
                        MQTT_TASK_STACK_SIZE,
                        &xMQTTTaskParam,
                        MQTT_TASK_PRIORITY,
                        &gxMQTTTaskHandle) == pdFAIL)
        {
            APP_PRINTFError("MQTT task create failed.");
            return MQTT_OPERATION_TASK_RESULT_FAILED;
        }
    }

    // MQTT Subscriptionを初期化
    memset(gxSubscribeElementList, 0x00, sizeof(SubscriptionElement_t) * MQTT_MAX_SUBSCRIBE_NUM);

    APP_PRINTFDebug("MQTT Connect To AWSIoT finished.");
    return MQTT_OPERATION_TASK_RESULT_SUCCESS;
}

MQTTOperationTaskResult_t eMQTTpublish(const MQTTPublishInfo_t *pxPublishInfo, StaticMQTTCommandBuffer_t *pxContextBuffer)
{
    // MQTTに接続しているか調査
    if (gxMQTTCommunicationContext.xMqttAgentContext.mqttContext.connectStatus != MQTTConnected)
    {
        APP_PRINTFWarn("MQTT is not connected");
        return MQTT_OPERATION_TASK_RESULT_NOT_MQTT_CONNECTED;
    }

    // MQTTPublishに必要なCallbackを登録
    memset(pxContextBuffer, 0x00, sizeof(StaticMQTTCommandBuffer_t));
    MQTTAgentCommandInfo_t *pxAgentCommandInfo = &(pxContextBuffer->u.xPublish.xMQTTAgentCommandInfo);
    MQTTAgentCommandContext_t *pxCommandContext = &(pxContextBuffer->u.xPublish.xMQTTAgentCommand);
    pxCommandContext->xNotifyTaskHandle = xTaskGetCurrentTaskHandle();

    pxAgentCommandInfo->cmdCompleteCallback = &vprvMQTTCommandDoneCallback;
    pxAgentCommandInfo->pCmdCompleteCallbackContext = pxCommandContext;
    pxAgentCommandInfo->blockTimeMs = MQTT_TASK_COMMAND_ENQUEUE_TIMEOUT_MS;

    // MQTT Agentに対してPublish
    MQTTStatus_t xMQTTResult = MQTTAgent_Publish((const MQTTAgentContext_t *)(&(gxMQTTCommunicationContext.xMqttAgentContext)),
                                                 (MQTTPublishInfo_t *)pxPublishInfo,
                                                 (const MQTTAgentCommandInfo_t *)pxAgentCommandInfo);
    if (xMQTTResult != MQTTSuccess)
    {
        APP_PRINTFError("MQTT publish error. Reason: %d", xMQTTResult);
        return MQTT_OPERATION_TASK_RESULT_FAILED;
    }

    APP_PRINTFDebug("MQTT publish command send success. Waiting for publish done...");

    // PublishのCallbackを待機
    bool bIsSuccess = bprvWaitTaskNotify(MQTT_PUB_SUB_TIMEOUT_MS);

    if (bIsSuccess == false)
    {
        APP_PRINTFError("MQTT publish timeout.");

        // コンテキストをNULLにしておく
        pxCommandContext->xNotifyTaskHandle = NULL;
        return MQTT_OPERATION_TASK_RESULT_FAILED;
    }

    APP_PRINTFDebug("MQTT publish success. TOPIC: %s", pxPublishInfo->pTopicName);
    return MQTT_OPERATION_TASK_RESULT_SUCCESS;
}

MQTTOperationTaskResult_t eMQTTSubscribe(const MQTTSubscribeInfo_t *pxSubscribeInfo,
                                         IncomingPubCallback_t xIncomingCallback,
                                         void *pxIncomingCallbackContext,
                                         StaticMQTTCommandBuffer_t *pxContextBuffer)
{

    // MQTTに接続しているか調査
    if (gxMQTTCommunicationContext.xMqttAgentContext.mqttContext.connectStatus != MQTTConnected)
    {
        APP_PRINTFWarn("MQTT is not connected");
        return MQTT_OPERATION_TASK_RESULT_NOT_MQTT_CONNECTED;
    }

    // CallbackContextを初期化

    // MQTT Agentに対してSubscribe
    memset(pxContextBuffer, 0x00, sizeof(StaticMQTTUnsubscribeCommandBuffer_t));
    MQTTAgentCommandContext_t *pxAgentContext = &(pxContextBuffer->u.xSubscribe.xMQTTCommandContext);
    MQTTCommandDoneArgs_t *pxMQTTCommandDone = &(pxContextBuffer->u.xSubscribe.xMQTTCommandDoneArgs);
    MQTTAgentCommandInfo_t *pxMQTTAgentCommandInfo = &(pxContextBuffer->u.xSubscribe.xMQTTCommandInfo);
    MQTTAgentSubscribeArgs_t *pxSubscribeArgs = &(pxContextBuffer->u.xSubscribe.xSubscribeArgs);

    // Subscribeの情報を格納
    pxSubscribeArgs->numSubscriptions = 1; // Subscribe1つ分しか受け取っていないため、必ず1になる。
    pxSubscribeArgs->pSubscribeInfo = (MQTTSubscribeInfo_t *)pxSubscribeInfo;

    // Subscribeしたトピックに受信があったときに呼ばれるCallbackに渡す引数を格納
    pxMQTTCommandDone->pxIncomingCallbackContext = (void *)pxIncomingCallbackContext;
    pxMQTTCommandDone->pxMQTTSubscribeIncomingPubCallback = (IncomingPubCallback_t *)xIncomingCallback;
    pxMQTTCommandDone->pxMQTTAgentSubscribeArgs = pxSubscribeArgs;

    // Subscribe Commandが完了した際に呼ばれるコールバックの引数を格納
    pxAgentContext->pxArgs = pxMQTTCommandDone;
    pxAgentContext->xNotifyTaskHandle = xTaskGetCurrentTaskHandle();

    // Subscribe Commandが完了した際に呼ばれるコールバックを登録
    pxMQTTAgentCommandInfo->cmdCompleteCallback = &vprvMQTTSubscribeCommandDoneCallback;
    pxMQTTAgentCommandInfo->pCmdCompleteCallbackContext = (MQTTAgentCommandContext_t *)pxContextBuffer;
    pxMQTTAgentCommandInfo->blockTimeMs = MQTT_TASK_COMMAND_ENQUEUE_TIMEOUT_MS;

    // AgentにSubscribe Commandを送信
    MQTTStatus_t xMQTTResult = MQTTAgent_Subscribe(&(gxMQTTCommunicationContext.xMqttAgentContext),
                                                   pxSubscribeArgs,
                                                   pxMQTTAgentCommandInfo);

    if (xMQTTResult != MQTTSuccess)
    {
        APP_PRINTFError("MQTT subscribe failed: %d", xMQTTResult);
        return MQTT_OPERATION_TASK_RESULT_FAILED;
    }

    // MQTT wait
    APP_PRINTFDebug("MQTT subscribe command send success. Waiting for subscribe command done...");

    // SubscribeCommandのCallbackを待機
    bool bIsSuccess = bprvWaitTaskNotify(MQTT_PUB_SUB_TIMEOUT_MS);
    if (bIsSuccess == false)
    {
        APP_PRINTFError("MQTT subscribe command timeout.");

        // コンテキストをNULLにしておく
        pxAgentContext->xNotifyTaskHandle = NULL;
        return MQTT_OPERATION_TASK_RESULT_FAILED;
    }

    APP_PRINTFDebug("MQTT subscribe success. TOPIC: %s", pxSubscribeInfo->pTopicFilter);
    return MQTT_OPERATION_TASK_RESULT_SUCCESS;
}

MQTTOperationTaskResult_t eMQTTUnsubscribe(const MQTTSubscribeInfo_t *pxSubscribeInfo, StaticMQTTCommandBuffer_t *pxContextBuffer)
{
    // MQTTに接続しているか調査
    if (gxMQTTCommunicationContext.xMqttAgentContext.mqttContext.connectStatus != MQTTConnected)
    {
        APP_PRINTFWarn("MQTT is not connected");
        return MQTT_OPERATION_TASK_RESULT_NOT_MQTT_CONNECTED;
    }

    // CallbackContextを初期化
    memset(pxContextBuffer, 0x00, sizeof(StaticMQTTUnsubscribeCommandBuffer_t));

    MQTTAgentCommandContext_t *pxAgentContext = &(pxContextBuffer->u.xUnsubscribe.xMQTTCommandContext);
    MQTTCommandDoneArgs_t *pxMQTTCommandDone = &(pxContextBuffer->u.xUnsubscribe.xMQTTCommandDoneArgs);
    MQTTAgentCommandInfo_t *pxMQTTAgentCommandInfo = &(pxContextBuffer->u.xUnsubscribe.xMQTTCommandInfo);
    MQTTAgentSubscribeArgs_t *pxSubscribeArgs = &(pxContextBuffer->u.xSubscribe.xSubscribeArgs);

    // Unsubscribeの情報を格納
    pxSubscribeArgs->numSubscriptions = 1; // Subscribe1つ分しか受け取っていないため、必ず1になる。
    pxSubscribeArgs->pSubscribeInfo = (MQTTSubscribeInfo_t *)pxSubscribeInfo;

    // Subscribeしたトピックに受信があったときに呼ばれるCallbackに渡す引数を格納
    pxMQTTCommandDone->pxIncomingCallbackContext = NULL;
    pxMQTTCommandDone->pxMQTTSubscribeIncomingPubCallback = NULL;
    pxMQTTCommandDone->pxMQTTAgentSubscribeArgs = pxSubscribeArgs;

    // Unsubscribe Commandが完了した際に呼ばれるコールバックの引数を格納
    pxAgentContext->pxArgs = pxMQTTCommandDone;
    pxAgentContext->xNotifyTaskHandle = xTaskGetCurrentTaskHandle();

    // Subscribe Commandが完了した際に呼ばれるコールバックを登録
    pxMQTTAgentCommandInfo->cmdCompleteCallback = &vprvMQTTUnsubscribeCommandDoneCallback;
    pxMQTTAgentCommandInfo->pCmdCompleteCallbackContext = pxAgentContext;
    pxMQTTAgentCommandInfo->blockTimeMs = MQTT_TASK_COMMAND_ENQUEUE_TIMEOUT_MS;

    // AgentにUnsubscribe Commandを送信
    MQTTStatus_t xMQTTResult = MQTTAgent_Unsubscribe(&(gxMQTTCommunicationContext.xMqttAgentContext),
                                                     pxSubscribeArgs,
                                                     pxMQTTAgentCommandInfo);

    if (xMQTTResult != MQTTSuccess)
    {
        APP_PRINTFError("MQTT subscribe failed: %d", xMQTTResult);
        return MQTT_OPERATION_TASK_RESULT_FAILED;
    }

    // MQTT wait
    APP_PRINTFDebug("MQTT unsubscribe command send success. Waiting for unsubscribe command done...");

    // SubscribeCommandのCallbackを待機
    bool bIsSuccess = bprvWaitTaskNotify(MQTT_PUB_SUB_TIMEOUT_MS);

    if (bIsSuccess == false)
    {
        APP_PRINTFError("MQTT unsubscribe command timeout.");
        return MQTT_OPERATION_TASK_RESULT_FAILED;
    }

    APP_PRINTFDebug("MQTT unsubscribe success. TOPIC: %s", pxSubscribeInfo->pTopicFilter);
    return MQTT_OPERATION_TASK_RESULT_SUCCESS;
}

MQTTOperationTaskResult_t eMQTTDisconnectAndTaskShutdown(void)
{
    // 既にMQTTがNULLの場合は何もしない
    if (gxMQTTTaskHandle == NULL)
    {
        APP_PRINTFInfo("MQTT already disconnected.");
        return MQTT_OPERATION_TASK_RESULT_SUCCESS;
    }

    static MQTTAgentCommandContext_t xDisconnectDoneCallbackContext; // 他のタスクに渡すコンテキストになるため、static領域に生成する

    // MQTT Agentに渡すパラメータを格納
    memset(&xDisconnectDoneCallbackContext, 0x00, sizeof(xDisconnectDoneCallbackContext));
    xDisconnectDoneCallbackContext.xNotifyTaskHandle = xTaskGetCurrentTaskHandle();
    MQTTAgentCommandInfo_t xMQTTCommand = {
        .cmdCompleteCallback = &vprvMQTTCommandDoneCallback,
        .pCmdCompleteCallbackContext = &xDisconnectDoneCallbackContext,
        .blockTimeMs = MQTT_TASK_COMMAND_ENQUEUE_TIMEOUT_MS};

    // MQTTをDisconnectし、MQTTAgent_CommandLoopを終了させる
    MQTTStatus_t xMQTTResult = MQTTAgent_Terminate(&(gxMQTTCommunicationContext.xMqttAgentContext), &xMQTTCommand);

    if (xMQTTResult != MQTTSuccess)
    {
        APP_PRINTFError("MQTT terminate failed: %d", xMQTTResult);
        return MQTT_OPERATION_TASK_RESULT_FAILED;
    }

    APP_PRINTFDebug("MQTT terminate command send success. Waiting for terminate done...");

    // Callbackを待機
    bool bIsSuccess = bprvWaitTaskNotify(MQTT_CONNECT_TIMEOUT_MS + 1000); // 設定したタイムアウト時間より長い時間（1秒）待機する
    // Waitの後にTaskNotifyが呼び出されても問題ないようにTaskHandleをNullにする
    xDisconnectDoneCallbackContext.xNotifyTaskHandle = NULL;

    if (bIsSuccess == false)
    {
        APP_PRINTFError("MQTT terminate timeout.");
        return MQTT_OPERATION_TASK_RESULT_FAILED;
    }

    // MQTT Taskが終了するまで待機
    const uint32_t xWaitTimeOneLoopMS = 500U;
    const uint32_t xWaitingMaxCount = (uint32_t)(MQTT_CONNECT_TIMEOUT_MS / xWaitTimeOneLoopMS);
    uint32_t xWaitingCount;
    for (xWaitingCount = 0; xWaitingCount < xWaitingMaxCount; xWaitingCount++)
    {
        if (gxMQTTTaskHandle == NULL)
        {
            break;
        }
        APP_PRINTFDebug("MQTT task delete waiting....");
        vTaskDelay(xWaitTimeOneLoopMS);
    }

    if (xWaitingCount == xWaitingMaxCount)
    {
        APP_PRINTFError("MQTT task delete timeout.");
        return MQTT_OPERATION_TASK_RESULT_FAILED;
    }

    APP_PRINTFDebug("MQTT terminate success.");
    return MQTT_OPERATION_TASK_RESULT_SUCCESS;
}

// --------------------------------------------------
// static関数定義
// --------------------------------------------------

static bool bprvSocketConnectWithRetry(NetworkContext_t *pNetworkContext,
                                       const ServerInfo_t *pServerInfo,
                                       const SocketsConfig_t *pSocketsConfig,
                                       const uint32_t uxMaxRetry,
                                       const MQTTConnectRejectConditionFunction_t *pxRetryConditionFunction)
{
    TransportSocketStatus_t xTransportResult = TRANSPORT_SOCKET_STATUS_CONNECT_FAILURE;
    bool bIsRetryContinue = true;

    // 指定のリトライ回数になるまでリトライし続ける。
    for (uint32_t uxRetryCount = 0; uxRetryCount < uxMaxRetry; uxRetryCount++)
    {
        // ソケット接続
        xTransportResult = SecureSocketsTransport_Connect(pNetworkContext,
                                                          pServerInfo,
                                                          pSocketsConfig);

        if (xTransportResult == TRANSPORT_SOCKET_STATUS_SUCCESS)
        {
            APP_PRINTFDebug("Socket connection established");
            return true;
        }

        // 接続の中断が行われるか判定する
        if (pxRetryConditionFunction != NULL && pxRetryConditionFunction->bRejects != NULL)
        {
            bIsRetryContinue = pxRetryConditionFunction->bRejects(uxRetryCount + 1);

            // ソケット接続を中断する
            if (bIsRetryContinue == false)
            {
                APP_PRINTFWarn("Socket connection retry interrupted by decision function");
                return false;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(SOCKET_CONNECT_WAITE_TIME_MS));
    }

    APP_PRINTFError("Socket connect failed.");
    return false;
}

static uint32_t prvGetTimeMs(void)
{
    TickType_t xTickCount = 0;
    uint32_t uxTimes = 0UL;

    // FreeRTOSタスクスケジューラーが起動してからのTickを取得
    xTickCount = xTaskGetTickCount();

    // TickをMSに変換
    uxTimes = (uint32_t)xTickCount * MILLISECONDS_PER_TICK;

    // MQTTAgent_Init() を行ってからのミリ秒を算出したいため、guxGlobalEntryTimeMsを引く
    uxTimes = (uint32_t)(uxTimes - guxGlobalEntryTimeMs);

    return uxTimes;
}

static bool bprvSocketDisconnect(const NetworkContext_t *pxNetworkContext)
{
    APP_PRINTFDebug("Disconnecting TLS connection.");

    // ネットワークレイヤの切断
    TransportSocketStatus_t xNetworkStatus = SecureSocketsTransport_Disconnect(pxNetworkContext);

    return (xNetworkStatus == TRANSPORT_SOCKET_STATUS_SUCCESS) ? true : false;
}

static bool bprvWaitTaskNotify(TickType_t xTimeoutMs)
{
    BaseType_t xIsSuccess = ulTaskNotifyTake(pdTRUE, xTimeoutMs);
    return xIsSuccess == pdTRUE ? true : false;
}

static bool bprvGetMQTTInfoFromFlash(const MQTTThingNameType eThingNameType, uint8_t *pucThingName, uint8_t *pucIoTEndpoint)
{
    FlashTaskResult_t xFlashReadResult;

    // AWS IoT Endpointを取得
    AWSIoTEndpoint_t xAWSIoTEndpoint;
    memset(&xAWSIoTEndpoint, 0x00, sizeof(xAWSIoTEndpoint));
    xFlashReadResult = eReadFlashInfo(READ_FLASH_TYPE_AWS_IOT_ENDPOINT, &xAWSIoTEndpoint, sizeof(xAWSIoTEndpoint));
    if (xFlashReadResult != FLASH_TASK_RESULT_SUCCESS)
    {
        APP_PRINTFError("AWS IoT Endpoint read error. Reason: %d", xFlashReadResult);
        return false;
    }
    strncpy((char *)pucIoTEndpoint, (const char *)xAWSIoTEndpoint.ucEndpoint, AWS_IOT_ENDPOINT_MAX_LENGTH);

    // ThingNameを取得。タイプがプロビジョニングか普段使い用かで取得するAPIを変える
    if (eThingNameType == MQTT_THING_NAME_TYPE_PROVISIONING)
    {
        /* プロビジョニング用の場合 */

        FactoryThingName_t xFactoryThingName;
        memset(&xFactoryThingName, 0x00, sizeof(xFactoryThingName));
        xFlashReadResult = eReadFlashInfo(READ_FLASH_TYPE_FACTORY_THING_NAME, &xFactoryThingName, sizeof(xFactoryThingName));
        if (xFlashReadResult != FLASH_TASK_RESULT_SUCCESS)
        {
            APP_PRINTFError("Factory thing name read error. Reason: %d", xFlashReadResult);
            return false;
        }
        strncpy((char *)pucThingName, (const char *)xFactoryThingName.ucName, FACTORY_THING_NAME_LENGTH);
    }
    else
    {
        /* 普段使い用の場合 */

        ThingName_t xUsualThingName;
        memset(&xUsualThingName, 0x00, sizeof(xUsualThingName));
        xFlashReadResult = eReadFlashInfo(READ_FLASH_TYPE_USUAL_THING_NAME, &xUsualThingName, sizeof(xUsualThingName));
        if (xFlashReadResult != FLASH_TASK_RESULT_SUCCESS)
        {
            APP_PRINTFError("Usual thing name read error. Reason: %d", xFlashReadResult);
            return false;
        }
        strncpy((char *)pucThingName, (const char *)xUsualThingName.ucName, THING_NAME_LENGTH);
    }

    APP_PRINTFDebug("MQTT info read success. Endpoint: %s, ThingName %s", pucIoTEndpoint, pucThingName);
    return true;
}

// --------------- CALLBACKS ----------------

static void vprvIncomingPublishCallback(MQTTAgentContext_t *pMqttAgentContext,
                                        uint16_t packetId,
                                        MQTTPublishInfo_t *pxPublishInfo)
{
    (void)packetId; // このパラメータは使用しない

    APP_PRINTFDebug("Incoming Publish Callback.");

    // SubscriptionListに登録されているトピックに一致するコールバックを呼び出す
    const bool bCanCallback = SubscriptionManager_HandleIncomingPublishes((SubscriptionElement_t *)(pMqttAgentContext->pIncomingCallbackContext),
                                                                          pxPublishInfo);

    // 登録されているコールバック関数が見つからなかった場合はエラーを表示する
    if (bCanCallback == false)
    {
        APP_PRINTFError("Received an unsolicited publish; topic: %.*s.",
                        pxPublishInfo->topicNameLength, pxPublishInfo->pTopicName);
    }
}

static void vprvMQTTCommandDoneCallback(MQTTAgentCommandContext_t *pCmdCallbackContext,
                                        MQTTAgentReturnInfo_t *pReturnInfo)
{
    if (pReturnInfo->returnCode != MQTTSuccess)
    {
        APP_PRINTFError("MQTTCommandDoneCallback Error. Reason %d", pReturnInfo->returnCode);
        return;
    }

    if (pCmdCallbackContext->xNotifyTaskHandle == NULL)
    {
        APP_PRINTFWarn("Notify task handle is NULL");
        return;
    }

    // タスクに通知
    xTaskNotifyGive(pCmdCallbackContext->xNotifyTaskHandle);
}

static void vprvMQTTSubscribeCommandDoneCallback(MQTTAgentCommandContext_t *pCmdCallbackContext, MQTTAgentReturnInfo_t *pReturnInfo)
{
    if (pCmdCallbackContext->xNotifyTaskHandle == NULL)
    {
        APP_PRINTFWarn("Notify task handle is NULL");
        return;
    }

    // Command結果を確認
    if (pReturnInfo->returnCode == MQTTSuccess)
    {
        if (pCmdCallbackContext->pxArgs == NULL)
        {
            APP_PRINTFError("pxArgs is NULL");
            return;
        }

        // SubscriptionManagerに当該トピックとコールバック関数を登録する
        MQTTCommandDoneArgs_t *pxSubscribeArgs = (MQTTCommandDoneArgs_t *)pCmdCallbackContext->pxArgs;
        APP_PRINTFDebug("Register with SubscriptionManager for topic %s.",
                        pxSubscribeArgs->pxMQTTAgentSubscribeArgs->pSubscribeInfo->pTopicFilter);
        bool bHaveAdded = SubscriptionManager_AddSubscription(&gxSubscribeElementList[0],
                                                              pxSubscribeArgs->pxMQTTAgentSubscribeArgs->pSubscribeInfo->pTopicFilter,
                                                              pxSubscribeArgs->pxMQTTAgentSubscribeArgs->pSubscribeInfo->topicFilterLength,
                                                              pxSubscribeArgs->pxMQTTSubscribeIncomingPubCallback,
                                                              pxSubscribeArgs->pxIncomingCallbackContext);

        // Subscriptionリストへの追加失敗判定。1度にSubscribeできる上限に達した可能性がある。
        // 本エラーが発生した場合は MQTT_MAX_SUBSCRIBE_NUM を見直す必要がある
        if (bHaveAdded == false)
        {
            APP_PRINTFError("Failed to register an incoming publish callback for topic %s.",
                            pxSubscribeArgs->pxMQTTAgentSubscribeArgs->pSubscribeInfo->pTopicFilter);
        }
    }

    // タスクに通知
    xTaskNotifyGive(pCmdCallbackContext->xNotifyTaskHandle);
}

static void vprvMQTTUnsubscribeCommandDoneCallback(MQTTAgentCommandContext_t *pCmdCallbackContext, MQTTAgentReturnInfo_t *pReturnInfo)
{
    if (pCmdCallbackContext->xNotifyTaskHandle == NULL)
    {
        APP_PRINTFWarn("Notify task handle is NULL");
        return;
    }

    // Command結果を確認
    if (pReturnInfo->returnCode == MQTTSuccess)
    {
        // SubscriptionManagerに当該トピックを削除する
        MQTTCommandDoneArgs_t *pxSubscribeArgs = (MQTTCommandDoneArgs_t *)pCmdCallbackContext->pxArgs;
        APP_PRINTFDebug("Remove with SubscriptionManager for topic %s.",
                        pxSubscribeArgs->pxMQTTAgentSubscribeArgs->pSubscribeInfo->pTopicFilter);

        SubscriptionManager_RemoveSubscription(&gxSubscribeElementList[0],
                                               pxSubscribeArgs->pxMQTTAgentSubscribeArgs->pSubscribeInfo->pTopicFilter,
                                               pxSubscribeArgs->pxMQTTAgentSubscribeArgs->pSubscribeInfo->topicFilterLength);
    }

    // タスクに通知
    xTaskNotifyGive(pCmdCallbackContext->xNotifyTaskHandle);
}

// ------------------ TASK ------------------

static void vprvMQTTTask(void *pvParams)
{

    MQTTStatus_t xMQTTStatus = MQTTSuccess;
    bool bSocketDisconnectResult = false;

    // パラメータをMQTTTaskParameters_tにキャスト
    MQTTTaskParameters_t *pxContext = (MQTTTaskParameters_t *)pvParams;

    APP_PRINTFDebug("Context Memory 0x%p 0x%p", pxContext->pxMqttAgentContext, pxContext->pxNetworkContext);

    do
    {
        // MQTT Agentのコマンドを処理する。この関数は、MQTTやその下位レイヤの接続が切れるまでMQTTコマンドを処理しし続け、返却されることはない。
        APP_PRINTFDebug("Start MQTT Command Loop");
        xMQTTStatus = MQTTAgent_CommandLoop(pxContext->pxMqttAgentContext);

        if ((xMQTTStatus == MQTTSuccess) &&
            (pxContext->pxMqttAgentContext->mqttContext.connectStatus == MQTTNotConnected))
        {
            // 既にMQTTレイヤはDisconnectされているため、ソケットレイヤを切断する
            bSocketDisconnectResult = bprvSocketDisconnect(pxContext->pxNetworkContext);
        }
        else if (xMQTTStatus == MQTTSuccess)
        {
            // MQTTレイヤが切断されていないため、切断する
            APP_PRINTFDebug("MQTT disconnect...");
            xMQTTStatus = MQTT_Disconnect((MQTTContext_t *)pxContext->pxMqttAgentContext);
            bSocketDisconnectResult = bprvSocketDisconnect(pxContext->pxNetworkContext);
        }

        if ((xMQTTStatus != MQTTSuccess) || (bSocketDisconnectResult == false))
        {
            APP_PRINTFError("MQTT task error. Reason: %d", xMQTTStatus);
        }

    } while (false); // MQTTAgent_CommandLoop内部でループ処理が行われているためfalseで問題ない。

    APP_PRINTFDebug("MQTT task completed. Therefore, it is deleted.");

    PRINT_TASK_REMAINING_STACK_SIZE();
    // タスクハンドルを破棄
    gxMQTTTaskHandle = NULL;
    vTaskDelete(NULL);
}

// --------------------------------------------------
// Unit Test用関数定義(関数のプロトタイプ宣言は「自身のファイル名+ "_test.h"」で宣言されていること)
// --------------------------------------------------
#if (BUILD_MODE_TEST == 1) /* BUILD_MODE_TESTが定義されているとき */
#endif                     /* end  BUILD_MODE_TEST */
