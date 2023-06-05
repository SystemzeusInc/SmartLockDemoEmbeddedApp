/**
 * @file device_register.c
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

#include "core_json.h"

// --------------------------------------------------
// ユーザ作成ヘッダの取り込み
// --------------------------------------------------
#include "common/include/application_define.h"
#include "config/task_config.h"

#include "tasks/flash/include/flash_data.h"
#include "tasks/flash/include/flash_task.h"
#include "tasks/mqtt/include/mqtt_operation_task.h"

#include "tasks/provisioning/private/include/device_register.h"

// --------------------------------------------------
// 自ファイル内でのみ使用する#defineマクロ
// --------------------------------------------------

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
} DeviceRegisterMQTTIncomingContext_t;

// --------------------------------------------------
// ファイル内で共有するstatic変数宣言
// --------------------------------------------------

// --------------------------------------------------
// static関数プロトタイプ宣言
// --------------------------------------------------

/**
 * @brief  SubscribeしたトピックにPublishが行われた時にCallbackされる関数
 *
 * @param[in] pvIncomingPublishCallbackContext eMQTTSubscribeの第3引数で指定されたコンテキスト。本APIは #DeviceRegisterMQTTIncomingContext_t にキャストする
 * @param[in] pxPublishInfo                    callbackが行われた時のMQTT情報。トピック名やペイロード等が格納されている。
 */
static void vprvDeviceRegisterIncomingPublishCallback(void *pvIncomingPublishCallbackContext,
                                                      MQTTPublishInfo_t *pxPublishInfo);

/**
 * @brief MQTTに接続してデバイス登録を行い、結果を受信してMQTT接続を閉じる
 *
 *
 * @param[in]     pxFactoryName              工場出荷ThingName
 * @param[in]     pxLOTT                     リンキングワンタイムトークン
 * @param[in,out] pucMQTTSubPayloadBuffer    受信したペイロードを格納するバッファ
 * @param[in]     uxMQTTSubPayloadBufferSize ペイロード受信バッファのサイズ
 * @param[out]    puxReceivedPayloadLength   受信したペイロードの長さ
 *
 * @retval true  成功
 * @retval false 失敗
 */
static bool bprvDeviceRegisterMQTTProcess(const FactoryThingName_t *pxFactoryName,
                                          const LinkingOneTimeToken_t *pxLOTT,
                                          uint8_t *pucMQTTSubPayloadBuffer,
                                          const uint32_t uxMQTTSubPayloadBufferSize,
                                          uint32_t *puxReceivedPayloadLength);

/**
 * @brief デバイ登録結果のJSONを解析してThingNameを取り出す
 *
 * @param[in]  pucDeviceRegisterResponse  デバイス登録結果
 * @param[in]  uxResponseBufferSize       デバイス登録結果が格納されているバッファのサイズ
 * @param[out] pxThingName                解析できたThingNameを格納するバッファ
 *
 * @warning
 * pucDeviceRegisterResponseはJSONの解析の中に変更される可能性がある。
 *
 * @retval #DEVICE_REGISTER_ALREADY_SUCCESS    成功
 * @retval #DEVICE_REGISTER_ALREADY_REGISTERED 既にデバイス登録されている
 * @retval #DEVICE_REGISTER_ALREADY_FAILED     失敗
 */
static DeviceRegisterResult_t eAnalysisDeviceRegisterResponseJSON(uint8_t *pucDeviceRegisterResponse,
                                                                  const uint32_t uxResponseBufferSize,
                                                                  ThingName_t *pxThingName);

// --------------------------------------------------
// 変数定義（staticを除く）
// --------------------------------------------------

// --------------------------------------------------
// 関数定義（staticを除く）
// --------------------------------------------------

