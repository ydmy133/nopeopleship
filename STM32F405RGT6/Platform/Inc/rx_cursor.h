/**
 * @file rx_cursor.h
 * @brief 循环接收位置到累计生产计数的转换。
 *
 * position 是环内位置，produced/consumed 为无符号累计字节计数。
 * HT/TC/IDLE 重复位置不增加计数；必须及时处理半满/全满事件，单凭同一位置无法识别错过的完整一圈。
 */

#ifndef RX_CURSOR_H
#define RX_CURSOR_H
#include <stdint.h>
typedef struct
{
    uint32_t produced, consumed; /**< 累计生产/消费字节数，未读量用无符号差。 */
    uint16_t position; /**< 上次处理的环位置，范围 0..capacity-1。 */
} RxCursor;
/**
 * @brief capacity 为环大小，pos 等于 capacity 表示回到零。
 * 按模差累计新增字节，重复通知同位置的增量为零，调用方保证及时处理环事件。
 */
void RxCursor_Advance(RxCursor *cursor, uint16_t position, uint16_t capacity);
/**
 * @brief 未消费字节达到容量即报告溢出，保守拒绝覆盖风险；
 * 无符号差值兼容累计计数回绕。
 */
uint8_t RxCursor_Overrun(const RxCursor *cursor, uint16_t capacity);
#endif
