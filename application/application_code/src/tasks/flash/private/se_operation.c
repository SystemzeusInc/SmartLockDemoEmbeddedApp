/**
 * @file se_operation.c
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

#include "iot_wifi.h"

#include "atca_basic.h"

// --------------------------------------------------
// ユーザ作成ヘッダの取り込み
// --------------------------------------------------
#include "common/include/device_state.h"
#include "common/include/application_define.h"

#include "tasks/flash/private/include/se_operation.h"

// --------------------------------------------------
// 自ファイル内でのみ使用する#defineマクロ
// --------------------------------------------------

// --------------------------------------------------
// 自ファイル内でのみ使用する#define関数マクロ
// --------------------------------------------------
/**
 * @brief ECC608のシリアル番号のサイズ（byte）
 */
#define ECC608_SERIAL_NUMBER_BINARY_SIZE (9U)

// --------------------------------------------------
// 自ファイル内でのみ使用するtypedef定義
// --------------------------------------------------

// --------------------------------------------------
// 自ファイル内でのみ使用するenumタグ定義（typedefを同時に行う）
// --------------------------------------------------

// --------------------------------------------------
// 自ファイル内でのみ使用するstruct/unionタグ定義（typedefを同時に行う）
// --------------------------------------------------

// --------------------------------------------------
// ファイル内で共有するstatic変数宣言
// --------------------------------------------------

// --------------------------------------------------
// static関数プロトタイプ宣言
// --------------------------------------------------

/**
 * @brief 指定スロットからデータを読み込む
 *
 * @sa atcab_read_bytes_zone
 *
 * @param[in]  uxZone   ECC608の領域(CONFIG, DATA, OTP)
 * @param[in]  uxSlot   読み込むスロットID
 * @param[in]  xOffset  アドレスオフセット値
 * @param[out] puxData  読み込んだデータが格納されるバッファ
 * @param[in]  xLength  puxDataのサイズ
 *
 * @return ATCA_STATUS cryptoauthlibのステータスコード
 */
static ATCA_STATUS eReadECC608Flash(uint8_t uxZone, uint16_t uxSlot, size_t xOffset, uint8_t *puxData, size_t xLength);

/**
 * @brief 指定スロットにデータを書き込み
 *
 * @sa atcab_write_bytes_zone
 *
 * @param[in] uxZone  ECC608の領域(CONFIG, DATA, OTP)
 * @param[in] uxSlot  書き込むスロットID
 * @param[in] xOffset アドレスオフセット値
 * @param[in] puxData 書き込むデータを格納するバッファ
 * @param[in] xLength puxDataのサイズ
 *
 * @return ATCA_STATUS cryptoauthlibのステータスコード
 */
static ATCA_STATUS eWriteECC608Flash(uint8_t uxZone, uint16_t uxSlot, size_t xOffset, const uint8_t *puxData, size_t xLength);

/**
 * @brief SEのセキュリティタイプからWIFISecurity_tに変換して格納する
 *
 * @param[in]  xSecTypeFromSE SEから取得したセキュリティタイプ
 * @param[out] pxSecurity     セットするセキュリティタイプのポインタ
 *
 * @retval true  成功
 * @retval false 失敗
 */
static bool bprvConvertSecurityTypeSEToEnum(const uint32_t xSecTypeFromSE, WIFISecurity_t *pxSecurity);

/**
 * @brief SEのセキュリティタイプからWIFISecurity_tに変換して格納する
 *
 * @param[in]  xSecurity           WIFISecurity_t
 * @param[out] pxSecTypeFromSE     変換後のSEに格納するセキュリティタイプを格納するポインタ
 *
 * @retval true  成功
 * @retval false 失敗
 */
static bool bprvConvertSecurityTypeEnumToSE(const WIFISecurity_t xSecurity, uint32_t *pxSecTypeFromSE);

/**
 * @brief I2CバスがReady状態になるまで待機する
 *
 * @return true  成功
 * @return false タイムアウト、またはその他エラー
 */