DeviceRegisterResult_t eRunDeviceRegisterProcess(const LinkingOneTimeToken_t *pxLOTT)
{

    // 工場出荷ThingNameを取得
    FactoryThingName_t xFactoryName = {0x00};
    if (eReadFlashInfo(READ_FLASH_TYPE_FACTORY_THING_NAME,
                       &xFactoryName,
                       sizeof(xFactoryName)) != FLASH_TASK_RESULT_SUCCESS)
    {
        APP_PRINTFError("Read factory thing name error.");
        return DEVICE_REGISTER_RESULT_FAILED;
    }

    // デバイス登録結果を得るためのバッファ
    uint8_t xIncomingPayloadBuffer[DEVICE_REGISTER_RESPONSE_PAYLOAD_SIZE];
    uint32_t uxPayloadLength = 0;

    // MQTTに接続してデバイス登録を行い、結果を受信してMQTT接続を閉じる
    if (bprvDeviceRegisterMQTTProcess(
            &xFactoryName,
            pxLOTT,
            &xIncomingPayloadBuffer,
            sizeof(xIncomingPayloadBuffer),
            &uxPayloadLength) == false)
    {
        APP_PRINTFError("Device register mqtt process failed.");
        return DEVICE_REGISTER_RESULT_FAILED;
    }

    // 待機が解除されると、xIncomingPayloadBuffer.puxPayloadで渡したバッファ「xIncomingPayloadBuffer」にペイロードが格納されているため表示
    xIncomingPayloadBuffer[uxPayloadLength] = '\0';
    APP_PRINTFDebug(" Subscribe success Payload: %s", xIncomingPayloadBuffer);

    // Responseを解析
    DeviceRegisterResult_t xJSONParseResult;
    ThingName_t xThingName;
    xJSONParseResult = eAnalysisDeviceRegisterResponseJSON(xIncomingPayloadBuffer, uxPayloadLength, &xThingName);
    if (xJSONParseResult != DEVICE_REGISTER_RESULT_SUCCESS)
    {
        return xJSONParseResult;
    }
    APP_PRINTFDebug("Receive thing name is %s", xThingName.ucName);

    // ThingNameをFlashに保存
    if (eWriteFlashInfo(WRITE_FLASH_TYPE_USUAL_THING_NAME, &xThingName) != FLASH_TASK_RESULT_SUCCESS)
    {
        APP_PRINTFError("Write thing name to flash error.");
        return DEVICE_REGISTER_RESULT_FAILED;
    }

    // プロビジョニングフラグを保存
    ProvisioningFlag_t xProvisioningFlag = true;
    if (eWriteFlashInfo(WRITE_FLASH_TYPE_PROVISIONING_FLAG,
                        &xProvisioningFlag) != FLASH_TASK_RESULT_SUCCESS)
    {
        APP_PRINTFError("Write provisioning flag to flash error.");
        return DEVICE_REGISTER_RESULT_FAILED;
    }

    APP_PRINTFDebug("Device register process success.");

    return DEVICE_REGISTER_RESULT_SUCCESS;
}

// --------------------------------------------------
// static関数定義
// --------------------------------------------------

