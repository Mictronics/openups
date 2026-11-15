/*
 *    Author: Nicu Pavel <npavel@mini-box.com>
 *    Copyright (c) 2020 Mini-Box.com
 *    Licensed under the terms of the GNU Lesser General Public License v2.1
 *  	http://www.opensource.org/licenses/lgpl-2.1.php 
 */

#include "util.h"
#include "HIDOpenUPS2.h"

#include <math.h>

double g_OpenUPS2_memConstants[] =
	{
		0.001, //Vin				//0
		0.001, //Vout				//1
		0.001, //Vbat+chg		    //2
		0.001, //A chg Hi range	    //3
		0.001, //A chg Low range	//4
		0.001, //A discharge		//5
		0.001  //battery
	};

ATXMSG g_OpenUPS2_memMessages[MAX_MESSAGE_CNT] =
	{
		{67, 1, "OPENUPSMODE", true, 17, 1, 0, 0, "OPENUPS mode\r\n0-Autorestart when input is present=yes\t(UPS)\r\n1-Autorestart when input is present=no\t(LAPTOP)\r\n2-Energy Pack mode\t\t\t(ON/OFF)", "[-]"},
		{28, 4, "CAPACITY", true, 16, 1, 0, 0, "Battery Capacity.\r\nDefault 1500mAh", "[mAh]"},
		{80, 2, "MOB_ONOFF_TOUT", true, 1, 10, 0, 0, "The powerswitch on the motherboard(if connected) will be shorted  this period to turn the Motherboard ON or OFF\r\nDefault is 500ms", "[mS]"},
		{66, 1, "UPS_BUTTON_ON_TOUT", true, 1, 10, 0, 0, "The pushbutton connected on the P10 header must be shorted this period to be considered as pressed.\r\nDefault 100ms.", "[mS]"},

		{0, 1, "UPS_CONFIG", true, 14, 1, 0, 0, "Configuration register. Used for enabling disabling modules.0-disabled;1-enabled\r\nbit7:reserved\r\nbit6:reserved\r\nbit5:reserved\r\nbit4:Enable No Battery Mode (FW 1.4+)\r\nbit3:reserved\r\nbit2:Enable temperature compensated charge algorithm\r\nbit1:Enable shutdown from battery if system was shut down from OS sensed by USB\r\nbit0:Enable output voltage settings directly from OUT_VOLTAGE parameter. If disabled output voltage is set by configuration switch on the back side of the PCBA.", "[bit7..bit0]"},
		{100, 1, "UPS_CONFIG2", true, 14, 0, 0, 1, "Configuration register.Used for enabling disabling modules.0-disabled;1-enabled\r\nbit7:reserved\r\nbit6:reserved\r\nbit5:reserved\r\nbit4:reserved\r\nbit3:reserved\r\nbit2:reserved\r\nbit1:Enable Ship mode, consumption is less than 1uA. Unit can be waked up only by input power presence.\r\nbit0:Enable Sleep mode, consumption is 530uA. Unit can be waked up by button or input power presence.\r\nThe combination of bit1 ,bit0 sets the configurable low power modes\r\n01: Sleep mode CPU OFF (530uA)\r\n10: Ship mode CPU OFF(1uA)\r\n11 or 00: Standby mode CPU ON(15mA) ==> Ship mode CPU OFF(1uA)", "[bit7..bit0]"},

		//	{ 1, 1, "UPS_INIT_DELAY_TOUT"				, true, 1, 1, 0, 0		, "Initial delay before starting the UPS\r\nDefault is 1 sec.", "[s]" },
		//	{ 2, 2, "UPS_VIN_MAX_SHUTDOWN"				, true,10, 0, 0, 0		, "Max allowed input voltage. In case input voltage exceeds predefined value shutdown will be initiated.\r\nDefault is 25V.", "[V]" },
		{4, 2, "UPS_VIN_MIN_START", true, 10, 0, 0, 0, "If input voltage is above this threshold the UPS will start and take power from the input.\r\nDefault is 11V.", "[V]"},
		//  { 6, 2, "UPS_VIN_MIN_STOP"					, true,10, 0, 0, 0		, "If input voltage is below this threshold the UPS will try to run on battery.\r\nDefault is 6V.", "[V]" },
		{8, 2, "UPS_VCELL_MIN_START", true, 10, 6, 0, 0, "If Vin is not present and all the battery cells are above this threshold the UPS can start and runs on battery\r\nDefault is 3V.", "[V]"},
		{10, 2, "UPS_VCELL_MIN_STOP", true, 10, 6, 0, 0, "If VIN is not present and one of the battery cells is less than this threshold during the time period specified by UPS_VBAT_UVP_OFF_TOUT parameter the UPS will initiate shut down procedure.\r\nDefault is 2.8V", "[V]"},
		/**/ {16, 2, "UPS_VCELL_MIN_HARD_STOP", true, 10, 6, 0, 0, "If VIN is not present and one of the battery cells is less than this threshold the UPS will instantly shut down to preserve battery.\r\nDefault is 2V ", "[V]"},
		{12, 2, "UPS_VBAT_UVP_OFF_TOUT", true, 1, 1, 0, 0, "During this period battery cells are checked against UPS_VCELL_MIN_STOP. If cell voltage exceeds UPS_VCELL_MIN_STOP the timer is rearmed so it can filter unwanted noise in the system.\r\nDefault is 5 sec.", "[s]"},
		{14, 2, "UPS_HARDOFF_TOUT", true, 1, 1, 0, 0, "After UPS_VBAT_UVP_OFF_TOUT passed motherboard is signaled to shut down. The UPS will wait this period so that the Motherboard can shut down gracefully, than cut power and enter deep sleep. This period should not be too long to prevent battery drain.\r\nDefault is 60 sec.", "[s]"},

		{101, 2, "UPS_VBAT_SLEEP_TOUT", true, 3, 1, 0, 0, "Deep sleep timeout..If system is running on battery and this period has elapsed with no input power applied, then the UPS will initiate shut down procedure. \"Never\" is allowed as value (to disable this feature).\r\nDefault is \"Never\".", "[HH:MM:SS]"},

		//moved up	{16, 2, "UPS_VCELL_MIN_HARD_STOP"			, true,10, 6, 0, 0		, "If VIN is not present and one of the battery cells is less than this threshold the UPS will instantly shut down to preserve battery.\r\nDefault is 2V ", "[V]" },
		//	{18, 2, "UPS_SWITCHOVER_VBAT_TOUT"			, true, 1, 10, 0, 0		, "After UPS switches to battery, can switch back to Vin only after this time ellapses\r\nDefault is 1000 ms", "[ms]" },
		//	{20, 2, "UPS_SWITCHOVER_VIN_TOUT"			, true, 1, 1, 0, 0		, "After UPS switches to input, can switch back to battery only after this time ellapses\r\nDefault is 1 ms", "[ms]" },
		{22, 2, "DCHG_IMAX", true, 10, 5, 0, 0, "Max allowed discharge current. In case discharge current exceeds this threshold shutdown will be initiated.\r\nDefault is 10 A.", "[A]"},
		{24, 1, "AFE_OLV", true, 40, 1, 0, 0, "Advanced parameter.\r\nAFE Overload Voltage treshold configuration register.\r\nDefault is 00.", "[hex]"},
		{25, 1, "AFE_OLD", true, 40, 1, 0, 0, "Advanced parameter.\r\nAFE Overload Delay time configuration register.\r\nDefault is 00.", "[hex]"},
		{26, 1, "AFE_SCC", true, 40, 1, 0, 0, "Advanced parameter.\r\nAFE Short Circuit In Charge configuration register.\r\nDefault is 50.", "[hex]"},
		{27, 1, "AFE_SCD", true, 40, 1, 0, 0, "Advanced parameter.\r\nAFE Short Circuit In Discharge configuration register.\r\nDefault is 50.", "[hex]"},
		// moved up	{28, 4, "CAPACITY"							, true,16, 1, 0, 0		, "Battery Capacity.\r\nDefault 3400mAh", "[mAh]" },

		//moved down	{32, 2, "BAL_ENABLE_TOUT"					, true,1, 5, 0, 0		, "Balancing enable timeout.\r\nDefault is 2 min.", "[min]" },
		//moved down	{34, 2, "BAL_DISABLE_TOUT"					, true,1, 5, 0, 0		, "Balancing disable timeout.\r\nDefault is 2 min.", "[min]" },
		{36, 2, "CHG_VCOND", true, 10, 6, 0, 0, "Conditioning/Pre-charge voltage. Charge current is limited until cell voltage exceeds this value and for at least CHG_TCOND time.\r\nDefault is 3V.", "[V]"},
		{38, 2, "CHG_TCOND", true, 1, 1, 0, 0, "Conditioning/Precharge time. Charge current is limited until cell voltage exceeds CHG_VCOND value and it is applied during this time.\r\nDefault is 30 sec.", "[s]"},
		//	{40, 2, "CHG_IBULK"							, true,10, 3, 0, 0		, "Fast charge current limit (constant current mode)\r\nDefault is 1750mA.", "[mA]" },
		{42, 2, "CHG_BULK_STOP_VOLTAGE", true, 10, 6, 0, 0, "Maximum allowed bulk charge voltage/cell during constant current/constant voltage charging\r\nDefault is 3.5[V/cell]", "[V/cell]"},
		{71, 1, "CHG_HYSTERESIS", true, 10, 6, 0, 0, "An over-voltage value (CHG_BULK_STOP_VOLTAGE+CHG_HYSTERESIS ) that it is allowed when charging.\r\nIf one of the cells exceeds this value charging is immediately stopped.\r\nDefault is 100mV/cell.", "[V/cell]"},
		//moved down	{44, 2, "OUT_FOLLOW_OFFSET"					, true,30, 10, 0, 0		, "-", "[V]" },
		//	{46, 2, "CHG_IMIN"							, true,10, 3, 0, 0		, "If charge current in CV mode is less than this value\r\nPbSO4: enter maintanence mode and apply the float charge voltage(CHG_START_VOLTAGE)\r\nLiFePO4: cut off the charge voltage\r\nDefault is 290mA", "[mA]" },

		{96, 2, "CHG_GLOBAL_TOUT", true, 1, 0.5, 0, 0, "Global charge timeout.\r\nDefault is 180 min.", "[min]"},
		//	{98, 2, "CHG_IFLOAT"						, true,10, 3, 0, 0		, "Charge current limit in float charge mode for PbSO4 batteries (Firmware v1.3+)", "[mA]" },
		{94, 2, "CHG_TOPPING_TIMER", true, 1, 1, 0, 0, "For Lithium based batteries after an overvoltage condition is detected for a cell a resting period is set by this timer before applying a small topping charge in case other cells are still not charged.\r\nDefault is 18000s.", "[s]"},

		{48, 2, "CHG_START_VOLTAGE", true, 10, 6, 0, 0, "If cell voltage is below this value charging can be started.\r\nDefault is 3.32V/cell", "[V/cell]"},
		{50, 1, "CHG_BAT_TYPE", true, 1, 1, 0, 0, "The battery chemistry to be charged.\r\n1-LiFePO4", ""},
		{51, 2, "CHG_TEMP_COLD", true, 15, 1, 0, 0, "COLD temperature treshold for temperature compensated charge current regulation\r\nDefault 5�C", "[�C]"},
		{53, 2, "CHG_TEMP_COOL", true, 15, 1, 0, 0, "COOL temperature treshold for temperature compensated charge current regulation\r\nDefault 10�C", "[�C]"},
		{55, 2, "CHG_TEMP_WARM", true, 15, 1, 0, 0, "WARM temperature treshold for temperature compensated charge current regulation\r\nDefault 50�C", "[�C]"},
		{57, 2, "CHG_TEMP_HOT", true, 15, 1, 0, 0, "HOT temperature treshold for temperature compensated charge current regulation\r\nDefault 55�C", "[�C]"},
		//moved down {59, 1, "OUT_STARTVOLTAGE_OFFSET"			, true, 1, 1, 0, 0		, "Soft start output voltage offset", "[nr]" },

		{60, 2, "BAL_VCELL_MIN", true, 10, 6, 0, 0, "Balancing is allowed  if cell voltages are above this value.\r\nDefault is 3.32V", "[V]"},
		{62, 1, "BAL_VCELL_DIFF_START", true, 10, 6, 0, 0, "If the voltage difference between cells exceeds this value start balancing the cells.\r\nDefault is 70mV.", "[V]"},
		{63, 1, "BAL_VCELL_DIFF_STOP", true, 10, 6, 0, 0, "If the voltage difference between cells is less than this value stop balancing the cells.\r\nDefault is 40mV.", "[V]"},
		/* {32, 2, "BAL_ENABLE_TOUT", true, 1, 5, 0, 0, "Balancing enable timeout (FW 1.5+)\r\nDefault 72000 min.", "[min]"}, */
		/* {34, 2, "BAL_DISABLE_TOUT", true, 1, 5, 0, 0, "Balancing disable timeout (FW 1.5+)\r\nDefault 1440 min.", "[min]"}, */

		{64, 2, "OUT_VOLTAGE", true, /*7*/ 10, 1, 0, 0, "The output voltage. Bit0 in UPS_CONFIG parameter should be set to �1� in order this parameter has effect.\r\nDefault is 12V.", "[V]"},
		/**/ {59, 1, "OUT_STARTVOLTAGE_OFFSET", true, 1, 1, 0, 0, "Soft start output voltage offset", "[nr]"},
		//moved up	{66, 1, "UPS_BUTTON_ON_TOUT"				, true, 1, 10, 0, 0		, "", "[]" },
		//moved up	{67, 1, "OPENUPSMODE"						, true,17, 1, 0, 0		, "OpenUPS Mode\r\n0-Auto restart when Input Power is Present=YES\r\n1-Auto restart when Input Power is Present=NO", "[-]" },
		//	{68, 1, "OUT_REGULATOR_OFFSET"				, true, 1, 1, 0, 0		, "Regulator offset", "[step]" },
		{69, 1, "OUT_MAX_REGULATOR_STEP", true, 1, 1, 0, 0, "Maximum allowed regulation step number for output module (0-255)\r\nDefault 255.", "[step]"},
		//removed	{70, 1, "CELLS"								, true, 1, 1, 0, 0		, "Number of configured cells to charge/balance\r\nDefault is 4.", "[pcs]" },
		//moved up	{71, 1, "CHG_HYSTERESIS"					, true,10, 2, 0, 0		, "An overvoltage value (CHG_BULK_STOP_VOLTAGE +CHG_HYSTERESIS ) that it is allowed when charging.If one of the cells exceeds this value charging is imediately stopped.\r\nDefault is 200mV/cell.", "[V]" },
		//removed	{72, 4, "POUT_LO"							, true,16, 0.000001, 0, 0	, "Output power low treshold for motherboard alive sensing.If output power is lower than this treshold shut down impulse is NOT sent to the motherboard.\r\nDefault is 2W.", "[W]" },
		//removed	{76, 4, "POUT_HI"							, true,16, 0.000001, 0, 0	, "Output power high treshold for motherboard alive sensing. If output power is higher than this treshold shut down impulse is sent to the motherboard.\r\nDefault is 6W.", "[W]" },
		//removed   {44, 2, "OUT_FOLLOW_OFFSET"					, true,30, 10, 0, 0		, "Offset for adaptive output regulation.\r\nDefault is 1V.", "[V]" },

		//moved up	{80, 2, "MOB_ONOFF_TOUT"					, true, 1, 10, 0, 0		, "The powerswitch on the motherboard(if connected) will be shorted  this period to turn the Motherboard ON or OFF\r\nDefault is 500ms", "[mS]" },
		{82, 2, "OCV_SOC0", true, 10, 6, 0, 0, "Open Circuit Voltage State Of Charge detection for initial 0% fuel gauge estimation.\r\nDefault is 3.1V", "[V]"},
		{84, 2, "OCV_SOC10", true, 10, 6, 0, 0, "Open Circuit Voltage State Of Charge detection for initial 10% fuel gauge estimation.\r\nDefault is 3.2V", "[V]"},
		{86, 2, "OCV_SOC25", true, 10, 6, 0, 0, "Open Circuit Voltage State Of Charge detection for initial 25% fuel gauge estimation.\r\nDefault is 3.25V", "[V]"},
		{88, 2, "OCV_SOC50", true, 10, 6, 0, 0, "Open Circuit Voltage State Of Charge detection for initial 50% fuel gauge estimation.\r\nDefault is 3.30V", "[V]"},
		{90, 2, "OCV_SOC75", true, 10, 6, 0, 0, "Open Circuit Voltage State Of Charge detection for initial 75% fuel gauge estimation.\r\nDefault is 3.32V", "[V]"},
		{92, 2, "OCV_SOC100", true, 10, 6, 0, 0, "Open Circuit Voltage State Of Charge detection for initial 100% fuel gauge estimation.\r\nDefault is 3.35V", "[V]"},
		//moved up	{94, 2, "CHG_TOPPING_TIMER"					, true, 1, 1, 0, 0		, "For Lithium based batteries after an overvoltage condition is detected for a cell a resting period is set by this timer before applying a small topping charge in case other cells are still not charged.\r\nDefault is 1800 sec.", "[s]" },

		//moved up	{96, 2, "CHG_GLOBAL_TOUT"					, true, 1, 0.5, 0, 0		, "Global charge timeout (Firmware v1.3+)", "[min]" },
		//deleted	{98, 2, "CHG_IFLOAT"						, true,10, 3, 0, 0		, "Charge current limit in float charge mode (Firmware v1.3+)", "[mA]" },
		//moved up  {100,1, "UPS_CONFIG2"						, true,14, 1, 0, 0		, "Configuration register (Firmware V1.4+). Used for enabling disabling modules. LSB bit is b0.\r\nbit0: if bit is set and running on battery and measured output power<POUT_LO than go to deep sleep", "[bits]" },
		//moved up	{101,2, "UPS_VBAT_SLEEP_TOUT"				, true, 3, 1, 0, 0		, "Deep sleep timeout (Firmware V1.4+).  If system is running on battery and this period has elapsed with no input power applied then the UPS will initiate shut down procedure.\r\n\"Never\" is allowed as value (to disable this feature)", "[HH:MM:SS]" },

		{1020, 2, "WRITE COUNT", true, 1, 1, 0, 0, "Number of times the flash memory has been written. Parameter is Read only.", "[count]"},
	};

