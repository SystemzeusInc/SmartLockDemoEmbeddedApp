/**
 * @file ota_app_config.h
 * @author Systemzeus Inc.
 * @copyright Copyright © 2023 Systemzeus Inc. All rights reserved.
 *
 * @note ota_config.h はOTAライブラリが読み込むファイルとして別に存在するためファイル名が重複しないようにしている
 */
#ifndef OTA_APP_CONFIG_H_
#define OTA_APP_CONFIG_H_

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

// --------------------------------------------------
// ユーザ作成ヘッダの取り込み
// --------------------------------------------------
#include "common/include/application_define.h"

// --------------------------------------------------
// #defineマクロ
// --------------------------------------------------
/**
 * @brief ジョブ作成時の「デバイス上のファイルのパス名」「デバイスのコード署名証明書のパス名」の最大バイト数
 */
#define OTA_APP_MAX_FILE_PATH_SIZE (260U)

/**
 * @brief ストリーム名の最大バイト数
 *
 * @see https://docs.aws.amazon.com/ja_jp/freertos/latest/userguide/ota-cli-workflow.html#ota-stream
 */
#define OTA_APP_MAX_STREAM_NAME_SIZE (128U)

/**
 * @brief OTAAgentTaskのシャットダウンを待つポーリングの間隔
 */
#define OTA_APP_SHUTDOWN_WAIT_DELAY_MS (100U)

/**
 * @brief OTAAgentTaskのシャットダウンを待つ最大時間
 */
#define OTA_APP_SHUTDOWN_TIMEOUT_MS    (10000U)

    // --------------------------------------------------
    // #define関数マクロ
    // --------------------------------------------------

    // --------------------------------------------------
    // typedef定義
    // --------------------------------------------------

    // --------------------------------------------------
    // enumタグ定義（typedefを同時に行う）
    // --------------------------------------------------

    // --------------------------------------------------
    // struct/unionタグ定義（typedefを同時に行う）
    // --------------------------------------------------

    // --------------------------------------------------
    // extern変数宣言
    // --------------------------------------------------

    // --------------------------------------------------
    // 関数プロトタイプ宣言
    // --------------------------------------------------

    // --------------------------------------------------
    // インライン関数
    // --------------------------------------------------

#ifdef __cplusplus
}
#endif

#endif /* end OTA_APP_CONFIG_H_ */
