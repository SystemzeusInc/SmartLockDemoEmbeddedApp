/**
 * @file flash_task.c
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
#include "config/flash_config.h"
#include "config/task_config.h"
#include "config/queue_config.h"

#include "common/include/application_define.h"

#include "tasks/flash/private/include/se_operation.h"
#include "tasks/flash/include/flash_task.h"

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
 * @brief Writeの時に用いるパラメータ
 */
typedef struct
{
    /**
     * @brief WriteType
     */
    WriteFlashType_t xWriteType;

    /**
     * @brief 書き込みたいデータ
     */
    void *pvData;
} FlashWriteParameters_t;

typedef struct
{
    /**
     * @brief ReadType
     */
    ReadFlashType_t xReadType;

    /**
     * @brief 書き込み用バッファ
     */
    void *pvBuffer;

    /**
     * @brief バッファサイズ
     */
    uint32_t uxBufferSize;
} FlashReadParameters_t;

/**
 * @brief ThingNameをキャッシュするための構造体
 */
typedef struct
{
    /**
     * @brief キャッシュを持っているか
     */
    bool bHasCache;

    /**
     * @brief キャッシュするThingName
     */
    ThingName_t xThingName;

} ThingNameCache_t;

/**
 * @brief Flash Taskのキューに渡すためのパラメータ
 */
typedef struct
{
    /**
     * @brief Flashに対する書き込みか。falseの場合は読み込み。
     */
    bool bIsWrite;

    /**
     * @brief 書き込み/読み込みの完了を待機しているタスクのハンドル
     */
    TaskHandle_t xTaskNotifyHandle;

    /**
     * @brief bIsWriteがtrueの場合はFlashWriteParameters_tが入り、falseの場合はFlashReadParameters_tが入る
     *
     */
    union
    {
        /**
         * @brief FlashのWriteに必要なパラメータ
         */
        FlashWriteParameters_t *pxWriteParam;

        /**
         * @brief FlashのReadに必要なパラメータ
         */
        FlashReadParameters_t *pxReadParam;
    } u;

    /**
     * @brief Flashに対する読み書き結果
     */
    FlashTaskResult_t xResult;

} FlashTaskAddQueueParameters_t;

// --------------------------------------------------
// ファイル内で共有するstatic変数宣言
// --------------------------------------------------

/**
 * @brief FlashTaskのタスクハンドル
 */
static TaskHandle_t gxTaskHandle = NULL;

/**
 * @brief FlashTaskとやり取りするためのキューハンドル
 */
static QueueHandle_t gxQueueHandle = NULL;

/**
 * @brief ThingNameのキャッシュに使用するバッファ
 */
static ThingNameCache_t gxThingNameCache = {0x00};

// --------------------------------------------------
// static関数プロトタイプ宣言
// --------------------------------------------------

/**
 * @brief FlashTaskのエントリポイント
 *
 * @param[in] pvParam キューのハンドル
 */
static void vprvFlashTask(void *pvParam);

/**
 * @brief FlashにWriteする
 *
 * @param[in] pxFlashWriteParams Writeに必要なパラメータ
 *
 * @retval true  成功
 * @retval false 失敗
 */
static bool bprvFlashWriteProcess(const FlashWriteParameters_t *pxFlashWriteParams);

/**
 * @brief FlashからReadする
 *
 * @param[in] pxFlashReadParams Readに必要なパラメータ
 *
 * @retval true  成功
 * @retval false 失敗
 */
static bool bprvFlashReadProcess(const FlashReadParameters_t *pxFlashReadParams);

/**
 * @brief 受け取ったQueueの中身をバリデートする
 *
 * @param[in] pxParam 受け取ったQueueのデータ
 *
 * @retval true  バリデート成功
 * @retval false バリデート失敗
 */
static bool bprvValidateQueueParam(const FlashTaskAddQueueParameters_t *pxParam);

/**
 * @brief キャッシュに普段使用用のThingNameがある場合は取得する
 *
 * @param[out] pxThingName 格納するバッファ
 *
 * @retval true  キャッシュからの読み取り成功
 * @retval false キャッシュが空であり読み取り失敗
 */
static bool bprvGetUsualThingNameCache(ThingName_t *pxThingName);

