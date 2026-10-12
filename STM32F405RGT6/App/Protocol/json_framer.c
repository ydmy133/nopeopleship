/**
 * @file json_framer.c
 * @brief 字节流 JSON 对象边界识别。
 *
 * 对象可能分包或连包；字符串内的花括号不改变深度，转义引号需要单独处理。
 * 只识别帧边界，字段合法性由 protocol 校验，未完成帧的超时由命令任务管理。
 */

#include "json_framer.h"
#include <string.h>
/**
 * @brief 清缓存、深度、引号、转义和溢出状态，用于初始化、超时或传输断裂。
 */
void JsonFramer_Reset(JsonFramer *f)
{
    memset(f, 0, sizeof(*f));
}
/**
 * @brief 每次输入一个字节；
 * 返回 1 为完整对象、0 为未完成、-1 为该对象溢出。
 * 外部读取 data 直到下一对象开始/reset；
 * 不校验 JSON 字段。
 */
int JsonFramer_Push(JsonFramer *f, uint8_t byte)
{
    if (!f->depth)
    {
        if (byte != '{')
            return 0;
        JsonFramer_Reset(f);
        f->depth = 1;
    }
    /* 字符串里的 {、} 只是内容，转义反斜杠决定下一个引号是否真正结束字符串。 */
    else if (f->quoted)
    {
        if (f->escaped)
            f->escaped = 0;
        else if (byte == '\\')
            f->escaped = 1;
        else if (byte == '"')
            f->quoted = 0;
    }
    else
    {
        if (byte == '"')
            f->quoted = 1;
        else if (byte == '{')
            f->depth++;
        else if (byte == '}')
            f->depth--;
    }
    /* 始终预留结尾 NUL；已溢出仍继续追踪边界，等对象结束才一次报告失败并重新同步。 */
    if (f->length < sizeof(f->data) - 1)
        f->data[f->length++] = (char)byte;
    else
        f->overflow = 1;
    f->data[f->length] = 0;
    if (!f->depth)
        return f->overflow ? -1 : 1;
    return 0;
}