static bool bprvWaitI2CBusReady(void);

// --------------------------------------------------
// 変数定義（staticを除く）
// --------------------------------------------------

/**
 * @brief ECC608にアクセスするための設定
 */
extern ATCAIfaceCfg atecc608_0_init_data;

/**
 * @brief I2Cドライバのシステムオブジェクト
 */
extern SYSTEM_OBJECTS sysObj;

// --------------------------------------------------
// 関数定義（staticを除く）
// --------------------------------------------------

SEOperation_t eSEOperationInit(void)
{
    // ECC608の初期化
    ATCAIfaceCfg *ifacecfg = &atecc608_0_init_data;
    ATCA_STATUS eStatus;

    // ECC608コンフィグレーションのリリース
    ATCADevice xDevice = atcab_get_device();
    if (xDevice != NULL)
    {
        eStatus = atcab_release();
        if (eStatus != ATCA_SUCCESS)
        {
            APP_PRINTFError("atcab release failed: 0x%02X", eStatus);
            return SE_OPERATION_RESULT_FAILURE;
        }
    }

    // ECC608の初期化
    eStatus = atcab_init(ifacecfg);
    if (eStatus != ATCA_SUCCESS)
    {
        APP_PRINTFError("atcab init failed: 0x%02X", eStatus);
        return SE_OPERATION_RESULT_FAILURE;
    }

    APP_PRINTFDebug("SE operation init success.");

    return SE_OPERATION_RESULT_SUCCESS;
}

/**
 * ##################################
 * # 読み込み用関数
 * ##################################
 */

SEOperation_t eGetWiFiInfoFromSE(WiFiInfo_t *pxWiFiInfo)
{

    ATCA_STATUS eResult;
    uint32_t uxSecType = 0xFFFFFFFF;

    // バリデート
    if (pxWiFiInfo == NULL)
    {
        APP_PRINTFError("Buffer provided is NULL");
        return SE_OPERATION_RESULT_FAILURE;
    }
    memset(pxWiFiInfo, 0x00, sizeof(WiFiInfo_t));

    // SSID、PW、セキュリティタイプの取得
    const uint8_t uxStatementNum = 3;
    for (uint8_t uxCount = 0; uxCount < uxStatementNum; uxCount++)
    {
        switch (uxCount)
        {
        case 0:
            eResult = eReadECC608Flash(ATCA_ZONE_DATA,
                                       SAVE_SLOT_ID,
                                       SE_SSID_START_ADDRESS,
                                       pxWiFiInfo->cWifiSSID,
                                       SE_SSID_LENGTH);
            break;
        case 1:
            eResult = eReadECC608Flash(ATCA_ZONE_DATA,
                                       SAVE_SLOT_ID,
                                       SE_PASSWORD_START_ADDRESS,
                                       pxWiFiInfo->cWiFiPassword,
                                       SE_PASSWORD_LENGTH);
            break;
        case 2:
            eResult = eReadECC608Flash(ATCA_ZONE_DATA,
                                       SAVE_SLOT_ID,
                                       SE_SECURITY_TYPE_START_ADDRESS,
                                       (uint8_t *)&uxSecType,
                                       SE_SECURITY_TYPE_LENGTH);
            break;
        }

        if (eResult != ATCA_SUCCESS)
        {
            APP_PRINTFError("Flash read error from SE.StatementNum: %d Reason: 0x%02X", uxCount, eResult);
            return SE_OPERATION_RESULT_FAILURE;
        }
    }

    // SEから取得したセキュリティタイプとWIFISecurity_tの変換を行う
    if (bprvConvertSecurityTypeSEToEnum(uxSecType, &(pxWiFiInfo->xWiFiSecurity)) == false)
    {
        APP_PRINTFError("Security transform error");
        return SE_OPERATION_RESULT_FAILURE;
    };

    APP_PRINTFDebug("Successfully obtained Wi-Fi information from SE.");

    return SE_OPERATION_RESULT_SUCCESS;
}

