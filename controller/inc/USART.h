#ifndef NOTEBOOK_USART_H
#define NOTEBOOK_USART_H

#include "stm32f4xx.h"
#include "stm32f4xx_usart.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_rcc.h"
#include "misc.h"

void InitUSART6(void);
void SendCharArrayUSART6(char arr[], int len);

	
#endif
