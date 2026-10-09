/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * Bàn phím công nghi?p — Modbus RTU slave (STM32G030F6P6 + TM1638 + SIT3485).
  *
  * Ghép chân (docs/SO_DO_NGUYEN_LY.md):
  *   PA1 USART2_DE, PA2 USART2_TX, PA3 USART2_RX  -> SIT3485
  *   PA4 TM_STB, PA5 TM_CLK, PA6 TM_DIO (open-drain, kéo lên +5V_LED 4,7k) -> TM1638
  *   PA7 BZ_ON (còi ch? d?ng)
  *   PA13/PA14 SWD
  *
  * Code ?ng d?ng n?m trong App/ và trong các kh?i USER CODE (gi? nguyên khi
  * sinh l?i code b?ng STM32CubeMX t? keyboard.ioc).
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "app.h"
#include "stm32g0xx_ll_bus.h"
#include "stm32g0xx_ll_gpio.h"
#include "stm32g0xx_ll_rcc.h"
#include "stm32g0xx_ll_system.h"
#include "stm32g0xx_ll_tim.h"
#include "stm32g0xx_ll_usart.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM14_Init(void);
/* USER CODE BEGIN PFP */

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
  MX_USART2_UART_Init();
  MX_TIM14_Init();
  /* USER CODE BEGIN 2 */
  app_init();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    app_loop();
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  /* HSI16 chia 1 làm SYSCLK, không PLL, 0 wait state (dùng LL d? ti?t ki?m flash so v?i HAL_RCC_OscConfig) */
  LL_RCC_HSI_Enable();
  while (!LL_RCC_HSI_IsReady())
  {
  }
  LL_RCC_SetHSIDiv(LL_RCC_HSI_DIV_1);
  LL_FLASH_SetLatency(LL_FLASH_LATENCY_0);
  LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_HSI);
  while (LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_HSI)
  {
  }
  LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
  LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_1);
  SystemCoreClockUpdate();
  HAL_InitTick(TICK_INT_PRIORITY);
}

/**
  * @brief TIM14 Initialization Function (LL)
  * TIM14 d?m 1 µs d? do kho?ng l?ng t3.5 c?a Modbus RTU; ARR/PSC d?t l?i trong rs485_init().
  */
static void MX_TIM14_Init(void)
{
  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_TIM14);
  NVIC_SetPriority(TIM14_IRQn, 1);
  NVIC_EnableIRQ(TIM14_IRQn);
  LL_TIM_SetPrescaler(TIM14, 15);
  LL_TIM_SetCounterMode(TIM14, LL_TIM_COUNTERMODE_UP);
  LL_TIM_SetAutoReload(TIM14, 4010);
  LL_TIM_SetClockDivision(TIM14, LL_TIM_CLOCKDIVISION_DIV1);
  LL_TIM_DisableARRPreload(TIM14);
}

/**
  * @brief USART2 Initialization Function (LL): RS485, DE ph?n c?ng
  *   PA1 USART2_DE, PA2 USART2_TX, PA3 USART2_RX. Baud / parity / stop do rs485_init() d?t.
  */
static void MX_USART2_UART_Init(void)
{
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_USART2);
  LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOA);

  LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_1, LL_GPIO_MODE_ALTERNATE);
  LL_GPIO_SetAFPin_0_7(GPIOA, LL_GPIO_PIN_1, LL_GPIO_AF_1);
  LL_GPIO_SetPinSpeed(GPIOA, LL_GPIO_PIN_1, LL_GPIO_SPEED_FREQ_LOW);
  LL_GPIO_SetPinPull(GPIOA, LL_GPIO_PIN_1, LL_GPIO_PULL_NO);
  LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_2, LL_GPIO_MODE_ALTERNATE);
  LL_GPIO_SetAFPin_0_7(GPIOA, LL_GPIO_PIN_2, LL_GPIO_AF_1);
  LL_GPIO_SetPinSpeed(GPIOA, LL_GPIO_PIN_2, LL_GPIO_SPEED_FREQ_LOW);
  LL_GPIO_SetPinPull(GPIOA, LL_GPIO_PIN_2, LL_GPIO_PULL_NO);
  LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_3, LL_GPIO_MODE_ALTERNATE);
  LL_GPIO_SetAFPin_0_7(GPIOA, LL_GPIO_PIN_3, LL_GPIO_AF_1);
  LL_GPIO_SetPinSpeed(GPIOA, LL_GPIO_PIN_3, LL_GPIO_SPEED_FREQ_LOW);
  LL_GPIO_SetPinPull(GPIOA, LL_GPIO_PIN_3, LL_GPIO_PULL_UP);

  NVIC_SetPriority(USART2_IRQn, 0);
  NVIC_EnableIRQ(USART2_IRQn);

  LL_USART_Disable(USART2);
  LL_USART_SetTransferDirection(USART2, LL_USART_DIRECTION_TX_RX);
  LL_USART_ConfigCharacter(USART2, LL_USART_DATAWIDTH_8B, LL_USART_PARITY_NONE, LL_USART_STOPBITS_1);
  LL_USART_SetOverSampling(USART2, LL_USART_OVERSAMPLING_16);
  LL_USART_SetBaudRate(USART2, SystemCoreClock, LL_USART_PRESCALER_DIV1, LL_USART_OVERSAMPLING_16, 9600);
  LL_USART_ConfigAsyncMode(USART2);
  LL_USART_SetDESignalPolarity(USART2, LL_USART_DE_POLARITY_HIGH);
  LL_USART_SetDEAssertionTime(USART2, 8);
  LL_USART_SetDEDeassertionTime(USART2, 8);
  LL_USART_EnableDEMode(USART2);
  LL_USART_Enable(USART2);
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, TM_STB_Pin|TM_CLK_Pin|TM_DIO_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(BZ_ON_GPIO_Port, BZ_ON_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : TM_STB_Pin TM_CLK_Pin TM_DIO_Pin */
  GPIO_InitStruct.Pin = TM_STB_Pin|TM_CLK_Pin|TM_DIO_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : BZ_ON_Pin */
  GPIO_InitStruct.Pin = BZ_ON_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(BZ_ON_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* Treo t?i dây; n?u IWDG dã ch?y chip s? t? reset */
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
