/**
 * @file log_config.h
 * @author Systemzeus Inc.
 * @copyright Copyright © 2021 Systemzeus Inc. All rights reserved.
 */
#ifndef LOG_CONFIG_H /* LOG_CONFIG_Hが定義されていないとき */
#define LOG_CONFIG_H

#ifdef __cplusplus // Provide Cplusplus Compatibility

extern "C"
{
#endif /* end Provide Cplusplus Compatibility */

// ######################################################################
// ###   システムヘッダの取り込み
// ######################################################################

// ######################################################################
// ###   ユーザ作成ヘッダの取り込み
// ######################################################################

// ######################################################################
// ###   #defineマクロ
// ######################################################################
/**
 * @brief タスクのスタックサイズサイズを表示する
 */
#define PRINT_TASK_REMAINING_STACK_SIZE_CONFIG (0)

/**
 * @brief シーケンス単位で見たタスクの突入ポイントと終了ポイントを表示する
 *
 * 例えば、デバイス登録シーケンスが開始されたタイミングで、「> Start Process: "DeviceRegister"」が表示される。
 *
 */
#define PRINT_TASK_MEASUREMENT_POINT (0)

/**
 * @brief ヒープサイズを表示するタスクを作成するか
 *
 */
#define CREATE_PRINT_REMAINING_HEAP_SIZE_TASK (0)

/**
 * @brief 残ヒープサイズ表示タスクの優先度
 *
 */
#define PRINT_REMAINING_HEAP_SIZE_TASK_PRIORITY (0)

/**
 * @brief 残ヒープサイズの表示間隔
 *
 */
#define HEAP_SIZE_DISPLAY_INTERVAL_MS (1000)

/**
 * @brief ログ出力を有効にする。
 * 値が1の場合ログを出力する。値が0の場合ログを出力しない。
 */
#define ENABLE_CONSOLE_OUTPUT (1)

#ifdef __cplusplus
}
#endif

#endif /* end LOG_CONFIG_H */