SEOperation_t eGetProvisioningFlag(ProvisioningFlag_t *pxProvisioningFlag)
{
    // バリデート
    if (pxProvisioningFlag == NULL)
    {
        APP_PRINTFError("No data to store");
        return SE_OPERATION_RESULT_FAILURE;
    }

    // SEから読み込み
    uint32_t xProvisioningFlagSE = 0x00;
    ATCA_STATUS eResult = eReadECC608Flash(ATCA_ZONE_DATA,
                                           SAVE_SLOT_ID,
                                           SE_PROVISIONING_FLAG_START_ADDRESS,
                                           (uint8_t *)&xProvisioningFlagSE,
                                           SE_PROVISIONING_FLAG_LENGTH);
    if (eResult != ATCA_SUCCESS)
    {
        APP_PRINTFError("Flash read error from SE. Reason: 0x%02X", eResult);
        return SE_OPERATION_RESULT_FAILURE;
    }

    // SEから読み込んだプロビジョニングフラグを変換
    if (xProvisioningFlagSE == PROVISIONING_FLAG_NOT_IMPLEMENTED)
    {
        *pxProvisioningFlag = false;
    }
    else if (xProvisioningFlagSE == PROVISIONING_FLAG_ALREADY_FINISHED)
    {
        *pxProvisioningFlag = true;
    }
    else
    {
        APP_PRINTFError("Provisioning flags read are undefined.");
        return SE_OPERATION_RESULT_FAILURE;
    }

    return SE_OPERATION_RESULT_SUCCESS;
}

SEOperation_t eGetIoTEndpoint(AWSIoTEndpoint_t *pxEndpoint)
{
    // バリデート
    if (pxEndpoint == NULL)
    {
        APP_PRINTFError("No data to store");
        return SE_OPERATION_RESULT_FAILURE;
    }

    // SEから読み込み
    ATCA_STATUS eResult = eReadECC608Flash(ATCA_ZONE_DATA,
                                           SAVE_SLOT_ID,
                                           SE_IOT_ENDPOINT_START_ADDRESS,
                                           pxEndpoint->ucEndpoint,
                                           SE_IOT_ENDPOINT_LENGTH);

    if (eResult != ATCA_SUCCESS)
    {
        APP_PRINTFError("Flash read error from SE. Reason: 0x%02X", eResult);
        return SE_OPERATION_RESULT_FAILURE;
    }

    return SE_OPERATION_RESULT_SUCCESS;
}

SEOperation_t eGetThingName(ThingName_t *pxName)
{
    // バリデート
    if (pxName == NULL)
    {
        APP_PRINTFError("No data to store");
        return SE_OPERATION_RESULT_FAILURE;
    }

    // SE内のThingName領域は128ByteでThingName本来のサイズは36Byte（文字）であるため、一端バッファにコピーした後pxNameにさらにコピーする
    uint8_t uxThingNameSE[SE_THING_NAME_LENGTH] = {0x00};

    // SEから読み込み
    ATCA_STATUS eResult = eReadECC608Flash(ATCA_ZONE_DATA,
                                           SAVE_SLOT_ID,
                                           SE_THING_NAME_START_ADDRESS,
                                           uxThingNameSE,
                                           SE_THING_NAME_LENGTH);

    if (eResult != ATCA_SUCCESS)
    {
        APP_PRINTFError("Flash read error from SE. Reason: 0x%02X", eResult);
        return SE_OPERATION_RESULT_FAILURE;
    }

    strncpy((char *)pxName->ucName, (const char *)uxThingNameSE, THING_NAME_LENGTH);

    return SE_OPERATION_RESULT_SUCCESS;
}

