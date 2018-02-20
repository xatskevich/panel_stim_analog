#include "main.h"

void TIM14_IRQHandler(void) { //1 мс индикация
	uint16_t i, tmp;

	TIM14->SR &= ~TIM_SR_UIF; //сброс флага

//счетчик ШИМ от 0 до 8
	if (++pwm_count == 8) pwm_count = 0;

//ШИМ кнопок
	if (pwm_count >= butt_b[0]) reset_led_power; //питание
	else set_led_power;
	if (pwm_count >= butt_b[1]) reset_led_clutch; //сцепление
	else set_led_clutch;
	if (pwm_count >= butt_b[2]) reset_led_left; //осв лев
	else set_led_left;
	if (pwm_count >= butt_b[3]) reset_led_right; //осв прав
	else set_led_right;
	if (pwm_count >= butt_b[4]) reset_led_reset; //сброс
	else set_led_reset;

//ШИМ нижних сегментов
	if (pwm_count >= analog_b[0]) reset_low_water_led; //нижний сегмент воды
	else set_low_water_led;
	if (pwm_count >= analog_b[1]) reset_low_foam_led; //нижний сегмент воды
	else set_low_foam_led;

//ШИМ индикаторов
	tmp = 0;
	for (i = 0; i < 16; i++) {
		if (pwm_count >= ind_b[i]) tmp &= ~(1 << i);
		else tmp |= (1 << i);
	}

//индикация уровней
	SPI_I2S_SendData16(SPI2, tmp);
	while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_BSY) == SET);
	set_strobe;
	reset_strobe;

}

