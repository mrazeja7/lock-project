//ART Sample Project
/**
******************************************************************************
* @file main.c
* @author Ac6
* @version V1.0
* @date 01-December-2013
* @brief Default main function.
******************************************************************************
*/
#include <string.h>
#include "stm32f4xx.h"
#include "stm32f4_discovery.h"
#include "USART.h"
#include "circularBuffer.h"
#include "ECE631JSON.h"

static uint32_t ticks = 0;
static uint32_t lastTicks = 0;
static uint32_t recvTicks = 0;
static uint32_t sendTicks = 0;
short recvOn = 0;
short sendOn = 0;
short recvLedOn = 0;
short sendLedOn = 0;
static uint32_t USARTCount = 0;

#define PRESSED_BUTTON_NONE 0x00
#define PRESSED_BUTTON_USER 0x01

commBuffer_t buffer;
commBuffer_t txbuffer;

jsmn_parser parser;

//UART6
int main(void)
{
	InitBuffer(&buffer);
	InitBuffer(&txbuffer);
	jsmn_init(&parser);
	memset(buffer.buffer, 0, MAXCOMMBUFFER+1);
	InitUSART6();

	STM_EVAL_PBInit(BUTTON_USER, BUTTON_MODE_GPIO);

	/*Setup SysTick */
	ticks = 0;
	lastTicks = 0;
	SystemCoreClockUpdate();
	/*Config so 1 tick = 1ms or 1000tick = 1 second*/
	if( SysTick_Config(SystemCoreClock / 1000))
		while(1);
	// setup LEDs
	STM_EVAL_LEDInit(LED3); //orange
	STM_EVAL_LEDInit(LED4); //green
	STM_EVAL_LEDInit(LED5); //red
	STM_EVAL_LEDInit(LED6); //blue

	//char* startText= "{\"Action\":\"Debug\",\"Info\":\"Testing UART6\"}";
	//SendCharArrayUSART6(startText,strlen(startText));
	while(1){
		handleLeds();
		if( ticks - lastTicks >=2000) {
			lastTicks = ticks;
		}

		checkButton();
	}
}

void handleLeds()
{
	if (ticks % 1000 == 0)
	{
		//blink status LED
		STM_EVAL_LEDToggle(LED6);
		return;
	}
	if (recvTicks + 500 < ticks)
		recvOn = 0;

	if (sendTicks + 500 < ticks)
		sendOn = 0;

	if (recvLedOn && !recvOn && (recvTicks + 500 < ticks)) //turn off RX LED
	{
		STM_EVAL_LEDToggle(LED5);
		recvTicks = 0;
		recvLedOn = 0;
	}
	else if (!recvLedOn && recvOn && (recvTicks + 500 > ticks)) // turn on RX LED
	{
		STM_EVAL_LEDToggle(LED5);
		recvLedOn = 1;
	}

	if (sendLedOn && !sendOn && (sendTicks + 500 < ticks)) //turn off TX LED
	{
		STM_EVAL_LEDToggle(LED4);
		sendTicks = 0;
		sendLedOn = 0;
	}
	else if (!sendLedOn && sendOn && (sendTicks + 500 > ticks)) // turn on TX LED
	{
		STM_EVAL_LEDToggle(LED4);
		sendLedOn = 1;
	}
}
void SysTick_Handler(void) {
	ticks++;
}

void stripEscapes(char *str)
{
	int oldLen = strlen(str);
	int newLen = 0;
	char newStr[256] = {0};

	for (int i = 0; i < oldLen; ++i)
	{
		if (str[i] != '\\')
			newStr[newLen++] = str[i];
	}
	memcpy(str, newStr, oldLen);
}

int8_t getLockStatus(char *jsonString)
{
	uint32_t tokenSize = 10;
	jsmntok_t *tokens = (jsmntok_t *)malloc(tokenSize * sizeof(jsmntok_t));
	stripEscapes(jsonString);
	uint32_t numtok = jsmn_parse(&parser, jsonString, strlen(jsonString), tokens, tokenSize);
	char status[16] = {0};
	extract_value(jsonString,"LockStatus",status,numtok,tokens);
	free(tokens);
	if (strcmp(status, "locked") == 0)
		return 1;
	else if (strcmp(status, "unlocked") == 0)
		return 0;

	return -1;
}

uint8_t ledStatus;

void checkStatus(char *msg)
{
	int8_t status = getLockStatus(msg);
	if (status != -1 && status != ledStatus)
		STM_EVAL_LEDToggle(LED3);
}

void handleMsg(char *msg)
{
	checkStatus(msg);
}

void flipLock()
{
	char msg[256] = "{\"Type\":\"Command\",\"FlipLock\":\"true\"}\n";

	putStr(&txbuffer, msg, strlen(msg));
}

void checkButton()
{
	return;
	uint8_t buttonStatus;
	//buttonStatus = Read_User_Button();
	if(buttonStatus == PRESSED_BUTTON_USER)
		flipLock();
}

/*
* This is a simple IRQ handler for USART6. A better design would
* be to have a circular list of strings. Head index pointing to
* where chars added. Tail index oldest string to be processed
*/
void USART6_IRQHandler(void) {
	USARTCount++;
	// check if the USART6 receive interrupt flag was set
	if( USART_GetITStatus(USART6, USART_IT_RXNE) ){
		char t = USART6->DR;
		putChar(&buffer, t);
		USART_ITConfig(USART6, USART_IT_TXE, ENABLE); // enable the USART6 TXE interrupt
		recvTicks = ticks;
		recvOn = 1;

		if (t == '\n')
		{
			char res[MAXCOMMBUFFER+1];
			memset(res, 0, MAXCOMMBUFFER+1);
			getStr(&buffer, res);
			handleMsg(res);

			putStr(&txbuffer, res, strlen(res));

			sendTicks = ticks;
			sendOn = 1;
		}
	}
	if( USART_GetITStatus(USART6, USART_IT_TXE) )
	{
		if (!bufferEmpty(&txbuffer))
			USART_SendData(USART6, getChar(&txbuffer));
		else USART_ITConfig(USART6, USART_IT_TXE, DISABLE);
	}
}
