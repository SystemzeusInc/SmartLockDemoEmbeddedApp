/**
 * @file ota_agent_task.c
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
#include "semphr.h"

#include "ota.h"
#include "ota_config.h"
#include "config/ota_app_config.h"
#include "ota_os_freertos.h"
#include "ota_pal.h"
#include "core_mqtt_serializer.h"

// --------------------------------------------------
// ユーザ作成ヘッダの取り込み
// --------------------------------------------------

#include "common/include/application_define.h"
#include "config/task_config.h"

#include "tasks/mqtt/include/mqtt_operation_task.h"
#include "tasks/flash/include/flash_data.h"
#include "tasks/flash/include/flash_task.h"

#include "tasks/ota/include/ota_agent_task.h"

// --------------------------------------------------
// 自ファイル内でのみ使用する#defineマクロ
// --------------------------------------------------

/*
 * OTAで使用するトピックフィルター
 */

/**
 * @brief トピックの先頭部分
 */
#define OTA_AGENT_TOPIC_PREFIX "$aws/things/"

/**
 * @brief OTA_AGENT_TOPIC_PREFIX の長さ(NULL文字含まず)
 */
#define OTA_AGENT_TOPIC_PREFIX_LENGTH ((uint16_t)(sizeof(OTA_AGENT_TOPIC_PREFIX) - 1))

/**
 * @brief ジョブが通知されるトピック
 */
#define OTA_AGENT_JOB_NOTIFY_TOPIC_FILTER OTA_AGENT_TOPIC_PREFIX "+/jobs/notify-next"

/**
 * @brief @ref OTA_AGENT_JOB_NOTIFY_TOPIC_FILTER の長さ(NULL文字含まず)
 */
#define OTA_AGENT_JOB_NOTIFY_TOPIC_FILTER_LENGTH ((uint16_t)(sizeof(OTA_AGENT_JOB_NOTIFY_TOPIC_FILTER) - 1))

/**
 * @brief ファイルブロックが送られてくるトピック
 */
#define OTA_AGENT_DATA_STREAM_TOPIC_FILTER OTA_AGENT_TOPIC_PREFIX "+/streams/#"

/**
 * @brief @ref OTA_AGENT_DATA_STREAM_TOPIC_FILTER の長さ(NULL文字含まず)
 */
#define OTA_AGENT_DATA_STREAM_TOPIC_FILTER_LENGTH ((uint16_t)(sizeof(OTA_AGENT_DATA_STREAM_TOPIC_FILTER) - 1))

/**
 * @brief ジョブ取得の結果が通知されるトピックのThingName以降の部分
 */
#define OTA_AGENT_JOBS_GET_RESPONSE_TOPIC_FILTER_BODY "/jobs/$next/get/+"

/**
 * @brief @ref OTA_AGENT_JOBS_GET_RESPONSE_TOPIC_FILTER_BODY の長さ(NULL文字含まず)
 */
#define OTA_AGENT_JOBS_GET_RESPONSE_TOPIC_FILTER_BODY_LENGTH ((uint16_t)(sizeof(OTA_AGENT_JOBS_GET_RESPONSE_TOPIC_FILTER_BODY) - 1))

/**
 * @brief ジョブステータス更新の結果が通知されるトピックのThingName以降の部分
 */
#define OTA_AGENT_JOB_STATUS_UPDATE_RESPONSE_TOPIC_FILTER_BODY "/jobs/+/update/+"

/**
 * @brief @ref OTA_AGENT_JOB_STATUS_UPDATE_RESPONSE_TOPIC_FILTER_BODY の長さ(NULL文字含まず)
 */
#define OTA_AGENT_JOB_STATUS_UPDATE_RESPONSE_TOPIC_FILTER_BODY_LENGTH ((uint16_t)(sizeof(OTA_AGENT_JOB_STATUS_UPDATE_RESPONSE_TOPIC_FILTER_BODY) - 1))

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
 * @brief トピックフィルターと対応するコールバック関数のセット
 */
typedef struct OTATopicFilterCallback
{
    const char *pcTopicFilter;       /**< トピックフィルタ */
    uint16_t uxTopicFilterLength;    /**< pcTopicFilterの文字数(NULL文字含まず)*/
    IncomingPubCallback_t xCallback; /**< pcTopicFilterのトピックを受信したときに呼ぶ関数 */
} OTATopicFilterCallback_t;

// --------------------------------------------------
// static関数プロトタイプ宣言
// --------------------------------------------------

/**
 * @brief ジョブイベント用のバッファをプールから取得する
 *
 * @return OtaEventData_t* 取得したバッファ
 */
static OtaEventData_t *xprvOtaEventBufferGet(void);

/**
 * @brief ジョブイベント用のバッファをプールに返す
 *
 * @param[in] pxBuffer バッファへのポインタ
 */
static void vprvOtaEventBufferFree(OtaEventData_t *const pxBuffer);

/**
 * @brief ジョブイベント用のバッファをすべて開放する
 *
 * @warning
 * 必ずOTAAgentTaskが終了してから呼ぶこと。さもないと開放したバッファを使われるおそれがある
 */
static void vprvOtaEventBufferFreeAll(void);

/**
 * @brief ジョブドキュメントを受信したときに呼ばれる関数。ジョブドキュメントをバッファにコピーしてOTAAgentに通知する。
 *
 * @param[in] pvContext      サブスクライブ時に設定したコンテキスト。使用しない。
 * @param[in] pxPublishInfo 受信したメッセージ
 */
static void vprvMqttJobCallback(void *pvContext, MQTTPublishInfo_t *pxPublishInfo);

/**
 * @brief FWのデータブロックを受信したときに呼ばれる関数。データブロックをバッファにコピーしてOTAAgentに通知する。
 *
 * @param[in] pvContext      サブスクライブ時に設定したコンテキスト。使用しない。
 * @param[in] pxPublishInfo 受信したメッセージ
 */
