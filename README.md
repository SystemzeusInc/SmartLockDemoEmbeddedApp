# スマートロックデモ組込みアプリ

WFI32-IoT Boardを用いたスマートロックデバイスのデモ組込みアプリです。スマホアプリからプロビジョニング、開施錠をすることができます。

## デモ

![実物](/doc/image/actual_item.png)  
*実物* 

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
image size       :887890
signature        :30 46 2 21 0
0 894 [Tmr Svc] [DEBUG]Start APPLICATION_ENTRY_RunWakeupTask
1 895 [WakeupTask] [DEBUG]APP VERSION :1.0.0
2 895 [WakeupTask] [DEBUG]Flash Task Init started.
...
```
*起動直後のログ*

## 必須ライブラリ

### ハードウェア

|No.|デバイス名|メーカー|型番・品名など|説明|
|---|---|---|---|---|
| 1 | WFI32-IoT Board | Microchip | [WFI32-IoT Development Board](https://www.microchip.com/en-us/development-tool/ev36w50a) | Microchip製のIoT開発用ボード。MCUはWFI32E01PC(PIC32MZW1)。Wi-Fiモジュールとセキュアエレメント搭載型。 |
| 2 | BLE             | MIKROE    | [RN4870 CLICK](https://www.mikroe.com/rn4870-click)    | アプリとの通信に用いる。モジュールはMicrochip製RN4870。 |
| 3 | サーボモーター  | Parallax  | [Parallax Feedback 360° High-Speed Servo](https://www.switch-science.com/products/3598) | 鍵を回すのに用いるモーター |

### ソフトウェア

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

## インストール方法

```bash
$ git clone https://github.com/SystemzeusInc/SmartLockDemoEmbeddedApp
```

> **Warning**
> プロジェクトの階層が深く、ビルド時に[WindowsのPATHの長さ制限](https://learn.microsoft.com/ja-jp/windows/win32/fileio/maximum-file-path-limitation?tabs=registry)に引っかかる可能性があるため、_C:\\_ 直下に配置することを推奨します。



## セットアップ方法

### ハードウェア

以下配線図を参照してセットアップしてください。

![配線図](/doc/image/wiring_diagram.png)
*配線図*

### ソフトウェア

以下コマンドを実行してamazon-freertosをクローンします。

```bash
$ cd SmartLockDemoEmbeddedApp
SmartLockDemoEmbeddedApp$ git clone https://github.com/MicrochipTech/amazon-freertos.git
SmartLockDemoEmbeddedApp/amazon-freertos$ git checkout mchpdev_20210700
SmartLockDemoEmbeddedApp/amazon-freertos$ git submodule update --init --recursive
```

詳しくは TODO:zennのurl を参照してください。

<!-- ## 使用方法 -->

## License

This projects is licensed under the MIT License, see the LICENSE.txt file for details.