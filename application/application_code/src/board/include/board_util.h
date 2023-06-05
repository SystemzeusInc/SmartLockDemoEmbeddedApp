/**
 * @file board_util.h
 * @author Systemzeus Inc.
 * @copyright Copyright © 2023 Systemzeus Inc. All rights reserved.
 */
#ifndef BOARD_UTIL_H_
#define BOARD_UTIL_H_

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

#include "definitions.h"
#include "FreeRTOS.h"

// --------------------------------------------------
// ユーザ作成ヘッダの取り込み
// --------------------------------------------------

// --------------------------------------------------
// #defineマクロ
// --------------------------------------------------

// --------------------------------------------------
// #define関数マクロ
// --------------------------------------------------

// --------------------------------------------------
// typedef定義
// --------------------------------------------------

// --------------------------------------------------
// enumタグ定義（typedefを同時に行う）
// --------------------------------------------------
/**
 * @brief LEDの色
 */
typedef enum
{
    RED = 0x1,
    YELLOW,
    GREEN,
    BLUE
} LEDColor_t;

// --------------------------------------------------
// struct/unionタグ定義（typedefを同時に行う）
// --------------------------------------------------

// --------------------------------------------------
// extern変数宣言
// --------------------------------------------------

// --------------------------------------------------
// 関数プロトタイプ宣言
// --------------------------------------------------
void vFlashRainbow(void);

/**
 * @brief LED点灯
 * 
 * @param [in] eColor LEDの色
 */
void vLEDOn(const LEDColor_t eColor);

/**
 * @brief LED消灯
 * 
 * @param [in] eColor LEDの色 
 */
void vLEDOff(const LEDColor_t eColor);

/**
 * @brief 一定期間LEDを点灯
 * 
 * @param [in] eColor LEDの色
 * @param [in] ulPeriod 点灯する期間 ms
 */
void vLEDOnCertainPeriodTime(const LEDColor_t eColor, const uint32_t ulPeriod);

/**
 * @brief LEDを点滅
 * 
 * @param [in] eColor　LEDの色 
 * @param [in] uxNum 点灯回数
 * @param [in] ulInterval 点灯間隔
 */
void vFlashLED(const LEDColor_t eColor, const uint8_t uxNum, const uint32_t ulInterval);

/**
 * @brief LEDをすべて消灯
 */
void vLEDAllOff(void);

// --------------------------------------------------
// インライン関数
// --------------------------------------------------

#ifdef __cplusplus
}
#endif

#endif /* end BOARD_UTIL_H_ */
