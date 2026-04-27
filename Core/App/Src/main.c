#include "gd32f4xx.h"
#include "main.h"

int main(void)
{
    /* 1. 初始化系统时钟 */
    systick_config();
    BSP_LED_InitAll();



    /* ========================================================== */
    /* 暴力测试：绕开封装，直接使用最底层的 GD32 库函数初始化 USART0 */
    /* ========================================================== */

    // A. 开启时钟：GPIOA时钟 和 USART0时钟
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_USART0);

    // B. 配置 GPIO 引脚复用 (PA9=TX, PA10=RX, 复用为 AF7)
    gpio_af_set(GPIOA, GPIO_AF_7, GPIO_PIN_9 | GPIO_PIN_10);
    gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_9 | GPIO_PIN_10);
    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_9 | GPIO_PIN_10);

    // C. 配置 USART0 参数
    usart_deinit(USART0);
    usart_baudrate_set(USART0, 115200);
    usart_receive_config(USART0, USART_RECEIVE_ENABLE);
    usart_transmit_config(USART0, USART_TRANSMIT_ENABLE);
    usart_enable(USART0);

    // D. 暴力发送三个字符 "SOS" 测试
    usart_data_transmit(USART0, 'S');
    while(RESET == usart_flag_get(USART0, USART_FLAG_TBE)); // 等待发送完成

    usart_data_transmit(USART0, 'O');
    while(RESET == usart_flag_get(USART0, USART_FLAG_TBE));

    usart_data_transmit(USART0, 'S');
    while(RESET == usart_flag_get(USART0, USART_FLAG_TBE));

    usart_data_transmit(USART0, '\r');
    while(RESET == usart_flag_get(USART0, USART_FLAG_TBE));
    usart_data_transmit(USART0, '\n');
    while(RESET == usart_flag_get(USART0, USART_FLAG_TBE));
    /* ========================================================== */

    // 如果上面能通，这里再初始化你的封装
    DebugUART.Init(115200);
    DebugUART.SendString("\r\n[SYS] BOOTLOADER READY !!!\r\n");

    // ... 下面的 3秒倒计时等代码保持不变 ...
    while(1) {
        LED6.Toggle();
        delay_1ms(500);
    }
}