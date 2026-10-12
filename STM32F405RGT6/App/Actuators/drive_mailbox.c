/**
 * @file drive_mailbox.c
 * @brief 最新输出目标与停止锁存的纯状态模块。
 *
 * 普通目标会覆盖旧目标，停止使用独立 stop_pending 和 revision 保留。
 * 纯 C 函数不自己加锁；调用方负责临界区和发送生命周期。
 */

#include "drive_mailbox.h"
/**
 * @brief 保存最新百分比目标；
 * 从非零目标转为全零时递增停止版本并锁存，随后非零提交不清此锁存。
 */
void DriveMailbox_Submit(DriveMailbox *b, int8_t move, int8_t steer)
{
    if (!move && !steer && (b->move || b->steer))
    {
        b->stop_pending = 1;
        b->stop_revision++;
    }
    b->move = move;
    b->steer = steer;
}
/**
 * @brief 新增故障位时生成一次停止版本；
 * 重复同位故障不重复增版本，清故障不表示停止帧已经完成。
 */
void DriveMailbox_Fault(DriveMailbox *b, uint8_t fault, uint8_t enabled)
{
    if (enabled)
    {
        if (!(b->faults & fault))
        {
            b->stop_pending = 1;
            b->stop_revision++;
        }
        b->faults |= fault;
    }
    else
        b->faults &= (uint8_t)~fault;
}
/**
 * @brief 有任一故障或待停止时选择零油门/转向，否则选择最新目标；
 * 不改变邮箱状态。
 */
void DriveMailbox_Select(const DriveMailbox *b, int8_t *move, int8_t *steer)
{
    *move = (b->faults || b->stop_pending) ? 0 : b->move;
    *steer = (b->faults || b->stop_pending) ? 0 : b->steer;
}
/**
 * @brief 仅发送前快照的 revision 仍为当前版本时清 stop_pending，避免并发新停止被旧帧完成吞掉。
 */
void DriveMailbox_CompleteStop(DriveMailbox *b, uint32_t revision)
{
    /* 版本检查避免这个竞态：开始发零帧→新停止出现→旧零帧完成把新停止清掉。 */
    if (revision == b->stop_revision)
        b->stop_pending = 0;
}
/**
 * @brief 编码 9 字节驱动帧，0x55 起始、0xAA 结束，百分比以 int8_t 二补码转换字节保存；
 * 协议不在此新增 CRC。
 */
void DriveMailbox_Frame(uint8_t frame[9], int8_t move, int8_t steer)
{
    frame[0] = 0x55;
    frame[1] = frame[2] = frame[3] = 0;
    frame[4] = (uint8_t)move;
    frame[5] = (uint8_t)steer;
    frame[6] = frame[7] = 0;
    frame[8] = 0xAA;
}
