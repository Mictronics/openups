#include "HIDOpenUPSBase.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include "util.h"

HIDOpenUPSBase::HIDOpenUPSBase(USBHID *d): HIDInterface(d)
{
	m_bReportWrittenPages = true;
	m_bEmptyFieldOk = false;
	m_ulSettingsAddr = OPENUPS_SETTINGS_ADDR_START;
}

HIDOpenUPSBase::~HIDOpenUPSBase()
{
}

void HIDOpenUPSBase::ReadConfigurationMemory() 
{
	unsigned char recv[32];

	m_ulSettingsAddr = OPENUPS_SETTINGS_ADDR_START;
	memset(m_chPackages, 0, SETTINGS_PACKS * 16);

	while (m_ulSettingsAddr < OPENUPS_SETTINGS_ADDR_END)
	{
		sendMessage(OPENUPS_MEM_READ_OUT, 4, m_ulSettingsAddr & 0xFF, (m_ulSettingsAddr >> 8) & 0xFF, 0x00, 0x10);
		recvMessage(recv);
		parseMessage(recv);
		m_ulSettingsAddr += 16;
	}
}

void HIDOpenUPSBase::EraseConfigurationMemory() 
{
	unsigned char recv[32];
	int ret;

	m_ulSettingsAddr = OPENUPS_SETTINGS_ADDR_START;
	sendMessage(OPENUPS_MEM_ERASE, 4, m_ulSettingsAddr & 0xFF, (m_ulSettingsAddr >> 8) & 0xFF, 0x00, 0x40);

	int retries = 5;
	while ((ret = recvMessage(recv) <= 0) && retries > 0) {
		retries--;
		usleep(500);
		fprintf(stderr, "Erase 0x%02x retry %d/5\n", recv[0], retries);
	}

	if (retries <= 0) {
		fprintf(stderr, "Error waiting for erase operation to finish\n");
	} else {
		fprintf(stderr, "Successfully erased (0x%02x) configuration memory\n", recv[0]);
	}
}

void HIDOpenUPSBase::WriteConfigurationMemory()
{
	unsigned char recv[32];
	int ret;

	m_ulSettingsAddr = OPENUPS_SETTINGS_ADDR_START;
	while (m_ulSettingsAddr < OPENUPS_SETTINGS_ADDR_END)
	{
		sendMessageWithBuffer(OPENUPS_MEM_WRITE_OUT, 16, m_chPackages+(m_ulSettingsAddr-OPENUPS_SETTINGS_ADDR_START), 4, m_ulSettingsAddr & 0xFF, (m_ulSettingsAddr >> 8) & 0xFF, 0x00, 0x10);
		ret = recvMessage(recv);
		
		if (ret <= 0 || recv[0] != OPENUPS_MEM_WRITE_IN) {
			fprintf(stderr, "Error (%d, 0x%02x) writing configuration variables , aborting ...\n", ret, recv[0]);
			break;
		} else {
			if (m_bReportWrittenPages) fprintf(stderr, "Wrote page 0x%lx of configuration\n", m_ulSettingsAddr);
		}		
		m_ulSettingsAddr += 16;
	}
}

// Linear interpolation in the thermistor table, in whole degrees C.
int HIDOpenUPSBase::thermistor2Temp(unsigned int termistor)
{
	unsigned int *consts = GetTermistorConsts();

	if (termistor <= consts[0])
		return -40;
	if (termistor >= consts[TERMISTOR_CONSTS - 1])
		return 125;

	int pos = -1;
	for (int i = TERMISTOR_CONSTS - 1; i >= 0; i--)
	{
		if (termistor >= consts[i])
		{
			pos = i;
			break;
		}
	}

	if (termistor == consts[pos])
		return pos * 5 - 40;

	int t1 = pos * 5 - 40;
	int t2 = (pos + 1) * 5 - 40;
	float dtemp = ((float)termistor - (float)consts[pos]) * ((float)t2 - (float)t1) / ((float)consts[pos + 1] - (float)consts[pos]);
	return (int)ceil(dtemp) + t1;
}

