
#include "main.h"


uint8_t is_idle, left, right, is_stop, power_up, morg, pto_address, can_on, can_cnt;
uint8_t w_morg, f_morg;		//8 бит - рост шкалы
							//значение без 8 бита - спад шкалы
uint16_t to_revs, rpm, rpm_buf;
uint8_t oil, temper;
uint8_t blink_cnt, blink;
uint16_t butt;	//0 - сцепление
				//1 - питание
				//2 - осв слева
				//3 -
				//4 - осв справа
				//5 - обор+
				//6 - обор-
				//7 - сброс
				//8 - стоп
				//9 - пуск

uint8_t pwm_count;		//счетчик до 8 для ШИМ

uint8_t buttons;	//0 - питание
					//1 - сцепление
					//2 - осв лев
					//3 - осв прав
					//4 - сброс
					//5 - сигнал на проверку калибровки
					//6 - моргалка
					//7 -

uint8_t butt_b[5],		//буфер ШИМ кнопок
		analog_b[2],	//буфер ШИМ нижних сегментов
		ind_b[16];		//буфер ШИМ индикаторов
uint8_t rev_p, rev_m;

levels water, foam;

CanTxMsg TxMessage;
GPIO_InitTypeDef GPIO_InitStructure;
ADC_InitTypeDef ADC_InitStructure;
NVIC_InitTypeDef NVIC_InitStructure;
CAN_InitTypeDef CAN_InitStructure;
CAN_FilterInitTypeDef CAN_FilterInitStructure;
SPI_InitTypeDef SPI_InitStructure;


int main(void)
{
	uint32_t temp;

	//Init_IWDG();			//

	//начальные установки для тока
	water.min=45;
	foam.min=45;
	water.max=150;
	foam.max=70;	//*/

	FLASH->KEYR = 0x45670123;
	FLASH->KEYR = 0xCDEF89AB;

	//загрузка данных из flash
	temp=(*(__IO uint32_t*) water_min_address);
	if(temp!=0xffffffff) water.min=temp;
	temp=(*(__IO uint32_t*) water_max_address);
	if(temp!=0xffffffff) water.max=temp;
	temp=(*(__IO uint32_t*) foam_min_address);
	if(temp!=0xffffffff) foam.min=temp;
	temp=(*(__IO uint32_t*) foam_max_address);
	if(temp!=0xffffffff) foam.max=temp;
	temp = (*(__IO uint32_t*) flash_pto_address);
	if(temp!=0xffffffff) pto_address=temp;


	//вычисление коэффициента
	if(water.max-water.min) water.koeff=koef_k/(water.max-water.min);
	else water.koeff=koef_k;
	if(foam.max-foam.min) foam.koeff=koef_k/(foam.max-foam.min);
	else foam.koeff=koef_k;


	Init_RCC();			//тактирование блоков
	Init_GPIO();		//инициализация портов
	Init_SPI();			//SPI для индикаторов
	Init_ADC();			//инициализация АЦП
	Init_CAN();			//инициализация CAN
	Init_Timer();		//инициализация таймеров

	to_revs=idle;
	is_idle=1;

    while(1)
    {

    }
}

//стирание flash
void flash_erase(void){

	while (FLASH->SR & FLASH_SR_BSY);
	if (FLASH->SR & FLASH_SR_EOP) {
		FLASH->SR = FLASH_SR_EOP;
	}
    FLASH->CR |= FLASH_CR_PER;
    FLASH->AR = water_min_address;
    FLASH->CR |= FLASH_CR_STRT;
    while (FLASH->SR & FLASH_SR_BSY);
    FLASH->SR = FLASH_SR_EOP;
    FLASH->CR &= ~FLASH_CR_PER;
}

//запись данных во flash
void flash_prog_all(void){

	while (FLASH->SR & FLASH_SR_BSY);
	if (FLASH->SR & FLASH_SR_EOP) {
		FLASH->SR = FLASH_SR_EOP;
	}
	FLASH->CR |= FLASH_CR_PG;
	*(__IO uint16_t*)water_min_address = water.min;
	while (!(FLASH->SR & FLASH_SR_EOP));
	FLASH->SR = FLASH_SR_EOP;
	*(__IO uint16_t*)water_max_address = water.max;
	while (!(FLASH->SR & FLASH_SR_EOP));
	FLASH->SR = FLASH_SR_EOP;
	*(__IO uint16_t*)foam_min_address = foam.min;
	while (!(FLASH->SR & FLASH_SR_EOP));
	FLASH->SR = FLASH_SR_EOP;
	*(__IO uint16_t*)foam_max_address = foam.max;
	while (!(FLASH->SR & FLASH_SR_EOP));
	FLASH->SR = FLASH_SR_EOP;
	*(__IO uint16_t*)flash_pto_address = pto_address;
	while (!(FLASH->SR & FLASH_SR_EOP));
	FLASH->SR = FLASH_SR_EOP;

	FLASH->CR &= ~(FLASH_CR_PG);
}


