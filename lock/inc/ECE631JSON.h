/*
Library for interpreting and sending JSON data in C
Dependent on the JSMN library by Serge Zaitsev distributed under the MIT license
Authors: Phillip Dix
Date: April 25, 2017
*/
#ifndef ECE631JSON_LIB
#define ECE631JSON_LIB

#include "jsmn.h"


int check_JSMN_error(int numtok, char* location);
void extract_value(char* JSONSTR, char* key, char *target, int numtok1, jsmntok_t *tokens1);
int pack_json(char* format, char* target, ...);
void output_string(char* s);

#endif
