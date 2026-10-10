/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "can.h"
#include "i2c.h"
#include "iwdg.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "led.h"
#include "protocol.h"
#include "gnss_sensor.h"
#include "heading_sensor.h"
#include "bms_sensor.h"
#include "boat_app.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define TELEMETRY_SEND_INTERVAL_MS  200
#define DRIVER_SEND_INTERVAL_MS     100
#define NAV_UPDATE_INTERVAL_MS      100
#define BMS_UPDATE_INTERVAL_MS      1000
#define BATTERY_LOW_VOLTAGE         10.0f
#define SENSOR_ANOMALY_LIMIT        3

#define DISCONNECT_ACTION_STOP_MOTOR    1
#define DISCONNECT_ACTION_STABLE_ANCHOR 2
#define DISCONNECT_ACTION               DISCONNECT_ACTION_STOP_MOTOR
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
static int8_t  s_move_speed;
static int8_t  s_steer_speed;
static float   s_battery_level;

static uint8_t s_return_home_active;
static uint8_t s_low_battery_return_active;
static uint8_t s_disconnect_stopped;
static ControlMode_t s_last_mode = CONTROL_MODE_INVALID;

static uint8_t s_nav_arrived_manual;
static double  s_nav_arrived_lat;
static double  s_nav_arrived_lon;

static STM32Telemetry_t s_telemetry;
static PCCommand_t      s_command;
/* 接收快照独立于执行指令，保护逻辑不能覆盖它。 */
static PCCommand_t      s_rx_command = { .control_mode = CONTROL_MODE_INVALID };
static int8_t s_output_move_speed;
static int8_t s_output_steer_speed;
static GNSSSensor_Data_t    s_gnss;
static uint8_t              s_gnss_valid;
static HeadingSensor_Data_t s_heading;
static uint8_t              s_heading_valid;
static uint8_t              s_battery_valid;
static GNSSSensor_Status_t  s_gnss_status = GNSS_SENSOR_TIMEOUT;
static HeadingSensor_Status_t s_heading_status = HEADING_SENSOR_TIMEOUT;
static BoatActuator         s_boat;