void TIM16_IRQHandler(void) { //40мс кнопки
	uint16_t i, tmp, temp;

	TIM16->SR &= ~TIM_SR_UIF; //сброс флага

//моргалка ~1c
	if (++blink_cnt == 20) {
		blink_cnt = 0;
		blink ^= 1;
	}

//моргание шкалами
	morg++;
	if (morg == 1) {
		morg = 0;
		if ((w_morg & 0x80) == 0) { //если спад шкалы
			if (w_morg != 0) w_morg--;
		} else {
			w_morg++;
			if ((w_morg & 0xf) == 8) w_morg &= ~0x80; //если максимум шкалы, то на спад
		}
		if ((f_morg & 0x80) == 0) { //если спад шкалы
			if (f_morg != 0) f_morg--;
		} else {
			f_morg++;
			if ((f_morg & 0xf) == 8) f_morg &= ~0x80; //если максимум шкалы, то на спад
		}
	}

//изменение скважности ШИМ кнопок
	tmp = buttons;
	if(blink == 0) tmp &= ~0x10;
	for (i = 0; i < 5; i++) {
		if (tmp & (1 << i)) {
			if (butt_b[i] < 8) butt_b[i]++;
		} else {
			if (butt_b[i] > 0) butt_b[i]--;
		}
	}

//изменение скважности ШИМ нижних сегментов
	if ((water.out == 0) && (w_morg == 0) && (blink) && (power_up)) {						//для воды
		if (analog_b[0] < 8) analog_b[0]++;
	} else {
		if (analog_b[0] > 0) analog_b[0]--;
	}

	if ((foam.out == 0) && (f_morg == 0) && (blink) && (power_up)) {						//для пены
		if (analog_b[1] < 8) analog_b[1]++;
	} else {
		if (analog_b[1] > 0) analog_b[1]--;
	}

//изменение скважности ШИМ индикаторов
	tmp = 0;
	temp = 0;
	if (((water.out) || (foam.out) || (w_morg) || (f_morg)) && (power_up)) {
		i = foam.out;				//вода, отсекаем нижний сегмент
		if(f_morg) i = f_morg & 0xF; 			//если есть моргание шкалой
		while (i--) temp = (temp>>1) | 0x8000;
		i = water.out;				//пена, отсекаем нижний сегмент
		if(w_morg) i = w_morg & 0xF;			//если есть моргание шкалой
		while (i--) tmp = (tmp<<1) | 1;
		tmp |= temp;
	}
	for (i = 0; i < 16; i++) {
		if (tmp & (1 << i)) {
			if (ind_b[i] < 8) ind_b[i]++;
		} else {
			if (ind_b[i] > 0) ind_b[i]--;
		}
	}

//кнопки
	tmp = ((GPIOA->IDR ^ 0xFF) & 0xF0) | ((GPIOB->IDR ^ 0xF) & 0x7) | (((GPIOC->IDR ^ 0xFF) & 0x30) << 4);
	temp = tmp ^ butt; //определение изменения состояния кнопок
	butt = tmp;

	//нажата клавиша обороты +
	if ((tmp & is_button_inc) && (power_up)) {
		set_out_inc;
		if ((rpm) && (power_up)) { //есть питание и обороты
			if (temp & is_button_inc) { //если клавиша только нажата
				if (is_idle == 1) to_revs = idle - 160;
				is_idle = 0;
				if (to_revs < 18000) to_revs += 160; //добавить оборотов
				if (to_revs > 18000) to_revs = 18000;
				rev_p = 80; //задержка при долгом нажатии клавиши
			}
			if (rev_p == 0) {
				if (to_revs < 18000) to_revs += 16; //если задержка прошла, увеличить обороты
			} else {
				rev_p--;
			}
		}
	} else {
		reset_out_inc;
	} //

	//нажата клавиша обороты -
	if ((tmp & is_button_dec) && (power_up)) {
		set_out_dec;
		if ((rpm) && (power_up)) { //есть питание и обороты
			if (temp & is_button_dec) { //если клавиша только нажата
				if (to_revs > idle) to_revs -= 160; //убавить оборотов
				if (to_revs < idle) {
					to_revs = idle;
					//is_idle = 1;
				}
				rev_m = 80; //задержка при долгом нажатии клавиши
			}
			if (rev_m == 0) {
				if (to_revs > idle) to_revs -= 16; //если задержка прошла, уменьшить обороты
				if (to_revs <= idle) {
					to_revs = idle;
					//is_idle = 1;
				}
			} else {
				rev_m--;
			}
		}
	} else {
		reset_out_dec;
	} //

	//клавиша ПУСК
	if ((tmp & is_button_start) && (power_up)) { //нажата клавиша ПУСК
			set_out_start;
		} else {
			reset_out_start;
		}

	//клавиша СТОП
	if (temp & is_button_stop) { //нажата клавиша СТОП
		if(tmp & is_button_stop){
			set_out_stop;
			is_stop = 1;
			left = 0;
			right = 0;
		} else { //отпущена СТОП
			reset_out_stop;
			is_stop = 0;
			buttons |= 0x20; //отпущена стоп, проверить калибровку
		}
	}

	//нажата клавиша питание
	if ((temp & is_button_power) && (tmp & is_button_power)) {
		power_up ^= 0x1;
		if (power_up) { //питание включено
			set_out_power;
			buttons |= 1;
		} else { //питание выключено
			is_idle = 1;
			to_revs = idle;
			reset_out_power;
			buttons &= ~0x13;
			reset_out_clutch;
			reset_led_oil;
			reset_led_temp;
		}
	}

	//нажата клавиша сцепление
	if ((temp & is_button_clutch) && (tmp & is_button_clutch) && (power_up)) {
		buttons ^= 0x2;
		if (buttons & 0x2) { //включение сцепления
			is_idle = 1;
			to_revs = idle;
			set_out_clutch;
		} else { //выключение сцепления
			reset_out_clutch;
		}
	}

	//нажата клавиша освещение лев.
	if ((temp & is_button_left) && (tmp & is_button_left)) {
		if (is_stop) { //СТОП нажат
			left++;
		} else { //СТОП не нажат
			buttons ^= 0x4;
			if (buttons & 0x4) { //включение освещения
				set_out_left;
			} else { //выключение освещения
				reset_out_left;
			}
		}
	}

	//нажата клавиша освещение прав.
	if ((temp & is_button_right) && (tmp & is_button_right)) {
		if (is_stop) { //СТОП нажат
			right++;
		} else { //СТОП не нажат
			buttons ^= 0x8;
			if (buttons & 0x8) { //включение освещения
				set_out_right;
			} else { //выключение освещения
				reset_out_right;
			}
		}
	}

	//нажата клавиша сброс
	if ((temp & is_button_reset) && (tmp & is_button_reset) && (power_up)) {
		buttons ^= 0x10;
		if (buttons & 0x10) { //клавиша активна
			//rpm_buf = to_revs;
			is_idle = 1;
			to_revs = idle;
		} else {
			//is_idle = 0;
			//to_revs = rpm_buf;
		}
	}

//проверка калибровки
	if (buttons & 0x20) {
		buttons &= ~0x20;
		switch (left) {
		case 3:
			water.min = water.input;
			if (water.max - water.min) water.koeff = koef_k	/ (water.max - water.min);
			else water.koeff = koef_k;
			//запрет прерывания CAN
			CAN_ITConfig(CAN, CAN_IT_FMP0, DISABLE);
			//стирание flash
			flash_erase();
			//запись
			flash_prog_all();
			w_morg = 0x80;
			//разрешение прерывания CAN
			CAN_ITConfig(CAN, CAN_IT_FMP0, ENABLE);
			break;
		case 5:
			water.max = water.input;
			if (water.max - water.min) water.koeff = koef_k	/ (water.max - water.min);
			else water.koeff = koef_k;
			//запрет прерывания CAN
			CAN_ITConfig(CAN, CAN_IT_FMP0, DISABLE);
			//стирание flash
			flash_erase();
			//запись
			flash_prog_all();
			w_morg = 0x80;
			//разрешение прерывания CAN
			CAN_ITConfig(CAN, CAN_IT_FMP0, ENABLE);
			break;
		case 6:
			pto_address = 0x7;
			//запрет прерывания CAN
			CAN_ITConfig(CAN, CAN_IT_FMP0, DISABLE);
			//стирание flash
			flash_erase();
			//запись
			flash_prog_all();
			w_morg = 0x80;
			//разрешение прерывания CAN
			CAN_ITConfig(CAN, CAN_IT_FMP0, ENABLE);
			break;
		case 7:
			pto_address = 0x24;
			//запрет прерывания CAN
			CAN_ITConfig(CAN, CAN_IT_FMP0, DISABLE);
			//стирание flash
			flash_erase();
			//запись
			flash_prog_all();
			f_morg = 0x80;
			//разрешение прерывания CAN
			CAN_ITConfig(CAN, CAN_IT_FMP0, ENABLE);
			break;
		}
		switch (right) {
		case 3:
			foam.min = foam.input;
			if (foam.max - foam.min) foam.koeff = koef_k / (foam.max - foam.min);
			else foam.koeff = koef_k;
			//запрет прерывания CAN
			CAN_ITConfig(CAN, CAN_IT_FMP0, DISABLE);
			//стирание flash
			flash_erase();
			//запись
			flash_prog_all();
			f_morg = 0x80;
			//разрешение прерывания CAN
			CAN_ITConfig(CAN, CAN_IT_FMP0, ENABLE);
			break;
		case 5:
			foam.max = foam.input;
			if (foam.max - foam.min) foam.koeff = koef_k / (foam.max - foam.min);
			else foam.koeff = koef_k;
			//запрет прерывания CAN
			CAN_ITConfig(CAN, CAN_IT_FMP0, DISABLE);
			//стирание flash
			flash_erase();
			//запись
			flash_prog_all();
			f_morg = 0x80;
			//разрешение прерывания CAN
			CAN_ITConfig(CAN, CAN_IT_FMP0, ENABLE);
			break;
		}
	}

}

