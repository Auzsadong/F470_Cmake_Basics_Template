#ifndef BSP_FLASH_H
#define BSP_FLASH_H

#include "gd32f4xx.h"
#include <stdint.h>

/* App 程序的起始地址 */
#define FLASH_APP_START_ADDR  0x08010000 
/* App 程序的结束地址 (假设芯片是 512KB Flash，结束地址是 0x0807FFFF) */
#define FLASH_APP_END_ADDR    0x0807FFFF 

/* GD32F4 系列的 Sector 4 起始地址正好是 0x08010000 */
#define FLASH_APP_START_SECTOR  CTL_SECTOR_NUMBER_4

void BSP_Flash_Init(void);
uint8_t BSP_Flash_EraseAppArea(void);
uint8_t BSP_Flash_Write(uint32_t write_addr, uint32_t *p_buffer, uint32_t num_words);

#endif /* BSP_FLASH_H */