unsigned int g_OpenUPS2_memTerm[] =
	{
		(unsigned int)0x31, 
		(unsigned int)0x40, 
		(unsigned int)0x53, 
		(unsigned int)0x68, 
		(unsigned int)0x82, 
		(unsigned int)0xA0, 
		(unsigned int)0xC3, 
		(unsigned int)0xE9, 
		(unsigned int)0x113, 
		(unsigned int)0x13F, 
		(unsigned int)0x16E, 
		(unsigned int)0x19F, 
		(unsigned int)0x1CF, 
		(unsigned int)0x200, 
		(unsigned int)0x22F, 
		(unsigned int)0x25C, 
		(unsigned int)0x286, 
		(unsigned int)0x2AE, 
		(unsigned int)0x2D3, 
		(unsigned int)0x2F4, 
		(unsigned int)0x312, 
		(unsigned int)0x32D, 
		(unsigned int)0x345, 
		(unsigned int)0x35A, 
		(unsigned int)0x36D, 
		(unsigned int)0x37E, 
		(unsigned int)0x38C, 
		(unsigned int)0x399, 
		(unsigned int)0x3A5, 
		(unsigned int)0x3AF, 
		(unsigned int)0x3B7, 
		(unsigned int)0x3BF, 
		(unsigned int)0x3C6, 
		(unsigned int)0x3CC
	};


