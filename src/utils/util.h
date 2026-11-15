#ifndef _UTIL_H_
#define _UTIL_H_
#pragma once

#include <string.h>
#include <stdlib.h>
#include <stdio.h>


unsigned char bcd2hex (unsigned char x);
void char2bin(char* destination, unsigned char ch);
unsigned char bin2char(char* str, bool* ok);

void str_mid(char* dest, char* source, int from, int cnt);
void str_left(char* dest, char* source, int cnt);
void str_right(char* dest, char* source, int cnt);

#ifdef _DEBUG
#include <stdarg.h>
void DBGOutput(const char* szFormat, ...);
#else
#define DBGOutput 
#endif

#define TRACE DBGOutput

#endif //_UTIL_H_