/**
 * @brief 普段使い用のThingNameのキャッシュをクリアする
 *
 */
static void vprvClearUsualThingNameCache(void);

/**
 * @brief キャッシュに普段使い用のキャッシュをセットする
 *
 * @param[in] pxThingName セキュアエレメントから取得したThingName
 */
static void vprvSetUsualThingNameCache(const ThingName_t *pxThingName);

// --------------------------------------------------
// 変数定義（staticを除く）
// --------------------------------------------------

// --------------------------------------------------
// 関数定義（staticを除く）
// --------------------------------------------------

FlashTaskResult_t eFlashTaskInit(void)
{

    APP_PRINTFDebug("Flash Task Init started.");

    // Queueの作成
    if (gxQueueHandle == NULL)
    {
        gxQueueHandle = xQueueCreate(FLASH_TASK_COMMAND_QUEUE_LENGTH, sizeof(FlashTaskAddQueueParameters_t *));

        if (gxQueueHandle == NULL)
        {
            APP_PRINTFError("Flash task create queue failed.");
            return FLASH_TASK_RESULT_FAILED;
        }
    }

    // タスクの作成
    if (gxTaskHandle == NULL)
    {
        BaseType_t xResult = xTaskCreate(
            &vprvFlashTask,
            "Flash Task",
            FLASH_TASK_SIZE,
            (void *)gxQueueHandle,
            FLASH_TASK_PRIORITY,
            &gxTaskHandle);

        if (xResult != pdTRUE)
        {
            APP_PRINTFError("Flash task create failed.");
            return FLASH_TASK_RESULT_FAILED;
        }
    }

    // セキュアエレメントの初期化
    if (eSEOperationInit() != SE_OPERATION_RESULT_SUCCESS)
    {
        APP_PRINTFError("SE operation init failed");
        return FLASH_TASK_RESULT_FAILED;
    }

    // 普段使い用のThingNameのキャッシュをクリア
    vprvClearUsualThingNameCache();

    return FLASH_TASK_RESULT_SUCCESS;
}

FlashTaskResult_t eReadFlashInfo(const ReadFlashType_t eReadFlashType,
                                 void *pvBuffer,
                                 const uint32_t uxBufferSize)
{
    if (pvBuffer == NULL)
    {
        APP_PRINTFError("Buffer is null");
        return FLASH_TASK_RESULT_BAD_RESULT;
    }

    // ---- Queueに送るパラメータを決定
    FlashTaskAddQueueParameters_t xQParams = {0x00};
    FlashTaskAddQueueParameters_t *pxQParams = NULL;
    FlashReadParameters_t xReadParams = {0x00};

    xReadParams.pvBuffer = pvBuffer;
    xReadParams.xReadType = eReadFlashType;
    xReadParams.uxBufferSize = uxBufferSize;

    xQParams.bIsWrite = false;
    xQParams.u.pxReadParam = &xReadParams;
    xQParams.xResult = FLASH_TASK_RESULT_FAILED;
    xQParams.xTaskNotifyHandle = xTaskGetCurrentTaskHandle();
    pxQParams = &xQParams;

    // QueueにSend
    if (xQueueSend(gxQueueHandle, &pxQParams, pdMS_TO_TICKS(FLASH_RESPONSE_TIMEOUT_MS)) != pdTRUE)
    {
        APP_PRINTFError("Flash read command failed. Timeout.");
        return FLASH_TASK_RESULT_TIMEOUT;
    }

    // Queueに送信したコマンドが処理されるまで待機
    if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(FLASH_RESPONSE_TIMEOUT_MS)) != pdTRUE)
    {
        APP_PRINTFError("Flash read command waiting timeout.");
        return FLASH_TASK_RESULT_TIMEOUT;
    }

    if (xQParams.xResult == FLASH_TASK_RESULT_SUCCESS)
    {
        APP_PRINTFDebug("Flash read success.");
    }

    return xQParams.xResult;
}