HIDOpenUPS2::HIDOpenUPS2(USBHID *d): HIDOpenUPSBase(d)
{
	m_bReportWrittenPages = false;
	m_bEmptyFieldOk = true;
}

HIDOpenUPS2::~HIDOpenUPS2()
{
}

ATXMSG* HIDOpenUPS2::GetMessages()
{
	return g_OpenUPS2_memMessages;
}

double HIDOpenUPS2::GetConstant(int i)
{
	return g_OpenUPS2_memConstants[i];
}

unsigned int* HIDOpenUPS2::GetTermistorConsts()
{
	return g_OpenUPS2_memTerm;
}

void HIDOpenUPS2::printValues()
{

	fprintf(stdout, "VIN: %f\n VBat: %f\n VOut: %f \n CCharge: %f\n", m_fVIN, m_fVBat, m_fVOut, m_fCCharge);
	fprintf(stdout, "Temp1: %f\n Temp2: %f\n Temp3: %f\n", m_fTemperature[0], m_fTemperature[1], m_fTemperature[2]);
	fprintf(stdout, "VCell1: %f\n VCell2: %f\n VCell3: %f\n", m_fVCell[0], m_fVCell[1], m_fVCell[2]);
	fprintf(stdout, "CDischarge: %f\n VDuty: %f\n", m_fCDischarge, m_fVDuty);
	fprintf(stdout, "VerMajor: %d\n VerMinor: %d\n", m_nVerMajor, m_nVerMinor);
	fprintf(stdout, "State: %d\n StateUPS: %d\n stateCHG: %d\n stateDBG: %d\n", m_nState, m_bStateUPS, m_bStateCHG, m_bStateDBG);
	fprintf(stdout, "Capacity: %d\n", m_bCapacity);
	fprintf(stdout, "BatOn 1: %d\n BatOn 2: %d\n BatOn 3: %d\n", m_bBatOn[0], m_bBatOn[1], m_bBatOn[2]);
	fprintf(stdout, "RTE: %d\n UPSMode: %d\n VOutPot: %d\n", m_nRTE, m_bUPSMode, m_bVOutPot);
	fprintf(stdout, "ConfigSwitch: %d\n ShutdownType: %d\n", m_bConfigSwitch, m_nShutdownType);

	/*
	unsigned char m_bCheck23[8];
	unsigned char m_bCheck24[8];
	unsigned char m_bCheck25[8];
	*/

	//unsigned char m_nState;//offline,usb,batpowered,vinpowered

	/*
	unsigned char m_bDBG[DBG_LEN];//4 byte + 8 byte + 4 byte
	unsigned char m_bDBG2[30];
	*/
}

