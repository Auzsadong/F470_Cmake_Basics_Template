#include "bsp_flash.h"
#include <stdio.h>

/**
 * @brief Flash 操作前的初始化配置
 */
void BSP_Flash_Init(void) {
    // GD32F4 的 Flash 频率等配置通常在 SystemInit() 中已完成
    // 这里保留接口以便后续扩展
}

/**
 * @brief 擦除 App 所在的 Flash 区域
 * @return 0: 成功, 1: 失败
 */
uint8_t BSP_Flash_EraseAppArea(void) {
    fmc_state_enum fmc_state = FMC_READY;
    
    printf("[FLASH] Unlocking Flash...\r\n");
    fmc_unlock(); // 1. 解锁 Flash 控制寄存器

    /* 2. 清除所有错误标志位 */
    fmc_flag_clear(FMC_FLAG_END | FMC_FLAG_OPERR | FMC_FLAG_WPERR | FMC_FLAG_PGMERR | FMC_FLAG_PGSERR);

    printf("[FLASH] Erasing App Sectors (Starting from Sector 4)...\r\n");
    
    /* 3. 逐个擦除 Sector 
     * GD32F470 (512KB版本): 
     * Sector 0~3 (各16KB) = 0x08000000 ~ 0x0800FFFF (Bootloader)
     * Sector 4 (64KB)     = 0x08010000 ~ 0x0801FFFF
     * Sector 5~7 (各128KB) = 0x08020000 ~ 0x0807FFFF
     */
    for (uint32_t sector = FLASH_APP_START_SECTOR; sector <= CTL_SECTOR_NUMBER_7; sector++) {
        fmc_state = fmc_sector_erase(sector);
        if (fmc_state != FMC_READY) {
            printf("[FLASH] Error: Erase failed at Sector %lu\r\n", sector);
            fmc_lock();
            return 1; // 擦除失败
        }
    }
    
    printf("[FLASH] App Area Erased Successfully!\r\n");
    fmc_lock(); // 4. 重新上锁
    return 0; // 成功
}

/**
 * @brief 将数据写入 Flash
 * @param write_addr 写入的目标首地址 (必须是 4 的倍数)
 * @param p_buffer   要写入的数据指针 (32位字)
 * @param num_words  要写入的字数 (1个字 = 4个字节)
 * @return 0: 成功, 1: 失败
 */
uint8_t BSP_Flash_Write(uint32_t write_addr, uint32_t *p_buffer, uint32_t num_words) {
    fmc_state_enum fmc_state = FMC_READY;
    uint32_t current_addr = write_addr;
    uint32_t i = 0;

    // 参数合法性检查
    if (write_addr < FLASH_APP_START_ADDR || write_addr > FLASH_APP_END_ADDR) {
        return 1; 
    }

    fmc_unlock();
    fmc_flag_clear(FMC_FLAG_END | FMC_FLAG_OPERR | FMC_FLAG_WPERR | FMC_FLAG_PGMERR | FMC_FLAG_PGSERR);

    // 逐字(32-bit)编程
    for (i = 0; i < num_words; i++) {
        fmc_state = fmc_word_program(current_addr, p_buffer[i]);
        
        // 校验是否写入成功
        if (fmc_state == FMC_READY) {
            // 读出来校验一下
            if (*(__IO uint32_t*)current_addr != p_buffer[i]) {
                fmc_lock();
                return 1; // 校验失败
            }
        } else {
            fmc_lock();
            return 1; // 写入失败
        }
        current_addr += 4;
    }

    fmc_lock();
    return 0; // 成功
}