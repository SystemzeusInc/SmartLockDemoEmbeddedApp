/**
 * @file device_shadow_task.c
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
#include "shadow.h"
#include "core_json.h"
#include "queue.h"
#include "core_mqtt.h"

// --------------------------------------------------
// ユーザ作成ヘッダの取り込み
// --------------------------------------------------
#include "config/task_config.h"
#include "config/queue_config.h"
#include "config/device_shadow_config.h"

#include "common/include/application_define.h"
#include "common/include/device_state.h"

#include "tasks/flash/include/flash_data.h"
#include "tasks/flash/include/flash_task.h"
#include "tasks/mqtt/include/mqtt_operation_task.h"
#include "tasks/shadow/include/device_shadow_task.h"

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
 * @brief ShadowTaskに送信するコマンドタイプ
 */
typedef enum
{
    /**
     * @brief ShadowUpdate
     */
    SHADOW_COMMAND_TYPE_UPDATE = 0x00,

    /**
     * @brief ShadowGet
     */
    SHADOW_COMMAND_TYPE_GET = 0x01,

    /**
     * @brief ShadowTaskの終了
     */
    SHADOW_COMMAND_TYPE_SHUTDOWN = 0x02,
} ShadowTaskCommandType_t;

/**
 * @brief MQTTSubscribeCallback関数によって呼ばれる際のコンテキスト
 */
typedef struct
{
    /**
     * @brief [in,out] ペイロードのバッファ
     */
    uint8_t *puxPayload;

    /**
     * @brief [in] ペイロードバッファのサイズ
     */
    uint32_t uxPayloadBufferSize;

    /**
     * @brief [out] 受信したペイロードの長さ
     */
    uint32_t uxPayloadLength;

    /**
     * @brief [in] 通知を受けるタスクハンドル
     */
    TaskHandle_t xNotifyTaskHandle;

    /**
     * @brief [in] クライアントトークン
     */
    uint8_t *pucClientToken;

    /**
     * @brief[in] クライアントトークンの長さ
     */
    uint32_t uxClientTokenLength;
} DeviceShadowMQTTIncomingContext_t;

/**
 * @brief update/deltaが発火した際にMQTTSubscribeCallback関数によって呼ばれる際のコンテキスト
 *
 */
typedef struct
{
    /**
     * @brief ShadowのDeltaが発火した際のコールバック関数
     */
    ShadowChangeCallback_t xCallback;

} DeviceShadowDeltaMQTTIncomingContext_t;

/**
 * @brief MQTTのリクエストを行う際のデータ型
 */
typedef struct
{
    /**
     * @brief[in] トピック名
     */
    uint8_t *pucTopicName;

    /**
     * @brief[in] トピック名の長さ。NULL文字は数えない。
     */
    uint32_t uxTopicNameLength;

    /**
     * @brief[in] ペイロード。NULL可。NULLの場合はuxPayloadLengthを0にする。
     */
    uint8_t *pucPayload;

    /**
     * @brief[in] ペイロード長
     */
    uint32_t uxPayloadLength;

    /**
     * @brief クライアントトークン
     */
    uint8_t *pucClientToken;

    /**
     * @brief クライアントトークンの長さ
     */
    uint32_t uxClientTokenLength;

} MQTTRequest_t;

/**
 * @brief MQTTのレスポンスを得るためのデータ型
 */
typedef struct
{
    /**
     * @brief[in] トピック名
     */
    uint8_t *pucTopicName;

    /**
     * @brief[in] トピック名の長さ。NULL文字は数えない。
     */
    uint32_t uxTopicNameLength;

    /**
     * @brief[in,out] クラウドから受け取ったペイロードを格納するバッファ。NULL不可。
     */
    uint8_t *pucPayloadBuffer;

    /**
     * @brief[in] ペイロードバッファの大きさ
     */
    uint32_t uxPayloadBufferSize;

    /**
     * @brief[out] 受信したペイロードの長さ
     */
    uint32_t uxReceivePayloadLength;
} MQTTResponse_t;

/**
 * @brief ShadowTaskにUpdateコマンドを送信する際に使用するコンテキスト
 */
typedef struct
{
    /**
     * @brief どのステータスを更新するかを示すタイプ
     */
    uint32_t xUpdateShadowType;

    /**
     * @brief 更新するデータ
     */
    ShadowState_t xShadowState;

    /**
     * @brief Shadow更新が終了したことを通知するタスクハンドル
     */
    TaskHandle_t xWaitingTaskHandle;
} ShadowUpdateCommand_t;

/**
 * @brief ShadowTaskにShutdownコマンドを送信しする際に使用するコンテキスト
 */
typedef struct
{
    /**
     * @brief 待機するタスクのタスクハンドル
     */
    TaskHandle_t xNotifyTaskHandle;
} ShadowShutdownCommand_t;

/**
 * @brief ShadowTaskにGetコマンドを送信しする際に使用するコンテキスト
 */
typedef struct
{
    /**
     * @brief[out] 更新するデータ
     */
    ShadowState_t *pxShadowState;

    /**
     * @brief[in] Shadow更新が終了したことを通知するタスクハンドル
     */
    TaskHandle_t xWaitingTaskHandle;
} ShadowGetCommand_t;

/**
 * @brief ShadowTaskにコマンドを送る際に使用するコンテキスト
 */
typedef struct
{
    /**
     * @brief コマンドタイプ
     */
    ShadowTaskCommandType_t eCommandType;

    union
    {
        /**
         * @brief Shadow Update Command
         */
        ShadowUpdateCommand_t xUpdateCommand;

        /**
         * @brief Shadow Shutdown Command
         */
        ShadowShutdownCommand_t xShutdownCommand;

        /**
         * @brief Shadow Get Command
         */
        ShadowGetCommand_t xGetCommand;
    } u;

} ShadowTaskQueueCommand_t;

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
 * @brief 本タスクのタスクハンドル
 */
static TaskHandle_t gxShadowTaskHandle = NULL;

/**
 * @brief 本タスクとやり取りするQueueのハンドル
 */
static QueueHandle_t gxShadowQueueHandle = NULL;

/**
 * @brief Shadow更新に使用するMQTTトピックの情報
 * スコープを維持するためグローバル変数化する。
 */
static MQTTSubscribeInfo_t gxDeltaSubscribeInfo = {0x00};

/**
 * @brief ShadowのDeltaに更新があった場合のコールバック関数に渡されるコンテキスト
 * スコープを維持するためグローバル変数化する。
 */
static DeviceShadowDeltaMQTTIncomingContext_t gxDeltaIncomingContext = {0x00};

// --------------------------------------------------
// static関数プロトタイプ宣言
// --------------------------------------------------