FlashTaskResult_t eWriteFlashInfo(const WriteFlashType_t eWriteFlashType,
                                  const void *pvWriteData)
{
    if (pvWriteData == NULL)
    {
        APP_PRINTFError("Buffer is null");
        return FLASH_TASK_RESULT_BAD_RESULT;
    }

    // ---- Queueに送るパラメータを決定
    FlashWriteParameters_t xWriteParm = {0x00};
    FlashTaskAddQueueParameters_t xQParams = {0x00};
    FlashTaskAddQueueParameters_t *pxQParams;

    xWriteParm.pvData = (void *)pvWriteData;
    xWriteParm.xWriteType = eWriteFlashType;

    xQParams.bIsWrite = true;
    xQParams.u.pxWriteParam = &xWriteParm;
    xQParams.xResult = FLASH_TASK_RESULT_FAILED;
    xQParams.xTaskNotifyHandle = xTaskGetCurrentTaskHandle();
    pxQParams = &xQParams;

    // QueueにSend
    if (xQueueSend(gxQueueHandle, &pxQParams, pdMS_TO_TICKS(FLASH_RESPONSE_TIMEOUT_MS)) != pdTRUE)
    {
        APP_PRINTFError("Flash write command failed. Timeout.");
        return FLASH_TASK_RESULT_TIMEOUT;
    }

    // Queueに送信したコマンドが処理されるまで待機
    if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(FLASH_RESPONSE_TIMEOUT_MS)) != pdTRUE)
    {
        APP_PRINTFError("Flash write command waiting timeout.");
        return FLASH_TASK_RESULT_TIMEOUT;
    }

    if (xQParams.xResult == FLASH_TASK_RESULT_SUCCESS)
    {
        APP_PRINTFDebug("Flash write success.");
    }

    return xQParams.xResult;
}

// --------------------------------------------------
// static関数定義
// --------------------------------------------------

static void vprvFlashTask(void *pvParam)
{
    if (pvParam == NULL)
    {
        APP_PRINTFFatal("QueueHandle is null");
        vTaskDelete(NULL);
    }

    QueueHandle_t xQHandle = (QueueHandle_t)pvParam; // Queueのハンドル
    FlashTaskAddQueueParameters_t *pxQParams;        // Queueから受け取るパラメータのバッファ
    bool bIsFlashProcessingSuccess = false;          // Flashに対して読み書きした結果を格納するバッファ

    while (true)
    {
        if (xQueueReceive(xQHandle, &pxQParams, portMAX_DELAY))
        {
            // Queueの中身をバリデート
            if (bprvValidateQueueParam(pxQParams) == false)
            {
                continue;
            }

            // WriteかReadを判定する
            if (pxQParams->bIsWrite == true)
            {
                // Flashに対する書き込みをお行う
                bIsFlashProcessingSuccess = bprvFlashWriteProcess(pxQParams->u.pxWriteParam);
            }
            else
            {
                // Flashに対して読み込みを行う
                bIsFlashProcessingSuccess = bprvFlashReadProcess(pxQParams->u.pxReadParam);
            }

            // 結果を格納して待機しているタスクに通知を発信する
            pxQParams->xResult = bIsFlashProcessingSuccess == true
                                     ? FLASH_TASK_RESULT_SUCCESS
                                     : FLASH_TASK_RESULT_FAILED;
            xTaskNotifyGive(pxQParams->xTaskNotifyHandle);
        }

        PRINT_TASK_REMAINING_STACK_SIZE();
    }
}

static bool bprvValidateQueueParam(const FlashTaskAddQueueParameters_t *pxParam)
{
    if (pxParam == NULL)
    {
        APP_PRINTFError("Queue data is null.");
        return false;
    }

    if (pxParam->xTaskNotifyHandle == NULL)
    {
        APP_PRINTFError("Task notify handle is null.");
        return false;
    }

    if (pxParam->bIsWrite == true)
    {
        if (pxParam->u.pxWriteParam == NULL || pxParam->u.pxWriteParam->pvData == NULL)
        {

            APP_PRINTFError("Write param is invalid.");
            return false;
        }
    }
    else
    {
        if (pxParam->u.pxReadParam == NULL || pxParam->u.pxReadParam->pvBuffer == NULL || pxParam->u.pxReadParam->uxBufferSize == 0)
        {
            APP_PRINTFError("Read param is invalid.");
            return false;
        }
    }

    return true;
}

