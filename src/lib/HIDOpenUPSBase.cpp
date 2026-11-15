#include "HIDOpenUPSBase.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

HIDOpenUPSBase::HIDOpenUPSBase(USBHID *d): HIDInterface(d)
{
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
			fprintf(stderr, "Wrote page 0x%lx of configuration\n", m_ulSettingsAddr);
		}		
		m_ulSettingsAddr += 16;
	}
}