// ----------------------------------- Task -----------------------------------

/**
 * @brief ShadowTaskのエントリ
 *
 * @param[in] pvParameter 使用しない
 */
static void vprvShadowTask(void *pvParameter);

// ----------------------------------- Static API -----------------------------------

/**
 * @brief get/acceptedで得たShadowメッセージを解析する。
 *
 * @param[in]  pucShadowData              クラウドから受け取ったShadowメッセージ
 * @param[in]  uxReceivedShadowDataLength 受信したペイロードの長さ
 * @param[out] pxOutShadowState           Shadowの状態
 * @param[in]  pucShadowSearchPath        Shadowをどのキーで探索するかを示す接頭辞。
 *                                        get/acceptedの場合は SHADOW_DESIRED_PATH  update/deltaの場合は SHADOW_STATE_PATH を指定する。
 *
 * @retval true  解析成功
 * @retval false 解析失敗
 */
static bool bprvGetDeviceShadowSateFromJSON(const uint8_t *pucShadowData,
                                            const uint32_t uxReceivedShadowDataLength,
                                            ShadowState_t *pxOutShadowState,
                                            const uint8_t *pucShadowSearchPath);

/**
 * @brief リクエストを行うとレスポンスが返却されるタイプのMQTT通信を処理する
 *
 * @param[in]     pxRequest  MQTTのリクエストに必要な情報
 * @param[in,out] pxResponse MQTTのレスポンスを得るために必要な情報
 * @param[in]     xTimeoutMs タイムアウト
 *
 * @retval true  成功
 * @retval false 失敗
 */
static bool bprvMQTTRequestWithResponse(const MQTTRequest_t *pxRequest,
                                        MQTTResponse_t *pxResponse,
                                        const uint32_t xTimeoutMs);

/**
 * @brief UpdateShadowを行う際のペイロードを作成する
 *
 * @param[in]     xUpdateType               Updateを行うShadowType
 * @param[in]     pxShadowState             実際のステータス
 * @param[in,out] pucUpdatePayloadBuffer    作成したペイロードを格納するバッファ
 * @param[in]     uxUpdatePayloadBufferSize バッファサイズ
 * @param[in,out] pucClientTokenBuffer      クライアントトークンを格納するバッファ
 * @param[in]     uxClientTokenBufferSize   クライアントトークンのバッファサイズ。CREATE_CLIENT_TOKEN_MAX_LENGTH + 1 の大きさを持ったバッファが必要。
 *
 * @retval true  成功
 * @retval false 失敗
 */
static bool bprvCreateUpdatePayload(const uint32_t xUpdateType,
                                    const ShadowState_t *pxShadowState,
                                    uint8_t *pucUpdatePayloadBuffer,
                                    const uint32_t uxUpdatePayloadBufferSize,
                                    uint8_t *pucClientTokenBuffer,
                                    const uint32_t uxClientTokenBufferSize);

/**
 * @brief クラウドのShadowステータス変化に応じて呼び出されるコールバック関数を登録し、同時にステータス変化の通知を受けるようにMQTTSubscribeする。
 *
 * @param[in] xCallbackFunction 登録するコールバック関数
 *
 * @retval true  成功
 * @retval false 失敗
 */
static bool bprvSubscribeAndRegisterShadowStateChangeCallback(const ShadowChangeCallback_t xCallbackFunction);

/**
 * @brief 指定したShadowStateをUpdateする
 *
 * @param[in] xUpdateShadowType UpdateするShadowのタイプ。 ShadowUpdateType_t の組み合わせ。
 *                              例えば施錠状態と解錠パターンをアップデートする場合は (SHADOW_UPDATE_TYPE_LOCK_STATE) とする
 * @param[in] pxShadowState     更新する情報
 *
 * @retval true  成功
 * @retval false 失敗
 */
static bool bprvUpdateShadowState(const uint32_t xUpdateShadowType, const ShadowState_t *pxShadowState);

/**
 * @brief 現在のShadowの状態を取得する
 *
 * @param[out] pxOutShadowSate クラウドから得たShadowの状態を格納するバッファ
 *
 * @retval true  成功
 * @retval false 失敗
 */
static bool bprvGetShadowState(ShadowState_t *pxOutShadowSate);

// ----------------------------------- CALLBACKS -----------------------------------

/**
 * @brief  SubscribeしたトピックにPublishが行われた時にCallbackされる関数
 *
 * @param[in] pvIncomingPublishCallbackContext eMQTTSubscribeの第3引数で指定されたコンテキスト。本APIは #DeviceShadowMQTTIncomingContext_t にキャストする
 * @param[in] pxPublishInfo                    callbackが行われた時のMQTT情報。トピック名やペイロード等が格納されている。
 */
static void vprvGetAndUpdateShadowIncomingPublishCallback(void *pvIncomingPublishCallbackContext,
                                                          MQTTPublishInfo_t *pxPublishInfo);

/**
 * @brief  SubscribeしたトピックにPublishが行われた時にCallbackされる関数
 *
 * @param[in] pvIncomingPublishCallbackContext eMQTTSubscribeの第3引数で指定されたコンテキスト。本APIは #DeviceShadowDeltaMQTTIncomingContext_t にキャストする
 * @param[in] pxPublishInfo                    callbackが行われた時のMQTT情報。トピック名やペイロード等が格納されている。
 */
static void vprvDeltaShadowIncomingPublishCallback(void *pvIncomingPublishCallbackContext,
                                                   MQTTPublishInfo_t *pxPublishInfo);

/**
 * @brief 受信したペイロードの中に含まれるクライアントトークンが一致するか調べる
 *
 * @param[in] pucPayload          受信したペイロード
 * @param[in] uxPayloadLength     受信したペイロードの長さ
 * @param[in] pucClientToken      クライアントトークン
 * @param[in] uxClientTokenLength クライアントトークンの長さ
 *
 * @retval true  一致
 * @retval false 不一致
 */
static bool bprvIsMatchClientToken(uint8_t *pucPayload,
                                   uint32_t uxPayloadLength,
                                   uint8_t *pucClientToken,
                                   uint32_t uxClientTokenLength);

// --------------------------------------------------
// 変数定義（staticを除く）
// --------------------------------------------------

// --------------------------------------------------
// 関数定義（staticを除く）
// --------------------------------------------------