static void vprvMqttDataCallback(void *pvContext, MQTTPublishInfo_t *pxPublishInfo);

/**
 * @brief 何も処理する必要がないメッセージを受信したときに呼ばれる関数。ログだけ表示し、何もしない。
 *
 * @param[in] pvContext      サブスクライブ時に設定したコンテキスト。使用しない。
 * @param[in] pxPublishInfo 受信したメッセージ
 */
static void vprvMqttDefaultCallback(void *pvContext, MQTTPublishInfo_t *pxPublishInfo);

/**
 * @brief 指定されたトピック名のメッセージ受診時に呼び出すべきコールバック関数を返す
 *
 * @param[in] pcTopicName        トピック名
 * @param[in] uxTopicNameLength  トピック名の長さ(NULL文字含まず)
 *
 * @return IncomingPubCallback_t コールバック関数
 */
static IncomingPubCallback_t xprvGetCallbackForTopic(const char *pcTopicName, uint16_t uxTopicNameLength);

/**
 * @brief OTAAgentがトピックをサブスクライブするときに使用する関数
 *
 * @param[in] pcTopicFilter       トピックフィルタ
 * @param[in] uxTopicFilterLength トピックフィルタの長さ(NULL文字含まず)
 * @param[in] uxQOS               QOS
 *
 * @retval OtaMqttSuccess         成功
 * @retval OtaMqttSubscribeFailed 失敗
 */
static OtaMqttStatus_t xprvMqttSubscribe(const char *pcTopicFilter,
                                         uint16_t uxTopicFilterLength,
                                         uint8_t uxQOS);

/**
 * @brief OTAAgentがトピックをアンサブスクライブするときに使用する関数
 *
 * @param[in] pcTopicFilter       トピックフィルタ
 * @param[in] uxTopicFilterLength トピックフィルタの長さ(NULL文字含まず)
 * @param[in] uxQOS               QOS
 *
 * @retval OtaMqttSuccess           成功
 * @retval OtaMqttUnsubscribeFailed 失敗
 */
static OtaMqttStatus_t xprvMqttUnSubscribe(const char *pcTopicFilter,
                                           uint16_t uxTopicFilterLength,
                                           uint8_t uxQOS);

/**
 * @brief OTAAgentがメッセージをパブリッシュするときに使用する関数
 *
 * @param[in] pcTopic    トピック名
 * @param[in] uxTopicLen トピック名の長さ(NULL文字含まず)
 * @param[in] pcMsg      メッセージ
 * @param[in] uxMsgSize  メッセージの長さ(NULL文字含まず)
 * @param[in] uxQOS      QOS
 *
 * @retval OtaMqttSuccess       成功
 * @retval OtaMqttPublishFailed 失敗
 */
static OtaMqttStatus_t xprvMqttPublish(const char *const pcTopic,
                                       uint16_t uxTopicLen,
                                       const char *pcMsg,
                                       uint32_t uxMsgSize,
                                       uint8_t uxQOS);

/**
 * @brief OTAAgentTaskのエントリポイント
 *
 * @param[in] pvParam xTaskCreateで指定するパラメータ。OTA_EventProcessingTaskに渡すが使用されていない。
 */
static void vprvOTAAgentTask(void *pvParam);

/**
 * @brief OTAAgentがアプリに処理を要求しているときに呼ばれる関数。
 *
 * @param[in] xEvent 発生したイベントの種類
 * @param[in] pvData イベントに応じたデータ
 */
static void vprvOtaAppCallback(OtaJobEvent_t xEvent, const void *pvData);

// --------------------------------------------------
// ファイル内で共有するstatic変数宣言
// --------------------------------------------------

/**
 * @brief OTAAgentタスクのハンドル。シャットダウンしたらNULLに戻す。
 *
 */
static TaskHandle_t gxOTAAgentTaskHandle;

/**
 * @brief MQTTAgentがサブスクライブを要求したときに登録するコールバック関数
 *
 */
static OTATopicFilterCallback_t gxOtaTopicFilterCallbacks[] = {
    {
        .pcTopicFilter = OTA_AGENT_JOB_NOTIFY_TOPIC_FILTER,
        .uxTopicFilterLength = OTA_AGENT_JOB_NOTIFY_TOPIC_FILTER_LENGTH,
        .xCallback = vprvMqttJobCallback,
    },
    {
        .pcTopicFilter = OTA_AGENT_DATA_STREAM_TOPIC_FILTER,
        .uxTopicFilterLength = OTA_AGENT_DATA_STREAM_TOPIC_FILTER_LENGTH,
        .xCallback = vprvMqttDataCallback,
    }};

/**
 * @brief OTAAgentが使用するインターフェース
 *
 * @note
 * OTA_Initは受け取った構造体のアドレスをグローバル変数に保持するので、この変数はOTA_Init呼び出し後も維持する必要がある
 */
const OtaInterfaces_t gxOtaInterfaces = {
    .os = {
        .event = {
            .init = OtaInitEvent_FreeRTOS,
            .send = OtaSendEvent_FreeRTOS,
            .recv = OtaReceiveEvent_FreeRTOS,
            .deinit = OtaDeinitEvent_FreeRTOS,
        },
        .timer = {
            .start = OtaStartTimer_FreeRTOS,
            .stop = OtaStopTimer_FreeRTOS,
            .delete = OtaDeleteTimer_FreeRTOS,
        },
        .mem = {
            .malloc = Malloc_FreeRTOS,
            .free = Free_FreeRTOS,
        },
    },
    .mqtt = {
        .subscribe = xprvMqttSubscribe,
        .publish = xprvMqttPublish,
        .unsubscribe = xprvMqttUnSubscribe,
    },
    .pal = {
        .getPlatformImageState = otaPal_GetPlatformImageState,
        .setPlatformImageState = otaPal_SetPlatformImageState,
        .writeBlock = otaPal_WriteBlock,
        .activate = otaPal_ActivateNewImage,
        .closeFile = otaPal_CloseFile,
        .reset = otaPal_ResetDevice,
        .abort = otaPal_Abort,
        .createFile = otaPal_CreateFileForRx,
    },
};

