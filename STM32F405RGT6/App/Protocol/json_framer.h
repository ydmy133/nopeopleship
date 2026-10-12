/**
 * @file json_framer.h
 * @brief 字节流 JSON 对象边界识别。
 *
 * 对象可能分包或连包；字符串内的花括号不改变深度，转义引号需要单独处理。
 * 只识别帧边界，字段合法性由 protocol 校验，未完成帧的超时由命令任务管理。
 */

#ifndef JSON_FRAMER_H
#define JSON_FRAMER_H
#include <stdint.h>
typedef struct
{
    char data[512]; /**< 最多 511 字节正文，预留 NUL，超长继续追踪边界后丢弃。 */
    uint16_t length, depth; /**< 已缓存字节数 / 字符串外对象花括号深度。 */
    uint8_t quoted, escaped, overflow; /**< 引号内 / 下一字符被转义 / 当前对象已超容量。 */
} JsonFramer;
/**
 * @brief 清缓存、深度、引号、转义和溢出状态，用于初始化、超时或传输断裂。
 */
void JsonFramer_Reset(JsonFramer *frame);
/* 返回 1=完整对象，-1=超容量对象被丢弃，0=尚未完成；JSON 合法性另行解析。 */
/**
 * @brief 每次输入一个字节；
 * 返回 1 为完整对象、0 为未完成、-1 为该对象溢出。
 * 外部读取 data 直到下一对象开始/reset；
 * 不校验 JSON 字段。
 */
int JsonFramer_Push(JsonFramer *frame, uint8_t byte);
#endif
