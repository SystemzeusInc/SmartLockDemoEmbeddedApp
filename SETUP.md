# セットアップ方法

## 1. はじめに

WFI32-IoT Boardを用いたスマートロックデバイスの組込みアプリを開発するための環境の構築方法について説明します。

> **Warning**
> これは2023/5時点に作成したものです。


## 2. 概要

### 2.1. 物理構成

![実際のHW](/doc/image/setup/actual_item.png)  
*実際のHW*

![ブロック図](/doc/image/setup/hw_block.png)  
*HWブロック図*

|No.|デバイス名|メーカー|型番・品名など|説明|
|---|---|---|---|---|
| 1 | WFI32-IoT Board | Microchip | [WFI32-IoT Development Board](https://www.microchip.com/en-us/development-tool/ev36w50a) | Microchip製のIoT開発用ボード。MCUはWFI32E01PC(PIC32MZW1)。Wi-Fiモジュールとセキュアエレメント搭載型。 |
| 2 | BLE             | MIKROE    | [RN4870 CLICK](https://www.mikroe.com/rn4870-click)    | アプリとの通信に用いる。モジュールはMicrochip製RN4870。 |
| 3 | サーボモーター  | Parallax  | [Parallax Feedback 360° High-Speed Servo](https://www.switch-science.com/products/3598) | 鍵を回すのに用いるモーター |


### 2.2. ソフトウェア一覧

動作確認済みのPC環境は`Windows 11 22H2`です。

|No.|ソフトウェア名|バージョン|説明|
|---|---|---|---|
| 1 | Git                       | >2.39.1.windows.1 | ソースコードの管理 |
| 2 | Python3                   | >3.10.5           | ビルド時に使用(署名の作成など) |
| 3 | SRecord                   | >1.64.D001        | ビルド時に使用(バイナリの結合など) |
| 4 | MPLAB® X IDE              | v6.00             | 統合開発環境 |
| 5 | MPLAB® Harmony 3 Launcher | 3.6.4             | 開発フレームワーク |
| 6 | MPLAB® XC32/32++ Complier | 2.40              | コンパイラ |
| 7 | TeraTerm                  | 4.106             | UARTターミナルエミュレーター |

※バージョンは動作確認がとれているものです

## 3. セットアップ手順

### 3.1. ハードウェアセットアップ

以下配線図を参照してセットアップしてください。

![配線図](/doc/image/setup/wiring_diagram.png)  
*配線図*

### 3.2. ソフトウェアセットアップ

#### 1. Gitのインストール

[git for windows](https://gitforwindows.org/)から最新バージョンのgitをインストールします。  
インストール完了後、以下のコマンドが実行できるか確認してください。

```bash
$ git --version
git version 2.40.1.windows.1
```

#### 2. Python3のインストール

[Python](https://www.python.org/downloads/)から最新バージョンのPythonをインストールします。  
インストール時には以下の画像のように「Add python.exe to PATH」にチェックを入れてください。  入れないとインストール先のパスを手動で環境変数に追加する手間が増えます。

![python installer](/doc/image/setup/install_python.png)  
*pythonインストーラー*

インストール完了後、以下のコマンドが実行できるか確認してください。

```bash
$ python --version
Python 3.11.3
```

#### 3. SRecordのインストール

[SRecord](https://srecord.sourceforge.net/)から最新のSRecordをインストールします。SRecordはインストールを忘れがちで、インストールしなかった場合はビルドに失敗します。  
インストール完了後、環境変数のPATHにSRecordを追加します。

![環境変数](/doc/image/setup/install_srecord.png)  
*環境変数*

※設定するPATHはSRecordのインストール先のbinフォルダです

インストール完了後、以下のコマンドが実行できるか確認してください。

```bash
$ srec_cat --version
srec_cat version 1.65.0 [git hash 5844fa801c]
...
```

> **Warning**
> インストーラーを起動したときに「WindowsによってPCが保護されました」と出る場合があります。このときは `詳細情報` > `実行`でインストールを続けてください

#### 4. MPLAB X IDEのインストール

[Microchip](https://www.microchip.com/en-us/tools-resources/archives/mplab-ecosystem)から**v6.00**の[MPLAB X IDE](https://www.microchip.com/content/dam/mchp/documents/DEV/ProductDocuments/SoftwareTools/MPLABX-v6.00-linux-installer.tar)をインストールします。
これより新しいバージョンは動作確認できていません。
<!-- 最新版6.05(2023/5時点)の場合は後述のHarmonyが使用(インストール)できない可能性があります。 -->

#### 5. Harmony関連のインストール

1. Harmonyのインストール
    MPLAB X IDEを起動し、`Tools` > `Plugins Download`を選択します。
    Harmonyを検索し、チェックボックスにチェックを入れて`Install`をクリックします。

    ![Plugins](/doc/image/setup/install_harmony_2.png)  
    *Plugins*

1. パッケージのインストール
    `Tools` > `Embedded` > `MPLAB® Harmony 3 Content Manager`を開きます。  
    ContentManagerから以下のものをダウンロード、バージョンを合わせてください。
    - cryptoauthlib: v3.3.2
    - csp: v3.10.0
    - core: v3.10.0
    - wireless_wifi: v3.6.1
    - dev_packs: v3.10.0
    - wolfssl: v4.7.0
    - bsp: v3.10.0
    - crypto: v3.7.1

    > **Warning**
    > この段階でamazon-freertosとaws_cloudはインストールしないでください。
    > インストールするとHarmony実行時にパスの設定がうまくいきません。
    
    ![Harmony Content Manager](/doc/image/setup/harmony_content_manager.png)  
    *Harmony Content Manager*

2. amazon-freertos, aws_cloudパッケージの手動インストール
    1. Harmony3のディレクトリ(通常は _C:\\User\\{ユーザー名}\\Harmony3_)に移動します
    2. ここでコマンドプロンプトなどから以下のコマンドを実行し、amazon-freertosを取得します
        ```bash
        $ git clone https://github.com/MicrochipTech/amazon-freertos.git
        $ cd amazon-freertos
        amazon-freertos$ git checkout mchpdev_20210700
        amazon-freertos$ git submodule update --init --recursive
        ```
    3. [ここ](https://microchiptechnology-my.sharepoint.com/personal/ben_poon_microchip_com/_layouts/15/onedrive.aspx?id=%2Fpersonal%2Fben%5Fpoon%5Fmicrochip%5Fcom%2FDocuments%2FAFR%2Fh3%20support%2Faws%5Fcloud%2Ezip&parent=%2Fpersonal%2Fben%5Fpoon%5Fmicrochip%5Fcom%2FDocuments%2FAFR%2Fh3%20support&ga=1)からaws_cloud.zipを取得し、Harmony3/下に展開します
    
    以上で次のような構成になっているはずです。
    ```
    C:\
    └ Users/
       └ user_name/
          └ Harmony3/
             ├ amazon-freertos/
             ├ aws_cloud/
             ├ bsp/
             └...
    ```

#### 6. XC32/32++のインストール
   
[Microchip](https://www.microchip.com/en-us/tools-resources/archives/mplab-ecosystem)からv2.40の[XC32/32++ Compiler](https://ww1.microchip.com/downloads/en/DeviceDoc/xc32-v2.40-full-install-windows-installer.exe)をインストール。  
インストール時には以下の画像のように「Add xc32 to the PATH environment variable」にチェックを入れてください。入れないとインストール先のパスを手動で環境変数に追加する手間が増えます。

![xc32インストーラー](/doc/image/setup/install_xc32.png)  
*xc32インストーラー*

#### 7. TeraTermのインストール

[TeraTerm](https://ttssh2.osdn.jp/)から最新の[TeraTerm](https://ja.osdn.net/projects/ttssh2/downloads/74780/teraterm-4.106.exe/)をインストールします。
シリアルポートの設定は以下のように設定します。

- ボーレート: `115200`
- データ: `8bit`
- パリティ: `none`
- ストップビット: `1bit`
- フロー制御: `none`


#### 8. リポジトリのクローン

1. スマートロックデバイス組込みアプリのリポジトリをクローン
    _C:\\_ 直下に以下のコマンドを用いてクローンします。

    ```bash
    $ git clone https://github.com/SystemzeusInc/SmartLockDemoEmbeddedApp.git
    ```

    > **Warning**
    > プロジェクトの階層が深く、ビルド時に[WindowsのPATHの長さ制限](https://learn.microsoft.com/ja-jp/windows/win32/fileio/maximum-file-path-limitation?tabs=registry)に引っかかる可能性があるため、_C:\\_ 直下に配置することを推奨します。

2. amazon-freertosのリポジトリをクローン
    
    上記でcloneしたプロジェクトのトップディレクトリでamazon-freertosをクローンします。

    ```bash
    $ git clone https://github.com/MicrochipTech/amazon-freertos.git
    ```

    この時点で以下のような構成になっているはずです。
    ```text
    C:\
    └ SmartLockDemoEmbeddedApp/
       ├ amazon-freertos/
       └ application/
    ```

    > **Warning**
    > `amazon-freertos`のディレクトリ名は変更しないでください。
    ディレクトリ名を`amazon-freertos`以外にするとHarmonyを使用したときにパスの設定がくずれます。2023/5時点。

3. amazon-freertosのリポジトリで`mchpdev_20210700`のブランチに移動
   
    ```bash
    $ cd amazon-freertos
    amazon-freertos$ git checkout mchpdev_20210700
    ```


3. submoduleのアップデート

    ```bash
    amazon-freertos$ git submodule update --init --recursive
    ```

    実行するとfreertos_kernelやライブラリのファイルがダウンロードされます。


#### 9. FreeRTOS関連コードの修正

今回の組込みアプリの内容に合わせてFreeRTOSのソースコードを一部修正する必要があります。
以下の内容に従ってソースコードを修正します。
※見た目をすっきりさせるため、正確なdiff表現ではありません。

1. Wi-FiのSSID、PWの読込みの変更

    デフォルトだとソースコードにSSID、パスワードを埋め込まないと接続できませんが、本デモシステムはスマホからSSIDなどを指定するため、動的に変更できるようにしています。  
    _amazon-freertos/libraries/freertos_plus/standard/freertos_plus_tcp/portable/NetworkInterface/pic32mzw1_h3/NetworkInterface_wifi.c_
    ```diff
    #include "system_config.h"

    +#include "tasks/flash/include/flash_task.h"
    +#include "tasks/flash/include/flash_data.h"

    #ifdef PIC32_USE_RIO2_WIFI
    ```
    ```diff
    WIFINetworkParams_t xNetworkParams;

    -memcpy(xNetworkParams.ucSSID, clientcredentialWIFI_SSID, strlen( clientcredentialWIFI_SSID )) ;
    -xNetworkParams.ucSSIDLength = strlen( clientcredentialWIFI_SSID );
    -memcpy(xNetworkParams.xPassword.xWPA.cPassphrase, clientcredentialWIFI_PASSWORD, strlen( clientcredentialWIFI_PASSWORD )) ;
    -xNetworkParams.xPassword.xWPA.ucLength = strlen( clientcredentialWIFI_PASSWORD );
    -xNetworkParams.xSecurity = clientcredentialWIFI_SECURITY;
    -xNetworkParams.ucChannel = 0; /* Scan all channels (255) */

    +WiFiInfo_t xWiFiInfo;
    +
    +memset(&xWiFiInfo, 0x00, sizeof(xWiFiInfo));
    +
    +if (eReadFlashInfo(READ_FLASH_TYPE_WIFI_INFO, &xWiFiInfo, sizeof(xWiFiInfo)) != FLASH_TASK_RESULT_SUCCESS)
    +{
    +    return pdFAIL;
    +}
    +
    +memcpy(xNetworkParams.ucSSID, xWiFiInfo.cWifiSSID, strlen(xWiFiInfo.cWifiSSID));
    +xNetworkParams.ucSSIDLength = strlen(xWiFiInfo.cWifiSSID);
    +memcpy(xNetworkParams.xPassword.xWPA.cPassphrase, xWiFiInfo.cWiFiPassword, strlen(xWiFiInfo.cWiFiPassword));
    +xNetworkParams.xPassword.xWPA.ucLength = strlen(xWiFiInfo.cWiFiPassword);
    +xNetworkParams.xSecurity = xWiFiInfo.xWiFiSecurity;
    +xNetworkParams.ucChannel = 0; /* Scan all channels (255) */
    +
    /*Turn  WiFi ON */
    if (WIFI_On() != eWiFiSuccess)
    ```

1. デーモンタスク起動フック関数の有効化

    本デモシステムではフック関数内にアプリケーションの開始点があるため有効化します。  
    _amazon-freertos/vendors/microchip/boards/curiosity_pic32mzw1/aws_demos/config_files/FreeRTOSConfig.h_
    ```diff
    #define configTIMER_TASK_STACK_DEPTH               1024
    -#define configUSE_DAEMON_TASK_STARTUP_HOOK         0 
    +#define configUSE_DAEMON_TASK_STARTUP_HOOK         1 

    /* Misc */
    ```

1. uxTaskGetStackHighWaterMark関数の有効化

    メモリの使用量を確認したい場合は有効にします。  
    _amazon-freertos/vendors/microchip/boards/curiosity_pic32mzw1/aws_demos/config_files/FreeRTOSConfig.h_
    ```diff
    #define INCLUDE_xTaskGetCurrentTaskHandle       1 
    -#define INCLUDE_uxTaskGetStackHighWaterMark     0 
    +#define INCLUDE_uxTaskGetStackHighWaterMark     1 
    #define INCLUDE_xTaskGetIdleTaskHandle          0 
    ```

1. TCP/IP層の初期化関数実行と同時にWi-Fi接続しないように修正
    
    本デモシステムではスマホアプリで指示したときにWi-Fiに接続したいため、TCP/IP層の初期化時にWi-Fi接続しないように修正します。  
    _amazon-freertos/libraries/freertos_plus/standard/freertos_plus_tcp/FreeRTOS_IP.c_
    ```diff
    #include "FreeRTOS_DNS.h"

    +#include "common/include/network_operation.h"

    /* Used to ensure the structure packing is having the desired effect.  The
     * 'volatile' is used to prevent compiler warnings about comparing a constant with
     * a constant. */
    ```
    ```diff
    /* A possibility to set some additional task properties. */
    iptraceIP_TASK_STARTING();

    +#if 1
    +#if ipconfigUSE_DHCP == 0
    +vIPNetworkUpCalls();
    +#endif
    +#else
    /* Generate a dummy message to say that the network connection has gone
    *  down.  This will cause this task to initialise the network interface.  After
    *  this it is the responsibility of the network interface hardware driver to
    *  send this message if a previously connected network is disconnected. */
    FreeRTOS_NetworkDown();
    +#endif

    #if ( ipconfigUSE_TCP == 1 )
    ```
    ```diff
    if( xNetworkInterfaceInitialise() != pdPASS )
    {
        /* Ideally the network interface initialisation function will only
        * return when the network is available.  In case this is not the case,
        * wait a while before retrying the initialisation. */
        vTaskDelay( ipINITIALISATION_RETRY_DELAY );
    +#if 0
        FreeRTOS_NetworkDown();
    +#endif
    }
    ```

1. Wi-Fiへの接続指示に使用する構造体のサイズ変更
    
    本デモシステムではデフォルトのサイズのままだとメモリが枯渇し、ハードフォルトしてしまうためサイズを変更します。  
    _amazon-freertos/vendors/microchip/boards/curiosity_pic32mzw1/aws_demos/config_files/aws_wifi_config.h_
    ```diff
    #define _AWS_WIFI_CONFIG_H_

    -#define wificonfigMAX_WEPKEYS               (64)
    +#define wificonfigMAX_WEPKEYS               (1)
    #define wificonfigMAX_WEPKEY_LEN             (128)
    ```

1. FreeRTOSのヒープサイズ拡大とタスク作成時の最小タスクサイズを変更
    
    本デモシステムではデフォルトのヒープサイズでは不足するため増加させます。  
    _amazon-freertos/vendors/microchip/boards/curiosity_pic32mzw1/aws_demos/config_files/FreeRTOSConfig.h_
    ```diff
    #define configMAX_PRIORITIES                       (10UL)
    -#define configMINIMAL_STACK_SIZE                   (1024)   /* 512 for OTA demo */
    +#define configMINIMAL_STACK_SIZE                   (512)
    #define configISR_STACK_SIZE                       (512)
    #define configSUPPORT_DYNAMIC_ALLOCATION           1
    #define configSUPPORT_STATIC_ALLOCATION            1
    -#define configTOTAL_HEAP_SIZE                      ( ( size_t ) 180000)
    +#define configTOTAL_HEAP_SIZE                      ( ( size_t ) 160000)
    #define configMAX_TASK_NAME_LEN                    (16 )
    ```

1. DHCPのDiscover時にホスト名を付与しないようにコンフィグファイルを修正
    
    デフォルトではDHCPのホスト名が、_aws_clientcredential.h_ で定義されたAWSIoTのThingNameになるように設定されています。本デモシステムではThingNameはセキュアエレメントから取得したいため、_aws_clientcredential.h_ のThingNameは削除しています。ThingNameを削除するとDHCPのホスト名もなくなり、Wi-FiルーターによってはDHCP処理に失敗し、インターネット接続できなくなります。この事象を回避するためにホスト名を付与しない設定にします。  
    _amazon-freertos/vendors/microchip/boards/curiosity_pic32mzw1/aws_demos/config_files/FreeRTOSIPConfig.h_
    ```diff
    #define ipconfigUSE_DHCP                               1
    -#define ipconfigDHCP_REGISTER_HOSTNAME                 1
    +#define ipconfigDHCP_REGISTER_HOSTNAME                 0
    #define ipconfigDHCP_USES_UNICAST                      1 
    ```

1. ネットワークインターフェース初期関数の処理順番入れ替え

    デフォルトでは①Wi-Fiドライバの初期化→②Wi-Fiルーターへの接続→③FreeRTOS IPライブラリへMACアドレス登録の順に処理されるが、①→③→②の順番に変更します。
    本デモシステムではWi-Fiルーターの接続に失敗したときに再接続するロジックを補強していますが、Wi-Fiルーター接続後にMACアドレスを登録する処理ではリトライ時にもMACアドレスを登録しなくてはいけなくなり、処理が煩雑になるため、処理の順番を入れ替えます。  
    _amazon-freertos/libraries/freertos_plus/standard/freertos_plus_tcp/portable/NetworkInterface/pic32mzw1_h3/NetworkInterface_wifi.c_
    ```diff
    /* Connect to the AP */
    if( WIFI_ConnectAP( &xNetworkParams ) != eWiFiSuccess )
    {
        return pdFAIL;
    }
    -xResult = pdPASS;
    -
    -if ( eWiFiSuccess == WIFI_GetMAC( mac_addr ))
    -{
    -    printf("MAC Addr = 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\r\n", mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5] );
    -    FreeRTOS_UpdateMACAddress(mac_addr);
    -}
    -else
    -{
    -    printf("Fail to get mac address from wifi module...\r\n");
    -    xResult = pdFAIL;
    -}
    -
    -
    -PIC32_MAC_DbgPrint( "xNetworkInterfaceInitialise: %d %d\r\n", ( int ) xMacInitStatus, ( int ) xResult );
    +if (eWiFiSuccess == WIFI_GetMAC(mac_addr))
    +{
    +    printf("MAC Addr = 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\r\n", mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    +    FreeRTOS_UpdateMACAddress(mac_addr);
    +}
    +else
    +{
    +    printf("Fail to get mac address from wifi module...\r\n");
    +    xResult = pdFAIL;
    +}
    +
    +PIC32_MAC_DbgPrint("xNetworkInterfaceInitialise: %d %d\r\n", (int)xMacInitStatus, (int)xResult);
    +
    +/* Connect to the AP */
    +WIFIReturnCode_t xResult2 = WIFI_ConnectAP(&xNetworkParams);
    +if (xResult2 != eWiFiSuccess)
    +{
    +    configPRINTF(("WIFI_ConnectAP fail. Reason %d\r\n", xResult2));
    +    return pdFAIL;
    +}
    +xResult = pdPASS;
    return xResult;
    ```

1.  Wi-FiのWPA3対応

    本デモシステムではWPA3を使いたいため、ロジックを追加します。  
    _amazon-freertos/vendors/microchip/boards/curiosity_pic32mzw1/ports/wifi/iot_wifi.c_
    ```diff
    case eWiFiSecurityWPA:
    {
        if (WDRV_PIC32MZW_STATUS_OK != WDRV_PIC32MZW_AuthCtxSetPersonal(&g_wifiConfig.authCtx, pxNetworkParams->xPassword.xWPA.cPassphrase, pxNetworkParams->xPassword.xWPA.ucLength, WDRV_PIC32MZW_AUTH_TYPE_WPAWPA2_PERSONAL))
        {
            WDRV_DBG_ERROR_MESSAGE(("APP: Unable to set authentication to WPAWPA2 MIXED\r\n"));
            return eWiFiFailure;
        }
        break;
    }

    +case eWiFiSecurityWPA3:
    +{
    +    if (WDRV_PIC32MZW_STATUS_OK != WDRV_PIC32MZW_AuthCtxSetPersonal(&g_wifiConfig.authCtx, pxNetworkParams->xPassword.xWPA.cPassphrase, pxNetworkParams->xPassword.xWPA.ucLength, WDRV_PIC32MZW_AUTH_TYPE_WPA2WPA3_PERSONAL))
    +    {
    +        WDRV_DBG_ERROR_MESSAGE(("APP: Unable to set authentication to WPA2WPA3 MIXED\r\n"));
    +        return eWiFiFailure;
    +    }
    +    break;
    +}

    case eWiFiSecurityWEP:
    ```

1.  エントリーポイントの設定
    
    本デモシステムのアプリケーションを実行するためのエントリーポイントを追加します。  
    _amazon-freertos/projects/microchip/wfi32_iot_pic32mzw1_ecc/mplab/aws_demos/firmware/src/main.c_
    ```diff
    #include "FreeRTOS_Sockets.h"

    +#include "application_entry.h"

    #define mainLOGGING_TASK_STACK_SIZE         ( configMINIMAL_STACK_SIZE * 5 )
    ```
    ```diff
    /* Initialize all modules */
    SYS_Initialize ( NULL );
    -//printf("main log1\r\n");
    -vApplicationIPInit();
    -//printf("main log2\r\n");
    while ( true )
    {
    ```
    ```diff
        return ( EXIT_FAILURE );
    }

    +void vApplicationDaemonTaskStartupHook()
    +{
    +    APPLICATION_ENTRY_RunWakeupTask();
    +}

    void vApplicationIPNetworkEventHook( eIPCallbackEvent_t eNetworkEvent )
    {
    ```

1.  フック関数の設定
   
    Mallocエラーまたはスタックオーバーフローが発生した際にリセットを行うフック関数を設定します。
    本プロジェクトではデフォルトのデモプログラムを除去していますが、その影響でMallocエラーやスタックオーバーフローが発生した際に呼ばれるフック関数の設定がONになっているにも関わらず実体のない状態になっており、このままだとビルドに失敗します。設定をOFFにすることもできますが、便利な機能なので設定を生かす方向で修正します。  
    _amazon-freertos/projects/microchip/wfi32_iot_pic32mzw1_ecc/mplab/aws_demos/firmware/src/main.c_
    ```diff
    +#if 0
    void vApplicationIPNetworkEventHook( eIPCallbackEvent_t eNetworkEvent )
    {
        uint32_t ulIPAddress, ulNetMask, ulGatewayAddress, ulDNSServerAddress;
        char cBuffer[ 16 ];
        static BaseType_t xTasksAlreadyCreated = pdFALSE;

        ...

            FreeRTOS_inet_ntoa( ulDNSServerAddress, cBuffer );
            FreeRTOS_printf( ( "DNS Server Address: %s\r\n\r\n\r\n", cBuffer ) );
        }
    }
    +#endif
    ```
    ```diff
    +void vApplicationMallocFailedHook()
    +{
    +    SYS_RESET_SoftwareReset();
    +
    +    for (;;)
    +    {
    +    }
    +}
    +
    +void vApplicationStackOverflowHook(TaskHandle_t xTask,
    +                           char *pcTaskName)
    +{
    +    (void)xTask;
    +    (void)pcTaskName;
    +
    +    printf("Error #### Stack Size Over %s ###", pcTaskName);
    +
    +    volatile uint32_t xWaitCount = 100000;
    +    for (volatile uint32_t i = 0; i < xWaitCount; i++)
    +    {
    +    }
    +
    +    SYS_RESET_SoftwareReset();
    +
    +    for (;;)
    +    {
    +    }
    +}
    /*******************************************************************************
        End of File
    */
    ```

#### 10. プロジェクトの設定
ビルドできる状態にするためにいくつか設定します。

プロジェクトはapplication(aws_demos)とbootloader(ota_bootloader)の2つから構成されますが、最初にapplicationの設定をします。

1. MPLAB X IDEで`File` > `Open Project...`から _projects\microchip\wfi32_iot_pic32mzw1_ecc\mplab\aws_demos\firmware\aws_demos.X_ を開きます
    > **Warning**
    > このときプロジェクト名が太文字でなければプロジェクトが選択されていません。プロジェクト名を右クリックし`Set as Main Project`を選べば太文字になります。

2. 文字コードの設定
    日本語の文字化けを防止するため文字コードを変更します。  
    プロジェクトを右クリック`Properties` > `General` > `Encoding`を`UTF-8`に設定します。

3. Include pathの設定
    プロジェクトを右クリック`Properties` > `XC32(Global Options)` > `xc32-gcc` > `Option categories` > `Preprocessing and messages` > `Include directories`を選択し以下のパスを追加します。
    - _..\\..\\..\\..\\..\\..\\..\\..\\application\\application_code\\src_
    - _..\\..\\..\\..\\..\\..\\..\\..\\application\\application_code\\src\\common_

4. Projectにファイル追加
    プロジェクトの`Source Files`を右クリック`Add Existing Items from Folders...` > `Add Folder...`から以下の項目を追加します。
    - ファイル名: `application`
    - ファイルのタイプ: `すべてのファイル`

5. ペリフェラル(Harmony)の設定
    `Tools` > `Embedded` > `MPLAB® Harmony 3 Configurator`からHarmonyを開き、以下項目をそれぞれ追加、設定します。

    - **BSP**
        もともと`PIC32MZ W1 Curiosity BSP`が設定されているので正しい基板の`PIC32 MZ W1 WFI32IoT BSP`に置き換えます。変更しないとLEDなどが使えないです。
        ![Harmony BSP](/doc/image/setup/harmony_bsp.png)  
        *Harmony BSP*

    - **UART2**, **GPIO**
        本デモシステムでのBLEに使用するペリフェラルの設定をします。
        - UART2
            - Operating Mode: `Ring buffer mode`
            - Configure Ring Buffer Size-
                - RX Ring Buffer Size: `512`
            - Stop Selection bit: `1 Stop bit`
            - Parity and Data Selection bits: `8-bit data, no parity`
            - UARTx Module Enable bits: `UxTX and UxRX pins are enabled and used; ...`
            - UARTx Module Clock Selection bits: `BRG clock is PBCLK3`
            - Baud Rate: `115200`
        - GPIO
            |Pin ID|Custom Name|Function|Direction|Latch|
            |---|---|---|---|---|
            |RA13|BLE_RST    |GPIO|Out|High|
            |RB9 |BLE_UART_RX|U2RX|In |n/a |
            |RK7 |BLE_UART_TX|U2TX|n/a|n/a |

        ![Harmony UART2, GPIO](/doc/image/setup/harmony_uart2.png)  
        *Harmony UART2, GPIO*

    - **OCMP1**, **TMR3**, **GPIO**
        本デモシステムでのモーターに使用するペリフェラル(PWM)の設定をします。
        - TMR3
            - Select Prescaler: `1:64 prescale value`
            - Timer Period Unit: `millisecond`
            - Time: `20`
        - OCMP1
            - Select Output Compare Mode: `PWM mode on OCx; Fault pin disabled`
            - Select Timer Source: `Timer3`
            - Compare Value: `15625`
        - GPIO
            |Pin ID|Custom Name|Function|Direction|Latch|
            |---|---|---|---|---|
            |RB12|PWM|OC1|n/a|n/a|

        ![Harmony OCMP1, TMR3, GPIO](/doc/image/setup/harmony_pwm.png)  
        *Harmony OCMP1, TMR3, GPIO*

    - **ICAP3**, **TMR2**, **GPIO** 
        本デモシステムでのモーターに使用するセンサーの設定をします。
        - TMR2
            - Select Prescaler: `1:8 prescale value`
            - Timer Period Unit: `microsecond`
            - Time: `1100`
        - ICAP3
            - Select Input Capture Mode: `Simple Capture Event mode every edge`
            - Select Timer Source: `Timer2`
            - Select Timer Width: `16-bit timer resource capture`
            - Select First Capture Edge: `Capture rising edge first`
            - Enable Capture Interrupt: `✓`
        - GPIO
            |Pin ID|Custom Name|Function|Direction|Latch|
            |---|---|---|---|---|
            |RK5|-|IC3|In|n/a|

        ![Harmony ICAP3, TMR2, GPIO](/doc/image/setup/harmony_icap.png)  
        *Harmony ICAP3, TMR2, GPIO*

    以上の設定を行い`Generate Code`を実行します。  
    これでapplicationの設定は完了です。
    
次にbootloaderの設定をします。
1. MPLAB X IDEで`File` > `Open Project...`から _projects\microchip\wfi32_iot_pic32mzw1_ecc\mplab\ota_bootloader\firmware\ota_bootloader.X_ を開きます
1. ペリフェラル(Harmony)の設定
    **OCMP1**, **ICAP3**, **TMR2**, **TMR3**についてapplicationと同様の内容で設定、Generate Codeを実行します。

    > **Warning**
    > 起動時はbootloader->applicationの順に実行されます。先に実行されるbootloaderでPeripheral Module Disable(PMD)が働き、次に実行されるapplicationでbootloaderで設定されていないペリフェラルが設定できなくなります。これを回避するためにbootloaderで使用しなくてもapplicationと同じ設定を行います。

## 4. 動作確認

ここではソフトウェアが正しくインストールされているか確認するため、ビルド、デバイスへの書き込み、ログの確認を行います。

1. PCとWFI32-IoT BoardをUSBケーブルで接続します

1. TeraTermを起動し、シリアルポートを選択します
    COMポートが2つ認識されるので`シリアルデバイス(COMXX)`の方を選択してください。
    ※COM番号はCOMポートの使用状況によって異なります  
    ![TeraTerm](/doc/image/setup/teraterm.png)  
    *TeraTerm*
  
1. MPLAB X IDEで`File` > `Open Project...`から _projects\\microchip\\wfi32_iot_pic32mzw1_ecc\\mplab\\aws_demos\\firnware\\aws_demos.X_ を開きます

1. `Clean And Build Main Project`を押しクリーンビルドします

1. `Make And Program Device Main Project`を押しデバイスに書き込みます
    ![書き込み](/doc/image/setup/build_program.png)  

1. TeraTermに以下のようなログが表示されれば成功です

    ```log
    Bootloader start...

    Factory reset...
    Bootloader start...

    APP verify success...
    =======Boot Descriptor=======
    header signature :0x40 0x41 0x46 0x52
    ucImgFlags       :0xfc
    ulSequenceNum    :0x0
    slot             :0x0
    image size       :901154
    signature        :30 44 2 20 42
    0 883 [Tmr Svc] [DEBUG]Start APPLICATION_ENTRY_RunWakeupTask
    ...
    ```

## 7. おわりに

ここではスマートロックデモ開発におけるデバイスの環境構築について説明しました。