void HIDOpenUPS2::parseMessage(unsigned char *msg)
{
	if (!msg)
		return;

	switch (msg[0])
	{
	case OPENUPS2_RECV_ALL_VALUES:
	{
		int i;

		m_nVerMajor = (msg[31] >> 4) & 0x0F;
		m_nVerMinor = msg[31] & 0x0F;

		m_fVIN = convertOneValue2Float(msg, 2, 1, 20, HIDOpenUPS2::GetConstant(0));
		m_fVOut = convertOneValue2Float(msg, 2, 3, 20, HIDOpenUPS2::GetConstant(1));
		m_fVBat = convertOneValue2Float(msg, 2, 5, 20, HIDOpenUPS2::GetConstant(6));

		for (i = 0; i < 3; i++)
			m_fVCell[i] = convertOneValue2Float(msg, 2, 7 + 2 * i, 20, HIDOpenUPS2::GetConstant(6));

		for (i = 0; i < 8; i++)
		{
			m_bCheck23[i] = ((msg[23] >> i) & 1);
			m_bCheck24[i] = ((msg[24] >> i) & 1);
			m_bCheck25[i] = ((msg[25] >> i) & 1);
		}

		m_fCCharge = convertOneValue2Float(msg, 2, 19, 2, (double)0.001);
		m_fCDischarge = convertOneValue2Float(msg, 2, 21, 2, (double)0.001);

		if (m_bCheck24[6])
			m_nState = 1;
		else if (m_bCheck24[5])
			m_nState = 2;
		else
			m_nState = 3; //only usb

		m_fTemperature[0] = convertOneValue2Float(msg, 2, 13, 15, 1);
		m_fTemperature[1] = convertOneValue2Float(msg, 2, 15, 15, 1);
		m_fTemperature[2] = convertOneValue2Float(msg, 2, 17, 15, 1);

		m_fVDuty = convertOneValue2Float(msg, 2, 26, 20, (double)0.0009765625);

		m_bVOutPot = 255 - msg[28];
		m_bConfigSwitch = msg[29];
	}
	break;
	case OPENUPS2_CLOCK_IN:
	{
		int i;

		if ((m_nVerMajor >= 1) && (m_nVerMinor >= 4))
		{
			m_nShutdownType = msg[7];
			m_nShutdownType = (m_nShutdownType << 8) | msg[6];
		}
		else
			m_nShutdownType = 0;

		m_bStateUPS = msg[9];
		m_bStateCHG = msg[10];
		m_bStateDBG = msg[11];
		m_bCapacity = msg[12];

		for (i = 0; i < 3; i++)
			m_bBatOn[i] = ((msg[15] >> i) & 1);

		m_nRTE = msg[17];
		m_nRTE = (m_nRTE << 8) | msg[16];

		m_bUPSMode = msg[25];

		for (i = 0; i < 4; i++)
			m_bDBG[i] = msg[28 + i];
	}
	break;
	case OPENUPS2_MEM_READ_IN:
	{
		memcpy(m_chPackages + (m_ulSettingsAddr - OPENUPS2_SETTINGS_ADDR_START), msg + 5, 16);
		/*
		fprintf(stderr, "%02lx %02lx", m_ulSettingsAddr & 0xFF, (m_ulSettingsAddr >> 8) & 0xFF);
		for (int len = 0; len < 16; len++)
			fprintf(stderr, " %02x", msg[5 + len]);
		fprintf(stderr, "\n");
		*/
	}
	break;
	}
}