DeviceShadowResult_t eDeviceShadowTaskInit(const ShadowChangeCallback_t xCallbackFuction)
{

    // Deltaのサブスクライブ
    if (bprvSubscribeAndRegisterShadowStateChangeCallback(xCallbackFuction) == false)
    {
        APP_PRINTFError("Failed to subscribe delta topic.");
        return DEVICE_SHADOW_RESULT_FAILED;
    }

    // Queueが作成されていない場合は、Queueの作成
    if (gxShadowQueueHandle == NULL)
    {
        gxShadowQueueHandle = xQueueCreate(SHADOW_TASK_QUEUE_LENGTH, sizeof(ShadowTaskQueueCommand_t));

        if (gxShadowQueueHandle == NULL)
        {
            APP_PRINTFFatal("Shadow task queue creation failed.");
            return DEVICE_SHADOW_RESULT_FAILED;
        }
    }

    // Taskが作成されていない場合は、タスクの作成
    if (gxShadowTaskHandle == NULL)
    {
        APP_PRINTFDebug("Init shadow task.");

        if (xTaskCreate(&vprvShadowTask,
                        "ShadowTask",
                        SHADOW_TASK_SIZE,
                        NULL,
                        SHADOW_TASK_PRIORITY,
                        &gxShadowTaskHandle) != pdTRUE)
        {
            APP_PRINTFFatal("Shadow task creation failed.");
            return DEVICE_SHADOW_RESULT_FAILED;
        }
    }

    return DEVICE_SHADOW_RESULT_SUCCESS;
}

DeviceShadowResult_t eDeviceShadowTaskShutdown(void)
{
    // 送信先のキューとタスクが作成されているかチェック
    if (gxShadowQueueHandle == NULL || gxShadowTaskHandle == NULL)
    {
        APP_PRINTFDebug("Unable to send Update command because no queue or task has been created.");
        return DEVICE_SHADOW_RESULT_SUCCESS;
    }

    // Queueの送信に必要なコンテキストを作成
    ShadowTaskQueueCommand_t xSendCommand = {0x00};
    xSendCommand.eCommandType = SHADOW_COMMAND_TYPE_SHUTDOWN;
    xSendCommand.u.xShutdownCommand.xNotifyTaskHandle = xTaskGetCurrentTaskHandle();

    APP_PRINTF("Shadow task shutdown command sending...");

    // QueueにUpdateコマンドを送信する。必ずShutdownに成功してほしいため、無限待機する
    if (xQueueSend(gxShadowQueueHandle, &xSendCommand, portMAX_DELAY) != pdPASS)
    {
        APP_PRINTFError("Could not send because Queue was full");
        return DEVICE_SHADOW_RESULT_FAILED;
    }

    // タスクの終了を待機
    if (ulTaskNotifyTake(pdTRUE, SHADOW_TASK_SHUTDOWN_WAIT_MS) != pdPASS)
    {
        APP_PRINTFError("Shadow update timeout. Timeout MS = %d", SHADOW_TASK_SHUTDOWN_WAIT_MS);
    }

    return DEVICE_SHADOW_RESULT_SUCCESS;
}

DeviceShadowResult_t eGetShadowState(ShadowState_t *pxOutShadowSate, uint32_t xTimeoutMS)
{
    // 送信先のキューが作成されているかチェック
    if (gxShadowQueueHandle == NULL)
    {
        APP_PRINTFError("Unable to send Update command because no queue has been created.");
        return DEVICE_SHADOW_RESULT_FAILED;
    }

    // Queueの送信に必要なコンテキストを作成
    ShadowTaskQueueCommand_t xSendCommand = {0x00};
    xSendCommand.eCommandType = SHADOW_COMMAND_TYPE_GET;
    xSendCommand.u.xGetCommand.pxShadowState = pxOutShadowSate;
    xSendCommand.u.xGetCommand.xWaitingTaskHandle = xTaskGetCurrentTaskHandle();

    // QueueにUpdateコマンドを送信する。呼び出しタスクのブロックはせず、Queueが満杯の場合は待機せず送信NGにする。
    if (xQueueSend(gxShadowQueueHandle, &xSendCommand, (TickType_t)0) != pdPASS)
    {
        APP_PRINTFError("Could not send because Queue was full");
        return DEVICE_SHADOW_RESULT_FAILED;
    }

    // タスクの終了を待機
    if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(xTimeoutMS)) != pdPASS)
    {
        APP_PRINTFError("Shadow get timeout. Timeout MS = %d", xTimeoutMS);
    }

    return DEVICE_SHADOW_RESULT_SUCCESS;
}

DeviceShadowResult_t eUpdateShadowStateSync(const uint32_t xUpdateShadowType, const ShadowState_t *pxShadowState, uint32_t xTimeoutMS)
{
    // 送信先のキューが作成されているかチェック
    if (gxShadowQueueHandle == NULL)
    {
        APP_PRINTFError("Unable to send Update command because no queue has been created.");
        return DEVICE_SHADOW_RESULT_FAILED;
    }

    // Queueの送信に必要なコンテキストを作成
    ShadowTaskQueueCommand_t xSendCommand = {0x00};
    xSendCommand.eCommandType = SHADOW_COMMAND_TYPE_UPDATE;
    xSendCommand.u.xUpdateCommand.xUpdateShadowType = xUpdateShadowType;
    xSendCommand.u.xUpdateCommand.xWaitingTaskHandle = xTaskGetCurrentTaskHandle();
    memcpy(&(xSendCommand.u.xUpdateCommand.xShadowState), pxShadowState, sizeof(ShadowState_t));

    // QueueにUpdateコマンドを送信する。呼び出しタスクのブロックはせず、Queueが満杯の場合は待機せず送信NGにする。
    if (xQueueSend(gxShadowQueueHandle, &xSendCommand, (TickType_t)0) != pdPASS)
    {
        APP_PRINTFError("Could not send because Queue was full");
        return DEVICE_SHADOW_RESULT_FAILED;
    }

    // タスクの終了を待機
    if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(xTimeoutMS)) != pdPASS)
    {
        APP_PRINTFError("Shadow update timeout. Timeout MS = %d", xTimeoutMS);
    }

    return DEVICE_SHADOW_RESULT_SUCCESS;
}

DeviceShadowResult_t eUpdateShadowStateAsync(const uint32_t xUpdateShadowType, const ShadowState_t *pxShadowState)
{
    // 送信先のキューが作成されているかチェック
    if (gxShadowQueueHandle == NULL)
    {
        APP_PRINTFError("Unable to send Update command because no queue has been created.");
        return DEVICE_SHADOW_RESULT_FAILED;
    }

    // Queueの送信に必要なコンテキストを作成
    ShadowTaskQueueCommand_t xSendCommand = {0x00};
    xSendCommand.eCommandType = SHADOW_COMMAND_TYPE_UPDATE;
    xSendCommand.u.xUpdateCommand.xUpdateShadowType = xUpdateShadowType;
    xSendCommand.u.xUpdateCommand.xWaitingTaskHandle = NULL; // 非同期なので通知を受けない
    memcpy(&(xSendCommand.u.xUpdateCommand.xShadowState), pxShadowState, sizeof(ShadowState_t));

    // QueueにUpdateコマンドを送信する。呼び出しタスクのブロックはせず、Queueが満杯の場合は待機せず送信NGにする。
    if (xQueueSend(gxShadowQueueHandle, &xSendCommand, (TickType_t)0) != pdPASS)
    {
        APP_PRINTFError("Could not send because Queue was full");
        return DEVICE_SHADOW_RESULT_FAILED;
    }

    return DEVICE_SHADOW_RESULT_SUCCESS;
}

