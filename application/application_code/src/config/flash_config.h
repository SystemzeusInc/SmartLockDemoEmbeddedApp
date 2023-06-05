/**
 * @file flash_config.h
 * @author Systemzeus Inc.
 * @copyright Copyright © 2023 Systemzeus Inc. All rights reserved.
 */
#ifndef FLASH_CONFIG_H_
#define FLASH_CONFIG_H_

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

// --------------------------------------------------
// #defineマクロ
// --------------------------------------------------
/**
 * @brief Wi-Fi SSIDの最大長
 */
#define WIFI_SSID_MAX_LENGTH (wificonfigMAX_SSID_LEN)

/**
 * @brief Wi-Ff パスワードの最大長
 */
#define WIFI_PASSWORD_MAX_LENGTH (wificonfigMAX_PASSPHRASE_LEN)

/**
 * @brief AWS IoT Endpointの最大長
 */
#define AWS_IOT_ENDPOINT_MAX_LENGTH (128U)

/**
 * @brief 工場出荷ThingNameの長さ（固定長）
 */
#define FACTORY_THING_NAME_LENGTH (18U)

/**
 * @brief 普段使い用のThingNameの長さ（固定長）
 *
 * @note
 * UUIDv4 36文字
 * 例) a0eebc99-9c0b-4ef8-bb6d-6bb9bd380a11
 */
#define THING_NAME_LENGTH (36U)

/**
 * @brief 本APIからレスポンスを得るために必要なタイムアウト時間
 * @details
 * 5秒もあれば十分だと思われる。もし頻繁にタイムアウトするような場合はさらにタイムアウト時間を伸ばす。
 */
#define FLASH_RESPONSE_TIMEOUT_MS (5U * 1000U)

#ifdef __cplusplus
}
#endif

#endif /* end FLASH_CONFIG_H_ */
