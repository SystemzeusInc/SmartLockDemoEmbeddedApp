/**
 * @file network_operation.c
 * @author Systemzeus Inc.
 * @copyright Copyright © 2023 Systemzeus Inc. All rights reserved.
 */

// --------------------------------------------------
// システムヘッダの取り込み
// --------------------------------------------------

/** C Standard */
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

/** FreeRTOS kernel */
#include "FreeRTOS.h"
#include "event_groups.h"
#include "semphr.h"

/** Network */
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"
#include "FreeRTOS_DHCP.h"
#include "NetworkInterface.h"
#include "iot_wifi.h"
#include "iot_system_init.h"

/** Wi-Fi Driver */
#include "wdrv_pic32mzw_client_api.h"

// --------------------------------------------------
// ユーザ作成ヘッダの取り込み
// --------------------------------------------------
#include "config/task_config.h"

#include "include/application_define.h"
#include "include/network_operation.h"

#include "tasks/flash/include/flash_data.h"
#include "tasks/flash/include/flash_task.h"

// --------------------------------------------------
// 自ファイル内でのみ使用する#defineマクロ
// --------------------------------------------------

// --------------------------------------------------
// 自ファイル内でのみ使用する#define関数マクロ
// --------------------------------------------------
/**
 * @brief Wi-Fiドライバの状態をチェックする間隔
 */
#define WIFI_DRIVER_STATUS_READY_CHECK_INTERVAL_MS (500)

// --------------------------------------------------
// 自ファイル内でのみ使用するtypedef定義
// --------------------------------------------------

// --------------------------------------------------
// 自ファイル内でのみ使用するenumタグ定義（typedefを同時に行う）
// --------------------------------------------------
/**
 * @brief IPタスクの開始を通知するために使用するバイナリイベント
 */
typedef enum
{
    /**
     * @brief IPタスク開始
     */
    IP_TASK_INIT_DONE_EVENT = (0x1 << 1)
} IPTaskInitDoneEvent_t;

/**
 * @brief ネットワークの確立に成功した際に使用するバイナリイベント
 */
typedef enum
{
    /**
     * @brief ネットワークの確立に成功
     *
     * @note
     * 万が一 IP_TASK_INIT_DONE_EVENT と間違えて使用してしまった場合でも気づけるように
     * 値をずらしている。
     */
    NETWORK_ESTABLISH_DONE_EVENT = (0x1 << 2)
} NetworkEstablishDoneEvent_t;

// --------------------------------------------------
// 自ファイル内でのみ使用するstruct/unionタグ定義（typedefを同時に行う）
// --------------------------------------------------

// --------------------------------------------------
// ファイル内で共有するstatic変数宣言
// --------------------------------------------------

/**
 * @brief FreeRTOS層のネットワークインターフェースの初期化を行ったか
 */
static bool gbIsInitializedNetworkInterface = false;

/**
 * @brief デフォルトのMACアドレス
 *
 * @note
 * ネットワーク層が初期化された後、Wi-FiモジュールのMACアドレスに置き換わるため、適当な値でよい。
 **/
uint8_t ucNO_MACAddress[6] =
    {
        configMAC_ADDR0,
        configMAC_ADDR1,
        configMAC_ADDR2,
        configMAC_ADDR3,
        configMAC_ADDR4,
        configMAC_ADDR5};

/**
 * @brief デフォルトのIPアドレス
 *
 * @note
 * 接続先のネットワークがDHCPに対応していた場合、本アドレスは使用されず
 * DHCPで取得したIPアドレスに置き換わるため、適当値でよい。
 */
static const uint8_t ucNO_IPAddress[4] =
    {
        configIP_ADDR0,
        configIP_ADDR1,
        configIP_ADDR2,
        configIP_ADDR3};
static const uint8_t ucNO_NetMask[4] =
    {
        configNET_MASK0,
        configNET_MASK1,
        configNET_MASK2,
        configNET_MASK3};
static const uint8_t ucNO_GatewayAddress[4] =
    {
        configGATEWAY_ADDR0,
        configGATEWAY_ADDR1,
        configGATEWAY_ADDR2,
        configGATEWAY_ADDR3};
static const uint8_t ucNO_DNSServerAddress[4] =
    {
        configDNS_SERVER_ADDR0,
        configDNS_SERVER_ADDR1,
        configDNS_SERVER_ADDR2,
        configDNS_SERVER_ADDR3};

