#include "MyFlash.h"

uint32_t Store_Data[STORE_SIZE];

static void MyFlash_ErasePage(uint32_t Addr)
{
    FLASH_EraseInitTypeDef EraseInit;
    uint32_t page_err = 0U;

    EraseInit.TypeErase = FLASH_TYPEERASE_SECTORS;
    EraseInit.Banks = FLASH_BANK_1;
    EraseInit.Sector = STORE_SECTOR;
    EraseInit.NbSectors = 1U;
    EraseInit.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    
    (void)HAL_FLASHEx_Erase(&EraseInit, &page_err);
	
}

void MyFlash_Init(void)
{
    if( *(volatile uint32_t *)STORE_ADDR != STORE_FLAG)
    {
        HAL_FLASH_Unlock();
        MyFlash_ErasePage(STORE_ADDR);

        HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, STORE_ADDR, STORE_FLAG);
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, STORE_ADDR + 4U, STORE_TEMP);
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, STORE_ADDR + 8U, STORE_HUMIDITY);
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, STORE_ADDR + 12U, STORE_LIGHT);

        for(uint32_t i = 4U; i < STORE_SIZE; i++)  //前面的是保存的数据，后面全部清零
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, STORE_ADDR + i * 4U , 0x00000000U);

        HAL_FLASH_Lock();
    }
    //不是第一次上电，把所有数据读出来保存到Store_Data数组
    for(uint32_t i = 0U; i < STORE_SIZE; i++)
    Store_Data[i] = *(volatile uint32_t *)(STORE_ADDR + i * 4);
}

void MyFlash_Save(void)
{
    HAL_FLASH_Unlock();
    MyFlash_ErasePage(STORE_ADDR);
    for(uint32_t i = 0; i < STORE_SIZE; i++)
    {
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, STORE_ADDR + i * 4, Store_Data[i]);
    }
    HAL_FLASH_Lock();

}

void MyFlash_Clear(void)
{
    
    for(uint32_t i = 1; i < STORE_SIZE; i++)
    {
        Store_Data[i] = 0x00000000U;
    }
    MyFlash_Save();

    
}