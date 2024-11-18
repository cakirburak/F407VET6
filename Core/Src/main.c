/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 */
#include "main.h"

/**
 * 0100 is fixed for pcf8574
 * A2 A1 A0 is defined by user meaning there can be 8 slave lcd device for one I2C connection
 * R/W "0" for writing to DDRAM, "1" for reading from DDRAM
 */
#define SLAVE_ADDRESS_LCD 0x4E // 0 1 0 0 A2 A1 A0 R/W

I2C_HandleTypeDef hi2c1;
uint8_t NumberBuffer[8];

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);

void BC_LCD_Init(void);
void BC_LCD_SendCmd(uint8_t);
void BC_LCD_SendData(uint8_t);
void BC_LCD_SendString(uint8_t*);
void BC_LCD_PutCursor(uint32_t, uint32_t);
void BC_LCD_SendInteger(int32_t);

volatile int32_t counter = 0;

/**
  * @brief This function handles EXTI line0 interrupt.
  */
volatile uint32_t lastInterruptTime = 0; // Track the last interrupt time
void EXTI0_IRQHandler(void)
{
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_0);
  
  uint32_t currentTime = HAL_GetTick(); // Get the current system time in milliseconds
  
  if (currentTime - lastInterruptTime > 40) { // Debounce delay of 40ms
      HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_3);
      counter++;
  }
  
  lastInterruptTime = currentTime;
}


uint8_t* BC_Util_itoa(int num, uint8_t* str, int base) {
    int i = 0;
    int isNegative = 0;

    // Handle 0 explicitly, otherwise empty string is printed
    if (num == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return str;
    }

    // Handle negative numbers only if base is 10
    if (num < 0 && base == 10) {
        isNegative = 1;
        num = -num;
    }

    // Process individual digits
    while (num != 0) {
        int rem = num % base;
        str[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        num = num / base;
    }

    // Append negative sign for negative numbers
    if (isNegative) {
        str[i++] = '-';
    }

    // Null-terminate the string
    str[i] = '\0';

    // Reverse the string
    int start = 0;
    int end = i - 1;
    while (start < end) {
        uint8_t temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }

    return str;
}

int main(void)
{
  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();
  /* Configure the system clock */
  SystemClock_Config();
  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  
  BC_LCD_Init();
  HAL_Delay(10);
  
  while (1)
  {  
    static int32_t lastCounter = -1;
    if (lastCounter != counter) {
        BC_LCD_PutCursor(0, 0);
        BC_LCD_SendInteger(counter);
        lastCounter = counter;
    }
  }
}

void BC_LCD_Init(void)
{
  // 4 bit initialisation
  HAL_Delay(50);        // wait for >40ms
  BC_LCD_SendCmd (0x30);
  HAL_Delay(5);         // wait for >4.1ms
  BC_LCD_SendCmd (0x30);
  HAL_Delay(1);         // wait for >100us
  BC_LCD_SendCmd (0x30);
  HAL_Delay(10);
  BC_LCD_SendCmd (0x20);  // 4bit mode
  HAL_Delay(10);

  // display initializetion
  BC_LCD_SendCmd (0x28); // Function set --> DL=0 (4 bit mode), N = 1 (2 line display) F = 0 (5x8 characters)
  HAL_Delay(1);
  BC_LCD_SendCmd (0x08); //Display on/off control --> D=0,C=0, B=0  ---> display off
  HAL_Delay(1);
  BC_LCD_SendCmd (0x01); // clear display
  HAL_Delay(2);
  BC_LCD_SendCmd (0x06); //Entry mode set --> I/D = 1 (increment cursor) & S = 0 (no shift)
  HAL_Delay(1);
  BC_LCD_SendCmd (0x0C); //Display on/off control --> D = 1, C and B = 0. (Cursor and blink, last two bits)
  HAL_Delay(1);
}

void BC_LCD_SendCmd(uint8_t cmd)
{
  uint8_t cmd_u, cmd_l;
  cmd_u = (cmd & 0xF0);
  cmd_l = ((cmd << 4) & 0xF0);
  
  uint8_t cmd_t[4] = {0};
  // To send the strobe(E), we need to send the same data twice
  cmd_t[0] = cmd_u | 0x0C; // E = 1, RS = 0 -> xxxx1100
  cmd_t[1] = cmd_u | 0x08; // E = 0, RS = 0 -> xxxx1000
  cmd_t[2] = cmd_l | 0x0C; // E = 1, RS = 0 -> xxxx1100
  cmd_t[3] = cmd_l | 0x08; // E = 0, RS = 0 -> xxxx1000
  HAL_I2C_Master_Transmit(&hi2c1, SLAVE_ADDRESS_LCD, cmd_t, 4, 100);
  HAL_Delay(1);
}


void BC_LCD_SendData(uint8_t data)
{
  uint8_t data_u, data_l;
  data_u = (data & 0xF0);
  data_l = ((data << 4) & 0xF0);
  
  uint8_t data_t[4] = {0};
  // To send the strobe(E), we need to send the same data twice
  data_t[0] = data_u | 0x0D; // E = 1, RS = 1 -> xxxx1101
  data_t[1] = data_u | 0x09; // E = 0, RS = 1 -> xxxx1001
  data_t[2] = data_l | 0x0D; // E = 1, RS = 1 -> xxxx1101
  data_t[3] = data_l | 0x09; // E = 0, RS = 1 -> xxxx1001
  HAL_I2C_Master_Transmit(&hi2c1, SLAVE_ADDRESS_LCD, data_t, 4, 100);
  HAL_Delay(1);
}

void BC_LCD_SendString(uint8_t* str)
{
  while (*str) BC_LCD_SendData(*str++);
}

void BC_LCD_SendInteger(int32_t number)
{
  uint8_t buffer[8] = {0};
  BC_Util_itoa(number, buffer, 10);
  uint8_t* ptr = buffer;
  while (*ptr) BC_LCD_SendData(*ptr++);
}

void BC_LCD_PutCursor(uint32_t row, uint32_t col)
{
  switch (row)
  {
    case 0:
    {
      col |= 0x80;
    } break;
    case 1:
    {
      col |= 0xC0;
    } break;
  }
  BC_LCD_SendCmd(col);
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
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 72;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  
  // I2C SLC and SDA Ports
  __HAL_RCC_GPIOB_CLK_ENABLE();
  // ----
  
  // KEY0 and KEY1 on PE4 and PE3
  // __HAL_RCC_GPIOE_CLK_ENABLE();
  // ----
  
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
  
  HAL_NVIC_SetPriority(EXTI0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI0_IRQn);
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}

#ifdef  USE_FULL_ASSERT
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
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