static const char *mode_names[] = {
    "MANUAL", "NAVIGATE", "STABLE_ANCHOR", "CRUISE_SPEED",
    "CRUISE_DIR", "TRACK_HOLD", "INVALID"
};
#define MODE_NAMES_COUNT  (sizeof(mode_names) / sizeof(mode_names[0]))
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
/** 200 ms 上报一次。经纬度为原始 GNSS，另带 gnss_yaw 与 gz。 */
static void System_UpdateTelemetry(void);
/** 按上位机模式进入船控。重复指令只改速度，不重新锁锚点或航向。 */
static void System_ProcessCommand(void);
/** 每 50 ms 轮流读 GNSS 和航向。连续异常则停桨并回手动。 */
static void System_UpdateSensors(void);
static void System_ApplySensorProtection(void);
/** 10 Hz 跑闭环。点到点到圈后推力归零并切回手动。 */
static void System_UpdateBoat(void);
/** 记下驱动板速度。前进为负时钳成 0。 */
static void System_SetSpeed(int8_t move, int8_t steer);
/** 按 100 ms 把 9 字节速度帧发到 USART6。正转向为左转。 */
static void System_SendDriverCommand(int8_t move_speed, int8_t turn_speed);
/** 断连返航或停车，以及低电量返航。返航走点到点。 */
static void System_CheckAutoReturn(void);
/** 是否跑闭环。手动和定速巡航返回 0。 */
static uint8_t System_ClosedLoop(void);
/** 组一拍传感器。对地速度从 km/h 换成 m/s。 */
static void System_FillSense(BoatSense *sense);
/** 滑条速度取绝对值。 */
static int8_t System_SpeedPercent(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_IWDG_Init();
  MX_CAN1_Init();
  MX_I2C3_Init();
  MX_UART4_Init();
  MX_UART5_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  MX_USART3_UART_Init();
  MX_USART6_UART_Init();
  /* USER CODE BEGIN 2 */
  LED_Init();
  LED_Blink(100, 10);
  GNSSSensor_Init();
  HeadingSensor_Init();
  BMSSensor_Init();
  Protocol_Init();
  BoatApp_Init();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    HAL_IWDG_Refresh(&hiwdg);
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    System_UpdateSensors();
    System_UpdateTelemetry();
    System_ProcessCommand();
    System_CheckAutoReturn();
    System_UpdateBoat();
    System_ApplySensorProtection();
#if DISCONNECT_ACTION != DISCONNECT_ACTION_STABLE_ANCHOR
    if (s_disconnect_stopped) {
        System_SetSpeed(0, 0);
    }
#endif
    System_SendDriverCommand(s_move_speed, s_steer_speed);
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI|RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

static void System_SetSpeed(int8_t move, int8_t steer)
{
    /* 仅手动直控允许负行进值；返航及所有自动模式仍禁止倒车。 */
    if (move < 0 && (s_command.control_mode != CONTROL_MODE_MANUAL ||
                     s_return_home_active || s_low_battery_return_active)) {
        move = 0;
    }
    s_move_speed = move;
    s_steer_speed = steer;
}

static void System_SendDriverCommand(int8_t move_speed, int8_t turn_speed)
{
    static uint32_t last_send_tick = 0;
    uint32_t current_tick = HAL_GetTick();
    uint8_t tx_buf[9];

    if (current_tick - last_send_tick < DRIVER_SEND_INTERVAL_MS) {
        return;
    }
    last_send_tick = current_tick;

    tx_buf[0] = 0x55;
    tx_buf[1] = 0;
    tx_buf[2] = 0;
    tx_buf[3] = 0;
    tx_buf[4] = (uint8_t)move_speed;
    tx_buf[5] = (uint8_t)turn_speed;
    tx_buf[6] = 0;
    tx_buf[7] = 0;
    tx_buf[8] = 0xAA;
    if (HAL_UART_Transmit(&huart6, tx_buf, 9, 100) == HAL_OK) {
        s_output_move_speed = move_speed;
        s_output_steer_speed = turn_speed;
    }
}

static uint8_t System_ClosedLoop(void)
{
    if (s_return_home_active || s_low_battery_return_active) {
        return 1u;
    }
    if (s_command.control_mode == CONTROL_MODE_NAVIGATE ||
        s_command.control_mode == CONTROL_MODE_STABLE_ANCHOR ||
        s_command.control_mode == CONTROL_MODE_CRUISE_DIR ||
        s_command.control_mode == CONTROL_MODE_TRACK_HOLD) {
        return 1u;
    }
    return 0u;
}

static int8_t System_SpeedPercent(void)
{
    int8_t slider = s_command.move_speed;
    if (slider < 0) {
        slider = (int8_t)(-slider);
    }
    return slider;
}

static void System_FillSense(BoatSense *sense)
{
    float heading = s_heading.yaw;
    while (heading < 0.0f) {
        heading += 360.0f;
    }
    while (heading >= 360.0f) {
        heading -= 360.0f;
    }
    sense->lat = s_gnss.latitude;
    sense->lon = s_gnss.longitude;
    sense->heading_deg = heading;
    sense->yaw_rate_dps = s_heading.gz;
    /* GNSS 模块给出的地速单位是 km/h。 */
    sense->ground_speed_ms = s_gnss.speed / 3.6f;
    sense->cog_deg = s_gnss.yaw;
    sense->pos_valid = (s_gnss_valid &&
                        !((s_gnss.latitude == 0.0) && (s_gnss.longitude == 0.0))) ? 1u : 0u;
    sense->heading_valid = s_heading_valid;
    sense->speed_valid = s_gnss_valid;
}

static void System_UpdateSensors(void)
{
    static uint32_t last_update_tick = 0;
    static uint8_t sensor_index = 0;
    static uint8_t gnss_anomaly_count = 0;
    static uint8_t heading_anomaly_count = 0;
    static uint32_t gnss_ok_tick = 0;
    static uint32_t heading_ok_tick = 0;
    static uint32_t bms_update_tick = 0;
    uint32_t current_tick = HAL_GetTick();

    if (current_tick - last_update_tick < 50) {
        return;
    }
    last_update_tick = current_tick;

    if (sensor_index == 0) {
        s_gnss_status = GNSSSensor_Update();
        if (s_gnss_status == GNSS_SENSOR_NO_FIX) {
            /* 完整有效回复但没有定位：不是通信故障，定位仍须标为无效。 */
            gnss_anomaly_count = 0;
            gnss_ok_tick = 0;
        } else if (s_gnss_status != GNSS_SENSOR_OK) {
            if (gnss_anomaly_count < 255u) {
                gnss_anomaly_count++;
            }
        } else {
            gnss_anomaly_count = 0;
            gnss_ok_tick = current_tick;
        }
        s_gnss = GNSSSensor_GetData();
    } else {
        s_heading_status = HeadingSensor_Update();
        if (s_heading_status != HEADING_SENSOR_OK) {
            if (heading_anomaly_count < 255u) {
                heading_anomaly_count++;
            }
        } else {
            heading_anomaly_count = 0;
            heading_ok_tick = current_tick;
        }
        s_heading = HeadingSensor_GetData();
    }
    sensor_index = (uint8_t)((sensor_index + 1u) % 2u);

    s_gnss_valid = (gnss_ok_tick != 0u &&
                    current_tick - gnss_ok_tick <= GNSS_SENSOR_FRESH_TIMEOUT_MS &&
                    !((s_gnss.latitude == 0.0) && (s_gnss.longitude == 0.0)) &&
                    gnss_anomaly_count <= SENSOR_ANOMALY_LIMIT) ? 1u : 0u;
    s_heading_valid = (heading_ok_tick != 0u &&
                       current_tick - heading_ok_tick <= HEADING_SENSOR_FRESH_TIMEOUT_MS &&
                       heading_anomaly_count <= SENSOR_ANOMALY_LIMIT) ? 1u : 0u;

    if (current_tick - bms_update_tick >= BMS_UPDATE_INTERVAL_MS) {
        bms_update_tick = current_tick;
        if (BMSSensor_Update() == BMS_SENSOR_OK) {
            s_battery_level = (float)BMSSensor_GetData();
            s_battery_valid = 1u;
        }
    }

    /* 只更新有效性。输出保护在处理指令后执行，不覆盖模式或已锁目标。 */
}

static void System_ApplySensorProtection(void)
{
    /* 手动和开环定速不依赖 GNSS/航向；其指令超时保护仍独立生效。
     * 全部闭环都需要航向和 GNSS 地速（定向巡航也使用速度环）。 */
    if (System_ClosedLoop() && (!s_gnss_valid || !s_heading_valid)) {
        System_SetSpeed(0, 0);
    }
}

static void System_ProcessCommand(void)
{
    static double s_last_lat = 0.0;
    static double s_last_lon = 0.0;
    int8_t pct;
    uint8_t mode_edge;
    uint8_t target_edge;

    if (!Protocol_GetLatestCommand(&s_rx_command)) {
        return;
    }
    s_command = s_rx_command;

    if (s_nav_arrived_manual) {
        if (s_command.control_mode == CONTROL_MODE_NAVIGATE &&
            s_command.target_lat == s_nav_arrived_lat &&
            s_command.target_lon == s_nav_arrived_lon) {
            s_command.control_mode = CONTROL_MODE_MANUAL;
            s_command.move_speed = 0;
            s_command.steer_speed = 0;
        } else {
            s_nav_arrived_manual = 0;
        }
    }

    s_return_home_active = 0;
    s_disconnect_stopped = 0;

    if (s_low_battery_return_active) {
        if (s_command.move_speed == 20) {
            s_low_battery_return_active = 0;
        }
        return;
    }

    if (s_last_mode != s_command.control_mode && s_command.control_mode < MODE_NAMES_COUNT) {
        printf("[MAIN] Mode switch: %s\r\n", mode_names[s_command.control_mode]);
    }

    mode_edge = (s_last_mode != s_command.control_mode) ? 1u : 0u;
    target_edge = (s_command.target_lat != s_last_lat || s_command.target_lon != s_last_lon) ? 1u : 0u;
    pct = System_SpeedPercent();

    switch (s_command.control_mode) {
        case CONTROL_MODE_MANUAL:
            BoatApp_Stop();
            System_SetSpeed(s_command.move_speed, s_command.steer_speed);
            break;
        case CONTROL_MODE_NAVIGATE:
            if (mode_edge || target_edge) {
                s_boat.arrived = 0;
                BoatApp_EnterWaypoint(s_gnss.latitude, s_gnss.longitude,
                                      s_command.target_lat, s_command.target_lon, pct);
            } else {
                BoatApp_SetSpeedPercent(pct);
            }
            s_last_lat = s_command.target_lat;
            s_last_lon = s_command.target_lon;
            break;
        case CONTROL_MODE_STABLE_ANCHOR:
            if (mode_edge) {
                BoatApp_EnterLoiter(s_gnss.latitude, s_gnss.longitude, s_heading.yaw);
            }
            break;
        case CONTROL_MODE_CRUISE_SPEED:
            if (mode_edge) {
                BoatApp_Stop();
            }
            {
                int8_t move = 0;
                int8_t steer = 0;
                BoatApp_CruiseOpenLoop(s_command.move_speed, &move, &steer);
                System_SetSpeed(move, steer);
            }
            break;
        case CONTROL_MODE_CRUISE_DIR:
            if (mode_edge) {
                BoatApp_EnterHeading(s_heading.yaw, pct);
            } else {
                BoatApp_SetSpeedPercent(pct);
            }
            break;
        case CONTROL_MODE_TRACK_HOLD:
            if (mode_edge) {
                BoatApp_EnterTrack(s_gnss.latitude, s_gnss.longitude, s_heading.yaw, pct);
            } else {
                BoatApp_SetSpeedPercent(pct);
            }
            break;
        default:
            BoatApp_Stop();
            System_SetSpeed(0, 0);
            break;
    }
    s_last_mode = s_command.control_mode;
}

static void System_CheckAutoReturn(void)
{
    int8_t pct = System_SpeedPercent();

    if (s_command.dc_prot && Protocol_IsTimeout()) {
        if (!s_return_home_active && !s_low_battery_return_active) {
            s_return_home_active = 1u;
            s_disconnect_stopped = 0;
            BoatApp_EnterWaypoint(s_gnss.latitude, s_gnss.longitude,
                                  s_command.return_lat, s_command.return_lon, pct);
            printf("[MAIN] Disconnect return\r\n");
        }
    }

    if (!s_command.dc_prot && Protocol_IsTimeout() &&
        !s_return_home_active && !s_low_battery_return_active) {
#if DISCONNECT_ACTION == DISCONNECT_ACTION_STABLE_ANCHOR
        if (!s_disconnect_stopped) {
            s_disconnect_stopped = 1u;
            s_command.control_mode = CONTROL_MODE_STABLE_ANCHOR;
            BoatApp_EnterLoiter(s_gnss.latitude, s_gnss.longitude, s_heading.yaw);
            printf("[MAIN] Disconnect loiter\r\n");
        }
#else
        if (!s_disconnect_stopped) {
            printf("[MAIN] Disconnect stop\r\n");
        }
        s_disconnect_stopped = 1u;
        s_command.control_mode = CONTROL_MODE_MANUAL;
        s_last_mode = CONTROL_MODE_MANUAL;
        BoatApp_Stop();
        System_SetSpeed(0, 0);
#endif
    }

    if (s_command.lb_prot && s_battery_level < BATTERY_LOW_VOLTAGE && s_battery_level != 0.0f) {
        if (!s_low_battery_return_active && !s_return_home_active) {
            s_low_battery_return_active = 1u;
            BoatApp_EnterWaypoint(s_gnss.latitude, s_gnss.longitude,
                                  s_command.return_lat, s_command.return_lon, pct);
            printf("[MAIN] Low battery return\r\n");
        }
    }

    if (s_low_battery_return_active && s_boat.arrived) {
        s_low_battery_return_active = 0;
        BoatApp_Stop();
        System_SetSpeed(0, 0);
    }
}

static void System_UpdateBoat(void)
{
    static uint32_t last_update_tick = 0;
    uint32_t now = HAL_GetTick();
    BoatSense sense;

    if (now - last_update_tick < NAV_UPDATE_INTERVAL_MS) {
        return;
    }
    last_update_tick = now;

    if (!System_ClosedLoop()) {
        return;
    }

    System_FillSense(&sense);
    if (!sense.pos_valid || !sense.heading_valid || !sense.speed_valid) {
        System_SetSpeed(0, 0);
        return;
    }

    BoatApp_Update(&sense, now);
    BoatApp_GetActuator(&s_boat);

    if (!s_return_home_active && !s_low_battery_return_active &&
        s_command.control_mode == CONTROL_MODE_NAVIGATE &&
        s_boat.arrived) {
        s_nav_arrived_manual = 1u;
        s_nav_arrived_lat = s_command.target_lat;
        s_nav_arrived_lon = s_command.target_lon;
        s_command.control_mode = CONTROL_MODE_MANUAL;
        s_last_mode = CONTROL_MODE_MANUAL;
        s_command.move_speed = 0;
        s_command.steer_speed = 0;
        System_SetSpeed(0, 0);
        printf("[MAIN] Navigate arrived, manual\r\n");
        return;
    }

    System_SetSpeed(s_boat.speed, s_boat.steering);
}

static void System_UpdateTelemetry(void)
{
    static uint32_t last_send_tick = 0;
    uint32_t now = HAL_GetTick();

    if (now - last_send_tick < TELEMETRY_SEND_INTERVAL_MS) {
        return;
    }
    last_send_tick = now;

    s_telemetry.telemetry_seq++;
    s_telemetry.uptime_ms = now;
    s_telemetry.cmd_age_ms = Protocol_GetCommandAgeMs();
    s_telemetry.data_valid = (int8_t)s_gnss_valid;
    s_telemetry.heading_valid = (int8_t)s_heading_valid;
    s_telemetry.battery_valid = (int8_t)s_battery_valid;
    s_telemetry.gnss_status = (int8_t)s_gnss_status;
    s_telemetry.heading_status = (int8_t)s_heading_status;
    s_telemetry.nav_reached = (int8_t)s_boat.arrived;
    s_telemetry.heading = s_heading.yaw;
    s_telemetry.battery_level = s_battery_level;
    s_telemetry.speed = s_gnss.speed / 3.6f;
    s_telemetry.gnss_yaw = s_gnss.yaw;
    s_telemetry.gz = s_heading.gz;
    s_telemetry.raw_lat = s_gnss.latitude;
    s_telemetry.raw_lon = s_gnss.longitude;
    s_telemetry.target_lat = s_boat.target_lat;
    s_telemetry.target_lon = s_boat.target_lon;
    s_telemetry.dist_error = s_boat.distance_m;
    s_telemetry.heading_error = s_boat.heading_error_deg;
    s_telemetry.rx_control_mode = s_rx_command.control_mode;
    s_telemetry.rx_move_speed = s_rx_command.move_speed;
    s_telemetry.rx_steer_speed = s_rx_command.steer_speed;
    s_telemetry.rx_target_lat = s_rx_command.target_lat;
    s_telemetry.rx_target_lon = s_rx_command.target_lon;
    s_telemetry.control_mode = (s_return_home_active || s_low_battery_return_active)
        ? CONTROL_MODE_NAVIGATE : s_command.control_mode;
    s_telemetry.output_move_speed = s_output_move_speed;
    s_telemetry.output_steer_speed = s_output_steer_speed;
    Protocol_SendTelemetry(&s_telemetry);
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  (void)file;
  (void)line;
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
