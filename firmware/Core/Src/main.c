/* USER CODE BEGIN Header */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dma.h"
#include "tim.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define ADC_BUF_SIZE 3072
#define DAC_BUF_SIZE 1024

#define HEARTBEAT_FRAMES_OK    1628
#define HEARTBEAT_FRAMES_FAIL  163
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
uint8_t adc_buf[ADC_BUF_SIZE];
uint8_t dac_buf[DAC_BUF_SIZE];

volatile uint8_t process_half_a = 0;
volatile uint8_t process_half_b = 0;
volatile uint32_t overrun_cnt = 0;
volatile uint32_t dma_err_cnt = 0;
volatile uint32_t frame_cnt = 0;
static uint8_t output_started = 0;
static uint32_t blink_div = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static void ProcessSamples(uint32_t in_start, uint32_t out_start, uint32_t count);
static void DmaHalfCpltCallback(DMA_HandleTypeDef *hdma);
static void DmaCpltCallback(DMA_HandleTypeDef *hdma);
static void DmaErrorCallback(DMA_HandleTypeDef *hdma);
static void StartOutputStage(void);
static void Heartbeat(void);
static void StartupBlink(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void ProcessSamples(uint32_t in_start, uint32_t out_start, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        uint32_t idx = in_start + (i * 3);

        int32_t v = (int32_t)adc_buf[idx] + adc_buf[idx + 2] - 2 * (int32_t)adc_buf[idx + 1];

        dac_buf[out_start + i] = (uint8_t)__USAT((v >> 2) + 128, 8);
    }
}

static void DmaHalfCpltCallback(DMA_HandleTypeDef *hdma) {
    (void)hdma;
    if (process_half_a) overrun_cnt++;
    process_half_a = 1;
}

static void DmaCpltCallback(DMA_HandleTypeDef *hdma) {
    (void)hdma;
    if (process_half_b) overrun_cnt++;
    process_half_b = 1;
}

static void DmaErrorCallback(DMA_HandleTypeDef *hdma) {
    (void)hdma;
    dma_err_cnt++;
}

static void StartOutputStage(void) {
    HAL_DMA_Start(htim3.hdma[TIM_DMA_ID_UPDATE], (uint32_t)dac_buf, (uint32_t)&GPIOB->ODR, DAC_BUF_SIZE);
    __HAL_TIM_ENABLE_DMA(&htim3, TIM_DMA_UPDATE);
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
}

static void StartupBlink(void) {
    for (int i = 0; i < 3; i++) {
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
        HAL_Delay(60);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
        HAL_Delay(120);
    }
}

static void Heartbeat(void) {
    uint32_t period = (overrun_cnt || dma_err_cnt) ? HEARTBEAT_FRAMES_FAIL
                                                   : HEARTBEAT_FRAMES_OK;
    frame_cnt++;
    if (++blink_div >= period) {
        blink_div = 0;
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
    }
}
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
  MX_DMA_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */

  StartupBlink();

  htim2.hdma[TIM_DMA_ID_CC2]->XferHalfCpltCallback = DmaHalfCpltCallback;
  htim2.hdma[TIM_DMA_ID_CC2]->XferCpltCallback = DmaCpltCallback;
  htim2.hdma[TIM_DMA_ID_CC2]->XferErrorCallback = DmaErrorCallback;

  HAL_DMA_Start_IT(htim2.hdma[TIM_DMA_ID_CC2], (uint32_t)&GPIOC->IDR, (uint32_t)adc_buf, ADC_BUF_SIZE);
  __HAL_TIM_ENABLE_DMA(&htim2, TIM_DMA_CC2);
  HAL_TIM_OC_Start(&htim2, TIM_CHANNEL_2);
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
      if (process_half_a) {
          ProcessSamples(0, 0, DAC_BUF_SIZE / 2);
          process_half_a = 0;
          Heartbeat();
      }
      else if (process_half_b) {
          ProcessSamples((ADC_BUF_SIZE / 2), (DAC_BUF_SIZE / 2), DAC_BUF_SIZE / 2);
          process_half_b = 0;
          Heartbeat();

          if (!output_started) {
              StartOutputStage();
              output_started = 1;
          }
      }
      else {
          __WFI();
      }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
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
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV4;
  RCC_OscInitStruct.PLL.PLLN = 85;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
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
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
