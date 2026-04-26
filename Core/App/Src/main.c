#include "gd32f4xx.h"
#include "main.h"

int main(void)
{
    /* 1. 初始化系统滴答定时器 (必须，提供 delay_1ms 延时基础) */
    systick_config();

    /* 2. 初始化 LED 硬件外设 */
    BSP_LED_InitAll();

    /* 3. 极简主循环 */
    while(1) {
        LED6.Toggle();       // 翻转 LED6 状态
        delay_1ms(1000);     // 延时 1000 毫秒 (1秒)
    }
}