/*
 * OTA_Init に渡す、OTAAgentが使用するバッファ
 */

/**
 * @brief ジョブ作成時の「デバイス上のファイルのパス名」を置くバッファ
 */
static uint8_t gxUpdateFilePath[OTA_APP_MAX_FILE_PATH_SIZE];

/**
 * @brief ジョブ作成時の「デバイスのコード署名証明書のパス名」を置くバッファ
 */
static uint8_t gxCertFilePath[OTA_APP_MAX_FILE_PATH_SIZE];

/**
 * @brief ストリーム名を置くバッファ
 *
 * @see https://docs.aws.amazon.com/ja_jp/freertos/latest/userguide/ota-cli-workflow.html#ota-stream
 */
static uint8_t gxStreamName[OTA_APP_MAX_STREAM_NAME_SIZE];

/**
 * @brief デコードされたファイルブロックを置くバッファ
 */
static uint8_t gxDecodeMem[otaconfigFILE_BLOCK_SIZE];

/**
 * @brief ブロックビットマップ（どのブロックを受信したか記録しておくもの）を置くバッファ
 */
static uint8_t gxBitmap[OTA_MAX_BLOCK_BITMAP_SIZE];

/**
 * @brief OTA_Init に渡すバッファの情報
 */
const static OtaAppBuffer_t gxOtaBuffer = {
    .pUpdateFilePath = gxUpdateFilePath,
    .updateFilePathsize = OTA_APP_MAX_FILE_PATH_SIZE,
    .pCertFilePath = gxCertFilePath,
    .certFilePathSize = OTA_APP_MAX_FILE_PATH_SIZE,
    .pStreamName = gxStreamName,
    .streamNameSize = OTA_APP_MAX_STREAM_NAME_SIZE,
    .pDecodeMemory = gxDecodeMem,
    .decodeMemorySize = otaconfigFILE_BLOCK_SIZE,
    .pFileBitmap = gxBitmap,
    .fileBitmapSize = OTA_MAX_BLOCK_BITMAP_SIZE,
};

/*
 * ジョブイベント受信から処理完了の間で使用するバッファ
 */

/**
 * @brief ジョブイベント用のバッファ
 */
static OtaEventData_t gxEventBufferPool[otaconfigMAX_NUM_OTA_DATA_BUFFERS];

/**
 * @brief ジョブイベント用のバッファの取得・開放時につかうセマフォ
 */
static SemaphoreHandle_t gxEventBufferSemaphore;

/*
 * 手動でサブスクライブするトピックフィルタ。ポリシー上ThingNameをワイルドカードにできないので初期化時に作る
 */

/**
 * @brief ジョブ取得の結果が通知されるトピック
 */
static uint8_t gucJobsGetResponseTopicFilter
    [OTA_AGENT_TOPIC_PREFIX_LENGTH + THING_NAME_LENGTH + OTA_AGENT_JOBS_GET_RESPONSE_TOPIC_FILTER_BODY_LENGTH + 1] =
        OTA_AGENT_TOPIC_PREFIX;

/**
 * @brief ジョブステータス更新の結果が通知されるトピック
 */
static uint8_t gucJobStatusUpdateResponseTopicTiler
    [OTA_AGENT_TOPIC_PREFIX_LENGTH + THING_NAME_LENGTH + OTA_AGENT_JOB_STATUS_UPDATE_RESPONSE_TOPIC_FILTER_BODY_LENGTH + 1] =
        OTA_AGENT_TOPIC_PREFIX;

// --------------------------------------------------
// 変数定義（staticを除く）
// --------------------------------------------------

// --------------------------------------------------
// 関数定義（staticを除く）
// --------------------------------------------------

