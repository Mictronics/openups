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

#define TERMISTOR_CONSTS 34

// Code shared by OpenUPS and OpenUPS2.
class HIDOpenUPSBase : public HIDInterface
{
public:
	HIDOpenUPSBase(USBHID *d);
	virtual ~HIDOpenUPSBase();

	void ReadConfigurationMemory();
	void EraseConfigurationMemory();
	void WriteConfigurationMemory();
	float convertOneValue2Float(unsigned char *buffer, int nLen, int nIndex, int nReadMode, double dMultiplier);
	void convertOneValue2String(char *destination, int nLen, int nIndex, int nReadMode, double dMultiplier);
	bool setVariableData(int mesg_no, char *str);
	unsigned char getUPSVariableData(unsigned int cnt, char *name, char *value, char *unit, char *comment);

protected:
	// Per-device read modes, tried before the shared ones. Return true if handled.
	virtual bool deviceValue2Float(unsigned char *buffer, int nLen, int nIndex, int nReadMode, double dMultiplier, double &value) { return false; }
	// value is the raw field already scaled by dMultiplier.
	virtual bool deviceValue2String(char *destination, int nLen, int nIndex, int nReadMode, double dMultiplier, double value) { return false; }

	bool m_bReportWrittenPages; // OpenUPS2 stays quiet while writing
	bool m_bEmptyFieldOk;       // setVariableData result for a zero-length field: OpenUPS false, OpenUPS2 true

private:
	int thermistor2Temp(unsigned int termistor);
};

#endif