float HIDOpenUPSBase::convertOneValue2Float(unsigned char *buffer, int nLen, int nIndex, int nReadMode, double dMultiplier)
{
	double value = -1;

	if (nLen == 1)
		value = (double)buffer[nIndex] * dMultiplier;
	else if (nLen == 2)
		value = (double)((((int)buffer[nIndex + 1]) << 8) | buffer[nIndex]) * dMultiplier;
	else if (nLen == 4)
		value = (double)((((int)buffer[nIndex + 3]) << 24) | (((int)buffer[nIndex + 2]) << 16) | (((int)buffer[nIndex + 1]) << 8) | buffer[nIndex]) * dMultiplier;

	if (deviceValue2Float(buffer, nLen, nIndex, nReadMode, dMultiplier, value))
		return (float)value;

	switch (nReadMode)
	{
	case 10:
		if (nLen == 1)
			value = (double)buffer[nIndex] * GetConstant((int)dMultiplier);
		else
			value = (double)((((int)buffer[nIndex + 1]) << 8) | buffer[nIndex]) * GetConstant((int)dMultiplier);
		break;
	case 15:
		value = thermistor2Temp(((unsigned int)buffer[nIndex + 1] << 8) | buffer[nIndex]);
		break;
	}

	return (float)value;
}

void HIDOpenUPSBase::convertOneValue2String(char *destination, int nLen, int nIndex, int nReadMode, double dMultiplier)
{
	destination[0] = 0; //empty string
	double value = 0;

	if (nLen == 1)
		value = (double)m_chPackages[nIndex] * dMultiplier;
	else if (nLen == 2)
		value = (double)((((int)m_chPackages[nIndex + 1]) << 8) | m_chPackages[nIndex]) * dMultiplier;
	else if (nLen == 4)
		value = (double)((((int)m_chPackages[nIndex + 3]) << 24) | (((int)m_chPackages[nIndex + 2]) << 16) | (((int)m_chPackages[nIndex + 1]) << 8) | m_chPackages[nIndex]) * dMultiplier;

	if (deviceValue2String(destination, nLen, nIndex, nReadMode, dMultiplier, value))
		return;

	switch (nReadMode)
	{
	case 20:
		sprintf(destination, "%.2f", (float)value);
		break;
	case 12:
	case 13:
		sprintf(destination, "%d", m_chPackages[nIndex]);
		break;
	case 14:
		char2bin(destination, m_chPackages[nIndex]);
		break;
	case 15:
		sprintf(destination, "%d", thermistor2Temp(((unsigned int)m_chPackages[nIndex + 1] << 8) | m_chPackages[nIndex]));
		break;
	case 16:
		if (dMultiplier == 1)
			sprintf(destination, "%.0f", (float)value);
		else
			sprintf(destination, "%.2f", (float)value);
		break;
	case 3:
	{
		unsigned int ivalue = (unsigned int)value;
		if (ivalue == 0xFFFF)
			strcpy(destination, "Never");
		else
			sprintf(destination, "%02d:%02d:%02d", ivalue / 3600, (ivalue / 60) % 60, ivalue % 60);
	}
	break;
	case 6:
		strcpy(destination, "-");
		break;
	case 8:
		if ((m_chPackages[nIndex] == 0xFF) && (nLen == 1 || m_chPackages[nIndex + 1] == 0xFF))
			strcpy(destination, "Never");
		else
			sprintf(destination, "%d", (int)value);
		break;
	case 17:
	case 1:
	default:
		sprintf(destination, "%d", (int)value);
	}
}

bool HIDOpenUPSBase::setVariableData(int mesg_no, char *str)
{
	ATXMSG &msg = GetMessages()[mesg_no];
	int len = msg.nLen;

	if (len == 0)
		return m_bEmptyFieldOk;

	unsigned char c1 = 0;
	unsigned char c2 = 0;
	unsigned char c3 = 0;
	unsigned char c4 = 0;

	if (!readOneValue(str, msg.nReadMode, msg.dMultiplier, len, c1, c2, c3, c4))
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

unsigned char HIDOpenUPSBase::getUPSVariableData(unsigned int cnt, char *name, char *value, char *unit, char *comment)
{
	if (cnt >= MAX_MESSAGE_CNT)
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
