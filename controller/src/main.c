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

		checkButton();
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

/* Gets rid of all occurences of "{ and }" pairs
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

/* Gets rid of all backslashes as the jsmn parser sometimes has trouble parsing them. */
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

/*
 * Checks the incoming JSON string for a lock status message.
 * Returns 1 if the lock is locked, 0 if unlocked, -1 if the message doesn't contain a lock status.
 */
int8_t getLockStatus(char *jsonString)
{
	jsmn_parser p;
	jsmn_init(&p);
	jsmntok_t tokens[50];
	stripEscapes(jsonString);
	unquoteBraces(jsonString);
	int32_t numtok = jsmn_parse(&p, jsonString, strlen(jsonString), tokens, 50);
	char status[32] = {0};
	extract_value(jsonString,"LockStatus",status,numtok,tokens);
	if (strcmp(status, "locked") == 0)
		return 1;
	if (strcmp(status, "unlocked") == 0)
		return 0;

	return -1;
}

int8_t ledStatus;

/* Compares the lock's status to what is displayed via the controller's LED.
 * If states differ, changes controller's LED to match lock's engagement.
 */
void checkStatus(char *msg)
{
	int8_t status = getLockStatus(msg);
	if (status != -1 && status != ledStatus)
	{
		ledStatus = status;
		STM_EVAL_LEDToggle(LED3);
	}
}

void handleMsg(char *msg)
{
	checkStatus(msg);
}

/* Constructs and publishes a message onto the MQTT channel.
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

/* Send a non-MQTT message via UART */
void sendMsg(char *msg)
{
	putStr(&txbuffer, msg, strlen(msg));
	USART_ITConfig(USART6, USART_IT_TXE, ENABLE);
	sendTicks = ticks;
	sendOn = 1;
}

/* Publishes a message that tells the lock to change its state */
void flipLock()
{
	char msg[128] = "{\\\"Type\\\":\\\"Command\\\",\\\"FlipLock\\\":\\\"true\\\"}";
	publishMQTT(msg);
}

/* The button can only be detected once every 1 second to prevent spamming */
uint32_t lastButton;
uint8_t debounce()
{
	if (lastButton + 1000 <= ticks)
		return 1;
	return 0;
}

/* Checks if the user (==blue) button is currently pressed and can be used. */
void checkButton()
{
	if( debounce() && GPIOA->IDR & 0x0001)
	{
		lastButton = ticks;
		flipLock();
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