void TIM17_IRQHandler(void) { //CAN
	TIM17->SR &= ~TIM_SR_UIF; //сброс флага

//анализ CAN
	can_cnt++;
	if (can_cnt == 127) { //интервал 1,27 с
		if (can_on == 0) { //CAN сообщений не было
			if (power_up) { //при включенном питании
				if (is_in1) reset_led_oil; //давление
				else set_led_oil;

				if (is_in2) reset_led_temp; //перегрев
				else set_led_temp;
			}
		}
		can_on = 0;
	}

	IWDG_ReloadCounter(); //watchdog

	if (power_up) {
		if (is_idle) { //холостой ход
			TxMessage.IDE = CAN_Id_Extended;
			TxMessage.ExtId = 0xC000000 | pto_address; //0xC000003;
			TxMessage.DLC = 8;
			TxMessage.Data[0] = 0xEC; //0xFC;
			TxMessage.Data[1] = 0xFF;
			TxMessage.Data[2] = 0xFF;
			TxMessage.Data[3] = 0xFF; //0x7D;
			TxMessage.Data[4] = 0x17; //0xFF;
			TxMessage.Data[5] = 0xFF;
			TxMessage.Data[6] = 0xFF;
			TxMessage.Data[7] = 0xFF;
			CAN_Transmit(CAN, &TxMessage);
			if (CAN_GetLastErrorCode(CAN)) { //ошибка отправки
				//GPIO_SetBits(GPIOA, GPIO_Pin_9);
				//set_led_temp;
			}
		} else { //регулировка оборотов
			TxMessage.IDE = CAN_Id_Extended;
			TxMessage.ExtId = 0xC000000 | pto_address; //0xC000003;
			TxMessage.DLC = 8;
			TxMessage.Data[0] = 0xED; //0xC1;
			TxMessage.Data[1] = to_revs;
			TxMessage.Data[2] = to_revs >> 8;
			TxMessage.Data[3] = 0xFF;
			TxMessage.Data[4] = 0x17; //0xFC;
			TxMessage.Data[5] = 0xFF;
			TxMessage.Data[6] = 0xFF;
			TxMessage.Data[7] = 0xFF;
			CAN_Transmit(CAN, &TxMessage);
			if (CAN_GetLastErrorCode(CAN)) { //ошибка отправки
				//GPIO_SetBits(GPIOA, GPIO_Pin_9);
				//set_led_temp;
			}
		}
	} else {
		TxMessage.IDE = CAN_Id_Extended;
		TxMessage.ExtId = 0xC000000 | pto_address; //0xC000003;
		TxMessage.DLC = 8;
		TxMessage.Data[0] = 0xEC; //0xFC;
		TxMessage.Data[1] = 0xFF;
		TxMessage.Data[2] = 0xFF;
		TxMessage.Data[3] = 0xFF; //0x7D;
		TxMessage.Data[4] = 0x17; //0xFF;
		TxMessage.Data[5] = 0xFF;
		TxMessage.Data[6] = 0xFF;
		TxMessage.Data[7] = 0xFF;
		CAN_Transmit(CAN, &TxMessage);
		if (CAN_GetLastErrorCode(CAN)) { //ошибка отправки
			//GPIO_SetBits(GPIOA, GPIO_Pin_9);
			//set_led_temp;
		}
	}

	//выбор следующего канала АЦП
	ADC1->CHSELR ^= 0x3;
	//запуск преобразования АЦП
	ADC_StartOfConversion(ADC1);
}

