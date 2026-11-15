/*
 *    Author: Nicu Pavel <npavel@mini-box.com>
 *    Copyright (c) 2020 Mini-Box.com
 *    Licensed under the terms of the GNU Lesser General Public License v2.1
 *  	http://www.opensource.org/licenses/lgpl-2.1.php 
 */

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include <ctype.h>

#include "HIDInterface.h"
#include <vector>

#define OPENUPS2_CMD_OUT					0xB1


HIDInterface::HIDInterface(USBHID *hidDevice) {
	d = hidDevice;
	m_nMaxMessages = MAX_MESSAGE_CNT;
	m_bEmptyFieldOk = true;
	memset(m_chPackages, 0, SETTINGS_PACKS * 16);
}

HIDInterface::~HIDInterface() {
	
}

void HIDInterface::printConfiguration(bool withComments)
{
	int var_max = 256;
	char name[256];
	char unit[64];
	char value[256];
	char comment[1024];

	for (int i = 0; i < var_max; i++)
	{
		if (getUPSVariableData(i, name, value, unit, comment))
		{
			if (withComments) {
				fprintf(stdout, "%d. %s: %s # %s %s\n", i, name, value, unit, comment);
				fprintf(stdout, "%s\n", "---------------------------------------------------------------------------------------------------------------");
			} else {
				fprintf(stdout, "%s=%s\n", name, value);
			}			
		}
	}
}

int HIDInterface::sendMessageWithBuffer(unsigned char cType, unsigned int buflen, unsigned char* buffer, unsigned int len, ...)
{
	std::vector<unsigned char> mesg(buflen + len + 1);
	mesg[0] = cType;

	va_list args;
	va_start(args, len);
	for (unsigned int cnt = 0; cnt < len; cnt++)
		mesg[1 + cnt] = va_arg(args, unsigned int);
	va_end(args);

	memcpy(&mesg[1 + len], buffer, buflen);
	return d->writeInterrupt(mesg.data(), mesg.size(), true);
}

int HIDInterface::sendMessage(unsigned char cType, unsigned int len, ...)
{
	std::vector<unsigned char> mesg(len + 1);
	mesg[0] = cType;

	va_list args;
	va_start(args, len);
	for (unsigned int cnt = 0; cnt < len; cnt++)
		mesg[1 + cnt] = va_arg(args, unsigned int);
	va_end(args);

	return d->writeInterrupt(mesg.data(), mesg.size(), true);
}

int HIDInterface::sendCommand(unsigned char command, unsigned char value)
{
	return sendMessage(COMMON_CMD_OUT, 3, command, value, 0); // OpenUPS, OpenUPS2 and NUCUPS share same CMD_OUT value
}
int HIDInterface::sendCommandEx(unsigned char command, unsigned char value1, unsigned char value2)
{
	return sendMessage(COMMON_CMD_OUT, 3, command, value1, value2);
}

int HIDInterface::recvMessage(unsigned char *buffer)
{
	int ret = d->readInterrupt(buffer);	
		    	
	if (ret <= 0 ) {
		fprintf(stderr, "Read error [0x%02x]: %d\n", buffer[0], ret);
	}
	return ret;
}

int HIDInterface::recvMessage(unsigned char *buffer, unsigned int len)
{
	int ret = d->readInterrupt(buffer, len);	
		    	
	if (ret <= 0 ) {
		fprintf(stderr, "Read error [0x%02x]: %d/%d\n", buffer[0], ret, len);
	}
	return ret;
}

int HIDInterface::GetMessageIdxByName(const char* name)
{
	if (!name)
		return -1;

	for (unsigned int i = 0; i < m_nMaxMessages; i++)
	{
		ATXMSG msg = GetMessages()[i];
		//fprintf(stderr, "GetMessage %d: %s nLen: %d cmp: %d\n", i, msg.strName, msg.nLen, strcasecmp(msg.strName, name));
		if (msg.nLen != 0)
			if (strcasecmp(msg.strName, name) == 0) 
				return i;
	}
	
	return -1;
}

int HIDInterface::varsToFile(const char *filename, bool withComments) {
	if (!filename) {
		return -1;
	}

	FILE *f = fopen(filename, "w");
	
	if (!f) {
		return -2;
	}

	int var_max = 256;
	char name[256];
	char unit[64];
	char value[256];
	char comment[1024];

	for (int i = 0; i < var_max; i++)
	{
		if (getUPSVariableData(i, name, value, unit, comment))
		{
			if (withComments) {
				// Replace \r \n with comment #, assumes windows style line break		
				char *p = comment;
				while (p && *p != '\0') {				
					if (*p == '\r') {
						if (*(p + 1) == '\n') {
							*p = '\n';
							*(p + 1) = '#';
						} 					
					}				
					p++;
				}			
				fprintf(f, "# %s %s %s\n\n", name, unit, comment);
			}
			fprintf(f, "%s=%s\n", name, value);
		}
	}
	fclose(f);
	return 0;
}

