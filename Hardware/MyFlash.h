#ifndef __MYFLASH_H__
#define __MYFLASH_H__

#include "main.h"

#define STORE_ADDR 0x080E0000U
#define STORE_SECTOR FLASH_SECTOR_11
#define STORE_SIZE 256U
#define STORE_FLAG 0x5A5A5A5AU


#define STORE_TEMP 30U
#define STORE_HUMIDITY 80U
#define STORE_LIGHT 40U

extern uint32_t Store_Data[STORE_SIZE];

void MyFlash_Init(void);
void MyFlash_Save(void);
void MyFlash_Clear(void);



#endif