SEOperation_t eGetFactoryThingName(FactoryThingName_t *pxName)
{
    if (pxName == NULL)
    {
        APP_PRINTFError("No data to store");
        return SE_OPERATION_RESULT_FAILURE;
    }

    if (bprvWaitI2CBusReady() == false)
    {
        return SE_OPERATION_RESULT_FAILURE;
    }

    uint8_t xSerialNumberBinary[ECC608_SERIAL_NUMBER_BINARY_SIZE] = {0x00};
    ATCA_STATUS eResult = atcab_read_serial_number(xSerialNumberBinary);

    if (eResult != ATCA_SUCCESS)
    {
        APP_PRINTFError("ECC608 serial number read error. Reason: 0x%02X", eResult);
        return SE_OPERATION_RESULT_FAILURE;
    }

    // バイナリから16進数の大文字文字列に変換
    snprintf((char *)pxName->ucName, sizeof(pxName->ucName), "%02X%02X%02X%02X%02X%02X%02X%02X%02X",
             xSerialNumberBinary[0],
             xSerialNumberBinary[1],
             xSerialNumberBinary[2],
             xSerialNumberBinary[3],
             xSerialNumberBinary[4],
             xSerialNumberBinary[5],
             xSerialNumberBinary[6],
             xSerialNumberBinary[7],
             xSerialNumberBinary[8]);

    return SE_OPERATION_RESULT_SUCCESS;
}

/**
 * ##################################
 * # 書き込み用関数
 * ##################################
 */

SEOperation_t eSetWiFiInfoToSE(const WiFiInfo_t *pxWiFiInfo)
{
    ATCA_STATUS eResult;
    uint32_t uxSecType = 0xFFFFFFFF;

    // バリデート
    if (pxWiFiInfo == NULL)
    {
        APP_PRINTFError("Buffer provided is NULL");
        return SE_OPERATION_RESULT_FAILURE;
    }

    // WIFISecurity_tからSEに保存するセキュリティタイプに変換
    if (bprvConvertSecurityTypeEnumToSE(pxWiFiInfo->xWiFiSecurity, &uxSecType) == false)
    {
        APP_PRINTFError("Buffer provided is NULL");
        return SE_OPERATION_RESULT_FAILURE;
    }

    // SSID、PW、セキュリティタイプの取得
    const uint8_t uxStatementNum = 3;
    for (uint8_t uxCount = 0; uxCount < uxStatementNum; uxCount++)
    {
        switch (uxCount)
        {
        case 0:
            eResult = eWriteECC608Flash(ATCA_ZONE_DATA,
                                        SAVE_SLOT_ID,
                                        SE_SSID_START_ADDRESS,
                                        pxWiFiInfo->cWifiSSID,
                                        SE_SSID_LENGTH);
            break;
        case 1:
            eResult = eWriteECC608Flash(ATCA_ZONE_DATA,
                                        SAVE_SLOT_ID,
                                        SE_PASSWORD_START_ADDRESS,
                                        pxWiFiInfo->cWiFiPassword,
                                        SE_PASSWORD_LENGTH);
            break;
        case 2:
            eResult = eWriteECC608Flash(ATCA_ZONE_DATA,
                                        SAVE_SLOT_ID,
                                        SE_SECURITY_TYPE_START_ADDRESS,
                                        (uint8_t *)&uxSecType,
                                        SE_SECURITY_TYPE_LENGTH);
            break;
        }

        if (eResult != ATCA_SUCCESS)
        {
            APP_PRINTFError("Flash write error to SE. StatementNum: %d Reason: 0x%02X", uxCount, eResult);
            return SE_OPERATION_RESULT_FAILURE;
        }
    }

    return SE_OPERATION_RESULT_SUCCESS;
}