static bool bprvDeviceRegisterMQTTProcess(const FactoryThingName_t *pxFactoryName,
                                          const LinkingOneTimeToken_t *pxLOTT,
                                          uint8_t *pucMQTTSubPayloadBuffer,
                                          const uint32_t uxMQTTSubPayloadBufferSize,
                                          uint32_t *puxReceivedPayloadLength)
{
    // MQTT 接続
    if (eMQTTConnectToAWSIoT(MQTT_THING_NAME_TYPE_PROVISIONING,
                             MQTT_CONNECT_RETRY_TIME,
                             NULL) != MQTT_OPERATION_TASK_RESULT_SUCCESS)
    {
        APP_PRINTFError("MQTT connect to aws iot failed.");
        return false;
    }

    APP_PRINTFDebug("MQTT connect to aws iot.");

    // デバイス登録結果をサブスクライブ
    DeviceRegisterMQTTIncomingContext_t xIncomingContext = {0x00};
    uint8_t ucMQTTSubTopicBuffer[DEVICE_REGISTER_RESPONSE_TOPIC_LENGTH] = {0x00};
    memset(pucMQTTSubPayloadBuffer, 0x00, uxMQTTSubPayloadBufferSize);

    // Subscribeしたトピックに対してPublishが行われた時にCallbackされる関数に渡すコンテキストを準備
    xIncomingContext.puxPayload = &pucMQTTSubPayloadBuffer[0];
    xIncomingContext.uxPayloadBufferSize = uxMQTTSubPayloadBufferSize;
    xIncomingContext.uxPayloadLength = 0;
    xIncomingContext.xNotifyTaskHandle = xTaskGetCurrentTaskHandle();

    // トピック名を作成
    snprintf(ucMQTTSubTopicBuffer, sizeof(ucMQTTSubTopicBuffer), DEVICE_REGISTER_RESPONSE_TOPIC_TEMPLATE, pxFactoryName->ucName);

    // サブスクライブに必要な情報を格納
    MQTTSubscribeInfo_t xSubscribeInfo = {0x00};
    xSubscribeInfo.pTopicFilter = ucMQTTSubTopicBuffer;
    xSubscribeInfo.topicFilterLength = strlen(ucMQTTSubTopicBuffer);
    xSubscribeInfo.qos = MQTTQoS0;

    // MQTT Subscribe
    static StaticMQTTCommandBuffer_t xSubscribeMQTTContextBuffer; // コンテキスト保存場所を永続化したいためStaticで宣言
    memset(&xSubscribeMQTTContextBuffer, 0x00, sizeof(xSubscribeMQTTContextBuffer));
    MQTTOperationTaskResult_t eResult = eMQTTSubscribe(&xSubscribeInfo,
                                                       &vprvDeviceRegisterIncomingPublishCallback,
                                                       &xIncomingContext,
                                                       &xSubscribeMQTTContextBuffer);

    if (eResult != MQTT_OPERATION_TASK_RESULT_SUCCESS)
    {
        APP_PRINTFError("Subscribe error: %d", eResult);
        return false;
    }

    APP_PRINTFDebug("Topic subscribe succeeded. Topic name: %s", ucMQTTSubTopicBuffer);

    // MQTT Publish

    // デバイス登録トピックを作成
    uint8_t ucMQTTPubTopic[DEVICE_REGISTER_TOPIC_LENGTH] = {0x00};
    snprintf(ucMQTTPubTopic, sizeof(ucMQTTPubTopic), DEVICE_REGISTER_TOPIC_TEMPLATE, pxFactoryName->ucName);

    // Payloadを作成
    uint8_t ucMQTTPublishPayload[DEVICE_REGISTER_PAYLOAD_SIZE] = {0x00};
    snprintf(ucMQTTPublishPayload, sizeof(ucMQTTPublishPayload), DEVICE_REGISTER_PAYLOAD_TEMPLATE, pxLOTT->uxLOTT);

    MQTTPublishInfo_t xMQTTPublishInfo = {
        .pTopicName = ucMQTTPubTopic,
        .topicNameLength = strlen(ucMQTTPubTopic),
        .pPayload = &ucMQTTPublishPayload[0],
        .payloadLength = strlen(&ucMQTTPublishPayload[0]),
        .qos = MQTTQoS0,
    };

    // Publishのためのコンテキストを準備
    static StaticMQTTCommandBuffer_t xPublishMQTTContextBuffer; // コンテキスト保存場所を永続化したいためStaticで宣言
    memset(&xPublishMQTTContextBuffer, 0x00, sizeof(xPublishMQTTContextBuffer));

    // Publishを行う
    // Publishが実際に行われるまで待機する
    if (eMQTTpublish(&xMQTTPublishInfo, &xPublishMQTTContextBuffer) != MQTT_OPERATION_TASK_RESULT_SUCCESS)
    {
        APP_PRINTFError("Publish failed.");
        return false;
    }

    APP_PRINTFDebug("MQTT publish success. Topic %s, Payload %s", ucMQTTPubTopic, ucMQTTPublishPayload);

    // 登録結果を受信するまで待機
    // PublishがあるとxIncomingContext.xNotifyTaskHandleに格納したタスクハンドルに対してxTaskNotifyGive()が起こる
    if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(DEVICE_REGISTER_RESPONSE_WITE_TIME_MS)) == pdFAIL)
    {
        APP_PRINTFError("Subscribe time out");
        return false;
    }

    // MQTT切断
    if (eMQTTDisconnectAndTaskShutdown() != MQTT_OPERATION_TASK_RESULT_SUCCESS)
    {
        APP_PRINTFError("MQTT disconnect failed.");
        return false;
    }

    // 最後にペイロードの長さを格納
    *puxReceivedPayloadLength = xIncomingContext.uxPayloadLength;

    return true;
}

