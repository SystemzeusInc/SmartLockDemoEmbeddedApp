/**
 * @file adc_sensor_config.h
 * @author Systemzeus Inc.
 * @copyright Copyright © 2023 Systemzeus Inc. All rights reserved.
 */
#ifndef ADC_SENSOR_CONFIG_H_
#define ADC_SENSOR_CONFIG_H_

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
#define WAIT_TO_CONVERT_INTERVAL 1 /**< 変換完了を待機する間隔 ms */


#ifdef __cplusplus
}
#endif

#endif /* end ADC_SENSOR_CONFIG_H_ */