static bool bprvFlashWriteProcess(const FlashWriteParameters_t *pxFlashWriteParams)
{
    // Write Typeによって何を書き込むのか決定する
    switch (pxFlashWriteParams->xWriteType)
    {
    case WRITE_FLASH_TYPE_WIFI_INFO:
        APP_PRINTFDebug("Set WRITE_FLASH_TYPE_WIFI_INFO");

        // セキュアエレメントへWi-Fi情報を書き込み
        WiFiInfo_t *pxWiFiInfo = (WiFiInfo_t *)pxFlashWriteParams->pvData;
        if (eSetWiFiInfoToSE(pxWiFiInfo) != SE_OPERATION_RESULT_SUCCESS)
        {
            return false;
        }

        return true;
    case WRITE_FLASH_TYPE_PROVISIONING_FLAG:
        APP_PRINTFDebug("Set WRITE_FLASH_TYPE_PROVISIONING_FLAG");

        // セキュアエレメントへプロビジョニングフラグ情報を書き込み
        ProvisioningFlag_t *pxProvisioningFlag = (ProvisioningFlag_t *)pxFlashWriteParams->pvData;
        if (eSetProvisioningFlag(*pxProvisioningFlag) != SE_OPERATION_RESULT_SUCCESS)
        {
            return false;
        }

        return true;
    case WRITE_FLASH_TYPE_AWS_IOT_ENDPOINT:
        APP_PRINTFDebug("Set WRITE_FLASH_TYPE_AWS_IOT_ENDPOINT");

        // セキュアエレメントへIoTEndpointの書き込み
        AWSIoTEndpoint_t *pxIoTEndpoint = (AWSIoTEndpoint_t *)pxFlashWriteParams->pvData;
        if (eSetIoTEndpoint(pxIoTEndpoint) != SE_OPERATION_RESULT_SUCCESS)
        {
            return false;
        }
        return true;
    case WRITE_FLASH_TYPE_USUAL_THING_NAME:
        APP_PRINTFDebug("Set WRITE_FLASH_TYPE_USUAL_THING_NAME");

        // セキュアエレメントへ普段使い用のThingNameを書き込み
        ThingName_t *pxThingName = (ThingName_t *)pxFlashWriteParams->pvData;
        if (eSetThingName(pxThingName) != SE_OPERATION_RESULT_SUCCESS)
        {
            return false;
        }

        // 書き込みが成功したらキャッシュをクリア
        APP_PRINTFDebug("Clear the cache because the ThingName was successfully written.");
        vprvClearUsualThingNameCache();

        return true;
    default:
        APP_PRINTFError("Unkown Wite type.");
        return false;
    }

    return true;
}
static bool bprvFlashReadProcess(const FlashReadParameters_t *pxFlashReadParams)
{
    // Read Typeによって読み込む情報を決定する
    switch (pxFlashReadParams->xReadType)
    {
    case READ_FLASH_TYPE_WIFI_INFO:
        APP_PRINTFDebug("Get WRITE_FLASH_TYPE_WIFI_INFO");

        // データを格納するためのバッファサイズが適切か調べる
        if (pxFlashReadParams->uxBufferSize != sizeof(WiFiInfo_t))
        {
            APP_PRINTFError("BufferSize size is not match.");
            return false;
        }

        // セキュアエレメントからWi-Fi情報を読み込み
        WiFiInfo_t *pxWiFiInfo = (WiFiInfo_t *)pxFlashReadParams->pvBuffer;
        if (eGetWiFiInfoFromSE(pxWiFiInfo) != SE_OPERATION_RESULT_SUCCESS)
        {
            return false;
        }
        return true;
    case READ_FLASH_TYPE_AWS_IOT_ENDPOINT:
        APP_PRINTFDebug("Get WRITE_FLASH_TYPE_WIFI_INFO");

        // データを格納するためのバッファサイズが適切か調べる
        if (pxFlashReadParams->uxBufferSize != sizeof(AWSIoTEndpoint_t))
        {
            APP_PRINTFError("BufferSize size is not match.");
            return false;
        }
        // セキュアエレメントからAWSIoTのEndpoint情報を読み込み
        AWSIoTEndpoint_t *pxIoTEndpoint = (AWSIoTEndpoint_t *)pxFlashReadParams->pvBuffer;
        if (eGetIoTEndpoint(pxIoTEndpoint) != SE_OPERATION_RESULT_SUCCESS)
        {
            return false;
        }
        return true;
    case READ_FLASH_TYPE_PROVISIONING_FLAG:
        APP_PRINTFDebug("Get READ_FLASH_TYPE_PROVISIONING_FLAG");

        // データを格納するためのバッファサイズが適切か調べる
        if (pxFlashReadParams->uxBufferSize != sizeof(ProvisioningFlag_t))
        {
            APP_PRINTFError("BufferSize size is not match.");
            return false;
        }

        // セキュアエレメントからプロビジョニングフラグを読み込み
        ProvisioningFlag_t *pxProvisioningFlag = (ProvisioningFlag_t *)pxFlashReadParams->pvBuffer;
        if (eGetProvisioningFlag(pxProvisioningFlag) != SE_OPERATION_RESULT_SUCCESS)
        {
            return false;
        }
        return true;
    case READ_FLASH_TYPE_FACTORY_THING_NAME:
        APP_PRINTFDebug("Get READ_FLASH_TYPE_FACTORY_THING_NAME");

        // データを格納するためのバッファサイズが適切か調べる
        if (pxFlashReadParams->uxBufferSize != sizeof(FactoryThingName_t))
        {
            APP_PRINTFError("BufferSize size is not match.");
            return false;
        }

        // セキュアエレメントから工場出荷ThingNameを読み込み
        FactoryThingName_t *pxFactoryThingName = (FactoryThingName_t *)pxFlashReadParams->pvBuffer;
        if (eGetFactoryThingName(pxFactoryThingName) != SE_OPERATION_RESULT_SUCCESS)
        {
            return false;
        }
        return true;
    case READ_FLASH_TYPE_USUAL_THING_NAME:
        APP_PRINTFDebug("Get READ_FLASH_TYPE_USUAL_THING_NAME");

        // データを格納するためのバッファサイズが適切か調べる
        if (pxFlashReadParams->uxBufferSize != sizeof(ThingName_t))
        {
            APP_PRINTFError("BufferSize size is not match.");
            return false;
        }

        ThingName_t *pxThingName = (ThingName_t *)pxFlashReadParams->pvBuffer;

        // メモリクリア
        memset(pxThingName, 0x00, sizeof(ThingName_t));

        // キャッシュを持っている場合は、キャッシュから取得
        if (bprvGetUsualThingNameCache(pxThingName) == true)
        {
            APP_PRINTFDebug("Loaded ThingName from cache.");
            return true;
        }

        // セキュアエレメントから普段使い用のThingNameを読み込み
        if (eGetThingName(pxThingName) != SE_OPERATION_RESULT_SUCCESS)
        {
            return false;
        }

        // キャッシュをセット
        APP_PRINTFDebug("ThingName is successfully read and set in cache.");
        vprvSetUsualThingNameCache(pxThingName);

        return true;
    default:
        APP_PRINTFError("Unkown read type.");
        return false;
    }

    return true;
}