static DeviceRegisterResult_t eAnalysisDeviceRegisterResponseJSON(uint8_t *pucDeviceRegisterResponse,
                                                                  const uint32_t uxResponseBufferSize,
                                                                  ThingName_t *pxThingName)
{

    // JSONの構造かバリデートする
    JSONStatus_t eJSONResult = JSON_Validate(pucDeviceRegisterResponse, uxResponseBufferSize);
    if (eJSONResult != JSONSuccess)
    {
        APP_PRINTFError("Device registration result does not satisfy Json structure. Reasons: %d", eJSONResult);
        return DEVICE_REGISTER_RESULT_FAILED;
    }

    // デバイス登録の結果を取得
    uint8_t *pucResultValue;
    uint32_t uxResultValueLength;
    if (JSON_Search(pucDeviceRegisterResponse,
                    uxResponseBufferSize,
                    DEVICE_REGISTER_RESPONSE_RESULT_JSON_KEY_STRING,
                    strlen(DEVICE_REGISTER_RESPONSE_RESULT_JSON_KEY_STRING),
                    &pucResultValue,
                    &uxResultValueLength) != JSONSuccess)
    {
        APP_PRINTFError("An error occurred during JSON parsing.");
        return DEVICE_REGISTER_RESULT_FAILED;
    }

    // JSONの検索結果はNULL終端でないため、一端、末尾にNULL文字を入れる
    uint8_t uxOriginalChar = pucResultValue[uxResultValueLength];
    pucResultValue[uxResultValueLength] = '\0';
    APP_PRINTFDebug("Device register result: %s", pucResultValue);

    // 既に登録されているかチェック
    if (strncmp(pucResultValue,
                DEVICE_REGISTER_RESPONSE_RESULT_JSON_VALUE_ALREADY_REGISTERED,
                sizeof(DEVICE_REGISTER_RESPONSE_RESULT_JSON_VALUE_ALREADY_REGISTERED)) == 0)
    {
        APP_PRINTFWarn("Device already registered.");
        return DEVICE_REGISTER_ALREADY_REGISTERED;
    }

    // 成功かチェック
    if (strncmp(pucResultValue,
                DEVICE_REGISTER_RESPONSE_RESULT_JSON_VALUE_SUCCESS,
                sizeof(DEVICE_REGISTER_RESPONSE_RESULT_JSON_VALUE_SUCCESS)) != 0)
    {
        APP_PRINTFError("Result key has an undefined value.");
        return DEVICE_REGISTER_RESULT_FAILED;
    }

    // 終端文字を格納した場所にオリジナルの文字を格納しなおす
    pucResultValue[uxResultValueLength] = uxOriginalChar;

    // ThingNameを検索
    if (JSON_Search(pucDeviceRegisterResponse,
                    uxResponseBufferSize,
                    DEVICE_REGISTER_RESPONSE_THING_NAME_JSON_KEY_STRING,
                    strlen(DEVICE_REGISTER_RESPONSE_THING_NAME_JSON_KEY_STRING),
                    &pucResultValue,
                    &uxResultValueLength) != JSONSuccess)
    {
        APP_PRINTFError("An error occurred during JSON parsing.");
        return DEVICE_REGISTER_RESULT_FAILED;
    }

    // 受信したThingNameがバッファサイズ以上になっていないかチェックする
    if (uxResultValueLength > THING_NAME_LENGTH)
    {
        APP_PRINTFError("Received ThingName cannot be stored with more than %d characters.", THING_NAME_LENGTH);
        return DEVICE_REGISTER_RESULT_FAILED;
    }

    // ThingNameをコピーする
    memset(pxThingName, 0x00, sizeof(ThingName_t));
    memcpy(pxThingName->ucName, pucResultValue, uxResultValueLength);

    return DEVICE_REGISTER_RESULT_SUCCESS;
}

// ------------------------------- CALLBACKs -------------------------------

static void vprvDeviceRegisterIncomingPublishCallback(void *pvIncomingPublishCallbackContext,
                                                      MQTTPublishInfo_t *pxPublishInfo)
{

    APP_PRINTFDebug("DeviceRegisterIncomingPublishCallback called.");

    DeviceRegisterMQTTIncomingContext_t *pxContext = (DeviceRegisterMQTTIncomingContext_t *)pvIncomingPublishCallbackContext;

    // コンテキストのバリデート
    if (pxContext == NULL || pxContext->xNotifyTaskHandle == NULL || pxContext->puxPayload == NULL)
    {
        APP_PRINTFError("DeviceRegisterIncomingPublishCallback: Context is NULL");
        return;
    }

    // バッファサイズが足りている場合はペイロードをコピーし、足りていない場合はペイロード長を0にする
    if (pxPublishInfo->payloadLength < pxContext->uxPayloadBufferSize)
    {
        memcpy(pxContext->puxPayload, pxPublishInfo->pPayload, pxPublishInfo->payloadLength);
        pxContext->uxPayloadLength = pxPublishInfo->payloadLength;
    }
    else
    {
        APP_PRINTFError("Insufficient buffer size to copy payload.");
        pxContext->uxPayloadLength = 0;
    }

    // 待機しているタスクに対して待機を解除
    xTaskNotifyGive(pxContext->xNotifyTaskHandle);
}

// --------------------------------------------------
// Unit Test用関数定義(関数のプロトタイプ宣言は「自身のファイル名+ "_test.h"」で宣言されていること)
// --------------------------------------------------
#if (BUILD_MODE_TEST == 1) /* BUILD_MODE_TESTが定義されているとき */
#endif                     /* end  BUILD_MODE_TEST */
