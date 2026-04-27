#include "gd32f4xx.h"
#include "main.h"
#include "bsp_flash.h"  // 引入 Flash 驱动

/* App 的起始地址，必须与你的 App 工程配置一致 */
#define APP_START_ADDR 0x08010000

/* ========================================================= */
/* 超大内存接收缓冲区 (专为 XCOM 裸流直传设计)               */
/* ========================================================= */
#define MAX_APP_SIZE (120 * 1024) // 预留 120KB 空间给 App
uint8_t app_buffer[MAX_APP_SIZE] __attribute__((aligned(4)));

typedef void (*pFunction)(void);

/* ========================================================= */
/* 1. 底层串口驱动封装                                       */
/* ========================================================= */
void Bootloader_UART_Init(void) {
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_USART0);

    gpio_af_set(GPIOA, GPIO_AF_7, GPIO_PIN_9 | GPIO_PIN_10);
    gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_9 | GPIO_PIN_10);
    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_9 | GPIO_PIN_10);

    usart_deinit(USART0);
    usart_baudrate_set(USART0, 115200);
    usart_receive_config(USART0, USART_RECEIVE_ENABLE);
    usart_transmit_config(USART0, USART_TRANSMIT_ENABLE);
    usart_enable(USART0);
}

// 单字节发送
void UART_SendByte(uint8_t byte) {
    usart_data_transmit(USART0, byte);
    while(RESET == usart_flag_get(USART0, USART_FLAG_TBE));
}

// 字符串发送
void UART_SendString(const char *str) {
    while (*str) {
        UART_SendByte((uint8_t)(*str++));
    }
}

/* ========================================================= */
/* 2. 核心跳转逻辑 (极度严谨的 GCC 跳转流程)                 */
/* ========================================================= */
void Jump_To_App(void) {
    uint32_t msp_val = *(__IO uint32_t*)APP_START_ADDR;
    uint32_t jump_addr = *(__IO uint32_t*)(APP_START_ADDR + 4);

    if ((msp_val & 0xFF000000) == 0x20000000) {
        UART_SendString("[BOOT] Valid App found! Jumping to 0x08010000...\r\n");
        delay_1ms(20);

        /* 彻底关闭外设和中断 */
        usart_disable(USART0);
        __disable_irq();
        SysTick->CTRL = 0;
        SysTick->LOAD = 0;
        SysTick->VAL  = 0;

        for (int i = 0; i < 8; i++) {
            NVIC->ICER[i] = 0xFFFFFFFF;
            NVIC->ICPR[i] = 0xFFFFFFFF;
        }

        /* 切换栈顶指针并跳转 */
        __set_MSP(msp_val);
        __set_CONTROL(0);
        __ISB();

        pFunction JumpToApplication = (pFunction)jump_addr;
        JumpToApplication();
    } else {
        UART_SendString("\r\n[BOOT] ERROR: No valid App found! Please Update.\r\n");
    }
}

/* ========================================================= */
/* 3. 固件升级模式主控逻辑 (XCOM 无协议裸奔版)               */
/* ========================================================= */
void Enter_Update_Mode(void) {
    UART_SendString("\r\n=================================\r\n");
    UART_SendString("   Entering XCOM RAW Update Mode \r\n");
    UART_SendString("=================================\r\n");
    UART_SendString("[BOOT] Please use XCOM: [Single Send] -> [Open File] -> [Send File] (.bin)\r\n");

    // 【修改点】：把提示语提前打印出来，进入循环后保持绝对静默！
    UART_SendString("[BOOT] Waiting for XCOM data stream...\r\n");

    uint32_t app_rx_length = 0;
    uint8_t receiving = 0;
    uint32_t timeout_counter = 0;

#define TIMEOUT_THRESHOLD 20000000

    /* 接收循环：此处必须极速轮询，严禁添加任何 printf 或 SendString */
    while(1) {
        // 1. 如果串口有数据进来
        if (usart_flag_get(USART0, USART_FLAG_RBNE) != RESET) {
            uint8_t c = (uint8_t)usart_data_receive(USART0);

            // 塞入大数组
            if (app_rx_length < MAX_APP_SIZE) {
                app_buffer[app_rx_length++] = c;
            }

            timeout_counter = 0; // 只要收到数据，就把超时计数器清零
            receiving = 1;       // 标记开始接收 (绝对不要在这里加打印)
        }
        // 2. 如果串口当前空闲
        else {
            if (receiving) {
                timeout_counter++;
                // 足够长时间没有新数据，判定为发送结束！
                if (timeout_counter > TIMEOUT_THRESHOLD) {
                    break;
                }
            }
        }
    }

    /* ============ 接收完毕，开始烧录 ============ */
    UART_SendString("\r\n[BOOT] Receive Complete! Burning to Flash...\r\n");

    // 1. 擦除 Flash
    UART_SendString("[BOOT] Step 1: Erasing Flash Sectors...\r\n");
    if(BSP_Flash_EraseAppArea() != 0) {
        UART_SendString("[BOOT] ERROR: Erase FAILED!\r\n");
        return;
    }

    // 2. 写入 Flash
    UART_SendString("[BOOT] Step 2: Writing Data to Flash...\r\n");

    // FMC 必须按字 (4字节) 写入，所以对总长度进行向上取整
    uint32_t words_to_write = (app_rx_length + 3) / 4;

    if (BSP_Flash_Write(APP_START_ADDR, (uint32_t*)app_buffer, words_to_write) == 0) {
        UART_SendString("\r\n=================================\r\n");
        UART_SendString("[BOOT] Firmware Update SUCCESS!\r\n");
        UART_SendString("=================================\r\n");
        delay_1ms(100);
        Jump_To_App();
    } else {
        UART_SendString("[BOOT] ERROR: Write FAILED! Rebooting...\r\n");
        delay_1ms(500);
        NVIC_SystemReset();
    }
}

/* ========================================================= */
/* 4. 主函数 Main                                            */
/* ========================================================= */
int main(void) {
    uint8_t update_requested = 0;
    uint32_t timeout_ms = 3000; // 3秒倒计时

    /* 系统级初始化 */
    systick_config();
    BSP_LED_InitAll();
    Bootloader_UART_Init();

    // 初始化 Flash 驱动
    BSP_Flash_Init();

    /* 打印启动菜单 */
    UART_SendString("\r\n\r\n");
    UART_SendString("*********************************\r\n");
    UART_SendString("* GD32F470 XCOM Bootloader V3.0 *\r\n");
    UART_SendString("*********************************\r\n");
    UART_SendString("[BOOT] Press 'U' within 3 seconds to Update...\r\n");

    /* 倒计时轮询检测串口 */
    while(timeout_ms > 0) {
        if (usart_flag_get(USART0, USART_FLAG_RBNE) != RESET) {
            uint8_t c = (uint8_t)usart_data_receive(USART0);
            if (c == 'U' || c == 'u') {
                update_requested = 1;
                break; // 抓到 'U' 立刻跳出
            }
        }

        // 慢闪提示正在等待
        if (timeout_ms % 500 == 0) {
            LED6.Toggle();
        }
        delay_1ms(1);
        timeout_ms--;
    }

    LED6.Off(); // 保持状态干净

    /* 决策：去升级，还是去运行代码？ */
    if (update_requested) {
        Enter_Update_Mode();
    } else {
        Jump_To_App();
    }

    /* 兜底死循环 */
    while(1) {
        LED6.Toggle();
        delay_1ms(1000); // 慢闪警报
    }
}