int HIDInterface::fileToVars(const char *filename) {
	if (!filename) {
		return -1;
	}

	FILE *f = fopen(filename, "r");

	if (!f) {
		fprintf(stderr, "Failed to open %s for writting\n", filename);
		return -2;
	}

	int ret;
	char name[256];
	char val[256];
	char buf[1024];

	#define MAX_LINE 1024
	char line[MAX_LINE];
	int len;
	int lineno = 0, last = 0, vars = 0;

	while (fgets(line, MAX_LINE, f) != NULL) {
		len = strlen(line) - 1;
		if (line[len] != '\n' && !feof(f)) {
			fprintf(stderr, "Line %d too long\n", lineno);
			fclose(f);
			return -3;
		}
		lineno++;
		
		/* Remove \n and spaces from the end */
		while ((len >= 0) && ((line[len]=='\n') || (isspace(line[len])))) {
            line[len] = 0 ;
            len--;
        }

        if (len < 0) {
            len = 0;
			continue;
        }
		if (sscanf(line, "%32[^=]=%32[^\n]%*c", name, val) == 2){
			//fprintf(stdout, "Found var: %s value: %s\n", name, val);
			int idx;
			idx = GetMessageIdxByName(name);
			if (idx >= 0) {
				if (setVariableData(idx, val)) {
					//fprintf(stdout, "Succesfully converted %s to %s\n", name, val);
				} else {
					fprintf(stderr, "Error converting %s to %s\n", name, val);
				}				
			} else {
				fprintf(stderr, "Variable '%s' unknown\n", name);
			}
			vars++;
		} 
	}
	fprintf(stdout, "Found %d vars\n", vars);
	fclose(f);

	return 0;
}

struct EXEC* HIDInterface::parseCommand(char *cmdexpr) {
	if (!cmdexpr) 
		return NULL;

	struct EXEC *exec = (struct EXEC *) malloc (sizeof (struct EXEC));
	unsigned int idx = 0;
	char *token;

	while ((token = strsep(&cmdexpr, CMD_DELIM))) {
		if (idx == 0) {
			strncpy(exec->cmd, token, MAX_CMD_LEN);
			exec->cmd[MAX_CMD_LEN] = '\0';
			fprintf(stdout, "Found command: %s\n", exec->cmd);
		} else {
			strncpy(exec->params[idx - 1], token, MAX_PARAM_LEN);
			exec->params[idx - 1][MAX_PARAM_LEN] = '\0';
			fprintf(stdout, "Found param %d: %s\n", idx, exec->params[idx - 1]);
		}
		idx++;
	}
	exec->params_count = idx;
	return exec;
}

bool HIDInterface::executeCommand(char *cmdexpr) {
	fprintf(stderr, "Commands not implemented !\n");	
	return false;
}
bool HIDInterface::readFieldValue(int mesg_no, char *str, unsigned char &c1, unsigned char &c2, unsigned char &c3, unsigned char &c4)
{
	const ATXMSG &msg = GetMessages()[mesg_no];
	return readOneValue(str, msg.nReadMode, msg.dMultiplier, msg.nLen, c1, c2, c3, c4);
}

bool HIDInterface::setVariableData(int mesg_no, char *str)
{
	ATXMSG &msg = GetMessages()[mesg_no];
	int len = msg.nLen;

	if (len == 0)
		return m_bEmptyFieldOk;

	unsigned char c1 = 0;
	unsigned char c2 = 0;
	unsigned char c3 = 0;
	unsigned char c4 = 0;

	if (!readFieldValue(mesg_no, str, c1, c2, c3, c4))
		return false;

	switch (len)
	{
	case 1:
		m_chPackages[msg.nIndex] = c2;
		break;
	case 2:
		m_chPackages[msg.nIndex] = c1;
		m_chPackages[msg.nIndex + 1] = c2;
		break;
	case 4:
		m_chPackages[msg.nIndex] = c1;
		m_chPackages[msg.nIndex + 1] = c2;
		m_chPackages[msg.nIndex + 2] = c3;
		m_chPackages[msg.nIndex + 3] = c4;
		break;
	}
	return true;
}

unsigned char HIDInterface::getUPSVariableData(unsigned int cnt, char *name, char *value, char *unit, char *comment)
{
	if (cnt >= m_nMaxMessages)
		return 0;

	ATXMSG &msg = GetMessages()[cnt];
	if (msg.nLen == 0)
		return 0;

	if (name)
		strcpy(name, msg.strName);
	if (unit)
		strcpy(unit, msg.strUnit);
	if (comment)
		strcpy(comment, msg.strText);

	if (value)
		convertOneValue2String(value, msg.nLen, msg.nIndex, msg.nReadMode, msg.dMultiplier);

	return msg.nReadMode;
}
