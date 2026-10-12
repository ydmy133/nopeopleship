/**
 * @file app_config.h
 * @brief 应用周期、缓冲容量与保护阈值。
 *
 * 时间单位统一为 ms，容量单位为字节或队列项数。
 * 算法参数由 Boat/boat_config.h 单独维护。
 * 修改超时时需同时检查波特率、完整应答传输时间与任务健康预算。
 */

#ifndef APP_CONFIG_H
#define APP_CONFIG_H
#define APP_SENSOR_PERIOD_MS 100u /**< GNSS/航向查询间隔，100 ms；超期后对齐而非追赶补发。 */
#define APP_SENSOR_TIMEOUT_MS 100u /**< 单次 TX 上限/静默排空上限/请求完成后应答窗口，各阶段独立计时。 */
#define APP_SENSOR_QUIET_MS 4u /**< 连续无新字节的静默阈值 ms，9600 8N1 下覆盖约 3.5 字符间隔。 */
#define APP_SENSOR_FRESH_MS 500u /**< 闭环允许的最后成功样本年龄 ms；失败不刷新时间。 */
#define APP_SENSOR_ANOMALY_LIMIT 3u /**< 连续失败最多允许 3 次，第 4 次无效，年龄门限仍独立生效。 */
#define APP_CONTROL_PERIOD_MS 10u /**< 命令、保护和目标邮箱维护周期 ms，非算法积分周期。 */
#define APP_NAV_PERIOD_MS 100u /**< 闭环算法更新间隔 ms；内部 dt 单位为秒。 */
#define APP_DRIVER_PERIOD_MS 100u /**< 正常驱动帧间隔 ms，停止可提前唤醒。 */
#define APP_CONTROL_STALL_MS 300u /**< 控制任务进度最大允许年龄 ms，超期独立停推。 */
#define APP_COMMAND_TIMEOUT_MS 5200u /**< 最后有效命令超时门限 ms，判据为严格大于此值。 */
#define APP_TELEMETRY_PERIOD_MS 200u /**< 遥测周期 ms，编码/发送在锁外。 */
#define APP_BMS_PERIOD_MS 1000u /**< 电池 RSOC 查询间隔 ms。 */
#define APP_COMMAND_QUEUE_LEN 8u /**< 静态命令 FIFO 项数，满队列触发停止和代次更新。 */
#define APP_COMMAND_RING_SIZE 2048u /**< UART4 原始字节环容量，单位字节。 */
#define APP_DMA_RX_SIZE 128u /**< 每端口接收环字节数；GNSS/航向 DMA，BMS 中断写入。 */
#define APP_CAPTURE_SIZE 64u /**< 单次原始事务捕获字节数；结构容量也需同步核对。 */
#define APP_JSON_ARENA_SIZE 8192u /**< cJSON 固定池字节数，仅串行命令解析分配。 */
#define APP_JSON_FRAME_TIMEOUT_MS 500u /**< 未完成对象的分帧窗口 ms，超时不计为新有效命令。 */
#define BATTERY_LOW_VOLTAGE 10.0f /**< 保留历史名字；当前实际比较电池 RSOC 百分比，不是电压 V。 */
#define DISCONNECT_ACTION_STOP_MOTOR 1 /**< 关闭返航保护时，失联默认转手动并给零目标。 */
#define DISCONNECT_ACTION_STABLE_ANCHOR 2 /**< 关闭返航保护时可选的失联稳泊策略编号。 */
#define DISCONNECT_ACTION DISCONNECT_ACTION_STOP_MOTOR /**< 编译期失联策略选择；dc_prot 打开仍走返航规则。 */
#endif
