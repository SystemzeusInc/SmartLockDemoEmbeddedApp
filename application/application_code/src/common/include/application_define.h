/**
 * @file application_define.h
 * @author Systemzeus Inc.
 * @copyright Copyright © 2021 Systemzeus Inc. All rights reserved.
 */
#ifndef APPLICATION_DEFINE_H /* APPLICATION_DEFINE_Hが定義されていないとき */
#define APPLICATION_DEFINE_H

#ifdef __cplusplus // Provide Cplusplus Compatibility

extern "C"
{
#endif /* end Provide Cplusplus Compatibility */

    // ######################################################################
    // ###   システムヘッダの取り込み
    // ######################################################################

#include "FreeRTOS.h"
#include "task.h"

// ######################################################################
// ###   ユーザ作成ヘッダの取り込み
// ######################################################################
#include "config/log_config.h"

    // ######################################################################
    // ###   #defineマクロ
    // ######################################################################

    // ######################################################################
    // ###   #define関数マクロ
    // ######################################################################

    /*
     * 通常ビルド時のみ出力されるログのマクロを定義
     */

/**
 * @brief レベルなしデバッグログ出力
 *
 * @param[in] format ログデータのフォーマット。printf形式
 * @param[in] ...    フォーマットに従った引数（可変長引数）
 *
 * @attention 通常ビルド時のみ出力される
 */
#define APP_PRINTF(format, ...) configPRINTF((format, ##__VA_ARGS__))

/**
 * @brief Debugレベルのデバッグログ出力
 *
 * @param[in] format ログデータのフォーマット。printf形式
 * @param[in] ...    フォーマットに従った引数（可変長引数）
 *
 * @attention 通常ビルド時のみ出力される
 */
#define APP_PRINTFDebug(format, ...) configPRINTF(("[\x1b[32mDEBUG\x1b[0m]" format, ##__VA_ARGS__))

/**
 * @brief Infoレベルのデバッグログ出力
 *
 * @param[in] format ログデータのフォーマット。printf形式
 * @param[in] ...    フォーマットに従った引数（可変長引数）
 *
 * @attention 通常ビルド時のみ出力される
 */
#define APP_PRINTFInfo(format, ...) configPRINTF(("[INFO ]" format, ##__VA_ARGS__))

/**
 * @brief Warnレベルのデバッグログ出力
 *
 * @param[in] format ログデータのフォーマット。printf形式
 * @param[in] ...    フォーマットに従った引数（可変長引数）
 *
 * @attention 通常ビルド時のみ出力される
 */
#define APP_PRINTFWarn(format, ...) configPRINTF(("[\x1b[33mWARN \x1b[0m]" format, ##__VA_ARGS__))

/**
 * @brief Errorレベルのデバッグログ出力
 *
 * @param[in] format ログデータのフォーマット。printf形式
 * @param[in] ...    フォーマットに従った引数（可変長引数）
 *
 * @attention 通常ビルド時のみ出力される
 */
#define APP_PRINTFError(format, ...) configPRINTF(("[\x1b[31mERROR\x1b[0m]" format, ##__VA_ARGS__))

/**
 * @brief Fatalレベルのデバッグログ出力
 *
 * @param[in] format ログデータのフォーマット。printf形式
 * @param[in] ...    フォーマットに従った引数（可変長引数）
 *
 * @attention 通常ビルド時のみ出力される
 */
#define APP_PRINTFFatal(format, ...) configPRINTF(("[\x1b[41mFATAL\x1b[0m]" format, ##__VA_ARGS__))

/**
 * @brief 割り込みハンドラ用デバッグログ出力
 *
 * @param[in] format ログデータのフォーマット。printf形式
 * @param[in] ...    フォーマットに従った引数（可変長引数）
 *
 * @attention 通常ビルド時のみ出力される。キューを介さず直接出力されるため、他のログに割り込んで出力される。
 */
#define APP_PRINTFError_FROM_ISR(format, ...) printf(("[\x1b[41mFATAL\x1b[0m]" format "\r\n", ##__VA_ARGS__))

/**
 * @brief ヒープ内の空きバイト数を出力
 *
 * @attention 通常ビルドかつInfoレベル以上のログを出力する設定のときのみ出力される
 */
#define PRINT_FREE_HEAP_SIZE() APP_PRINTF("Free Heap Size(byte): %d", xPortGetFreeHeapSize());

    /*
     * 処理の開始ポイントと終了ポイントを出力するためのマクロを定義
     */

#if ((PRINT_TASK_MEASUREMENT_POINT == 1))

/**
 * @brief 処理の開始ログを出力
 *
 * @param[in] processName 処理名
 *
 * @attention 通常ビルドかつ PRINT_TASK_MEASUREMENT_POINT が1のときのみ出力される。
 * @note 仕様書の通信シーケンスとログの対応を分かりやすくするために用いる。
 */
#    define PRINT_PROCESS_MEASUREMENT_POINT_START(processName) APP_PRINTF("> Start Process: %s", processName)

/**
 * @brief 処理の終了ログを出力
 *
 * @param[in] processName 処理名
 * @attention 通常ビルドかつ PRINT_TASK_MEASUREMENT_POINT が1のときのみ出力される。
 * @note 仕様書の通信シーケンスとログの対応を分かりやすくするために用いる。
 */
#    define PRINT_PROCESS_MEASUREMENT_POINT_END(processName) APP_PRINTF("< End Process: %s", processName)

/**
 * @brief タスクの開始ログを出力
 *
 * @attention 通常ビルドかつ PRINT_TASK_MEASUREMENT_POINT が1のときのみ出力される。
 * @note 仕様書の通信シーケンスとログの対応を分かりやすくするために用いる。
 */
#    define PRINT_TASK_MEASUREMENT_POINT_START() APP_PRINTF("> Start Task")

/**
 * @brief タスクの終了ログを出力
 *
 * @attention 通常ビルドかつ PRINT_TASK_MEASUREMENT_POINT が1のときのみ出力される。
 * @note 仕様書の通信シーケンスとログの対応を分かりやすくするために用いる。
 */
#    define PRINT_TASK_MEASUREMENT_POINT_END() APP_PRINTF("< End Task")

#else /* not ((PRINT_TASK_MEASUREMENT_POINT == 1) */

/**
 * @brief 処理の開始ログを出力
 */
#    define PRINT_PROCESS_MEASUREMENT_POINT_START(processName) (void)0

/**
 * @brief 処理の終了ログを出力
 */
#    define PRINT_PROCESS_MEASUREMENT_POINT_END(processName)   (void)0

/**
 * @brief タスクの開始ログを出力
 */
#    define PRINT_TASK_MEASUREMENT_POINT_START()               (void)0

/**
 * @brief タスクの終了ログを出力
 */
#    define PRINT_TASK_MEASUREMENT_POINT_END()                 (void)0

#endif /* end ((PRINT_TASK_MEASUREMENT_POINT == 1) */

    /*
     * スタックメモリの空きバイト数を出力するマクロを定義
     */

#if ((PRINT_TASK_REMAINING_STACK_SIZE_CONFIG == 1))

/**
 * @brief スタックメモリの空きバイト数を出力
 *
 * @attention 通常ビルドかつ PRINT_TASK_REMAINING_STACK_SIZE_CONFIG が1のときのみ出力される。
 */
#    define PRINT_TASK_REMAINING_STACK_SIZE() APP_PRINTF("Remaining Stack Size(word): %d", uxTaskGetStackHighWaterMark(NULL))

#else

/**
 * @brief スタックメモリの空きバイト数を出力
 */
#    define PRINT_TASK_REMAINING_STACK_SIZE() (void)0

#endif

/*
 * malloc関数をラップしたマクロを定義
 */

/**
 * @brief malloc関数をラップしたマクロ
 * @details
 * 通常ビルドでは pvPortMalloc を呼び出す。単体テストビルドでは xMallocDummy を呼び出す。
 *
 * @param[in] xSize 確保するメモリのバイト数
 *
 * @sa heap_4.c
 * @sa pvPortMalloc
 * @sa malloc_dummy.h
 * @sa xMallocDummy
 */
#define xMalloc(xSize) (pvPortMalloc(xSize))

/**
 * @brief malloc関数のアドレス
 * @details
 * 通常ビルドでは pvPortMalloc のアドレスを返す。単体テストビルドでは xMallocDummy のアドレスを返す。
 *
 * @sa heap_4.c
 * @sa pvPortMalloc
 * @sa malloc_dummy.h
 * @sa xMallocDummy
 */
#define xMallocPointer (pvPortMalloc)

    /*
     * free関数をラップしたマクロを定義
     */

/**
 * @brief malloc関数をラップしたマクロ
 * @details
 * 通常ビルドでは vPortFree を呼び出す。単体テストビルドでは vFreeDummy を呼び出す。
 *
 * @param[in] pvTarget 開放するメモリのアドレス
 *
 * @sa heap_4.c
 * @sa vPortFree
 * @sa malloc_dummy.h
 * @sa vFreeDummy
 */
#define vFree(pvTarget) (vPortFree(pvTarget))

/**
 * @brief free関数のアドレス
 * @details
 * 通常ビルドでは vPortFree のアドレスを返す。単体テストビルドでは vFreeDummy のアドレスを返す。
 *
 * @sa heap_4.c
 * @sa vPortFree
 * @sa malloc_dummy.h
 * @sa vFreeDummy
 */
#define vFreePointer (vPortFree)

    // ######################################################################
    // ###   typedef定義
    // ######################################################################

    // ######################################################################
    // ###   enumタグ定義（typedefを同時に行う）
    // ######################################################################

    // ######################################################################
    // ###   struct/unionタグ定義（typedefを同時に行う）
    // ######################################################################

    // ######################################################################
    // ###   extern変数宣言
    // ######################################################################

    // ######################################################################
    // ###   関数プロトタイプ宣言
    // ######################################################################

    // ######################################################################
    // ###   インライン関数
    // ######################################################################

#ifdef __cplusplus
}
#endif

#endif /* end APPLICATION_DEFINE_H */
