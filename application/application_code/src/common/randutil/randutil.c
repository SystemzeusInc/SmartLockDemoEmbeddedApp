/**
 * @file randutil.c
 * @author Systemzeus Inc.
 * @copyright Copyright © 2023 Systemzeus Inc. All rights reserved.
 */

// --------------------------------------------------
// システムヘッダの取り込み
// --------------------------------------------------
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "definitions.h"
#include "FreeRTOS.h"
#include "atca_basic.h"

// --------------------------------------------------
// ユーザ作成ヘッダの取り込み
// --------------------------------------------------
#include "include/randutil.h"

// --------------------------------------------------
// 自ファイル内でのみ使用する#defineマクロ
// --------------------------------------------------
#define USE_ECC608_RANDOM /**< ECC608を使用した乱数生成 */

/* -------------------------------------------------- */

#define ECC608_GENERATE_RANDOM_BYTE 32 /**< ECC608が生成する乱数のバイト数 */

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

// --------------------------------------------------
// ファイル内で共有するstatic変数宣言
// --------------------------------------------------

// --------------------------------------------------
// static関数プロトタイプ宣言
// --------------------------------------------------

// --------------------------------------------------
// 変数定義（staticを除く）
// --------------------------------------------------

// --------------------------------------------------
// 関数定義（staticを除く）
// --------------------------------------------------
size_t xGetRandomBytes(uint8_t *puxBuf, size_t xBytes)
{
#ifdef USE_ECC608_RANDOM
    // Initialize atcab
    ATCADevice xDevice = atcab_get_device();
#    if 0
    if (xDevice != NULL)
        atcab_release();
#    endif
    if (xDevice == NULL)
    {
        ATCAIfaceCfg *ifacecfg = &atecc608_0_init_data;
        atcab_init(ifacecfg);
    }

    uint8_t uxRandomValue[ECC608_GENERATE_RANDOM_BYTE] = {0};
    uint32_t i = 0;
    while (1)
    {
        if (i % ECC608_GENERATE_RANDOM_BYTE == 0)
        {
            if (atcab_random(uxRandomValue) != ATCA_SUCCESS)
                break;
        }
        puxBuf[i] = uxRandomValue[i % ECC608_GENERATE_RANDOM_BYTE];
        i++;
        if (i > xBytes)
            break;
    }
    return i;
#else
    for (int i = 0; i < xBytes; i++)
    {
        puxBuf[i] = rand() % 256;
    }
    return xBytes;
#endif
}

// --------------------------------------------------
// static関数定義
// --------------------------------------------------

// --------------------------------------------------
// Unit Test用関数定義(関数のプロトタイプ宣言は「自身のファイル名+ "_test.h"」で宣言されていること)
// --------------------------------------------------
#if (BUILD_MODE_TEST == 1) /* BUILD_MODE_TESTが定義されているとき */
#endif                     /* end  BUILD_MODE_TEST */