/**
 * @brief デバイスとの切断通知を受けるタスクのタスクハンドラ
 */
static TaskHandle_t gpxInitDoneNotifyTaskHandle = NULL;

/**
 * @brief Wi-Fi接続後、DHCPプロセスを経てネットワークが使用できるようになったことの通知を受けるタスクハンドラ
 */
static TaskHandle_t gpxNetworkEstablishDoneNotifyTaskHandle = NULL;

// --------------------------------------------------
// static関数プロトタイプ宣言
// --------------------------------------------------
/**
 * @brief Wi-Fi接続後にDHCPプロセスを実行する
 */
static void vprvDHCPProccessAfterWiFiConnect(void);

/**
 * @brief Wi-FiドライバがREADY状態であるかチェックする
 *
 * @retval #NETWORK_OPERATION_RESULT_SUCCESS 成功
 * @retval #NETWORK_OPERATION_RESULT_TIMEOUT タイムアウト。時間内にWi-FiドライバのステータスがREADYにならなかった場合。
 */
static NetworkOperationResult_t eprvCheckWiFiStateReady(void);

/**
 * @brief Wi-Fiルータへの接続を行う
 *
 * @param[in] uxMaxRetryCount           最大リトライ回数
 * @param[in] pxRejectConditionFunction 接続失敗時に呼び出されるコールバック関数
 *
 * @retval true  Wi-Fiルータへの接続成功
 * @retval false Wi-Fiルータへの接続失敗
 */
static bool bWiFiConnect(const uint32_t uxMaxRetryCount, const WiFiConnectRejectConditionFunction_t *pxRejectConditionFunction);

/**
 * @brief ネットワーク接続が確立後、呼び出すことでネットワーク情報（IPアドレス、デフォルトゲートウェイ、DNSサーバー）を出力する
 *
 */
static void vprvPrintNetworkInfo(void);

/**
 * @brief ネットワークの切断、接続された時にTCP/IPスタックによって呼び出されるアプリケーションフック関数
 *        ipconfigUSE_DHCP が１に設定されている場合、DHCPからIPアドレスを取得した時にeNetworkUpとして呼び出される。
 *
 * @note
 * このフック関数を利用して、Wi-Fiルータへの接続が成功した後、DHCPによってIPアドレスが取得されるまで待機するロジックを作成する。
 *
 * @see https://www.freertos.org/FreeRTOS-Plus/FreeRTOS_Plus_TCP/API/vApplicationIPNetworkEventHook.html
 *
 * @param[in] eNetworkEvent ネットワークイベントの種類。
 *            eNetworkUp:   DHCPからIPアドレスを取得した
 *            eNetworkDown: ネットワーク接続が失われたことをネットワークドライバーから通知されると発火するが、PIC32では実装されていない。
 */
void vApplicationIPNetworkEventHook(eIPCallbackEvent_t eNetworkEvent);

// --------------------------------------------------
// 変数定義（staticを除く）
// --------------------------------------------------

// --------------------------------------------------
// 関数定義（staticを除く）
// --------------------------------------------------
NetworkOperationResult_t eNetworkInit(void)
{
    // 本APIを１回でも呼び出し成功したらtrueになる
    static bool bIsInitializedIPInit = false;

    APP_PRINTFDebug("Network initialization start.");

    // 既に実行済みの場合は以降の処理を行わいない
    if (bIsInitializedIPInit == true)
    {
        APP_PRINTFWarn("Network is already initialized.");
        return NETWORK_OPERATION_RESULT_ALREADY_INIT;
    }

    // Wi-Fiドライバのステータスチェック
    NetworkOperationResult_t xResult = eprvCheckWiFiStateReady();
    if (xResult != NETWORK_OPERATION_RESULT_SUCCESS)
    {
        return xResult;
    }

    APP_PRINTFDebug("Wi-Fi Driver check ok.");

    // グローバル変数に格納
    gpxInitDoneNotifyTaskHandle = xTaskGetCurrentTaskHandle();

    // TCP/IP層の初期化とWi-Fi接続
    BaseType_t xReturn = FreeRTOS_IPInit(
        ucNO_IPAddress,
        ucNO_NetMask,
        ucNO_GatewayAddress,
        ucNO_DNSServerAddress,
        ucNO_MACAddress);

    // 正常に終了しなかった場合
    if (xReturn != pdTRUE)
    {
        APP_PRINTFError("FreeRTOS_IPInit failed.");
        return NETWORK_OPERATION_RESULT_FAILED;
    }

    // 初期化が完了するまで待機
    EventBits_t xEventBits = 0x0;
    xTaskNotifyWait(IP_TASK_INIT_DONE_EVENT, IP_TASK_INIT_DONE_EVENT, &xEventBits, portMAX_DELAY);

    // ハンドルをNULLにすることで、万が一Notifyが他で呼び出されても問題ないようにする。
    gpxInitDoneNotifyTaskHandle = NULL;

    // IP_TASK_INIT_DONE_EVENTが発火されたか判定する
    if ((xEventBits & IP_TASK_INIT_DONE_EVENT) != IP_TASK_INIT_DONE_EVENT)
    {
        APP_PRINTFError("eNetworkInit: Notify wait failed.");
        return NETWORK_OPERATION_RESULT_FAILED;
    }

    // イニシャライズ完了をマーク
    bIsInitializedIPInit = true;
    APP_PRINTFDebug("Network initialization finished.");
    return NETWORK_OPERATION_RESULT_SUCCESS;
}