// --------------------------------------------------
// static関数定義
// --------------------------------------------------

// ---------------------- Layer 1 ---------------------------

static void vprvShadowTask(void *pvParameter)
{
    (void)pvParameter;

    ShadowTaskQueueCommand_t xReceiveCommand;
    bool bIsShutdownCommand = false;

    APP_PRINTFDebug("Start shadow task");

    while (true)
    {
        memset(&xReceiveCommand, 0x00, sizeof(xReceiveCommand));
        if (xQueueReceive(gxShadowQueueHandle, &xReceiveCommand, portMAX_DELAY) == pdTRUE)
        {
            switch (xReceiveCommand.eCommandType)
            {
            // Shadow update command
            case SHADOW_COMMAND_TYPE_UPDATE:

                APP_PRINTFDebug("Start processing update command");

                // ShadowのUpdate処理を行う
                if (bprvUpdateShadowState(xReceiveCommand.u.xUpdateCommand.xUpdateShadowType,
                                          &(xReceiveCommand.u.xUpdateCommand.xShadowState)) == false)
                {
                    APP_PRINTFError("Failed to update shadow status.");
                }

                // 待ち合わせのタスクがある場合は、タスクにUpdate終了を通知
                if (xReceiveCommand.u.xUpdateCommand.xWaitingTaskHandle != NULL)
                {
                    xTaskNotifyGive(xReceiveCommand.u.xUpdateCommand.xWaitingTaskHandle);
                }

                break;
            // Shadow get command
            case SHADOW_COMMAND_TYPE_GET:

                APP_PRINTFDebug("Start processing get command");

                // ShadowのGet処理を行う
                if (bprvGetShadowState(xReceiveCommand.u.xGetCommand.pxShadowState) == false)
                {
                    APP_PRINTFError("Failed to get shadow status.");
                }

                // 待ち合わせのタスクがある場合は、タスクにGet終了を通知
                if (xReceiveCommand.u.xGetCommand.xWaitingTaskHandle != NULL)
                {
                    xTaskNotifyGive(xReceiveCommand.u.xGetCommand.xWaitingTaskHandle);
                }
                break;
            // Shadow Task shutdown command
            case SHADOW_COMMAND_TYPE_SHUTDOWN:
                bIsShutdownCommand = true; // 2重でBreakはできないため、フラグを立てるだけにする
                break;
            }
        }

        // Shutdownコマンドが立っている場合は、ループ処理を抜けShutdown処理を行う
        if (bIsShutdownCommand == true)
        {
            APP_PRINTFDebug("Received shadow task shutdown command.");
            break;
        }

        PRINT_TASK_REMAINING_STACK_SIZE();
    }

    // ---------------------- 以下ShadowTaskの終了処理 ----------------------

    // Delta関数のUnsubscribe
    if (gxDeltaSubscribeInfo.pTopicFilter != NULL)
    {
        APP_PRINTFDebug("Unsubscribe delta topic.");

        // DeltaトピックのUnsubscribe
        static StaticMQTTCommandBuffer_t xUnsubscribeMQTTContextBuffer; // コンテキスト保存場所を永続化したいためStaticで宣言
        memset(&xUnsubscribeMQTTContextBuffer, 0x00, sizeof(xUnsubscribeMQTTContextBuffer));
        if (eMQTTUnsubscribe(&gxDeltaSubscribeInfo, &xUnsubscribeMQTTContextBuffer) != MQTT_OPERATION_TASK_RESULT_SUCCESS)
        {
            // MQTTが既に切断され、Unsubscribeに失敗するかもしれないが、
            // 問題がないため、Warningログだけ出力して処理は継続する
            APP_PRINTFWarn("Unsubscribe delta topic failed.");
        }
    }

    APP_PRINTFDebug("Shadow Task Shutdown");

    // Taskに通知
    if (xReceiveCommand.u.xShutdownCommand.xNotifyTaskHandle != NULL)
    {
        xTaskNotifyGive(xReceiveCommand.u.xShutdownCommand.xNotifyTaskHandle);
    }

    PRINT_TASK_REMAINING_STACK_SIZE();

    // タスクを削除する前にNULLにしておく。
    gxShadowTaskHandle = NULL;

    // 自身のタスクを削除
    vTaskDelete(NULL);
}