OTAAgentTaskResult_t eOTAAgentTaskInit(void)
{
    // すでにOTAAgentTaskが起動していないか確認
    if (gxOTAAgentTaskHandle != NULL)
    {
        APP_PRINTFWarn("OTAAgentTask already running.");
        return OTA_AGENT_TASK_RESULT_SUCCESS;
    }

    // イベントバッファ管理用セマフォ作成
    if (gxEventBufferSemaphore == NULL)
    {
        gxEventBufferSemaphore = xSemaphoreCreateMutex();
        if (gxEventBufferSemaphore == NULL)
        {
            APP_PRINTFError("Failed to initialize OTAAgent; failed to initialize buffer semaphore.");
            return OTA_AGENT_TASK_RESULT_FAILED;
        }
    }

    // ClientIDとして使用するThingNameを取得
    ThingName_t xUsualThingName = {.ucName = {0x00}};
    FlashTaskResult_t xFlashReadResult = eReadFlashInfo(READ_FLASH_TYPE_USUAL_THING_NAME,
                                                        &xUsualThingName,
                                                        sizeof(xUsualThingName));
    if (xFlashReadResult != FLASH_TASK_RESULT_SUCCESS)
    {
        APP_PRINTFError("Failed to initialize OTAAgent; failed to read thingName from flash with code %d", xFlashReadResult);
        return OTA_AGENT_TASK_RESULT_FAILED;
    }

    uint16_t xUsualThingNameLength = strlen((char *)xUsualThingName.ucName);

    // 実際のThingNameが書かれた、ジョブ取得の結果が通知されるトピックを作成
    memcpy(gucJobsGetResponseTopicFilter + OTA_AGENT_TOPIC_PREFIX_LENGTH,
           xUsualThingName.ucName,
           xUsualThingNameLength);
    memcpy(gucJobsGetResponseTopicFilter + OTA_AGENT_TOPIC_PREFIX_LENGTH + xUsualThingNameLength,
           OTA_AGENT_JOBS_GET_RESPONSE_TOPIC_FILTER_BODY,
           OTA_AGENT_JOBS_GET_RESPONSE_TOPIC_FILTER_BODY_LENGTH);
    gucJobsGetResponseTopicFilter
        [OTA_AGENT_TOPIC_PREFIX_LENGTH + xUsualThingNameLength + OTA_AGENT_JOBS_GET_RESPONSE_TOPIC_FILTER_BODY_LENGTH] = '\0';

    // 実際のThingNameが書かれた、ジョブステータス更新の結果が通知されるトピックを作成
    memcpy(gucJobStatusUpdateResponseTopicTiler + OTA_AGENT_TOPIC_PREFIX_LENGTH,
           xUsualThingName.ucName,
           xUsualThingNameLength);
    memcpy(gucJobStatusUpdateResponseTopicTiler + OTA_AGENT_TOPIC_PREFIX_LENGTH + xUsualThingNameLength,
           OTA_AGENT_JOB_STATUS_UPDATE_RESPONSE_TOPIC_FILTER_BODY,
           OTA_AGENT_JOB_STATUS_UPDATE_RESPONSE_TOPIC_FILTER_BODY_LENGTH);
    gucJobStatusUpdateResponseTopicTiler
        [OTA_AGENT_TOPIC_PREFIX_LENGTH + xUsualThingNameLength + OTA_AGENT_JOB_STATUS_UPDATE_RESPONSE_TOPIC_FILTER_BODY_LENGTH] = '\0';

    // OTAジョブ取得の結果返却トピックをサブスクライブ
    MQTTSubscribeInfo_t xJobsGetSubscribeInfo = {
        .qos = 0,
        .pTopicFilter = (char *)gucJobsGetResponseTopicFilter,
        .topicFilterLength = strlen((char *)gucJobsGetResponseTopicFilter),
    };

    static StaticMQTTCommandBuffer_t xSubscribeMQTTContextBuffer1; // コンテキスト保存場所を永続化したいためStaticで宣言
    memset(&xSubscribeMQTTContextBuffer1, 0x00, sizeof(xSubscribeMQTTContextBuffer1));
    MQTTOperationTaskResult_t xJobsGetSubscribeResult = eMQTTSubscribe(&xJobsGetSubscribeInfo, vprvMqttJobCallback, NULL, &xSubscribeMQTTContextBuffer1);
    if (xJobsGetSubscribeResult != MQTT_OPERATION_TASK_RESULT_SUCCESS)
    {
        APP_PRINTFError("Failed to initialize OTAAgent; failed to subscribe to topic %.*s with code %d",
                        xJobsGetSubscribeInfo.topicFilterLength, xJobsGetSubscribeInfo.pTopicFilter,
                        xJobsGetSubscribeResult);
        return OTA_AGENT_TASK_RESULT_FAILED;
    }
    APP_PRINTFInfo("SUBSCRIBED to topic %.*s to broker.",
                   xJobsGetSubscribeInfo.topicFilterLength, xJobsGetSubscribeInfo.pTopicFilter);

    // OTAジョブステータス更新の結果返却トピックをサブスクライブ
    MQTTSubscribeInfo_t xSubscribeInfo = {
        .qos = 0,
        .pTopicFilter = (char *)gucJobStatusUpdateResponseTopicTiler,
        .topicFilterLength = strlen((char *)gucJobStatusUpdateResponseTopicTiler),
    };

    static StaticMQTTCommandBuffer_t xSubscribeMQTTContextBuffer2; // コンテキスト保存場所を永続化したいためStaticで宣言
    memset(&xSubscribeMQTTContextBuffer2, 0x00, sizeof(xSubscribeMQTTContextBuffer2));
    MQTTOperationTaskResult_t xMQTTSubscribeResult = eMQTTSubscribe(&xSubscribeInfo, vprvMqttDefaultCallback, NULL, &xSubscribeMQTTContextBuffer2);
    if (xMQTTSubscribeResult != MQTT_OPERATION_TASK_RESULT_SUCCESS)
    {
        APP_PRINTFError("Failed to initialize OTAAgent; failed to subscribe to topic %.*s with code %d",
                        xSubscribeInfo.topicFilterLength, xSubscribeInfo.pTopicFilter, xMQTTSubscribeResult);
        return OTA_AGENT_TASK_RESULT_FAILED;
    }
    APP_PRINTFInfo("SUBSCRIBED to topic %.*s to broker.",
                   xSubscribeInfo.topicFilterLength, xSubscribeInfo.pTopicFilter);

    // OTAライブラリを初期化
    OtaErr_t xOTAInitResult = OTA_Init((OtaAppBuffer_t *)&gxOtaBuffer,
                                       (OtaInterfaces_t *)&gxOtaInterfaces,
                                       xUsualThingName.ucName,
                                       vprvOtaAppCallback);
    if (xOTAInitResult != OtaErrNone)
    {
        APP_PRINTFError("Failed to initialize OTAAgent; OTA_Init returned %u.", xOTAInitResult);
        return OTA_AGENT_TASK_RESULT_FAILED;
    }

    // OTAAgentタスクを起動
    BaseType_t xTaskCreateResult = xTaskCreate(vprvOTAAgentTask,
                                               "OTAAgentTask",
                                               OTA_AGENT_STACK_SIZE,
                                               NULL,
                                               OTA_AGENT_TASK_PRIORITY,
                                               &gxOTAAgentTaskHandle);
    if (xTaskCreateResult != pdPASS)
    {
        APP_PRINTFError("Failed to initialize OTAAgent; failed to create OTAAgent task with code %d", xTaskCreateResult);
        return OTA_AGENT_TASK_RESULT_FAILED;
    }

    // OTAAgentの開始を通知
    OtaEventMsg_t xEventMsg = {
        .pEventData = NULL,
        .eventId = OtaAgentEventStart,
    };
    if (!OTA_SignalEvent(&xEventMsg))
    {
        APP_PRINTFError("ailed to initialize OTAAgent; failed to signal OtaAgentEventStart to OTAAgent.");
        return OTA_AGENT_TASK_RESULT_FAILED;
    };

    APP_PRINTFInfo("Succeeded to initialize OTAAgent task.");
    return OTA_AGENT_TASK_RESULT_SUCCESS;
}

