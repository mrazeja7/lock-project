#ifndef JAH_CIRCULAR_BUFFER_C
#define JAH_CIRCULAR_BUFFER_C

#include "circularBuffer.h"

//Initializes the struct commBuffer_t to zero
void InitBuffer(commBuffer_t* comm)
{
	comm->head = 0;
	comm->tail = 0;
	memset(comm->buffer, 0, MAXCOMMBUFFER+1);
}
//Test if a complete string is in buffer delimiter is \n
uint8_t haveStr(commBuffer_t* comm)
{
	int i = comm->tail;
	while (i != comm->head)
	{
		if (comm->buffer[i] == '\n')
			return 1;
		i = (i+1) % (MAXCOMMBUFFER);
	}
	return 0;
}
//Put character in buffer and update head
void putChar(commBuffer_t* comm, char ch)
{
	uint32_t newhead = ((comm->head + 1) % MAXCOMMBUFFER);
	if (newhead == comm->tail)
		return;
	comm->buffer[comm->head] = ch;
	comm->head = newhead;
}
//Get character from buffer and update tail;
char getChar(commBuffer_t* comm)
{
	if (comm->head == comm->tail)
		return '\0';
	uint32_t newtail = ((comm->tail + 1) % MAXCOMMBUFFER);
	char val = comm->buffer[comm->tail];
	comm->tail = newtail;
	return val;
}
//put C string into buffer
void putStr(commBuffer_t* comm, char* str, uint8_t length)
{
	int i;
	for (i = 0; i < length; ++i)
	{
		putChar(comm, str[i]);
	}
}
//get C string from buffer
void getStr(commBuffer_t* comm, char* str)
{
	int i = 0;
	char res[MAXCOMMBUFFER+1];
	while(1)
	{
		char r = getChar(comm);
		str[i++] = r;
		if (r == '\n' || i == MAXCOMMBUFFER)
			return;
	}
}
//get Size of Buffer
int getBufferSize(commBuffer_t* comm)
{
	return MAXCOMMBUFFER;
}
int bufferEmpty(commBuffer_t* comm)
{
	if (comm->head == comm->tail)
		return 1;
	return 0;
}
#endif