NetworkOperationResult_t eWiFiConnectToRouter(
    const uint32_t uxMaxConnectNum,
    const WiFiConnectRejectConditionFunction_t *pxRejectConditionFunction)
{
    APP_PRINTFDebug("eWiFiConnectToRouter");

    // 引数バリデーション
    if (uxMaxConnectNum < 1)
    {
        APP_PRINTFError("eWiFiConnectToRouter validation error.");
        return NETWORK_OPERATION_RESULT_FAILED;
    }

    bool bIsConnectToRouter = false;
    bool bIsInitNetwork = false;
    // FreeRTOSのネットワークインターフェースが初期化されていない場合
    if (gbIsInitializedNetworkInterface == false)
    {
        APP_PRINTFDebug("Network interface is not initialized. So init network interface.");

        bIsInitNetwork = true;

        // ARPのキャッシュをクリアする
        FreeRTOS_ClearARP();

        // この関数はWi-Fiルータへの接続に失敗した場合pdFAILになる。pdFAILが返却された場合でもWi-Fiモジュールの
        // 初期化はできているため、gbIsInitializedNetworkInterfaceをTrueにしてもう一度この関数が呼ばれないようにする
        BaseType_t xInitResult = xNetworkInterfaceInitialise();

        if (SYSTEM_Init() == pdFAIL)
        {
            APP_PRINTFError("SYSTEM_Init failed.");
            return NETWORK_OPERATION_RESULT_FAILED;
        }

        gbIsInitializedNetworkInterface = true;
        // xInitResult == pdTRUEの場合は、Wi-FIルータへの接続まで終わっているため、DHCPプロセスを実行してReturnする。
        if (xInitResult == pdTRUE)
        {
            APP_PRINTFDebug("Network interface is initialized and Wi-Fi connect success.");
            bIsConnectToRouter = true;
        }
    }

    if (bIsConnectToRouter == false)
    {

        // ARPのキャッシュをクリアする
        FreeRTOS_ClearARP();

        // Wi-Fiルータへの接続に失敗した場合は、uxConnectNum - 1回だけ再接続する
        const uint32_t uxRetryMax = bIsInitNetwork ? (uxMaxConnectNum - 1) : uxMaxConnectNum; // イニシャライズで1回Wi-Fiルータへの接続を試みているため、-1回する。
        bIsConnectToRouter = bWiFiConnect(uxRetryMax, pxRejectConditionFunction);
    }

    if (bIsConnectToRouter == true)
    {
        // 通知を受けるためグローバル変数にTaskHandleを格納
        gpxNetworkEstablishDoneNotifyTaskHandle = xTaskGetCurrentTaskHandle();

        // Wi-Fi接続が成功したらDHCPのリクエストを行う
        vprvDHCPProccessAfterWiFiConnect();

        // DHCPを含めたネットワーク全体の接続が確立できるまで待機

        // 初期化が完了するまで待機
        EventBits_t xEventBits = 0x0;
        BaseType_t xIsNotifyReceived = xTaskNotifyWait(NETWORK_ESTABLISH_DONE_EVENT, NETWORK_ESTABLISH_DONE_EVENT, &xEventBits, NETWORK_DONE_WAIT_TIME_MS);

        // ハンドルをNULLにすることで、万が一Notifyが他で呼び出されても問題ないようにする。
        gpxNetworkEstablishDoneNotifyTaskHandle = NULL;

        // タイムアウトの判定
        if (xIsNotifyReceived == false)
        {
            APP_PRINTFError("Network establish done event timeout.");
            return NETWORK_OPERATION_RESULT_TIMEOUT;
        }

        // NETWORK_ESTABLISH_DONE_EVENTが発火されたか判定する
        if ((xEventBits & NETWORK_ESTABLISH_DONE_EVENT) != NETWORK_ESTABLISH_DONE_EVENT)
        {
            APP_PRINTFError("Network establish done event timeout. failed.");
            return NETWORK_OPERATION_RESULT_FAILED;
        }

        APP_PRINTFDebug("Network is available.");

        // アドレス情報をコンソールに表示
        vprvPrintNetworkInfo();

        return NETWORK_OPERATION_RESULT_SUCCESS;
    }

    return NETWORK_OPERATION_RESULT_FAILED;
}