OTAAgentTaskResult_t eOTAAgentTaskShutdown(void)
{
    if (gxOTAAgentTaskHandle == NULL)
    {
        APP_PRINTFWarn("OTAAgentTask already shut down.");
        return OTA_AGENT_TASK_RESULT_SUCCESS;
    }

    // OTAAgentをシャットダウンする。
    // 第1引数: OTAのシャットダウン完了を待たない。OTA_Shutdownの実装がビジーウェイトになっているため↓で手動でやる。
    // 第2引数: サブスクライブしていたトピックをアンサブスクライブする。
    // 戻り値: 現在のOTAAgentの状態。↓で確認しているためここでは確認しない。
    OTA_Shutdown(0, 1);

    // OTAAgentのシャットダウン完了を待つ
    uint32_t uxShutdownTimeout = OTA_APP_SHUTDOWN_TIMEOUT_MS;
    while (OTA_GetState() != OtaAgentStateStopped && uxShutdownTimeout > 0)
    {
        vTaskDelay(pdMS_TO_TICKS(OTA_APP_SHUTDOWN_WAIT_DELAY_MS));
        uxShutdownTimeout -= OTA_APP_SHUTDOWN_WAIT_DELAY_MS;
    }

    // OTAAgentがシャットダウンされているか確認する
    OtaState_t xOTAAgentState = OTA_GetState();
    if (xOTAAgentState != OtaAgentStateStopped)
    {
        APP_PRINTFError("Failed to shut down OTAAgent; OTA_Agent state didn't changed to OtaAgentStateStopped"
                        " within %d ms. Final state is %d.",
                        OTA_APP_SHUTDOWN_TIMEOUT_MS, xOTAAgentState);
        return OTA_AGENT_TASK_RESULT_FAILED;
    }

    // OTAジョブステータス更新の結果返却トピックをアンサブスクライブ
    MQTTSubscribeInfo_t xSubscribeInfo = {
        .qos = 0,
        .pTopicFilter = (char *)gucJobStatusUpdateResponseTopicTiler,
        .topicFilterLength = strlen((char *)gucJobStatusUpdateResponseTopicTiler),
    };

    static StaticMQTTCommandBuffer_t xUnsubscribeMQTTContextBuffer1; // コンテキスト保存場所を永続化したいためStaticで宣言
    memset(&xUnsubscribeMQTTContextBuffer1, 0x00, sizeof(xUnsubscribeMQTTContextBuffer1));
    MQTTOperationTaskResult_t xMQTTSubscribeResult = eMQTTUnsubscribe(&xSubscribeInfo, &xUnsubscribeMQTTContextBuffer1);
    if (xMQTTSubscribeResult != MQTT_OPERATION_TASK_RESULT_SUCCESS)
    {
        APP_PRINTFError("Failed to shut down OTAAgent; failed to unsubscribe to topic %.*s with code %d",
                        xSubscribeInfo.topicFilterLength, xSubscribeInfo.pTopicFilter, xMQTTSubscribeResult);
        return OTA_AGENT_TASK_RESULT_FAILED;
    }
    APP_PRINTFInfo("Unsubscribed to topic %.*s to broker.",
                   xSubscribeInfo.topicFilterLength, xSubscribeInfo.pTopicFilter);

    // OTAジョブ取得の結果返却トピックをアンサブスクライブ
    MQTTSubscribeInfo_t xJobsGetSubscribeInfo = {
        .qos = 0,
        .pTopicFilter = (char *)gucJobsGetResponseTopicFilter,
        .topicFilterLength = strlen((char *)gucJobsGetResponseTopicFilter),
    };

    static StaticMQTTCommandBuffer_t xUnsubscribeMQTTContextBuffer2; // コンテキスト保存場所を永続化したいためStaticで宣言
    memset(&xUnsubscribeMQTTContextBuffer2, 0x00, sizeof(xUnsubscribeMQTTContextBuffer2));
    MQTTOperationTaskResult_t xJobsGetSubscribeResult = eMQTTUnsubscribe(&xJobsGetSubscribeInfo, &xUnsubscribeMQTTContextBuffer2);
    if (xJobsGetSubscribeResult != MQTT_OPERATION_TASK_RESULT_SUCCESS)
    {
        APP_PRINTFError("Failed to shut down OTAAgent; failed to unsubscribe to topic %.*s with code %d",
                        xJobsGetSubscribeInfo.topicFilterLength, xJobsGetSubscribeInfo.pTopicFilter,
                        xJobsGetSubscribeResult);
        return OTA_AGENT_TASK_RESULT_FAILED;
    }
    APP_PRINTFInfo("Unsubscribed to topic %.*s to broker.",
                   xJobsGetSubscribeInfo.topicFilterLength, xJobsGetSubscribeInfo.pTopicFilter);

    // ジョブイベント用のバッファをすべて開放する
    vprvOtaEventBufferFreeAll();

    APP_PRINTFInfo("Succeeded to shut down OTAAgent task.");
    return OTA_AGENT_TASK_RESULT_SUCCESS;
}

// --------------------------------------------------
// static関数定義
// --------------------------------------------------

