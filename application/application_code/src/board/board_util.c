/**
 * @file board_util.c
 * @author Systemzeus Inc.
 * @copyright Copyright © 2023 Systemzeus Inc. All rights reserved.
 */

// --------------------------------------------------
// システムヘッダの取り込み
// --------------------------------------------------
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "definitions.h"
#include "FreeRTOS.h"

// --------------------------------------------------
// ユーザ作成ヘッダの取り込み
// --------------------------------------------------
#include "include/board_util.h"

// --------------------------------------------------
// 自ファイル内でのみ使用する#defineマクロ
// --------------------------------------------------

// --------------------------------------------------
// 自ファイル内でのみ使用する#define関数マクロ
// --------------------------------------------------

// --------------------------------------------------
// 自ファイル内でのみ使用するtypedef定義
// --------------------------------------------------
typedef void (*LED_On)(void);
typedef void (*LED_Off)(void);
typedef bool (*LED_Get)(void);

// --------------------------------------------------
// 自ファイル内でのみ使用するenumタグ定義（typedefを同時に行う）
// --------------------------------------------------

// --------------------------------------------------
// 自ファイル内でのみ使用するstruct/unionタグ定義（typedefを同時に行う）
// --------------------------------------------------
typedef struct
{
    LED_On on;
    LED_Off off;
    LED_Get get;
} LEDControl_t;

// --------------------------------------------------
// ファイル内で共有するstatic変数宣言
// --------------------------------------------------

// --------------------------------------------------
// static関数プロトタイプ宣言
// --------------------------------------------------

// --------------------------------------------------
// 変数定義（staticを除く）
// --------------------------------------------------
static void vprvSetLEDOnOffFunc(const LEDColor_t eColor, LEDControl_t *pxLEDControl);

static void vprvRedOn();
static void vprvRedOff();
static bool bprvRedGet();
static void vprvYellowOn();
static void vprvYellowOff();
static bool bprvYellowGet();
static void vprvGreenOn();
static void vprvGreenOff();
static bool bprvGreenGet();
static void vprvBlueOn();
static void vprvBlueOff();
static bool bprvBlueGet();

// --------------------------------------------------
// 関数定義（staticを除く）
// --------------------------------------------------
void vFlashRainbow(void)
{
    vLEDAllOff();

    vprvRedOn();
    vTaskDelay(30);
    vprvRedOff();
    vprvYellowOn();
    vTaskDelay(30);
    vprvYellowOff();
    vprvGreenOn();
    vTaskDelay(30);
    vprvGreenOff();
    vprvBlueOn();
    vTaskDelay(30);
    vprvBlueOff();
}

void vLEDOn(const LEDColor_t eColor)
{
    LEDControl_t xLEDControl = {0};
    vprvSetLEDOnOffFunc(eColor, &xLEDControl);

    xLEDControl.on();
}

void vLEDOff(const LEDColor_t eColor)
{
    LEDControl_t xLEDControl = {0};
    vprvSetLEDOnOffFunc(eColor, &xLEDControl);

    xLEDControl.off();
}

void vLEDOnCertainPeriodTime(const LEDColor_t eColor, const uint32_t ulPeriod)
{
    LEDControl_t xLEDControl = {0};
    vprvSetLEDOnOffFunc(eColor, &xLEDControl);

    xLEDControl.on();
    vTaskDelay(ulPeriod);
    xLEDControl.off();
}

void vFlashLED(const LEDColor_t eColor, const uint8_t uxNum, const uint32_t ulInterval)
{
    LEDControl_t xLEDControl = {0};
    vprvSetLEDOnOffFunc(eColor, &xLEDControl);

    bool bState = xLEDControl.get(); // 現在点灯してるか確認

    for (uint8_t i = 0; i < uxNum; i++)
    {
        xLEDControl.on();
        vTaskDelay(ulInterval);
        xLEDControl.off();
        vTaskDelay(ulInterval);
    }

    if (bState) // 前に点灯していればもとに戻す
    {
        xLEDControl.on();
    }
}

void vLEDAllOff(void)
{
    vprvRedOff();
    vprvYellowOff();
    vprvGreenOff();
    vprvBlueOff();
}

// --------------------------------------------------
// static関数定義
// --------------------------------------------------
static void vprvSetLEDOnOffFunc(const LEDColor_t eColor, LEDControl_t *pxLEDControl)
{
    switch (eColor)
    {
    case RED:
        pxLEDControl->on = vprvRedOn;
        pxLEDControl->off = vprvRedOff;
        pxLEDControl->get = bprvRedGet;
        break;
    case YELLOW:
        pxLEDControl->on = vprvYellowOn;
        pxLEDControl->off = vprvYellowOff;
        pxLEDControl->get = bprvYellowGet;
        break;
    case GREEN:
        pxLEDControl->on = vprvGreenOn;
        pxLEDControl->off = vprvGreenOff;
        pxLEDControl->get = bprvGreenGet;
        break;
    case BLUE:
        pxLEDControl->on = vprvBlueOn;
        pxLEDControl->off = vprvBlueOff;
        pxLEDControl->get = bprvBlueGet;
        break;
    default:
        break;
    }
}

static void vprvRedOn()
{
    LED_RED_On();
}

static void vprvRedOff()
{
    LED_RED_Off();
}

static bool bprvRedGet()
{
    return !LED_RED_Get();
}

static void vprvYellowOn()
{
    LED_YELLOW_On();
}

static void vprvYellowOff()
{
    LED_YELLOW_Off();
}

static bool bprvYellowGet()
{
    return !LED_YELLOW_Get();
}

static void vprvGreenOn()
{
    LED_GREEN_On();
}

static void vprvGreenOff()
{
    LED_GREEN_Off();
}

static bool bprvGreenGet()
{
    return !LED_GREEN_Get();
}

static void vprvBlueOn()
{
    LED_BLUE_On();
}

static void vprvBlueOff()
{
    LED_BLUE_Off();
}

static bool bprvBlueGet()
{
    return !LED_BLUE_Get();
}

// --------------------------------------------------
// Unit Test用関数定義(関数のプロトタイプ宣言は「自身のファイル名+ "_test.h"」で宣言されていること)
// --------------------------------------------------