static bool bprvSubscribeAndRegisterShadowStateChangeCallback(const ShadowChangeCallback_t xCallbackFunction)
{

    // 普段使用するThingNameを取得
    ThingName_t xThingName;
    memset(&xThingName, 0x00, sizeof(xThingName));
    if (eReadFlashInfo(READ_FLASH_TYPE_USUAL_THING_NAME,
                       &xThingName,
                       sizeof(xThingName)) != FLASH_TASK_RESULT_SUCCESS)
    {
        APP_PRINTFError("Read factory thing name error.");
        return false;
    }

    // トピック名を作成
    // 本関数をコールすると、Shadowに変化があるといつでもDeltaトピックが呼び出される可能性があるため、Static領域に保存する
    static uint8_t ucDeltaTopicName[SHADOW_TOPIC_LENGTH_UPDATE_DELTA(THING_NAME_LENGTH) + 1];
    uint32_t uxDeltaShadowTopicLength = 0;
    memset(ucDeltaTopicName, 0x00, sizeof(ucDeltaTopicName));
    ShadowStatus_t xShadowTopicStatus = Shadow_GetTopicString(ShadowTopicStringTypeUpdateDelta,
                                                              xThingName.ucName,
                                                              THING_NAME_LENGTH,
                                                              ucDeltaTopicName,
                                                              SHADOW_TOPIC_LENGTH_UPDATE_DELTA(THING_NAME_LENGTH),
                                                              &uxDeltaShadowTopicLength);

    if (xShadowTopicStatus != SHADOW_SUCCESS)
    {
        APP_PRINTFError("Create failed get delta shadow topic. Reason: %u", xShadowTopicStatus);
        return false;
    }

    // deltaが発火した際にコールバック関数に渡されるコンテキストを作成
    // 本関数をコールすると、Shadowに変化があるといつでもDeltaトピックが呼び出される可能性があるため、Static領域に保存する
    memset(&gxDeltaIncomingContext, 0x00, sizeof(gxDeltaIncomingContext));
    gxDeltaIncomingContext.xCallback = xCallbackFunction;

    APP_PRINTFDebug("bprvShadowChangeCallback pointer is %p", gxDeltaIncomingContext.xCallback);

    // サブスクライブに必要な情報を格納
    // 本関数をコールすると、Shadowに変化があるといつでもDeltaトピックが呼び出される可能性があるため、Static領域に保存する
    memset(&gxDeltaSubscribeInfo, 0x00, sizeof(gxDeltaSubscribeInfo));
    gxDeltaSubscribeInfo.pTopicFilter = ucDeltaTopicName;
    gxDeltaSubscribeInfo.topicFilterLength = uxDeltaShadowTopicLength;
    gxDeltaSubscribeInfo.qos = MQTTQoS0;

    // Deltaトピックをサブスクライブ
    static StaticMQTTCommandBuffer_t xSubscribeMQTTContextBuffer; // コンテキスト保存場所を永続化したいためStaticで宣言
    memset(&xSubscribeMQTTContextBuffer, 0x00, sizeof(xSubscribeMQTTContextBuffer));

    MQTTOperationTaskResult_t eMQTTResult = eMQTTSubscribe(&gxDeltaSubscribeInfo,
                                                           &vprvDeltaShadowIncomingPublishCallback,
                                                           &gxDeltaIncomingContext,
                                                           &xSubscribeMQTTContextBuffer);
    if (eMQTTResult != MQTT_OPERATION_TASK_RESULT_SUCCESS)
    {
        APP_PRINTFError("Subscribe error: %d", eMQTTResult);
        return false;
    }

    return true;
}

// ---------------------- Layer 2 ---------------------------

static bool bprvUpdateShadowState(const uint32_t xUpdateShadowType, const ShadowState_t *pxShadowState)
{
    if (pxShadowState == NULL)
    {
        APP_PRINTFError("pxShadowState is null.");
        return false;
    }

    // 普段使用するThingNameを取得
    ThingName_t xThingName = {0x00};
    if (eReadFlashInfo(READ_FLASH_TYPE_USUAL_THING_NAME,
                       &xThingName,
                       sizeof(xThingName)) != FLASH_TASK_RESULT_SUCCESS)
    {
        APP_PRINTFError("Read factory thing name error.");
        return false;
    }

    ShadowStatus_t xShadowTopicStatus;
    uint8_t ucUpdateShadowResponseBuffer[UPDATE_DEVICE_SHADOW_PAYLOAD_BUFFER_SIZE] = {0x00};

    // ShadowのUpdateAcceptedトピックを作成
    uint8_t ucUpdateAcceptShadowTopicName[SHADOW_TOPIC_LENGTH_UPDATE_ACCEPTED(THING_NAME_LENGTH) + 1] = {0x00};
    uint16_t uxUpdateAcceptShadowTopicLength = 0;
    xShadowTopicStatus = Shadow_GetTopicString(ShadowTopicStringTypeUpdateAccepted,
                                               xThingName.ucName,
                                               THING_NAME_LENGTH,
                                               ucUpdateAcceptShadowTopicName,
                                               SHADOW_TOPIC_LENGTH_UPDATE_ACCEPTED(THING_NAME_LENGTH),
                                               &uxUpdateAcceptShadowTopicLength);

    if (xShadowTopicStatus != SHADOW_SUCCESS)
    {
        APP_PRINTFError("Create failed get accept shadow topic. Reason: %u", xShadowTopicStatus);
        return false;
    }

    // UpdateAccepted用のコンテキストを作成
    MQTTResponse_t xMQTTResponse = {
        .pucTopicName = ucUpdateAcceptShadowTopicName,
        .uxTopicNameLength = uxUpdateAcceptShadowTopicLength,
        .pucPayloadBuffer = ucUpdateShadowResponseBuffer,
        .uxPayloadBufferSize = sizeof(ucUpdateShadowResponseBuffer)};

    // ShadowのUpdateトピックを作成
    uint8_t ucUpdateShadowTopicName[SHADOW_TOPIC_LENGTH_UPDATE(THING_NAME_LENGTH) + 1] = {0x00};
    uint16_t uxUpdateShadowTopicLength = 0;
    xShadowTopicStatus = Shadow_GetTopicString(ShadowTopicStringTypeUpdate,
                                               xThingName.ucName,
                                               THING_NAME_LENGTH,
                                               ucUpdateShadowTopicName,
                                               SHADOW_TOPIC_LENGTH_UPDATE(THING_NAME_LENGTH),
                                               &uxUpdateShadowTopicLength);

    if (xShadowTopicStatus != SHADOW_SUCCESS)
    {
        APP_PRINTFError("Create failed get accept shadow topic. Reason: %u", xShadowTopicStatus);
        return false;
    }

    // ShadowのUpdate用ペイロード作成
    uint8_t ucShadowPayload[SHADOW_UPDATE_MAX_LENGTH + 1] = {0x00};
    uint8_t ucClientToken[CREATE_CLIENT_TOKEN_MAX_LENGTH + 1] = {0x00};
    if (bprvCreateUpdatePayload(xUpdateShadowType,
                                pxShadowState,
                                ucShadowPayload,
                                sizeof(ucShadowPayload),
                                ucClientToken,
                                sizeof(ucClientToken)) == false)
    {
        APP_PRINTFError("Create update shadow payload failed.");
        return false;
    }

    // Update用のコンテキストを作成
    MQTTRequest_t xMQTTRequest = {
        .pucTopicName = ucUpdateShadowTopicName,
        .uxTopicNameLength = uxUpdateShadowTopicLength,
        .pucPayload = ucShadowPayload,
        .uxPayloadLength = strlen(ucShadowPayload),
        .pucClientToken = ucClientToken,
        .uxClientTokenLength = strlen(ucClientToken)};

    if (bprvMQTTRequestWithResponse(&xMQTTRequest, &xMQTTResponse, SHADOW_MQTT_TIMEOUT_MS) == false)
    {
        APP_PRINTFError("Update shadow mqtt process failed.");
        return false;
    }

    APP_PRINTFDebug("Received shadow mqtt response: %s", ucUpdateShadowResponseBuffer);

    return true;
}