static OtaEventData_t *xprvOtaEventBufferGet(void)
{
    OtaEventData_t *pxFreeBuffer = NULL;

    // 取得・開放を排他するためにセマフォを取る
    if (xSemaphoreTake(gxEventBufferSemaphore, portMAX_DELAY) != pdTRUE)
    {
        APP_PRINTFError("Failed to get event buffer; failed to get semaphore.");
        return NULL;
    }

    // 先頭から順番に開いているバッファを探し、見つかったらそのバッファに使用中フラグを付けて返す
    for (uint32_t ulIndex = 0; ulIndex < otaconfigMAX_NUM_OTA_DATA_BUFFERS; ulIndex++)
    {
        if (gxEventBufferPool[ulIndex].bufferUsed == false)
        {
            gxEventBufferPool[ulIndex].bufferUsed = true;
            pxFreeBuffer = &gxEventBufferPool[ulIndex];
            break;
        }
    }

    // セマフォが取れていればエラーは出ない
    xSemaphoreGive(gxEventBufferSemaphore);

    if (pxFreeBuffer != NULL)
    {
        APP_PRINTFDebug("Succeeded to get event buffer. Index: %d, Address: %p", pxFreeBuffer - gxEventBufferPool, pxFreeBuffer);
    }
    else
    {
        APP_PRINTFWarn("Failed to get event buffer; buffer is out of stock.");
    }

    return pxFreeBuffer;
}

static void vprvOtaEventBufferFree(OtaEventData_t *const pxBuffer)
{
    // 取得・開放を排他するためにセマフォを取る
    if (xSemaphoreTake(gxEventBufferSemaphore, portMAX_DELAY) != pdTRUE)
    {
        APP_PRINTFError("Failed to free event buffer; failed to get semaphore.");
        return;
    }

    // 使用していたバッファを開放する
    pxBuffer->bufferUsed = false;

    // セマフォが取れていればエラーは出ない
    xSemaphoreGive(gxEventBufferSemaphore);

    APP_PRINTFDebug("Succeeded to free event buffer. Index: %d, Address: %p", pxBuffer - gxEventBufferPool, pxBuffer);
}

static void vprvOtaEventBufferFreeAll(void)
{
    // 取得・開放を排他するためにセマフォを取る
    if (xSemaphoreTake(gxEventBufferSemaphore, portMAX_DELAY) != pdTRUE)
    {
        APP_PRINTFError("Failed to free all event buffers; failed to get semaphore.");
        return;
    }

    // 先頭から順番にバッファの使用中フラグを解除する
    uint32_t ulNumUnfreedBuffers = 0;
    for (uint32_t ulIndex = 0; ulIndex < otaconfigMAX_NUM_OTA_DATA_BUFFERS; ulIndex++)
    {
        if (gxEventBufferPool[ulIndex].bufferUsed == true)
        {
            ulNumUnfreedBuffers++;
            gxEventBufferPool[ulIndex].bufferUsed = false;
        }
    }

    // セマフォが取れていればエラーは出ない
    xSemaphoreGive(gxEventBufferSemaphore);

    APP_PRINTFDebug("Succeeded to free all event buffers. Number of buffers freed: %d", ulNumUnfreedBuffers);
}

static void vprvMqttJobCallback(void *pvContext, MQTTPublishInfo_t *pxPublishInfo)
{
    // 使用しない
    (void)pvContext;

    APP_PRINTFInfo("Received job message. topic: %.*s, message: %.*s .",
                   pxPublishInfo->topicNameLength, pxPublishInfo->pTopicName,
                   pxPublishInfo->payloadLength, pxPublishInfo->pPayload);

    // 受信したデータを入れるバッファを取得
    OtaEventData_t *pxEventData = xprvOtaEventBufferGet();
    if (pxEventData == NULL)
    {
        APP_PRINTFError("Failed to signal OtaAgentEventReceivedJobDocument to OTAAgent; failed to get event buffer.");
        return;
    }

    // 受信したデータをバッファに入れる
    memcpy(pxEventData->data, pxPublishInfo->pPayload, pxPublishInfo->payloadLength);
    pxEventData->dataLength = pxPublishInfo->payloadLength;

    // OTAAgentにジョブ受信を通知
    OtaEventMsg_t xEventMsg = {
        .pEventData = pxEventData,
        .eventId = OtaAgentEventReceivedJobDocument,
    };
    if (!OTA_SignalEvent(&xEventMsg))
    {
        APP_PRINTFError("Failed to signal OtaAgentEventReceivedJobDocument to OTAAgent; OTA_SignalEvent failed.");
    }

    APP_PRINTFDebug("Succeeded to signal OtaAgentEventReceivedJobDocument to OTAAgent.");
}

static void vprvMqttDataCallback(void *pvContext, MQTTPublishInfo_t *pxPublishInfo)
{
    // 使用しない
    (void)pvContext;

    // 受信したデータを入れるバッファを取得
    OtaEventData_t *pxEventData = xprvOtaEventBufferGet();
    if (pxEventData == NULL)
    {
        APP_PRINTFWarn("Failed to signal OtaAgentEventReceivedFileBlock to OTAAgent; failed to get event buffer.");
        return;
    }

    // 受信したデータをバッファに入れる
    memcpy(pxEventData->data, pxPublishInfo->pPayload, pxPublishInfo->payloadLength);
    pxEventData->dataLength = pxPublishInfo->payloadLength;

    // OTAAgentにファイルブロック受信を通知
    OtaEventMsg_t xEventMsg = {
        .pEventData = pxEventData,
        .eventId = OtaAgentEventReceivedFileBlock,
    };
    if (!OTA_SignalEvent(&xEventMsg))
    {
        APP_PRINTFError("Failed to signal OtaAgentEventReceivedFileBlock to OTAAgent; OTA_SignalEvent failed.");
    }

    APP_PRINTFDebug("Succeeded to signal OtaAgentEventReceivedFileBlock to OTAAgent.");
}

