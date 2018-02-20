
#include "main.h"

void CEC_CAN_IRQHandler (void)
{

	if (CAN_GetITStatus(CAN,CAN_IT_FMP0)){
		CAN_ClearITPendingBit(CAN, CAN_IT_FMP0);	//сброс флага
		can_on = 1;
		CanRxMsg msg_buf;
		CAN_Receive(CAN, CAN_FIFO0, &msg_buf);
		if(power_up){
			switch(msg_buf.FMI){		//номер фильтра
			case 0:						//обороты 3,4 байт
				rpm=msg_buf.Data[4]<<8;
				rpm |= msg_buf.Data[3];
				if(buttons & 0x10){		//если нажата сброс
									if(rpm < idle + 400) buttons &= ~0x10;	//сброс моргания кнопки сброс
								}

			break;
			case 1:						//температура 0 байт
				temper=msg_buf.Data[0];
				if(temper>149) set_led_temp;
				else reset_led_temp;
			break;
			case 2:						//давление 3 байт
				oil=msg_buf.Data[3];
				if(oil<32) set_led_oil;
				else reset_led_oil;
			break;
			}
		}
	}
}

void Init_CAN(){
	/* CAN register init */
	CAN_DeInit(CAN);

	/* CAN cell init */
	CAN_InitStructure.CAN_TTCM = DISABLE;
	CAN_InitStructure.CAN_ABOM = ENABLE;
	CAN_InitStructure.CAN_AWUM = ENABLE;
	CAN_InitStructure.CAN_NART = DISABLE;
	CAN_InitStructure.CAN_RFLM = DISABLE;
	CAN_InitStructure.CAN_TXFP = DISABLE;
	CAN_InitStructure.CAN_Mode = CAN_Mode_Normal;
	CAN_InitStructure.CAN_SJW = CAN_SJW_1tq;

	/* CAN Baudrate = 250 kBps (CAN clocked at 48 MHz) */
	CAN_InitStructure.CAN_BS1 = CAN_BS1_8tq;
	CAN_InitStructure.CAN_BS2 = CAN_BS2_7tq;	//1+8+7=16
	CAN_InitStructure.CAN_Prescaler = 12;		//48MHz / 16 / 12 == 250k
	CAN_Init(CAN, &CAN_InitStructure);

	/* CAN filter init */
	CAN_FilterInitStructure.CAN_FilterNumber = 0;
	CAN_FilterInitStructure.CAN_FilterMode = CAN_FilterMode_IdMask;
	CAN_FilterInitStructure.CAN_FilterScale = CAN_FilterScale_32bit;
	CAN_FilterInitStructure.CAN_FilterIdHigh = 0x0CF0<<3; //0x0000;	id 0CF004
	CAN_FilterInitStructure.CAN_FilterIdLow = 0x0400<<3; //0x0000;
	CAN_FilterInitStructure.CAN_FilterMaskIdHigh = (0x1fff<<3)|7; //0x0000;
	CAN_FilterInitStructure.CAN_FilterMaskIdLow = 0x1f00<<3;
	CAN_FilterInitStructure.CAN_FilterFIFOAssignment = 0;
	CAN_FilterInitStructure.CAN_FilterActivation = ENABLE;
	CAN_FilterInit(&CAN_FilterInitStructure);

	CAN_FilterInitStructure.CAN_FilterNumber = 1;
	CAN_FilterInitStructure.CAN_FilterMode = CAN_FilterMode_IdMask;
	CAN_FilterInitStructure.CAN_FilterScale = CAN_FilterScale_32bit;
	CAN_FilterInitStructure.CAN_FilterIdHigh = (0x18FE<<3)|7; //0x0000;	id 18FEEE
	CAN_FilterInitStructure.CAN_FilterIdLow = 0x0E00<<3; //0x0000;
	CAN_FilterInitStructure.CAN_FilterMaskIdHigh = (0x1fff<<3)|7; //0x0000;
	CAN_FilterInitStructure.CAN_FilterMaskIdLow = 0x1f00<<3;
	CAN_FilterInitStructure.CAN_FilterFIFOAssignment = 0;
	CAN_FilterInitStructure.CAN_FilterActivation = ENABLE;
	CAN_FilterInit(&CAN_FilterInitStructure);

	CAN_FilterInitStructure.CAN_FilterNumber = 2;
	CAN_FilterInitStructure.CAN_FilterMode = CAN_FilterMode_IdMask;
	CAN_FilterInitStructure.CAN_FilterScale = CAN_FilterScale_32bit;
	CAN_FilterInitStructure.CAN_FilterIdHigh = (0x18FE<<3)|7; //0x0000;		id 18FEEF
	CAN_FilterInitStructure.CAN_FilterIdLow = 0x0F00<<3; //0x0000;
	CAN_FilterInitStructure.CAN_FilterMaskIdHigh = (0x1fff<<3)|7; //0x0000;
	CAN_FilterInitStructure.CAN_FilterMaskIdLow = 0x1f00<<3;
	CAN_FilterInitStructure.CAN_FilterFIFOAssignment = 0;
	CAN_FilterInitStructure.CAN_FilterActivation = ENABLE;
	CAN_FilterInit(&CAN_FilterInitStructure);

	//инициализация платы в системе
	TxMessage.IDE = CAN_Id_Extended;
	TxMessage.ExtId = 0x18EAFFFE;
	TxMessage.DLC = 3;
	TxMessage.Data[0] = 0x00;
	TxMessage.Data[1] = 0xEE;
	TxMessage.Data[2] = 0x00;
	CAN_Transmit(CAN, &TxMessage);
	if (CAN_GetLastErrorCode(CAN)) {

	}
	//задержка 400мс
	delay_us(100000);

	TxMessage.IDE = CAN_Id_Extended;
	TxMessage.ExtId = 0x18EEFF00 | pto_address;
	TxMessage.DLC = 8;
	TxMessage.Data[0] = 0x80;
	TxMessage.Data[1] = 0x06;
	TxMessage.Data[2] = 0x00;
	TxMessage.Data[3] = 0x00;
	TxMessage.Data[4] = 0x11;
	TxMessage.Data[5] = 0x22;
	TxMessage.Data[6] = 0x33;
	TxMessage.Data[7] = 0x44;
	CAN_Transmit(CAN, &TxMessage);
	if (CAN_GetLastErrorCode(CAN)) {

	}

	//задержка 250мс
	delay_us(2000);

	/* Enable FIFO 0 message pending Interrupt */
	CAN_ITConfig(CAN, CAN_IT_FMP0, ENABLE);

	//вектор
	NVIC_InitStructure.NVIC_IRQChannel = CEC_CAN_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPriority = 0x00;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);



}