bool HIDOpenUPS2::readOneValue(char *str, int nReadMode, double dMultiplier, int len, unsigned char &c1, unsigned char &c2, unsigned char &c3, unsigned char &c4)
{
	bool ok = false;
	c1 = 0;
	c2 = 0;

	double val = 0;

	switch (nReadMode)
	{
	case 3:
	{ //hh:mm:ss
		if (strcmp(str, "Never") == 0)
		{
			val = 0xFFFF;
			ok = true;
		}
		else if ((strlen(str) == 8) && (str[2] == ':') && (str[5] == ':'))
		{
			char *end = NULL;
			char temps[10];

			str_left(temps, str, 2);
			int hh = strtol(temps, &end, 10);
			if (((end != str) && (hh >= 0) && (hh < 18)))
			{
				str_mid(temps, str, 3, 2);
				int mm = strtol(temps, &end, 10);
				if ((end != str) && (mm >= 0) && (mm < 60))
				{
					str_right(temps, str, 2);
					int ss = strtol(temps, &end, 10);

					if (((end != str) && (ss >= 0) && (ss < 60)))
					{
						val = hh * 3600 + mm * 60 + ss;
						ok = true;
					}
				}
			}
		}
	}
	break;
	case 4:
	{ //"signed" float
		if (strlen(str) != 0)
		{
			bool negative = false;
			if (str[0] == '-')
			{
				str = str + 1;
				negative = true;
			}

			char *end = NULL;
			val = strtod(str, &end);

			if (end != str)
			{
				ok = true;

				val /= dMultiplier;
				if (val < 0)
					val = 0;
				double rem = val - (int)val;
				unsigned long uval = (unsigned long)val;
				if (rem >= 0.5)
					uval += 1;

				if (uval > 0x7F)
					uval = 0x7F;

				c1 = 0;
				c2 = (unsigned char)uval;
				if (negative)
					c2 |= 0x80;
			}
		}
	}
	break;
	case 6:
	{
		val = 0xFF;
		ok = true;
	}
	break;
	case 7:
	{
		/*char* end = NULL;
			val = strtod(str, &end);

			c2 = 0;
			c1 = HIDOpenUPS2::GetDataVOut(val);
			ok = true;*/
	}
	break;
	case 8:
	{
		if (strcmp(str, "Never") == 0)
		{
			val = (double)0xFFFF * dMultiplier;
			ok = true;
		}
		else
		{
			char *end = NULL;
			val = strtod(str, &end);
			if (end != str)
				ok = true;
		}
	}
	break;
	case 12:
	case 13:
	{
		c1 = 0;
		c2 = atoi(str);
		ok = true;
	}
	break;
	case 14:
	{
		c1 = 0;
		c2 = bin2char(str, &ok);
	}
	break;
	case 15:
	{
		unsigned int termistor = 0;

		int temp = atoi(str);
		if (temp <= -40)
			termistor = HIDOpenUPS2::GetTermistorConsts()[0];
		else if (temp >= 125)
			termistor = HIDOpenUPS2::GetTermistorConsts()[TERMISTOR_CONSTS - 1];
		else
		{
			int pos = (temp + 40) / 5;
			int temp_dif = (temp + 40) % 5;

			if (temp_dif == 0)
				termistor = HIDOpenUPS2::GetTermistorConsts()[pos];
			else
			{
				unsigned int d1 = HIDOpenUPS2::GetTermistorConsts()[pos];
				unsigned int d2 = HIDOpenUPS2::GetTermistorConsts()[pos + 1];

				termistor = d1 + temp_dif * (d2 - d1) / 5;
			}
		}

		c1 = (unsigned char)(((unsigned int)termistor) & 0xFF);
		c2 = (unsigned char)((((unsigned int)termistor) >> 8) & 0xFF);
		ok = true;
	}
	break;
	case 16:
	{
		char *end = NULL;
		val = strtod(str, &end);

		if ((val != 0) && (end != str))
		{
			val = ceil(val / dMultiplier);
			unsigned int ival = (unsigned int)val;

			c1 = (unsigned char)(ival & 0xFF);
			c2 = (unsigned char)((ival >> 8) & 0xFF);
			c3 = (unsigned char)((ival >> 16) & 0xFF);
			c4 = (unsigned char)((ival >> 24) & 0xFF);
			ok = true;
		}
	}
	break;
	case 17:
	{
		char *end = NULL;
		val = strtod(str, &end);
		if (end != str)
			ok = true;
		if ((val < 0) || (val > 255))
			ok = false;
	}
	break;
	case 40:
	{
		char *end = NULL;
		val = strtol(str, &end, 16);
		ok = true;
	}
	break; //hex
	case 1:
		//normal read - integer
	case 2:
	case 9:
	case 5:
	default:
	{ //normal read - float
		char *end = NULL;
		val = strtod(str, &end);

		if (end != str)
			ok = true;
	}
	}

	if (ok)
	{
		if ((nReadMode != 4) && (nReadMode != 7) && (nReadMode != 9) && (nReadMode != 11) && (nReadMode != 12) && (nReadMode != 13) && (nReadMode != 14) && (nReadMode != 15) && (nReadMode != 16))
		{
			switch (nReadMode)
			{
			case 10:
				val /= HIDOpenUPS2::GetConstant((int)dMultiplier);
				break;
			case 30:
				val /= HIDOpenUPS2::GetConstant(6);
				break;
			default:
				val /= dMultiplier;
			}

			if (nReadMode != 5)
			{ //unsigned
				if (val < 0)
					val = 0;
				double rem = val - (int)val;
				unsigned long uval = (unsigned long)val;
				if (rem >= 0.5)
					uval += 1;

				if (len == 1)
				{
					if (uval > 0xFF)
						uval = 0xFF;

					c1 = 0;
					c2 = (unsigned char)uval;
				}
				else
				{
					if (uval > 0xFFFF)
						uval = 0xFFFF;

					c1 = (unsigned char)(((unsigned int)uval) & 0xFF);
					c2 = (unsigned char)((((unsigned int)uval) >> 8) & 0xFF);
				}

				//DBGOutput("      uval1=%d %x\n",uval,uval);
			}
			else
			{
				double rem = -val + (int)val;
				long uval = (long)val;
				if (rem >= 0.5)
					uval += 1;

				if (len == 1)
				{
					if (uval > 127)
						uval = 127;
					if (uval < -128)
						uval = -128;

					c1 = 0;
					c2 = (char)uval;
				}
				else
				{
					if (uval > 32767)
						uval = 32767;
					if (uval < -32768)
						uval = -32768;

					c1 = (char)(((int)uval) & 0xFF);
					c2 = (char)((((int)uval) >> 8) & 0xFF);
				}
				//DBGOutput("      uval2=%d %x\n",uval,uval);
			}
		}
	}

	//DBGOutput("readOneValue %f [%02x][%02x]\n",(float)val,(unsigned int)c2,(unsigned int)c1);

	return ok;
}