static void vprvMqttDefaultCallback(void *pvContext, MQTTPublishInfo_t *pxPublishInfo)
{
    // 使用しない
    (void)pvContext;

    APP_PRINTFDebug("Received mqtt message with no operation needed. topic: %.*s, message: %.*s .",
                    pxPublishInfo->topicNameLength, pxPublishInfo->pTopicName,
                    pxPublishInfo->payloadLength, pxPublishInfo->pPayload);
}

static IncomingPubCallback_t xprvGetCallbackForTopic(const char *pcTopicName, uint16_t uxTopicFilterLength)
{
    uint32_t ulNumTopicFilters = sizeof(gxOtaTopicFilterCallbacks) / sizeof(OTATopicFilterCallback_t);

    // gxOtaTopicFilterCallbacksのトピックを先頭から順番にたどって、
    // pcTopicFilterがそのトピックにマッチしたら、対応するコールバック関数を返す
    for (uint32_t ulIndex = 0U; ulIndex < ulNumTopicFilters; ulIndex++)
    {
        bool xIsMatch = false;
        MQTTStatus_t xMAtchTopicResult = MQTT_MatchTopic(pcTopicName,
                                                         uxTopicFilterLength,
                                                         gxOtaTopicFilterCallbacks[ulIndex].pcTopicFilter,
                                                         gxOtaTopicFilterCallbacks[ulIndex].uxTopicFilterLength,
                                                         &xIsMatch);
        if (xMAtchTopicResult != MQTTSuccess)
        {
            APP_PRINTFError("Failed to get callback function for topic %*.s; MQTT_MatchTopic returned %d.",
                            uxTopicFilterLength, pcTopicName, xMAtchTopicResult);
            return NULL;
        }

        if (xIsMatch)
        {
            APP_PRINTFDebug("A callback found for topic %*.s. matched topic: %*.s.",
                            uxTopicFilterLength,
                            pcTopicName,
                            gxOtaTopicFilterCallbacks[ulIndex].uxTopicFilterLength,
                            gxOtaTopicFilterCallbacks[ulIndex].pcTopicFilter);
            return gxOtaTopicFilterCallbacks[ulIndex].xCallback;
        }
    }

    APP_PRINTFError("Callback not found for %*.s.", uxTopicFilterLength, pcTopicName);
    return NULL;
}

static OtaMqttStatus_t xprvMqttSubscribe(const char *pcTopicFilter,
                                         uint16_t uxTopicFilterLength,
                                         uint8_t uxQOS)
{
    // トピック名に対応するコールバック関数を取得
    IncomingPubCallback_t xCallback = xprvGetCallbackForTopic(pcTopicFilter, uxTopicFilterLength);
    if (xCallback == NULL)
    {
        APP_PRINTFError("Failed to subscribe to topic %.*s; there is no callback function for the topic.",
                        uxTopicFilterLength, pcTopicFilter);
        return OtaMqttSubscribeFailed;
    }

    APP_PRINTFDebug("pcTopicFilter: %.*s, uxTopicFilterLength: %d",
                    uxTopicFilterLength, pcTopicFilter, uxTopicFilterLength);
    APP_PRINTFDebug("uxQOS: %d", uxQOS);

    // MQTTAgentが要求するトピックをサブスクライブ
    MQTTSubscribeInfo_t xSubscribeInfo = {
        .qos = uxQOS,
        .pTopicFilter = pcTopicFilter,
        .topicFilterLength = uxTopicFilterLength,
    };

    static StaticMQTTCommandBuffer_t xSubscribeMQTTContextBuffer; // コンテキスト保存場所を永続化したいためStaticで宣言
    memset(&xSubscribeMQTTContextBuffer, 0x00, sizeof(xSubscribeMQTTContextBuffer));

    MQTTOperationTaskResult_t xMQTTSubscribeResult = eMQTTSubscribe(&xSubscribeInfo, xCallback, NULL, &xSubscribeMQTTContextBuffer);
    if (xMQTTSubscribeResult != MQTT_OPERATION_TASK_RESULT_SUCCESS)
    {
        APP_PRINTFError("Failed to subscribe to topic %.*s; eMQTTSubscribe returned %d",
                        uxTopicFilterLength, pcTopicFilter, xMQTTSubscribeResult);
        return OtaMqttSubscribeFailed;
    }

    APP_PRINTFInfo("SUBSCRIBED to topic %.*s to broker.", uxTopicFilterLength, pcTopicFilter);
    return OtaMqttSuccess;
}

static OtaMqttStatus_t xprvMqttUnSubscribe(const char *pcTopicFilter,
                                           uint16_t uxTopicFilterLength,
                                           uint8_t uxQOS)
{
    // MQTTAgentが要求するトピックをアンサブスクライブ
    MQTTSubscribeInfo_t xSubscribeInfo = {
        .qos = uxQOS,
        .pTopicFilter = pcTopicFilter,
        .topicFilterLength = uxTopicFilterLength,
    };

    static StaticMQTTCommandBuffer_t xUnsubscribeMQTTContextBuffer; // コンテキスト保存場所を永続化したいためStaticで宣言
    memset(&xUnsubscribeMQTTContextBuffer, 0x00, sizeof(xUnsubscribeMQTTContextBuffer));

    MQTTOperationTaskResult_t xMQTTUnsubscribeResult = eMQTTUnsubscribe(&xSubscribeInfo, &xUnsubscribeMQTTContextBuffer);
    if (xMQTTUnsubscribeResult != MQTT_OPERATION_TASK_RESULT_SUCCESS)
    {
        APP_PRINTFError("Failed to unsubscribe to topic %.*s; eMQTTUnsubscribe returned %d.",
                        uxTopicFilterLength, pcTopicFilter, xMQTTUnsubscribeResult);
        return OtaMqttUnsubscribeFailed;
    }

    APP_PRINTFInfo("Unsubscribed from topic %.*s to broker.", uxTopicFilterLength, pcTopicFilter);
    return OtaMqttSuccess;
}