static bool bprvGetShadowState(ShadowState_t *pxOutShadowSate)
{
    if (pxOutShadowSate == NULL)
    {
        APP_PRINTFError("pxOutShadowSate is null.");
        return false;
    }

    // 普段使用するThingNameを取得
    ThingName_t xThingName;
    memset(&xThingName, 0x00, sizeof(xThingName));
    if (eReadFlashInfo(READ_FLASH_TYPE_USUAL_THING_NAME,
                       &xThingName,
                       sizeof(xThingName)) != FLASH_TASK_RESULT_SUCCESS)
    {
        APP_PRINTFError("Read factory thing name error.");
        return false;
    }

    ShadowStatus_t xShadowTopicStatus;
    uint8_t ucGetShadowResponseBuffer[GET_DEVICE_SHADOW_PAYLOAD_BUFFER_SIZE] = {0x00};

    // ShadowのGetAcceptトピックを作成
    uint8_t ucGetAcceptShadowTopicName[SHADOW_TOPIC_LENGTH_GET_ACCEPTED(THING_NAME_LENGTH) + 1] = {0x00};
    uint16_t uxGetAcceptShadowTopicLength = 0;
    xShadowTopicStatus = Shadow_GetTopicString(ShadowTopicStringTypeGetAccepted,
                                               xThingName.ucName,
                                               THING_NAME_LENGTH,
                                               ucGetAcceptShadowTopicName,
                                               SHADOW_TOPIC_LENGTH_GET_ACCEPTED(THING_NAME_LENGTH),
                                               &uxGetAcceptShadowTopicLength);

    if (xShadowTopicStatus != SHADOW_SUCCESS)
    {
        APP_PRINTFError("Create failed update accept shadow topic. Reason: %u", xShadowTopicStatus);
        return false;
    }

    MQTTResponse_t xMQTTResponse = {
        .pucTopicName = ucGetAcceptShadowTopicName,
        .uxTopicNameLength = uxGetAcceptShadowTopicLength,
        .pucPayloadBuffer = ucGetShadowResponseBuffer,
        .uxPayloadBufferSize = sizeof(ucGetShadowResponseBuffer)};

    // ShadowのGETトピックを作成
    uint8_t ucGetShadowTopicName[SHADOW_TOPIC_LENGTH_GET(THING_NAME_LENGTH) + 1] = {0x00};
    uint16_t uxGetShadowTopicLength = 0;
    xShadowTopicStatus = Shadow_GetTopicString(ShadowTopicStringTypeGet,
                                               xThingName.ucName,
                                               THING_NAME_LENGTH,
                                               ucGetShadowTopicName,
                                               SHADOW_TOPIC_LENGTH_GET(THING_NAME_LENGTH),
                                               &uxGetShadowTopicLength);

    if (xShadowTopicStatus != SHADOW_SUCCESS)
    {
        APP_PRINTFError("Create failed update shadow topic. Reason: %u", xShadowTopicStatus);
        return false;
    }

    // MQTT通信
    MQTTRequest_t xMQTTRequest = {
        .pucTopicName = ucGetShadowTopicName,
        .uxTopicNameLength = uxGetShadowTopicLength,
        .pucPayload = NULL,
        .uxPayloadLength = 0,
        .pucClientToken = NULL,
        .uxClientTokenLength = 0};

    if (bprvMQTTRequestWithResponse(&xMQTTRequest, &xMQTTResponse, SHADOW_MQTT_TIMEOUT_MS) == false)
    {
        APP_PRINTFError("Get shadow mqtt process failed. Reason: %u", xShadowTopicStatus);
        return false;
    }

    APP_PRINTFDebug("Received shadow length %u", xMQTTResponse.uxReceivePayloadLength);

    // 受信したShadowメッセージのJsonを解析
    ShadowState_t xShadowStatus;
    bprvGetDeviceShadowSateFromJSON(ucGetShadowResponseBuffer,
                                    xMQTTResponse.uxReceivePayloadLength,
                                    &xShadowStatus,
                                    SHADOW_DESIRED_PATH);

    APP_PRINTFDebug("shadow lock status: %u", xShadowStatus.xLockState);

    // Out変数に格納
    memcpy(pxOutShadowSate, &xShadowStatus, sizeof(ShadowState_t));

    return true;
}

// ---------------------- Layer 3 ---------------------------

static bool bprvCreateUpdatePayload(const uint32_t xUpdateType,
                                    const ShadowState_t *pxShadowState,
                                    uint8_t *pucUpdatePayloadBuffer,
                                    const uint32_t uxUpdatePayloadBufferSize,
                                    uint8_t *pucClientTokenBuffer,
                                    const uint32_t uxClientTokenBufferSize)
{
    uint8_t ucShadowStatePart[SHADOW_JSON_STATE_PART_MAX_LENGTH + 1] = {0x00};

    // バッファの初期化
    memset(pucUpdatePayloadBuffer, 0x00, uxUpdatePayloadBufferSize);
    memset(pucClientTokenBuffer, 0x00, uxClientTokenBufferSize);

    switch (xUpdateType)
    {
    case (SHADOW_UPDATE_TYPE_LOCK_STATE):
    {

        if (pxShadowState->xLockState == LOCK_STATE_UNDEFINED || pxShadowState->xUnlockingOperator == UNLOCKING_OPERATOR_TYPE_UNDEFINED)
        {
            APP_PRINTFError("Shadow state unknown error.");
            return false;
        }

        uint8_t uxLockStateString[LOCK_STATE_STRING_MAX_LENGTH + 1] = {0x00};
        uint8_t uxOperator[UNLOCKING_OPERATOR_TYPE_STRING_MAX_LENGTH + 1] = {0x00};

        // EnumをStringに変換
        vConvertEnumToStringLockState(pxShadowState->xLockState, uxLockStateString, sizeof(uxLockStateString));
        vConvertEnumToStringUnlockingOperatorType(pxShadowState->xUnlockingOperator, uxOperator, sizeof(uxOperator));

        snprintf(&ucShadowStatePart[0], sizeof(ucShadowStatePart), SHADOW_JSON_LOCK_STATE "," SHADOW_JSON_OPERATOR, uxLockStateString, uxOperator);
        break;
    }
    }

    // ClientTokenを生成
    CREATE_CLIENT_TOKEN(pucClientTokenBuffer, uxClientTokenBufferSize);

    // Payloadを生成
    CREATE_SHADOW(pucUpdatePayloadBuffer, uxUpdatePayloadBufferSize, ucShadowStatePart, pucClientTokenBuffer);

    APP_PRINTF("Create shadow payload: %s", pucUpdatePayloadBuffer);

    return true;
}