SEOperation_t eSetProvisioningFlag(const ProvisioningFlag_t xProvisioningFlag)
{
    // プロビジョニングフラグをSEに書き込む値に変換
    uint32_t uxWriteData;
    if (xProvisioningFlag == true)
    {
        uxWriteData = PROVISIONING_FLAG_ALREADY_FINISHED;
    }
    else
    {
        uxWriteData = PROVISIONING_FLAG_NOT_IMPLEMENTED;
    }

    // SEに書き込み
    ATCA_STATUS eResult = eWriteECC608Flash(ATCA_ZONE_DATA,
                                            SAVE_SLOT_ID,
                                            SE_PROVISIONING_FLAG_START_ADDRESS,
                                            (uint8_t *)&uxWriteData,
                                            SE_PROVISIONING_FLAG_LENGTH);

    if (eResult != ATCA_SUCCESS)
    {
        APP_PRINTFError("Flash write error to SE. Reason: 0x%02X", eResult);
        return SE_OPERATION_RESULT_FAILURE;
    }

    return SE_OPERATION_RESULT_SUCCESS;
}

SEOperation_t eSetIoTEndpoint(const AWSIoTEndpoint_t *pxEndpoint)
{
    // バリデート
    if (pxEndpoint == NULL)
    {
        APP_PRINTFError("Buffer provided is NULL");
        return SE_OPERATION_RESULT_FAILURE;
    }

    // SEに書き込み
    ATCA_STATUS eResult = eWriteECC608Flash(ATCA_ZONE_DATA,
                                            SAVE_SLOT_ID,
                                            SE_IOT_ENDPOINT_START_ADDRESS,
                                            pxEndpoint->ucEndpoint,
                                            SE_IOT_ENDPOINT_LENGTH);

    if (eResult != ATCA_SUCCESS)
    {
        APP_PRINTFError("Flash write error to SE. Reason: 0x%02X", eResult);
        return SE_OPERATION_RESULT_FAILURE;
    }

    return SE_OPERATION_RESULT_SUCCESS;
}

SEOperation_t eSetThingName(const ThingName_t *pxName)
{
    // バリデート
    if (pxName == NULL)
    {
        APP_PRINTFError("Buffer provided is NULL");
        return SE_OPERATION_RESULT_FAILURE;
    }

    // SE内のThingName領域は128ByteでThingName本来のサイズは36Byte（文字）であるため、128Byteに合わせるためバッファにコピーする
    uint8_t uxThingName[SE_THING_NAME_LENGTH];
    memset(uxThingName, 0x00, sizeof(uxThingName));
    strncpy((char *)uxThingName, (const char *)pxName->ucName, THING_NAME_LENGTH);

    // SEに書き込み
    ATCA_STATUS eResult = eWriteECC608Flash(ATCA_ZONE_DATA,
                                            SAVE_SLOT_ID,
                                            SE_THING_NAME_START_ADDRESS,
                                            uxThingName,
                                            SE_THING_NAME_LENGTH);

    if (eResult != ATCA_SUCCESS)
    {
        APP_PRINTFError("Flash write error to SE. Reason: 0x%02X", eResult);
        return SE_OPERATION_RESULT_FAILURE;
    }

    return SE_OPERATION_RESULT_SUCCESS;
}

// --------------------------------------------------
// static関数定義
// --------------------------------------------------