static OtaMqttStatus_t xprvMqttPublish(const char *const pcTopic,
                                       uint16_t uxTopicLen,
                                       const char *pcMsg,
                                       uint32_t uxMsgSize,
                                       uint8_t uxQOS)
{
    // MQTTAgentが要求するパケットをパブリッシュ
    MQTTPublishInfo_t xMQTTPublishInfo = {
        .qos = uxQOS,
        .retain = false,
        .dup = false,
        .pTopicName = pcTopic,
        .topicNameLength = uxTopicLen,
        .pPayload = pcMsg,
        .payloadLength = uxMsgSize,
    };

    APP_PRINTFDebug("pcTopic: %.*s, uxTopicLen: %d", uxTopicLen, pcTopic, uxTopicLen);
    APP_PRINTFDebug("uxQOS: %d", uxQOS);

    static StaticMQTTCommandBuffer_t xSubscribeMQTTContextBuffer; // コンテキスト保存場所を永続化したいためStaticで宣言
    memset(&xSubscribeMQTTContextBuffer, 0x00, sizeof(xSubscribeMQTTContextBuffer));
    MQTTOperationTaskResult_t xMQTTpublishResult = eMQTTpublish(&xMQTTPublishInfo, &xSubscribeMQTTContextBuffer);
    if (xMQTTpublishResult != MQTT_OPERATION_TASK_RESULT_SUCCESS)
    {
        APP_PRINTFError("Failed to publish message. Topic: %.*s, Message: %.*s; eMQTTpublish returned %d.1",
                        uxTopicLen, pcTopic, uxMsgSize, pcMsg, xMQTTpublishResult);
        return OtaMqttPublishFailed;
    }

    APP_PRINTFInfo("Sent PUBLISH packet to broker %.*s to broker.", uxTopicLen, pcTopic);
    return OtaMqttSuccess;
}

static void vprvOTAAgentTask(void *pvParam)
{
    OTA_EventProcessingTask(pvParam);
    APP_PRINTFInfo("OTAAgentTask shut down.");

    gxOTAAgentTaskHandle = NULL;
    PRINT_TASK_REMAINING_STACK_SIZE();

    vTaskDelete(NULL);
}

static void vprvOtaAppCallback(OtaJobEvent_t xEvent, const void *pvData)
{
    switch (xEvent)
    {
    case OtaJobEventActivate:
        APP_PRINTFInfo("Received OtaJobEventActivate callback from OTAAgent.");

        // リブートする前にスタックサイズを表示
        PRINT_TASK_REMAINING_STACK_SIZE();

        // MQTT Task Shutdown
        eMQTTDisconnectAndTaskShutdown();

        // リブートする前にスタックサイズを表示
        PRINT_TASK_REMAINING_STACK_SIZE();

        // スタックサイズを表示するためディレイ入れる。
        vTaskDelay(pdMS_TO_TICKS(5 * 1000));

        // 新しいFWを有効化する(リブートする)
        OTA_ActivateNewImage();

        // アクティベート(リブート)失敗時の処理
        APP_PRINTFError("New image activation failed.");
        break;

    case OtaJobEventFail:
        APP_PRINTFInfo("Received OtaJobEventFail callback from OTAAgent.");

        // 何もしない

        break;

    case OtaJobEventStartTest:
        APP_PRINTFInfo("Received OtaJobEventStartTest callback from OTAAgent.");

        // セルフテスト実行(現状何もしない)

        // MCU Imageを承認状態(正しくファームウェアを書き込め、セルフテストに成功した状態)にする
        OtaErr_t xOtaError = OTA_SetImageState(OtaImageStateAccepted);
        if (xOtaError != OtaErrNone)
        {
            APP_PRINTFError("Failed to set image state as accepted.");
        }
        else
        {
            APP_PRINTFInfo("Successfully updated with the new image.");
        }

        break;

    case OtaJobEventProcessed:
        APP_PRINTFInfo("Received OtaJobEventProcessed callback from OTAAgent.");

        // ジョブイベント処理後の後処理
        if (pvData != NULL)
        {
            vprvOtaEventBufferFree((OtaEventData_t *)pvData);
        }

        PRINT_TASK_REMAINING_STACK_SIZE();

        break;

    case OtaJobEventSelfTestFailed:
        APP_PRINTFInfo("Received OtaJobEventSelfTestFailed callback from OTAAgent.");

        // セルフテスト失敗時の処理
        APP_PRINTFError("Self-test of new image failed.");
        break;

    case OtaJobEventParseCustomJob:
        APP_PRINTFWarn("Received OtaJobEventParseCustomJob callback from OTAAgent, but no custom jobs supported.");

        // カスタムジョブの処理(現状何も対応していない)
        break;

    case OtaJobEventReceivedJob:
        APP_PRINTFInfo("Received OtaJobEventReceivedJob callback from OTAAgent.");

        // 何もしない
        break;

    case OtaJobEventUpdateComplete:
        APP_PRINTFInfo("Received OtaJobEventUpdateComplete callback from OTAAgent.");

        // 何もしない
        break;

    default:
        APP_PRINTFWarn("Received invalid callback event from OTAAgent; Event: %d", xEvent);

        break;
    }
}

// --------------------------------------------------
// Unit Test用関数定義(関数のプロトタイプ宣言は「自身のファイル名+ "_test.h"」で宣言されていること)
// --------------------------------------------------
#if (BUILD_MODE_TEST == 1) /* BUILD_MODE_TESTが定義されているとき */
#endif                     /* end  BUILD_MODE_TEST */