static bool bprvGetUsualThingNameCache(ThingName_t *pxThingName)
{
    // キャッシュを持っていなかったら何もしない
    if (gxThingNameCache.bHasCache == false)
    {
        return false;
    }

    // キャッシュを持っている場合はコピーを行う
    memcpy(pxThingName, &gxThingNameCache.xThingName, sizeof(ThingName_t));

    return true;
}

static void vprvClearUsualThingNameCache(void)
{
    // キャッシュのフラグを折る
    gxThingNameCache.bHasCache = false;

    // ThingName構造体の全部を0x00でクリア
    memset(&gxThingNameCache.xThingName, 0x00, sizeof(ThingName_t));
}

static void vprvSetUsualThingNameCache(const ThingName_t *pxThingName)
{
    // キャッシュフラグを立てる
    gxThingNameCache.bHasCache = true;

    // キャッシュをセット
    memcpy(&gxThingNameCache.xThingName, pxThingName, sizeof(ThingName_t));
}
// --------------------------------------------------
// Unit Test用関数定義(関数のプロトタイプ宣言は「自身のファイル名+ "_test.h"」で宣言されていること)
// --------------------------------------------------
#if (BUILD_MODE_TEST == 1) /* BUILD_MODE_TESTが定義されているとき */
#endif                     /* end  BUILD_MODE_TEST */
