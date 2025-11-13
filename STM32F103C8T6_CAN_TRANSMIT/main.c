

#include "stm32f10x_rcc.h"
#include "stm32f10x_flash.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_tim.h"
#include "stm32f10x_can.h"
#include <stdbool.h>
static void RCC_Setup_72MHz(void)
{
    RCC_DeInit();
    RCC_HSEConfig(RCC_HSE_ON);
    if (RCC_WaitForHSEStartUp() != SUCCESS) { while (1); }

    FLASH_PrefetchBufferCmd(FLASH_PrefetchBuffer_Enable);
    FLASH_SetLatency(FLASH_Latency_2);

    RCC_HCLKConfig(RCC_SYSCLK_Div1);   // AHB  = 72 MHz
    RCC_PCLK1Config(RCC_HCLK_Div2);    // APB1 = 36 MHz
    RCC_PCLK2Config(RCC_HCLK_Div1);    // APB2 = 72 MHz

    RCC_ADCCLKConfig(RCC_PCLK2_Div6);  // 12 MHz

    RCC_PLLConfig(RCC_PLLSource_HSE_Div1, RCC_PLLMul_9);
    RCC_PLLCmd(ENABLE);
    while (RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET);

    RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);
    while (RCC_GetSYSCLKSource() != 0x08);
}

static void Enable_Periph_Clocks(void)
{
    // Call AFTER RCC_Setup_72MHz()
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOC, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2 | RCC_APB1Periph_CAN1, ENABLE);
}

static void Timer2_Init_1ms(void)
{
    TIM_TimeBaseInitTypeDef tb;

    tb.TIM_Prescaler         = 7199;      
    tb.TIM_CounterMode       = TIM_CounterMode_Up;
    tb.TIM_Period            = 0xffff;      // ~65s span
    tb.TIM_ClockDivision     = TIM_CKD_DIV1;
    tb.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM2, &tb);
    TIM_Cmd(TIM2, ENABLE);
}

void delay_ms(uint32_t ms)
{
	bool Flasher = true;
	uint32_t ms_1 = 0 ;
	while(ms > ms_1){
		
	TIM_SetCounter(TIM2,0);
	while(Flasher){
		
			if (TIM_GetCounter(TIM2) > 10000){
			
					Flasher = false ;
				} ;
	
			} ;
		
		ms_1 ++ ;
	}

}

static void GPIO_Init_All(void)
{
    GPIO_InitTypeDef io;

    // PC13: LED (push-pull)
    io.GPIO_Pin   = GPIO_Pin_13;
    io.GPIO_Mode  = GPIO_Mode_Out_PP;
    io.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOC, &io);

    // CAN1 default pins: PA12 = TX (AF_PP), PA11 = RX (IN_FLOATING)
    io.GPIO_Pin   = GPIO_Pin_12;               // TX
    io.GPIO_Mode  = GPIO_Mode_AF_PP;
    io.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &io);

    io.GPIO_Pin   = GPIO_Pin_11;               // RX
    io.GPIO_Mode  = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &io);
}

static void CAN1_Init_500k(void)
{
    
    CAN_FilterInitTypeDef f;

    CAN_InitTypeDef CAN_InitStructure;

    // Configure CAN settings (CAN Initialization)
    CAN_InitStructure.CAN_TTCM = DISABLE;     // Time Triggered Communication Mode
    CAN_InitStructure.CAN_ABOM = ENABLE;      // Automatic Bus-Off Management
    CAN_InitStructure.CAN_AWUM = ENABLE;      // Automatic Wake-Up Mode
    CAN_InitStructure.CAN_NART = DISABLE;     // No Automatic Retransmission
    CAN_InitStructure.CAN_RFLM = DISABLE;     // Receive FIFO Locked Mode
    CAN_InitStructure.CAN_TXFP = DISABLE;     // Transmit FIFO Priority
    CAN_InitStructure.CAN_Mode = CAN_Mode_Normal;  // Normal Mode

    // Configure bit timing (bit timing segments)
    CAN_InitStructure.CAN_SJW = CAN_SJW_1tq;  // Synchronization Jump Width = 1 time quantum
    CAN_InitStructure.CAN_BS1 = CAN_BS1_6tq;  // Bit Segment 1 = 6 time quanta
    CAN_InitStructure.CAN_BS2 = CAN_BS2_8tq;  // Bit Segment 2 = 8 time quanta
    CAN_InitStructure.CAN_Prescaler = 6;      // Time clock = 36 MHz / (Prescaler * 12) = 500 Kbps



    while (CAN_Init(CAN1, &CAN_InitStructure) == CAN_InitStatus_Failed){};   // << REQUIRED >>

    // Accept all
    f.CAN_FilterNumber = 0;
    f.CAN_FilterMode   = CAN_FilterMode_IdMask;
    f.CAN_FilterScale  = CAN_FilterScale_32bit;
    f.CAN_FilterIdHigh = (1<<13);
    f.CAN_FilterIdLow  = 0x0000;
    f.CAN_FilterMaskIdHigh = 0xE000;
    f.CAN_FilterMaskIdLow  = 0x0000;
    f.CAN_FilterFIFOAssignment = CAN_FIFO0;
    f.CAN_FilterActivation = ENABLE;
    CAN_FilterInit(&f);
}

static uint8_t CAN_TransmitMessage(uint32_t id, const uint8_t *data, uint8_t length)
{
    CanTxMsg TxMessage;

    TxMessage.StdId = 0x123;  // S? d?ng d?nh danh 11-bit
    TxMessage.RTR = CAN_RTR_DATA;  // Data Frame
    TxMessage.IDE = CAN_ID_STD;  // Standard ID
    TxMessage.DLC = length;  // S? byte d? li?u

    // Copy d? li?u vào tru?ng d? li?u c?a khung
    for (int i = 0; i < length; i++) {
        TxMessage.Data[i] = data[i];
		}
		
		uint8_t mb = CAN_Transmit(CAN1, &TxMessage);
    uint32_t guard = 0;
    while (CAN_TransmitStatus(CAN1, mb) != CANTXOK && guard++ < 0xFFFF) { }
    return (CAN_TransmitStatus(CAN1, mb) == CANTXOK);

}

int main(void)
{
    RCC_Setup_72MHz();
    Enable_Periph_Clocks();
    GPIO_Init_All();
    Timer2_Init_1ms();
    CAN1_Init_500k();

    uint8_t data1[] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11 };
    uint8_t data2[] = {0x07, 0x08, 0x05, 0x06, 0x10, 0x20, 0x13, 0x18 };
		uint8_t data[8] ;
    while (1) {


        GPIO_SetBits(GPIOC, GPIO_Pin_13);
         delay_ms(10);

        CAN_TransmitMessage(0x123, data2, 8);
        GPIO_ResetBits(GPIOC, GPIO_Pin_13);
        delay_ms(5000);
				CAN_TransmitMessage(0x123, data1, 8);
    }
}