static bool bprvGetDeviceShadowSateFromJSON(const uint8_t *pucShadowData,
                                            const uint32_t uxReceivedShadowDataLength,
                                            ShadowState_t *pxOutShadowState,
                                            const uint8_t *pucShadowSearchPath)
{
    uint8_t *pucResultValue;
    uint32_t uxResultValueLength;
    char cOriginal;

    // JSONの構造かバリデートする
    JSONStatus_t eJSONResult = JSON_Validate(pucShadowData, uxReceivedShadowDataLength);
    if (eJSONResult != JSONSuccess)
    {
        APP_PRINTFError("Shadow get result does not satisfy Json structure. Reasons: %d", eJSONResult);
        return false;
    }

    // ------------  施錠状態の取得 -------------

    // 施錠状態を取得するためのJsonのキーを生成
    uint8_t uxLockStateJsonPath[SHADOW_MAX_PATH_LENGTH + SHADOW_STATE_JSON_KEY_LOCK_STATE_LENGTH + 1 /* dot */ + 1 /* \0 */] = {0x00};
    snprintf(uxLockStateJsonPath, sizeof(uxLockStateJsonPath), "%s.%s", pucShadowSearchPath, SHADOW_STATE_JSON_KEY_LOCK_STATE);
    eJSONResult = JSON_Search(pucShadowData,
                              uxReceivedShadowDataLength,
                              uxLockStateJsonPath,
                              strlen(uxLockStateJsonPath),
                              &pucResultValue,
                              &uxResultValueLength);

    if (eJSONResult == JSONSuccess)
    {
        // 検索結果はNULL終端でないないので末尾に一端NULLを格納する
        cOriginal = pucResultValue[uxResultValueLength];
        pucResultValue[uxResultValueLength] = '\0';
        APP_PRINTFDebug("LockState is %s", pucResultValue);

        // ステータス文字列からLockState_tに変換
        LockState_t xLockState;
        vConvertStringToEnumLockState(pucResultValue, &xLockState);
        if (xLockState == LOCK_STATE_UNDEFINED)
        {
            APP_PRINTFError("Lockstate undefined");
            return false;
        }
        // 開閉状態を保存
        pxOutShadowState->xLockState = xLockState;

        // 末尾をもとの文字に戻す
        pucResultValue[uxResultValueLength] = cOriginal;
    }
    else if (eJSONResult == JSONNotFound)
    {
        // 見つからなかった場合
        APP_PRINTFDebug("Lockstate not found.");
        pxOutShadowState->xLockState = LOCK_STATE_UNDEFINED;
    }
    else
    {
        // その他エラー
        APP_PRINTFError("An error occurred during JSON parsing.");
        return false;
    }
    return true;
}

static bool bprvMQTTRequestWithResponse(const MQTTRequest_t *pxRequest,
                                        MQTTResponse_t *pxResponse,
                                        const uint32_t xTimeoutMs)
{
    // Responseをサブスクライブ
    DeviceShadowMQTTIncomingContext_t xIncomingContext = {0x00};

    // サブスクライブしたトピックに受信があった際に必要となるコンテキストを作成
    xIncomingContext.puxPayload = pxResponse->pucPayloadBuffer;
    xIncomingContext.uxPayloadBufferSize = pxResponse->uxPayloadBufferSize;
    xIncomingContext.uxPayloadLength = 0;
    xIncomingContext.xNotifyTaskHandle = xTaskGetCurrentTaskHandle();
    xIncomingContext.pucClientToken = pxRequest->pucClientToken;
    xIncomingContext.uxClientTokenLength = pxRequest->uxClientTokenLength;

    // サブスクライブに必要な情報を格納
    MQTTSubscribeInfo_t xSubscribeInfo = {0x00};
    xSubscribeInfo.pTopicFilter = pxResponse->pucTopicName;
    xSubscribeInfo.topicFilterLength = pxResponse->uxTopicNameLength;
    xSubscribeInfo.qos = MQTTQoS0;

    static StaticMQTTCommandBuffer_t xSubscribeMQTTContextBuffer; // コンテキスト保存場所を永続化したいためStaticで宣言
    memset(&xSubscribeMQTTContextBuffer, 0x00, sizeof(xSubscribeMQTTContextBuffer));

    // Requestをサブスクライブ
    MQTTOperationTaskResult_t eMQTTResult = eMQTTSubscribe(&xSubscribeInfo,
                                                           &vprvGetAndUpdateShadowIncomingPublishCallback,
                                                           &xIncomingContext,
                                                           &xSubscribeMQTTContextBuffer);
    if (eMQTTResult != MQTT_OPERATION_TASK_RESULT_SUCCESS)
    {
        APP_PRINTFError("Subscribe error: %d", eMQTTResult);
        return false;
    }

    APP_PRINTFDebug("Topic subscribe succeeded. Topic name: %s", pxResponse->pucTopicName);

    // Publishする情報を格納
    MQTTPublishInfo_t xMQTTPublishInfo = {
        .pTopicName = pxRequest->pucTopicName,
        .topicNameLength = pxRequest->uxTopicNameLength,
        .pPayload = pxRequest->pucPayload,
        .payloadLength = pxRequest->uxPayloadLength,
        .qos = MQTTQoS0,
    };

    // Publishを行う
    // Publishが実際に行われるまで待機する
    static StaticMQTTCommandBuffer_t xPublishMQTTContextBuffer; // コンテキスト保存場所を永続化したいためStaticで宣言
    memset(&xPublishMQTTContextBuffer, 0x00, sizeof(xPublishMQTTContextBuffer));
    eMQTTResult = eMQTTpublish(&xMQTTPublishInfo, &xPublishMQTTContextBuffer);
    if (eMQTTResult != MQTT_OPERATION_TASK_RESULT_SUCCESS)
    {
        APP_PRINTFError("Publish failed. Reasons: %d", eMQTTResult);
        return false;
    }

    APP_PRINTFDebug("MQTT publish success. Topic %s", xMQTTPublishInfo.pTopicName);

    // 登録結果を受信するまで待機
    // PublishがあるとxIncomingContext.xNotifyTaskHandleに格納したタスクハンドルに対してxTaskNotifyGive()が起こる
    if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(xTimeoutMs)) == pdFAIL)
    {
        APP_PRINTFError("Subscribe time out");
        return false;
    }

    // 受信したペイロードを出力
    APP_PRINTFDebug("Received MQTT length %d", xIncomingContext.uxPayloadLength);

    // ペイロードの終端をNULL文字にする
    pxResponse->pucPayloadBuffer[xIncomingContext.uxPayloadLength] = '\0';

    // 受信したペイロードサイズを格納
    pxResponse->uxReceivePayloadLength = xIncomingContext.uxPayloadLength;

    // SubscribeしたMQTTトピックをUnsubscribe
    static StaticMQTTCommandBuffer_t xUnsubscribeMQTTContextBuffer; // コンテキスト保存場所を永続化したいためStaticで宣言
    memset(&xUnsubscribeMQTTContextBuffer, 0x00, sizeof(xUnsubscribeMQTTContextBuffer));
    eMQTTResult = eMQTTUnsubscribe(&xSubscribeInfo, &xUnsubscribeMQTTContextBuffer);
    if (eMQTTResult != MQTT_OPERATION_TASK_RESULT_SUCCESS)
    {
        APP_PRINTFError("Unsubscribe failed. Reasons: %d", eMQTTResult);
        return false;
    }

    return true;
}

