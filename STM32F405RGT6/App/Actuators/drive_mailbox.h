/**
 * @file drive_mailbox.h
 * @brief 最新输出目标与停止锁存的纯状态模块。
 *
 * 普通目标会覆盖旧目标，停止使用独立 stop_pending 和 revision 保留。
 * 纯 C 函数不自己加锁；调用方负责临界区和发送生命周期。
 */

#ifndef DRIVE_MAILBOX_H
#define DRIVE_MAILBOX_H
#include <stdint.h>
typedef struct
{
    int8_t move, steer; /**< 最新百分比目标，左转为正。 */
    uint8_t faults, stop_pending; /**< 独立故障位集合 / 待确认零帧，不被新移动覆盖。 */
    uint32_t stop_revision; /**< 新停止递增，完成旧版本不能清除新停止。 */
} DriveMailbox;
/**
 * @brief 保存最新百分比目标；
 * 从非零目标转为全零时递增停止版本并锁存，随后非零提交不清此锁存。
 */
void DriveMailbox_Submit(DriveMailbox *box, int8_t move, int8_t steer);
/**
 * @brief 新增故障位时生成一次停止版本；
 * 重复同位故障不重复增版本，清故障不表示停止帧已经完成。
 */
void DriveMailbox_Fault(DriveMailbox *box, uint8_t fault, uint8_t enabled);
/**
 * @brief 有任一故障或待停止时选择零油门/转向，否则选择最新目标；
 * 不改变邮箱状态。
 */
void DriveMailbox_Select(const DriveMailbox *box, int8_t *move, int8_t *steer);
/**
 * @brief 仅发送前快照的 revision 仍为当前版本时清 stop_pending，避免并发新停止被旧帧完成吞掉。
 */
void DriveMailbox_CompleteStop(DriveMailbox *box, uint32_t revision);
/**
 * @brief 编码 9 字节驱动帧，0x55 起始、0xAA 结束，百分比以 int8_t 二补码转换字节保存；
 * 协议不在此新增 CRC。
 */
void DriveMailbox_Frame(uint8_t frame[9], int8_t move, int8_t steer);
#endif
