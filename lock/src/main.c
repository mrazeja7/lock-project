#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "stm32f4xx.h"
#include "stm32f4_discovery.h"
#include "USART.h"
#include "circularBuffer.h"
#include "ECE631JSON.h"
#include "stm32f4xx_exti.h"

static uint32_t ticks = 0;
static uint32_t lastTicks = 0;
static uint32_t recvTicks = 0;
static uint32_t sendTicks = 0;
short recvOn = 0;
short sendOn = 0;
short recvLedOn = 0;
short sendLedOn = 0;
static uint32_t USARTCount = 0;

commBuffer_t buffer;
commBuffer_t txbuffer;
jsmn_parser parser;

int main(void)
{
	InitBuffer(&buffer);
	InitBuffer(&txbuffer);
	jsmn_init(&parser);
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
	while(1)
	{
		handleLeds();
		if( ticks - lastTicks >=2000)
			lastTicks = ticks;
	}
}

/* Wonderful piece of spaghetti code I produced for the fifth lab assignment.
 * Keeping it here because it works.
 */
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

/* gets rid of all occurences of "{ and }" pairs
 * (and replaces them with { and } respectively),
 * as the jsmn parser has trouble with those.
 */
void unquoteBraces(char *str)
{
	int oldLen = strlen(str);
	int newLen = 0;
	char newStr[512] = {0};

	for (int i = 0; i < oldLen -1; ++i)
	{
		if (str[i] == '\"' && str[i+1] == '{')
			continue;
		else if (str[i] == '}' && str[i+1] == '\"')
		{
			newStr[newLen++] = '}';
			i++;
		}
		else
			newStr[newLen++] = str[i];
	}
	newStr[newLen++] = str[oldLen-1];
	memcpy(str, newStr, oldLen);
}

/* gets rid of all backslashes as the jsmn parser sometimes has trouble parsing them. */
void stripEscapes(char *str)
{
	int oldLen = strlen(str);
	int newLen = 0;
	char newStr[512] = {0};

	for (int i = 0; i < oldLen; ++i)
	{
		if (str[i] != '\\')
			newStr[newLen++] = str[i];
	}
	memcpy(str, newStr, oldLen);
}

int8_t lockStatus;
/*
 * Checks the incoming JSON string for a flip lock command.
 * If the check passes, flips the lockStatus variable which indicates if the lock is engaged.
 */
void setLockStatus()
{
	STM_EVAL_LEDToggle(LED3);
	if (lockStatus == 0)
		lockStatus = 1;
	else
		lockStatus = 0;
}

/* Checks if the received message contains a command to flip the lock's status (ie. lock or unlock the lock) */
void checkMessage(char *jsonString)
{
	jsmn_parser p;
	jsmn_init(&p);
	jsmntok_t tokens[50];
	stripEscapes(jsonString);
	unquoteBraces(jsonString);
	int32_t numtok = jsmn_parse(&p, jsonString, strlen(jsonString), tokens, 50);
	char value[32] = {0};
	extract_value(jsonString,"FlipLock",value,numtok,tokens);

	if (strcmp(value, "true") == 0)
	{
		setLockStatus();
		sendStatus();
	}
}

void handleMsg(char *msg)
{
	checkMessage(msg);
}

/* constructs and publishes a message onto the MQTT channel.
 * Uses multiple putStr() calls because I couldn't get sprintf to work for some reason.
 */
void publishMQTT(char *msg)
{
	// sprintf doesn't work?
	char resp[512] = "{\"Action\":\"MQTTPub\",\"MQTT\":{\"Topic\":\"lock\",\"Message\":\"";
	putStr(&txbuffer, resp, strlen(resp));
	putStr(&txbuffer, msg, strlen(msg));
	char resp3[256] = "\"}}\n";
	putStr(&txbuffer, resp3, strlen(resp3));
	USART_ITConfig(USART6, USART_IT_TXE, ENABLE);
	sendTicks = ticks;
	sendOn = 1;
}

/* send a non-MQTT message via UART */
void sendMsg(char *msg)
{
	putStr(&txbuffer, msg, strlen(msg));
	USART_ITConfig(USART6, USART_IT_TXE, ENABLE);
	sendTicks = ticks;
	sendOn = 1;
}

/* DRY am I right?
 * could use a sprintf redo
 */
void sendStatus()
{
	if (lockStatus == 0)
	{
		char msg[128] = "{\\\"Type\\\":\\\"Status\\\",\\\"LockStatus\\\":\\\"unlocked\\\"}";
		publishMQTT(msg);
	}
	else if (lockStatus == 1)
	{
		char msg[128] = "{\\\"Type\\\":\\\"Status\\\",\\\"LockStatus\\\":\\\"locked\\\"}";
		publishMQTT(msg);
	}
	else
	{
		char msg[128] = "{\\\"Type\\\":\\\"Status\\\",\\\"LockStatus\\\":\\\"unknown\\\"}";
		publishMQTT(msg);
	}
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
		}
	}
	if( USART_GetITStatus(USART6, USART_IT_TXE) )
	{
		if (!bufferEmpty(&txbuffer))
			USART_SendData(USART6, getChar(&txbuffer));
		else USART_ITConfig(USART6, USART_IT_TXE, DISABLE);
	}
}