NetworkOperationResult_t eWiFiDisconnectFromRouter(void)
{
    // ルータから切断する
    WIFIReturnCode_t xResult = WIFI_Disconnect();

    return xResult == eWiFiSuccess ? NETWORK_OPERATION_RESULT_SUCCESS : NETWORK_OPERATION_RESULT_FAILED;
}

// --------------------------------------------------
// static関数定義
// --------------------------------------------------
static void vprvDHCPProccessAfterWiFiConnect(void)
{
    // DHCPプロセスを実行する
    vDHCPProcess(pdTRUE, eInitialWait);
}

static NetworkOperationResult_t eprvCheckWiFiStateReady(void)
{
    // タイムアウトするまでWi-Fiドライバの状態チェック
    uint32_t uxTimeoutCount = (uint32_t)(WIFI_DRIVER_STATUS_READY_TIMEOUT_MS / WIFI_DRIVER_STATUS_READY_CHECK_INTERVAL_MS);
    for (uint32_t uxCount = 0; uxCount < uxTimeoutCount; uxCount++)
    {
        // Wi-Fiドライバのステータスチェック
        if (WDRV_PIC32MZW_Status(sysObj.drvWifiPIC32MZW1) == SYS_STATUS_READY)
        {
            return NETWORK_OPERATION_RESULT_SUCCESS;
        }

        // 待機
        vTaskDelay(pdMS_TO_TICKS(WIFI_DRIVER_STATUS_READY_CHECK_INTERVAL_MS));
    }

    APP_PRINTFDebug("Wi-Fi driver status check timed out.");
    // タイムアウトを返却
    return NETWORK_OPERATION_RESULT_TIMEOUT;
}

static bool bWiFiConnect(const uint32_t uxMaxRetryCount, const WiFiConnectRejectConditionFunction_t *pxRejectConditionFunction)
{

    // Wi-Fi接続情報を取得
    WiFiInfo_t xWiFiInfoFromSE;
    memset(&xWiFiInfoFromSE, 0x00, sizeof(xWiFiInfoFromSE));
    WIFINetworkParams_t xNetworkParams = {0x00};

    // SEからWi-Fi情報の取得
    if (eReadFlashInfo(READ_FLASH_TYPE_WIFI_INFO, &xWiFiInfoFromSE, sizeof(xWiFiInfoFromSE)) != FLASH_TASK_RESULT_SUCCESS)
    {
        APP_PRINTFError("Failed to retrieve WiFi connection information from SE.");
        return false;
    }

    // Wi-Fi接続関数に渡せる形に変形
    memcpy(xNetworkParams.ucSSID, xWiFiInfoFromSE.cWifiSSID, strlen((const char *)xWiFiInfoFromSE.cWifiSSID));
    xNetworkParams.ucSSIDLength = strlen((const char *)xWiFiInfoFromSE.cWifiSSID);
    memcpy(xNetworkParams.xPassword.xWPA.cPassphrase, xWiFiInfoFromSE.cWiFiPassword, strlen((const char *)xWiFiInfoFromSE.cWiFiPassword));
    xNetworkParams.xPassword.xWPA.ucLength = strlen((const char *)xWiFiInfoFromSE.cWiFiPassword);
    xNetworkParams.xSecurity = xWiFiInfoFromSE.xWiFiSecurity;
    xNetworkParams.ucChannel = 0; /* Scan all channels (255) */

    APP_PRINTFDebug("SSID:%s, PASS: %s, SEC: %d", xNetworkParams.ucSSID, xNetworkParams.xPassword.xWPA.cPassphrase, xNetworkParams.xSecurity);
    APP_PRINTFDebug("SSIDL:%d, PASSL: %d", xNetworkParams.ucSSIDLength, xNetworkParams.xPassword.xWPA.ucLength);

    for (uint32_t ucRetryNum = 1; ucRetryNum < uxMaxRetryCount; ucRetryNum++)
    {
        // Wi-Fi接続
        if (WIFI_ConnectAP(&xNetworkParams) == eWiFiSuccess)
        {

            APP_PRINTFDebug("WiFi Connect success.");
            return true;
        }

        // Wi-Fi接続の中断判定
        if (pxRejectConditionFunction != NULL && pxRejectConditionFunction->bRejectConditionFunction != NULL)
        {
            if (pxRejectConditionFunction->bRejectConditionFunction(ucRetryNum) == false)
            {
                APP_PRINTFDebug("The connection to the Wi-Fi router was canceled by the interruption function.");
                return false;
            }
        }

        APP_PRINTFDebug("WiFi Connect fail. Connect: %d", ucRetryNum);
        vTaskDelay(pdMS_TO_TICKS(WIFI_CONNECT_WAIT_TIME_MS));
    }

    return false;
}