void HIDOpenUPS2::restartUPS()
{
	sendCommand(OPENUPS2_DCMD_RESTART_UPS, 1);
}

void HIDOpenUPS2::restartUPSInBootloaderMode()
{
	sendMessage(OPENUPS2_ENTER_BOOTLOADER_OUT, 2, 0xFA, 0xCD);
}

void HIDOpenUPS2::setVOutVolatile(float vout)
{
	int voltage = (int)(vout * 1000.0);//mV
	sendCommandEx(OPENUPS2_DCMD_SPI_WRITE_DATA_VOL_VOUT, (voltage >> 8) & 0xFF, voltage & 0xFF);
}

void HIDOpenUPS2::incDecVOutVolatile(unsigned char inc)
{
	if (inc) // wiper is opposite to vout
		sendCommand(OPENUPS2_DCMD_SPI_DEC_VOL_WIPER_VOUT, 0);
	else 
		sendCommand(OPENUPS2_DCMD_SPI_INC_VOL_WIPER_VOUT, 0);
}

void HIDOpenUPS2::GetStatus() 
{
	unsigned char recv[32];
	int ret;

	sendMessage(OPENUPS2_GET_ALL_VALUES, 0);
	recvMessage(recv);
	parseMessage(recv);

	ret = sendMessage(OPENUPS2_GET_ALL_VALUES_2, 0);
	recvMessage(recv);
	parseMessage(recv);

	ret = sendMessage(OPENUPS2_GET_ALL_VALUES_3, 0);
	recvMessage(recv);
	parseMessage(recv);

	ret = sendMessage(OPENUPS2_CLOCK_OUT, 0);
	recvMessage(recv);
	parseMessage(recv);
}

