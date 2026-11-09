#ifndef __DHT11_H__
#define __DHT11_H__

#define DHT11_OK            0U   /* 读成功 */
#define DHT11_ERR_NO_RESP   1U   /* 没应答：接线 / 上拉 / 供电 */
#define DHT11_ERR_TIMEOUT   2U   /* 位读取超时 */
#define DHT11_ERR_CRC       3U   /* 校验和错误 */

#include "main.h"


void DHT11_Init(void);
uint8_t DHT11_GetData(uint8_t *temp_int, uint8_t *humi_int, uint8_t *temp_dec, uint8_t *humi_dec);
void DHT11_Request(void);
#endif
