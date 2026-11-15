#ifndef _HIDOPENUPSBASE_H_
#define _HIDOPENUPSBASE_H_

#include "HIDInterface.h"

#define OPENUPS_MEM_READ_OUT			0xA1
#define OPENUPS_MEM_READ_IN				0xA2
#define OPENUPS_MEM_WRITE_OUT			0xA3
#define OPENUPS_MEM_WRITE_IN			0xA4
#define OPENUPS_MEM_ERASE				0xA5

#define OPENUPS_SETTINGS_ADDR_START		0x007800
#define OPENUPS_SETTINGS_ADDR_END		0x007C00

// Code shared by OpenUPS and OpenUPS2.
class HIDOpenUPSBase : public HIDInterface
{
public:
	HIDOpenUPSBase(USBHID *d);
	virtual ~HIDOpenUPSBase();

	void ReadConfigurationMemory();
	void EraseConfigurationMemory();
	void WriteConfigurationMemory();
};

#endif