static void vprvPrintNetworkInfo(void)
{
    uint32_t uxIPAddress, uxNetMask, uxGatewayAddress, uxDNSServerAddress;
    uint8_t ucBuffer[16] = {0x00};

    APP_PRINTFDebug("### Address information obtained from DHCP. ###");

    /* DHCPサーバーから取得した情報を表示する */
    FreeRTOS_GetAddressConfiguration(
        &uxIPAddress,
        &uxNetMask,
        &uxGatewayAddress,
        &uxDNSServerAddress);

    FreeRTOS_inet_ntoa(uxIPAddress, ucBuffer);
    APP_PRINTFDebug("IP Address: %s", ucBuffer);

    FreeRTOS_inet_ntoa(uxNetMask, ucBuffer);
    APP_PRINTFDebug("Subnet Mask: %s", ucBuffer);

    FreeRTOS_inet_ntoa(uxGatewayAddress, ucBuffer);
    APP_PRINTFDebug("Gateway Address: %s", ucBuffer);

    FreeRTOS_inet_ntoa(uxDNSServerAddress, ucBuffer);
    APP_PRINTFDebug("DNS Server Address: %s", ucBuffer);

    APP_PRINTFDebug("########");
}

// --------------- Task Notify関数 -------------------

void vprvNotifyIPInitTaskUp(void)
{
    APP_PRINTFDebug("Notification of initialization has been received.");

    if (gpxInitDoneNotifyTaskHandle == NULL)
    {
        APP_PRINTFDebug("gpxInitDoneNotifyTaskHandle is null.");
        return;
    }

    // タスクの待機を解除する
    xTaskNotify(gpxInitDoneNotifyTaskHandle, IP_TASK_INIT_DONE_EVENT, eSetBits);
}

void vApplicationIPNetworkEventHook(eIPCallbackEvent_t eNetworkEvent)
{

    APP_PRINTFDebug("ApplicationIPNetworkEventHook received. Event: %d", eNetworkEvent);

    if (eNetworkEvent == eNetworkUp)
    {

        APP_PRINTFDebug("Network up event received.");

        if (gpxNetworkEstablishDoneNotifyTaskHandle == NULL)
        {
            APP_PRINTFDebug("gpxNetworkEstablishDoneNotifyTaskHandle is null.");
            return;
        }

        // タスクの待機を解除する
        xTaskNotify(gpxNetworkEstablishDoneNotifyTaskHandle, NETWORK_ESTABLISH_DONE_EVENT, eSetBits);
    }
}

// --------------------------------------------------
// Unit Test用関数定義(関数のプロトタイプ宣言は「自身のファイル名+ "_test.h"」で宣言されていること)
// --------------------------------------------------
#if (BUILD_MODE_TEST == 1) /* BUILD_MODE_TESTが定義されているとき */
#endif                     /* end  BUILD_MODE_TEST */