bool HIDOpenUPS2::deviceValue2Float(unsigned char *buffer, int nLen, int nIndex, int nReadMode, double dMultiplier, double &value)
{
	if (nReadMode != 30)
		return false;

	if (nLen == 1)
		value = (double)buffer[nIndex] * GetConstant(6);
	else
		value = (double)((((int)buffer[nIndex + 1]) << 8) | buffer[nIndex]) * GetConstant(6);
	return true;
}

bool HIDOpenUPS2::deviceValue2String(char *destination, int nLen, int nIndex, int nReadMode, double dMultiplier, double value)
{
	switch (nReadMode)
	{
	case 2:
	case 9:
		sprintf(destination, "%.3f", (float)value);
		break;
	case 10:
		if (nLen == 1)
			value = (double)m_chPackages[nIndex] * GetConstant((int)dMultiplier);
		else if (nLen == 2)
			value = (double)((((int)m_chPackages[nIndex + 1]) << 8) | m_chPackages[nIndex]) * GetConstant((int)dMultiplier);
		else if (nLen == 4)
			value = (double)((((int)m_chPackages[nIndex + 3]) << 24) | (((int)m_chPackages[nIndex + 2]) << 16) | (((int)m_chPackages[nIndex + 1]) << 8) | m_chPackages[nIndex]) * GetConstant((int)dMultiplier);
		sprintf(destination, "%.3f", (float)value);
		break;
	case 30:
		if (nLen == 1)
			value = (double)m_chPackages[nIndex] * GetConstant(6);
		else if (nLen == 2)
			value = (double)((((int)m_chPackages[nIndex + 1]) << 8) | m_chPackages[nIndex]) * GetConstant(6);
		else if (nLen == 4)
			value = (double)((((int)m_chPackages[nIndex + 3]) << 24) | (((int)m_chPackages[nIndex + 2]) << 16) | (((int)m_chPackages[nIndex + 1]) << 8) | m_chPackages[nIndex]) * GetConstant(6);
		if (value > dMultiplier)
			value = dMultiplier;
		sprintf(destination, "%.3f", (float)value);
		break;
	case 4: // sign bit set: negated output of the unmasked raw value (existing behavior, kept as is)
		if ((m_chPackages[nIndex] & 0x80) != 0)
			sprintf(destination, "-%.3f", (float)value);
		else
			sprintf(destination, "%.3f", (float)value);
		break;
	case 5:
		sprintf(destination, "%d", (int)value);
		break;
	case 7: // intentionally empty
		break;
	case 40: // hex
		if (nLen == 1)
			sprintf(destination, "%02x", (int)value);
		else if (nLen == 2)
			sprintf(destination, "%04x", (int)value);
		else if (nLen == 4)
			sprintf(destination, "%08x", (int)value);
		break;
	default:
		return false;
	}
	return true;
}