static ATCA_STATUS eReadECC608Flash(uint8_t uxZone, uint16_t uxSlot, size_t xOffset, uint8_t *puxData, size_t xLength)
{

    ATCA_STATUS eResult = ATCA_SUCCESS;

    // I2CバスがReady状態になるまで待機
    if (bprvWaitI2CBusReady() == false)
    {
        return ATCA_TIMEOUT;
    }
    uint8_t uxRetryCount;
    for (uxRetryCount = 0; uxRetryCount < SE_OPERATION_FLASH_RETRY_COUNT; uxRetryCount++)
    {
        // 指定箇所からデータ読み込み
        eResult = atcab_read_bytes_zone(uxZone, uxSlot, xOffset, puxData, xLength);

        if (eResult == ATCA_SUCCESS)
        {
            break;
        }

        APP_PRINTFWarn("Flash read failed. Retry...");

        // 100ms待機
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    return eResult;
}

static ATCA_STATUS eWriteECC608Flash(uint8_t uxZone, uint16_t uxSlot, size_t xOffset, const uint8_t *puxData, size_t xLength)
{
    ATCA_STATUS eResult = ATCA_SUCCESS;

    // I2CバスがReady状態になるまで待機
    if (bprvWaitI2CBusReady() == false)
    {
        return ATCA_TIMEOUT;
    }

    uint8_t uxRetryCount;
    for (uxRetryCount = 0; uxRetryCount < SE_OPERATION_FLASH_RETRY_COUNT; uxRetryCount++)
    {
        // 指定箇所へのデータ書き込み
        eResult = atcab_write_bytes_zone(uxZone, uxSlot, xOffset, puxData, xLength);

        if (eResult == ATCA_SUCCESS)
        {
            break;
        }

        APP_PRINTFWarn("Flash write failed. Retry...");

        // 100ms待機
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    return eResult;
}

static bool bprvWaitI2CBusReady(void)
{

    // ドライバがBusy状態じゃないか確認
    uint8_t uxRetryCout;
    const uint8_t uxWaitInterval = 100; // 待機する間隔
    const uint8_t uxMaxRetryCount = (SE_OPERATION_FLASH_RW_TIMEOUT_MS / uxWaitInterval);
    for (uxRetryCout = 0; uxRetryCout < uxMaxRetryCount; uxRetryCout++)
    {
        bool isBusy = I2C2_IsBusy();
        if (isBusy == false)
        {
            APP_PRINTFDebug("I2C bus is ready.");
            break;
        }
        APP_PRINTFWarn("Wait for the I2C bus to be ready....");

        // 100ms待機
        vTaskDelay(pdMS_TO_TICKS(uxWaitInterval));
    }

    // タイムアウトしたかの判定
    if (uxRetryCout == uxMaxRetryCount)
    {
        APP_PRINTFError("The I2C bus remained busy and timed out. TIMEOUT = %dms", SE_OPERATION_FLASH_RW_TIMEOUT_MS);
        return false;
    }

    return true;
}

static bool bprvConvertSecurityTypeSEToEnum(const uint32_t xSecTypeFromSE, WIFISecurity_t *pxSecurity)
{
    switch (xSecTypeFromSE)
    {
    case SE_SECURITY_TYPE_WEP:
        *pxSecurity = eWiFiSecurityWEP;
        break;
    case SE_SECURITY_TYPE_WPA:
        *pxSecurity = eWiFiSecurityWPA;
        break;
    case SE_SECURITY_TYPE_WPA2:
        *pxSecurity = eWiFiSecurityWPA2;
        break;
    case SE_SECURITY_TYPE_WPA3:
        *pxSecurity = eWiFiSecurityWPA3;
        break;
    default:
        APP_PRINTFError("Invalid security type %d", xSecTypeFromSE);
        return false;
    }

    return true;
}

static bool bprvConvertSecurityTypeEnumToSE(const WIFISecurity_t xSecurity, uint32_t *pxSecTypeFromSE)
{
    switch (xSecurity)
    {
    case eWiFiSecurityWEP:
        *pxSecTypeFromSE = SE_SECURITY_TYPE_WEP;
        break;
    case eWiFiSecurityWPA:
        *pxSecTypeFromSE = SE_SECURITY_TYPE_WPA;
        break;
    case eWiFiSecurityWPA2:
        *pxSecTypeFromSE = SE_SECURITY_TYPE_WPA2;
        break;
    case eWiFiSecurityWPA3:
        *pxSecTypeFromSE = SE_SECURITY_TYPE_WPA3;
        break;
    default:
        APP_PRINTFError("Invalid security type %d", xSecurity);
        return false;
    }
    return true;
}

// --------------------------------------------------
// Unit Test用関数定義(関数のプロトタイプ宣言は「自身のファイル名+ "_test.h"」で宣言されていること)
// --------------------------------------------------
#if (BUILD_MODE_TEST == 1) /* BUILD_MODE_TESTが定義されているとき */
#endif                     /* end  BUILD_MODE_TEST */