void delay_ms(uint16_t dl) {
	TIM3->CNT = 0;
	TIM3->PSC = sys_clock; //период 1мс
	TIM3->ARR = dl; //
	TIM3->CR1 |= TIM_CR1_CEN; //запуск таймера
	TIM3->SR &= ~TIM_SR_UIF; //сброс флага
	while ((TIM3->SR & TIM_SR_UIF) == 0)
		;
	TIM3->CR1 &= ~TIM_CR1_CEN;
}

void delay_us (uint32_t dl){
	volatile uint32_t nCount;

	RCC_ClocksTypeDef RCC_Clocks;
	RCC_GetClocksFreq (&RCC_Clocks);

	nCount=(RCC_Clocks.HCLK_Frequency/4000000)*dl;
	for (; nCount!=0; nCount--);
}



void Init_Timer() {

	//таймер кнопок
	TIM16->PSC = sys_clock / 100 - 1;
	TIM16->ARR = 3300; //период 33 мс
	TIM16->DIER |= TIM_DIER_UIE;
	TIM16->CR1 |= TIM_CR1_CEN;

	//таймер CAN + ADC
	TIM17->PSC = sys_clock / 100 - 1;
	TIM17->ARR = 1000; //период 10 мс
	TIM17->DIER |= TIM_DIER_UIE;
	TIM17->CR1 |= TIM_CR1_CEN;

	//таймер 1 мс подсветки и индикации
	TIM14->PSC = sys_clock / 100 - 1;
	TIM14->ARR = 100; //период 1 мс
	TIM14->DIER |= TIM_DIER_UIE;
	TIM14->CR1 |= TIM_CR1_CEN;

	//вектор
	NVIC_InitStructure.NVIC_IRQChannel = TIM16_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPriority = 0x02;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

	NVIC_InitStructure.NVIC_IRQChannel = TIM17_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPriority = 0x01;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

	NVIC_InitStructure.NVIC_IRQChannel = TIM14_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPriority = 0x02;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
}