static bool bprvIsMatchClientToken(uint8_t *pucPayload,
                                   uint32_t uxPayloadLength,
                                   uint8_t *pucClientToken,
                                   uint32_t uxClientTokenLength)
{

    uint8_t *pucResultValue;
    uint32_t uxResultValueLength;

    // JSONの構造かバリデートする
    JSONStatus_t eJSONResult = JSON_Validate(pucPayload, uxPayloadLength);
    if (eJSONResult != JSONSuccess)
    {
        APP_PRINTFError("Shadow get result does not satisfy Json structure. Reasons: %d", eJSONResult);
        return false;
    }

    // ClientTokenを探すキーを生成
    uint8_t ucClientTokenSearchKey[] = CLIENT_TOKEN_PATH;

    // ClientTokenを探索
    eJSONResult = JSON_Search(pucPayload,
                              uxPayloadLength,
                              ucClientTokenSearchKey,
                              strlen(ucClientTokenSearchKey),
                              &pucResultValue,
                              &uxResultValueLength);
    // 検索出来なかったらNG
    if (eJSONResult != JSONSuccess)
    {
        return false;
    }

    // pucResultValueはNULL終端になっていないため、一端NULLを格納
    char cOriginal = pucResultValue[uxResultValueLength];
    pucResultValue[uxResultValueLength] = '\0';

    // CLientTokenが一致しているか調べる
    const int32_t xResult = strncmp(pucResultValue, pucClientToken, uxResultValueLength);

    // 退避したオリジナル文字に戻す
    pucResultValue[uxResultValueLength] = cOriginal;

    // 一致していない場合はNG
    if (xResult != 0)
    {
        return false;
    }

    APP_PRINTFDebug("Client token matched.");
    return true;
}

// --------------------------------CALLBACKS--------------------------------

static void vprvGetAndUpdateShadowIncomingPublishCallback(void *pvIncomingPublishCallbackContext,
                                                          MQTTPublishInfo_t *pxPublishInfo)
{
    APP_PRINTFDebug("vprvGetAndUpdateShadowIncomingPublishCallback called.");

    DeviceShadowMQTTIncomingContext_t *pxContext = (DeviceShadowMQTTIncomingContext_t *)pvIncomingPublishCallbackContext;

    // コンテキストのバリデート
    if (pxContext == NULL ||
        pxContext->xNotifyTaskHandle == NULL ||
        pxContext->puxPayload == NULL)
    {
        APP_PRINTFError("vprvGetAndUpdateShadowIncomingPublishCallback: Context is NULL");
        return;
    }

    APP_PRINTFDebug("vprvGetAndUpdateShadowIncomingPublishCallback payload length: %d", pxPublishInfo->payloadLength);

    if (pxContext->pucClientToken != NULL)
    {
        // ClientTokenが一致しているか調べる
        if (bprvIsMatchClientToken(pxPublishInfo->pPayload,
                                   pxPublishInfo->payloadLength,
                                   pxContext->pucClientToken,
                                   pxContext->uxClientTokenLength) == false)
        {
            APP_PRINTFDebug("vprvGetAndUpdateShadowIncomingPublishCallback: Skipping processing because the client token did not match.");
            return;
        }
    }

    // バッファサイズが足りている場合はペイロードをコピーし、足りていない場合はペイロード長を0にする
    if (pxPublishInfo->payloadLength < pxContext->uxPayloadBufferSize)
    {
        memcpy(pxContext->puxPayload, pxPublishInfo->pPayload, pxPublishInfo->payloadLength);
        pxContext->uxPayloadLength = pxPublishInfo->payloadLength;
    }
    else
    {
        APP_PRINTFError("vprvGetAndUpdateShadowIncomingPublishCallback: Insufficient buffer size to copy payload.");
        pxContext->uxPayloadLength = 0;
    }

    // 待機しているタスクに対して待機を解除
    xTaskNotifyGive(pxContext->xNotifyTaskHandle);
}

static void vprvDeltaShadowIncomingPublishCallback(void *pvIncomingPublishCallbackContext,
                                                   MQTTPublishInfo_t *pxPublishInfo)
{
    APP_PRINTFDebug("vprvDeltaShadowIncomingPublishCallback called.");

    DeviceShadowDeltaMQTTIncomingContext_t *pxContext = (DeviceShadowDeltaMQTTIncomingContext_t *)pvIncomingPublishCallbackContext;

    if (pxContext == NULL || pxContext->xCallback == NULL)
    {
        APP_PRINTFError("vprvDeltaShadowIncomingPublishCallback: Context is NULL");
        return;
    }

    APP_PRINTFDebug("vprvDeltaShadowIncomingPublishCallback payload length: %d", pxPublishInfo->payloadLength);

    // Shadowの中身を解析
    ShadowState_t xReceivedShadowState = {0x00};
    if (bprvGetDeviceShadowSateFromJSON((const uint8_t *)pxPublishInfo->pPayload,
                                        (const uint32_t)pxPublishInfo->payloadLength,
                                        &xReceivedShadowState,
                                        SHADOW_STATE_PATH) == false)
    {
        APP_PRINTFError("vprvDeltaShadowIncomingPublishCallback: Json purse error.");
        return;
    }

    // ShadowTypeの生成
    uint32_t xShadowType = 0;
    if (xReceivedShadowState.xLockState != LOCK_STATE_UNDEFINED)
    {
        xShadowType |= SHADOW_UPDATE_TYPE_LOCK_STATE;
    }

    APP_PRINTFDebug("vprvDeltaShadowIncomingPublishCallback pointer is %p", pxContext->xCallback);

    // コールバックをコール
    pxContext->xCallback(xShadowType, &xReceivedShadowState);
}

// --------------------------------------------------
// Unit Test用関数定義(関数のプロトタイプ宣言は「自身のファイル名+ "_test.h"」で宣言されていること)
// --------------------------------------------------
#if (BUILD_MODE_TEST == 1) /* BUILD_MODE_TESTが定義されているとき */
#endif                     /* end  BUILD_MODE_TEST */
