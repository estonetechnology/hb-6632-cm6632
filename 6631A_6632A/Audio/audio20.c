#include "audio.h"
#include "config.h"

//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------
enum 
{
    AC_NODEF = 0,
    AC_IT,
    AC_OT,
    AC_FEATURE,
    AC_MIXER,
    AC_SELECTOR,
    AC_CLKSRC,
    AC_EXTENSION
};

enum 
{
    CMD_NODEF = 0,
    CMD_CURRENT,
    CMD_RANGE,
    CMD_MEMORY
};

enum 
{
    CLOCK_INTERNAL = 0,
    CLOCK_SPDIF
};

//-----------------------------------------------------------------------------
// Data Type Definition
//-----------------------------------------------------------------------------
typedef struct
{
   BYTE   id;
   BYTE   channel_no;
   WORD   channel_cfg;
}TE_CLUSTER_DSCR;

typedef struct
{
   BYTE   id;
   USB_INTERFACE*   pInterface;
}CLOCK_SOURCE_DSCR;

typedef struct
{
   BYTE   interface;
   BYTE   SampleRate;
   BYTE   Valid;
   BYTE   Valid2;   
}AlternateValid;

//-----------------------------------------------------------------------------
// USB Interface Structure Definition
//-----------------------------------------------------------------------------
BOOL StreamSetInterface();
BOOL HandleControlCmnd();
BOOL HandleControlDataOut();
BOOL HandleStreamCmnd();
BOOL HandleStreamDataOut();

BOOL PlayMultiChOpen(BYTE alt);
BOOL PlaySpdifOpen(BYTE alt);
BOOL PlayTwoChOpen(BYTE alt);
BOOL RecMultiChOpen(BYTE alt);
BOOL RecTwoChOpen(BYTE alt);
BOOL RecSpdifOpen(BYTE alt);

USB_INTERFACE code g_Audio20InterfaceAudioCtrl;
USB_INTERFACE code g_Audio20InterfaceSpeaker;
USB_INTERFACE code g_Audio20InterfaceSpdifOut;
USB_INTERFACE code g_Audio20InterfaceHeadphone;
USB_INTERFACE code g_Audio20InterfaceLineIn;
USB_INTERFACE code g_Audio20InterfaceMicIn;
USB_INTERFACE code g_Audio20InterfaceSpdifIn;

static USB_ENDPOINT code s_AudioIntEndpoints[1] = 
{
	{
		EP_AUDIO_INT,
		&g_Audio20InterfaceAudioCtrl,

		NULL,
		NULL
	}
};

static USB_ENDPOINT code s_SpeakerEndpoints[1] = 
{
	{
		EP_MULTI_CH_PLAY,
		&g_Audio20InterfaceSpeaker,

		NULL,
		NULL
	}
};

static USB_ENDPOINT code s_SpdifOutEndpoints[1] = 
{
	{
		EP_SPDIF_PLAY,
		&g_Audio20InterfaceSpdifOut,

		NULL,
		NULL
	}
};

static USB_ENDPOINT code s_HeadphoneEndpoints[1] = 
{
	{
		EP_TWO_CH_PLAY,
		&g_Audio20InterfaceHeadphone,

		NULL,
		NULL
	}
};

static USB_ENDPOINT code s_LineInEndpoints[1] = 
{
	{
		EP_MULTI_CH_REC,
		&g_Audio20InterfaceLineIn,

		NULL,
		NULL
	}
};

static USB_ENDPOINT code s_MicInEndpoints[1] = 
{
	{
		EP_TWO_CH_REC,
		&g_Audio20InterfaceMicIn,

		NULL,
		NULL
	}
};

static USB_ENDPOINT code s_SpdifInEndpoints[1] = 
{
	{
		EP_SPDIF_REC,
		&g_Audio20InterfaceSpdifIn,

		NULL,
		NULL
	}
};

USB_INTERFACE code g_Audio20InterfaceAudioCtrl = 
{
	0,
	NULL,

	1,
	s_AudioIntEndpoints, 

	NULL,
	NULL,

	HandleControlCmnd,
	HandleControlDataOut
};

BYTE idata g_AltSpeaker;
USB_INTERFACE code g_Audio20InterfaceSpeaker = 
{
#ifdef _MODEL_CM6632A_
	8,
#else
	2,
#endif
	&g_AltSpeaker,

	1,
	s_SpeakerEndpoints,

	StreamSetInterface,
	NULL,

	HandleStreamCmnd,
	HandleStreamDataOut
};

BYTE idata g_AltSpdifOut;
USB_INTERFACE code g_Audio20InterfaceSpdifOut = 
{
	3,
	&g_AltSpdifOut,

	1,
	s_SpdifOutEndpoints,

	StreamSetInterface,
	NULL,

	HandleStreamCmnd,
	HandleStreamDataOut
};

BYTE idata g_AltHeadphone;
USB_INTERFACE code g_Audio20InterfaceHeadphone = 
{
#ifdef _MODEL_CM6632A_
	2,
#else
	3,	// support 32bit
#endif

	&g_AltHeadphone,

	1,
	s_HeadphoneEndpoints,

	StreamSetInterface,
	NULL,

	HandleStreamCmnd,
	HandleStreamDataOut
};

BYTE idata g_AltLineIn;
USB_INTERFACE code g_Audio20InterfaceLineIn = 
{
	2,
	&g_AltLineIn,

	1,
	s_LineInEndpoints,

	StreamSetInterface,
	NULL,

	HandleStreamCmnd,
	HandleStreamDataOut
};

BYTE idata g_AltMicIn;
USB_INTERFACE code g_Audio20InterfaceMicIn = 
{
	2,
	&g_AltMicIn,

	1,
	s_MicInEndpoints,

	StreamSetInterface,
	NULL,

	HandleStreamCmnd,
	HandleStreamDataOut
};

BYTE idata g_AltSpdifIn;
USB_INTERFACE code g_Audio20InterfaceSpdifIn = 
{
	2,
	&g_AltSpdifIn,

	1,
	s_SpdifInEndpoints,

	StreamSetInterface,
	NULL,

	HandleStreamCmnd,
	HandleStreamDataOut
};

//-----------------------------------------------------------------------------
// Static Variables
//-----------------------------------------------------------------------------
static BYTE code AudioUnitTable[] = 
{
	AC_NODEF,		// 0
	AC_IT,			// 1
	AC_IT,			// 2
	AC_IT,			// 3
	AC_IT,			// 4
	AC_IT,			// 5
	AC_IT,			// 6
	AC_OT,			// 7
	AC_OT,			// 8
	AC_OT,			// 9
	AC_OT,			// 10
	AC_OT,			// 11
	AC_OT,			// 12
	AC_FEATURE,		// 13
	AC_FEATURE,		// 14
	AC_FEATURE,		// 15
	AC_FEATURE,		// 16
	AC_FEATURE,		// 17
	AC_CLKSRC,		// 18
	AC_CLKSRC,		// 19
	AC_CLKSRC,		// 20
	AC_CLKSRC,		// 21
	AC_CLKSRC,		// 22
	AC_CLKSRC		// 23
};

#define TERMINAL_CLUSTER_DSCR_NUM 6
static TE_CLUSTER_DSCR idata s_TerminalClusterTable[TERMINAL_CLUSTER_DSCR_NUM] = 
{
	{1, 0, 0x0000}, {2, 0, 0x0000}, {3, 0, 0x0000}, {4, 2, 0x0003}, {5, 2, 0x0003}, {6, 2, 0x0003}
};

#define TERMINAL_CONNECTOR_DSCR_NUM 6
#ifdef _MODEL_CM6632A_
static TE_CLUSTER_DSCR code s_TerminalConnectorTable[TERMINAL_CONNECTOR_DSCR_NUM] = 
{
	{4, 2, 0x0003}, {5, 2, 0x0003}, {6, 2, 0x0003}, {7, 8, 0x063F}, {8, 2, 0x0003}, {9, 2, 0x0003}
};
#else
static TE_CLUSTER_DSCR code s_TerminalConnectorTable[TERMINAL_CONNECTOR_DSCR_NUM] = 
{
	{4, 2, 0x0003}, {5, 2, 0x0003}, {6, 2, 0x0003}, {7, 2, 0x0003}, {8, 2, 0x0003}, {9, 2, 0x0003}
};
#endif

#ifdef _MODEL_CM6632A_

static BYTE code s_JackMicrophine[] =
{
	1,

	0x03, 0x00, 0x00, 0x00,
	0x00, 0x90, 0x01, 0x02,	// Pink, 1/8'' jack, front
};

static BYTE code s_JackLineIn[] =
{
	1,

	0x03, 0x00, 0x00, 0x00,
	0x00, 0x30, 0x01, 0x02,	// Blue, 1/8'' jack, front
};

static BYTE code s_JackSpeaker[] =
{
	4,

	0x03, 0x00, 0x00, 0x00,	// Front
	0x00, 0x40, 0x01, 0x02,	// Green, 1/8'' jack, front

	0x0C, 0x00, 0x00, 0x00,	// C/LFE
	0x00, 0x60, 0x01, 0x02,	// Orange, 1/8'' jack, front

	0x30, 0x00, 0x00, 0x00,	// Back
	0x00, 0x10, 0x01, 0x02,	// Black, 1/8'' jack, front

	0x00, 0x06, 0x00, 0x00,	// Side
	0x00, 0x20, 0x01, 0x02,	// Grey, 1/8'' jack, front
};

static BYTE code s_JackHeadphone[] =
{
	1,

	0x03, 0x00, 0x00, 0x00,
	0x00, 0x40, 0x01, 0x02,	// Green, 1/8'' jack, front
};

#else

static BYTE code s_JackMicrophine[] =
{
	1,

	0x03, 0x00, 0x00, 0x00,
	0x00, 0x90, 0x01, 0x02,	// Pink, 1/8'' jack, front
};

static BYTE code s_JackLineIn[] =
{
	2,

	0x01, 0x00, 0x00, 0x00,	// Left
	0x00, 0xE0, 0x04, 0x02,	// White, RCA, front

	0x02, 0x00, 0x00, 0x00,	// Right
	0x00, 0x50, 0x04, 0x02,	// Red, RCA, front
};

static BYTE code s_JackSpeaker[] =
{
	2,

	0x01, 0x00, 0x00, 0x00,	// Left
	0x00, 0xE0, 0x04, 0x02,	// White, RCA, front

	0x02, 0x00, 0x00, 0x00,	// Right
	0x00, 0x50, 0x04, 0x02,	// Red, RCA, front
};

static BYTE code s_JackHeadphone[] =
{
	1,

	0x03, 0x00, 0x00, 0x00,
	0x00, 0x40, 0x01, 0x02,	// Green, 1/8'' jack, front
};

#endif

static BYTE code s_JackSpdifIn[] =
{
	1,

	0x03, 0x00, 0x00, 0x00,
	0x00, 0x10, 0x04, 0x01,	// Black, RCA, rear
};

static BYTE code s_JackSpdifOut[] =
{
	1,

	0x03, 0x00, 0x00, 0x00,
	0x00, 0x20, 0x04, 0x01,	// Grey, RCA, rear
};

static BYTE* code s_TerminalJackTable[TERMINAL_CONNECTOR_DSCR_NUM] = 
{
	s_JackMicrophine,
	s_JackLineIn,
	s_JackSpdifIn,
	s_JackSpeaker,
	s_JackSpdifOut,
	s_JackHeadphone
};

#define I2S_HS_CLOSK_SOURCE_RANGE_SIZE 74
static BYTE code s_I2sHighSpeedClockSourceRange[I2S_HS_CLOSK_SOURCE_RANGE_SIZE] = 
{
	6,0,
//	0x40,0x1F,0,0,0x40,0x1F,0,0,0,0,0,0,	// 8000
//	0x80,0x3E,0,0,0x80,0x3E,0,0,0,0,0,0,	// 16000
//	0x00,0x7D,0,0,0x00,0x7D,0,0,0,0,0,0,	// 32000
	0x44,0xAC,0,0,0x44,0xAC,0,0,0,0,0,0,	// 44100
	0x80,0xBB,0,0,0x80,0xBB,0,0,0,0,0,0,	// 48000
//	0x00,0xFA,0,0,0x00,0xFA,0,0,0,0,0,0,	// 64000
	0x88,0x58,1,0,0x88,0x58,1,0,0,0,0,0,	// 88200
	0x00,0x77,1,0,0x00,0x77,1,0,0,0,0,0,	// 96000
	0x10,0xB1,2,0,0x10,0xB1,2,0,0,0,0,0,	// 176400
	0x00,0xEE,2,0,0x00,0xEE,2,0,0,0,0,0	// 192000
};

#define SPDIF_HS_CLOSK_SOURCE_RANGE_SIZE 74
static BYTE code s_SpdifHighSpeedClockSourceRange[SPDIF_HS_CLOSK_SOURCE_RANGE_SIZE] = 
{
	6,0,
//	0x00,0x7D,0,0,0x00,0x7D,0,0,0,0,0,0,	// 32000
	0x44,0xAC,0,0,0x44,0xAC,0,0,0,0,0,0,	// 44100
	0x80,0xBB,0,0,0x80,0xBB,0,0,0,0,0,0,	// 48000
	0x88,0x58,1,0,0x88,0x58,1,0,0,0,0,0,	// 88200
	0x00,0x77,1,0,0x00,0x77,1,0,0,0,0,0,	// 96000
	0x10,0xB1,2,0,0x10,0xB1,2,0,0,0,0,0,	// 176400
	0x00,0xEE,2,0,0x00,0xEE,2,0,0,0,0,0	// 192000
};

#define FS_CLOSK_SOURCE_RANGE_SIZE 26
static BYTE code s_FullSpeedClockSourceRange[FS_CLOSK_SOURCE_RANGE_SIZE] = 
{
	2,0,
	0x44,0xAC,0,0,0x44,0xAC,0,0,0,0,0,0,
	0x80,0xBB,0,0,0x80,0xBB,0,0,0,0,0,0
};

#define CLOCK_SOURCE_DSCR_NUM 6
static CLOCK_SOURCE_DSCR code s_ClockSourceTable[CLOCK_SOURCE_DSCR_NUM] = 
{
	{18, &g_Audio20InterfaceSpeaker},
	{19, &g_Audio20InterfaceSpdifOut},
	{20, &g_Audio20InterfaceHeadphone},
	{21, &g_Audio20InterfaceMicIn},
	{22, &g_Audio20InterfaceLineIn},
	{23, &g_Audio20InterfaceSpdifIn}
};

static BYTE idata s_CurrentClock[CLOCK_SOURCE_DSCR_NUM] = 
{
	DMA_48000,
	DMA_48000,
	DMA_48000,
	DMA_48000,
	DMA_48000,
	DMA_48000
};

//-----------------------------------------------------------------------------
// Code
//-----------------------------------------------------------------------------
static BOOL HandleBoost(BOOL data_out_stage)
{
	if(g_UsbRequest.request == CMD_CURRENT)
	{
		for(g_Index = 0; g_Index < BOOST_DSCR_NUM; ++g_Index)
		{
			if((g_UsbRequest.index_H == g_BoostTable[g_Index].id) &&
				(g_UsbRequest.value_L == g_BoostTable[g_Index].ch))
				break;
		}

		if(BOOST_DSCR_NUM == g_Index)
		{
			return FALSE;
		}
		
		if(g_UsbRequest.type & bmBIT7) // Device to Host, Get request
		{	
			g_UsbCtrlData[0] = GetCurrentBoost(g_Index);
			SetUsbCtrlData(g_UsbCtrlData, 1);
			return TRUE;
		}
		else
		{
			if(data_out_stage)
			{
				SetCurrentBoost(g_Index, g_UsbCtrlData[0]);
			}
			else
			{
				SetUsbCtrlData(g_UsbCtrlData, 1);
			}
			return TRUE;
		}
	}

	return FALSE;		
}

static BOOL HandleVolume(BOOL data_out_stage)
{
	for(g_Index = 0; g_Index < VOLUME_DSCR_NUM; ++g_Index)
	{
		if((g_UsbRequest.index_H == g_VolumeTable[g_Index].id) && 
			(g_UsbRequest.value_L == g_VolumeTable[g_Index].ch))
			break;
	}
	
	if(VOLUME_DSCR_NUM == g_Index)
	{
		return FALSE;
	}
	
	if(g_UsbRequest.type & bmBIT7) // Device to Host, Get request
	{
		if(CMD_CURRENT == g_UsbRequest.request)
		{
			g_TempWord1 = GetCurrentVol(g_Index);
			g_UsbCtrlData[0] = LSB(g_TempWord1);
			g_UsbCtrlData[1] = MSB(g_TempWord1);
			SetUsbCtrlData(g_UsbCtrlData, 2);
			return TRUE;
		}	  
		else if(CMD_RANGE == g_UsbRequest.request)
		{
			g_UsbCtrlData[0] = 1;
			g_UsbCtrlData[1] = 0;
			g_UsbCtrlData[2] = LSB(g_VolumeTable[g_Index].min);
			g_UsbCtrlData[3] = MSB(g_VolumeTable[g_Index].min);
			g_UsbCtrlData[4] = LSB(g_VolumeTable[g_Index].max);
			g_UsbCtrlData[5] = MSB(g_VolumeTable[g_Index].max);
			g_UsbCtrlData[6] = LSB(g_VolumeTable[g_Index].res);
			g_UsbCtrlData[7] = MSB(g_VolumeTable[g_Index].res);
			SetUsbCtrlData(g_UsbCtrlData, 8);
			return TRUE;
		}
	}
	else
	{
		if(CMD_CURRENT == g_UsbRequest.request)
		{
			if(data_out_stage)
			{
				SetCurrentVol(g_Index, (g_UsbCtrlData[1] << 8) | g_UsbCtrlData[0]);
			}
			else
			{
				SetUsbCtrlData(g_UsbCtrlData, 2);
			}
			return TRUE;
		}
	}      
	
	return FALSE;
}

static BOOL HandleMute(BOOL data_out_stage)
{
	if((g_UsbRequest.value_L == 0) && (CMD_CURRENT == g_UsbRequest.request)) /* CN = 0 */
	{
		for(g_Index = 0; g_Index<MUTE_DSCR_NUM; ++g_Index)
		{
			if(g_UsbRequest.index_H == g_MuteTable[g_Index])
				break;
		}		
	
		if(MUTE_DSCR_NUM == g_Index)
		{
			return FALSE;
		}
	
		if(g_UsbRequest.type & bmBIT7) // Device to Host, Get request
		{
			g_UsbCtrlData[0] = GetCurrentMute(g_Index);
			SetUsbCtrlData(g_UsbCtrlData, 1);
			return TRUE;
		}
		else
		{
			if(data_out_stage)
			{
				SetCurrentMute(g_Index, g_UsbCtrlData[0]);
			}
			else
			{
				SetUsbCtrlData(g_UsbCtrlData, 1);
			}
			return TRUE;
		}
	}
	
	return FALSE;
}

static BOOL HandleTerminalCluster()
{
	/* Device to Host, Get Current request, and CN = 0 */
	if((g_UsbRequest.type & bmBIT7) && (CMD_CURRENT == g_UsbRequest.request) && (g_UsbRequest.value_L == 0))
	{
		for(g_Index = 0; g_Index<TERMINAL_CLUSTER_DSCR_NUM; ++g_Index)
		{
			if(g_UsbRequest.index_H == s_TerminalClusterTable[g_Index].id)
				break;
		}
	
		if(TERMINAL_CLUSTER_DSCR_NUM == g_Index)
			return FALSE;

		g_UsbCtrlData[0] = s_TerminalClusterTable[g_Index].channel_no;
		g_UsbCtrlData[1] = LSB(s_TerminalClusterTable[g_Index].channel_cfg);
		g_UsbCtrlData[2] = MSB(s_TerminalClusterTable[g_Index].channel_cfg);
		g_UsbCtrlData[3] = 0;
		g_UsbCtrlData[4] = 0;
		g_UsbCtrlData[5] = 0;

		SetUsbCtrlData(g_UsbCtrlData, 6);
	}
	else
	{
		return FALSE;
	}

	return TRUE;
}

static BOOL HandleTerminalConnector()
{
	/* Device to Host, Get Current request, and CN = 0 */
	if((g_UsbRequest.type & bmBIT7) && (CMD_CURRENT == g_UsbRequest.request))
	{
		for(g_Index = 0; g_Index < TERMINAL_CONNECTOR_DSCR_NUM; ++g_Index)
		{
			if(g_UsbRequest.index_H == s_TerminalConnectorTable[g_Index].id)
				break;
		}

		if(TERMINAL_CONNECTOR_DSCR_NUM == g_Index)
			return FALSE;

		if(g_UsbRequest.value_L == 0)
		{
			g_UsbCtrlData[0] = s_TerminalConnectorTable[g_Index].channel_no;
			g_UsbCtrlData[1] = LSB(s_TerminalConnectorTable[g_Index].channel_cfg);
			g_UsbCtrlData[2] = MSB(s_TerminalConnectorTable[g_Index].channel_cfg);
			g_UsbCtrlData[3] = 0;
			g_UsbCtrlData[4] = 0;
			g_UsbCtrlData[5] = 0;

			SetUsbCtrlData(g_UsbCtrlData, 6);
		}
		else if(g_UsbRequest.value_L == 0xFF)
		{
			SetUsbCtrlData(s_TerminalJackTable[g_Index], (s_TerminalJackTable[g_Index][0]<<3)+1);
		}
		else
		{
			return FALSE;
		}
	}
	else
	{
		return FALSE;
	}

	return TRUE;
}

static BOOL HandleClockValid()
{
	/* Get request and CN = 0 */
	if((g_UsbRequest.type & bmBIT7) && (CMD_CURRENT == g_UsbRequest.request) && (g_UsbRequest.value_L == 0))
	{
		for(g_Index = 0; g_Index<CLOCK_SOURCE_DSCR_NUM; ++g_Index)
		{
			if(g_UsbRequest.index_H == s_ClockSourceTable[g_Index].id)
				break;
		}
		
		if(CLOCK_SOURCE_DSCR_NUM == g_Index)
		{
			return FALSE;
		}

		if((s_ClockSourceTable[g_Index].pInterface->pEndpoints[0].number==EP_SPDIF_REC) && (!g_SpdifLockStatus))
		{
			g_UsbCtrlData[0] = 0;
		}
		else
		{
			g_UsbCtrlData[0] = 1;
		}

		SetUsbCtrlData(g_UsbCtrlData, 1);
		return TRUE;
	}

	return FALSE;
}

static BOOL HandleClockFrequency(BOOL data_out_stage)
{
	for(g_Index = 0; g_Index < CLOCK_SOURCE_DSCR_NUM; ++g_Index)
	{
		if(g_UsbRequest.index_H == s_ClockSourceTable[g_Index].id)
			break;
	}

	if(CLOCK_SOURCE_DSCR_NUM == g_Index)
	{
		return FALSE;
	}

	if(g_UsbRequest.type & bmBIT7) // Device to Host, Get request
	{
		if(CMD_CURRENT == g_UsbRequest.request)
		{
			if(s_ClockSourceTable[g_Index].pInterface->pEndpoints[0].number == EP_SPDIF_REC)
			{
				g_TempByte1 = g_SpdifRateStatus;
			}
			else
			{
				g_TempByte1 = s_CurrentClock[g_Index];
			}

			ControlByteToFreq(g_TempByte1);
			SetUsbCtrlData(g_UsbCtrlData, 4);
			return TRUE;
		}
		else if(CMD_RANGE == g_UsbRequest.request)
		{
			if(g_UsbIsHighSpeed)
			{
				if((s_ClockSourceTable[g_Index].pInterface->pEndpoints[0].number==EP_SPDIF_REC) || 
					(s_ClockSourceTable[g_Index].pInterface->pEndpoints[0].number==EP_SPDIF_PLAY))
				{
					SetUsbCtrlData(s_SpdifHighSpeedClockSourceRange, SPDIF_HS_CLOSK_SOURCE_RANGE_SIZE);
				}
				else
				{
					SetUsbCtrlData(s_I2sHighSpeedClockSourceRange, I2S_HS_CLOSK_SOURCE_RANGE_SIZE);
				}
			}
			else
			{
				SetUsbCtrlData(s_FullSpeedClockSourceRange, FS_CLOSK_SOURCE_RANGE_SIZE);
			}
			return TRUE;
		}	  
	}
	else /* Host to Device, Set request */
	{
		//if(s_ClockSourceTable[g_Index].endpoint == EP_SPDIF_REC)
		//	return FALSE;

		if(CMD_CURRENT == g_UsbRequest.request)
		{
			if(data_out_stage)
			{
 				g_TempByte1 = FreqToControlByte();
				s_CurrentClock[g_Index] = g_TempByte1;

				// If alt-setting of the interface is not 0, restart the stream.
				if(*(s_ClockSourceTable[g_Index].pInterface->pAltSetting))
				{
					switch(s_ClockSourceTable[g_Index].pInterface->pEndpoints[0].number)
					{
						case EP_MULTI_CH_PLAY:
							PlayMultiChStop();
							PlayMultiChOpen(*(s_ClockSourceTable[g_Index].pInterface->pAltSetting));
							break;

						case EP_SPDIF_PLAY:
							PlaySpdifStop();
							PlaySpdifOpen(*(s_ClockSourceTable[g_Index].pInterface->pAltSetting));
							break;

						case EP_TWO_CH_PLAY:
							PlayTwoChStop();
							PlayTwoChOpen(*(s_ClockSourceTable[g_Index].pInterface->pAltSetting));
							break;

						case EP_TWO_CH_REC:
							RecTwoChStop();
							RecTwoChOpen(*(s_ClockSourceTable[g_Index].pInterface->pAltSetting));
							break;

						case EP_MULTI_CH_REC:
							RecMultiChStop();
							RecMultiChOpen(*(s_ClockSourceTable[g_Index].pInterface->pAltSetting));
							break;

						case EP_SPDIF_REC:
							RecSpdifStop();
							RecSpdifOpen(*(s_ClockSourceTable[g_Index].pInterface->pAltSetting));
					}
				}
			}
			else
			{
				SetUsbCtrlData(g_UsbCtrlData, 4);
			}

			return TRUE;
		}
	}

	return FALSE;
}

static BOOL PlayMultiChOpen(BYTE alt)
{
	for(g_Index = 0; g_Index < CLOCK_SOURCE_DSCR_NUM; ++g_Index)
	{
		if(EP_MULTI_CH_PLAY == s_ClockSourceTable[g_Index].pInterface->pEndpoints[0].number)
			break;
	}

	if(CLOCK_SOURCE_DSCR_NUM == g_Index)
	{
		return FALSE;
	}

	SetDmaFreq(EP_MULTI_CH_PLAY, s_CurrentClock[g_Index]);

	if(g_UsbIsHighSpeed)
	{
		switch(alt)
		{
			case 1: // 2ch 16bits
				s_TerminalClusterTable[0].channel_no = 2;
				s_TerminalClusterTable[0].channel_cfg = 0x0003;
				return PlayMultiChStart(DMA_2CH, DMA_16Bit);
		
			case 2: // 2ch 24bits
				s_TerminalClusterTable[0].channel_no = 2;
				s_TerminalClusterTable[0].channel_cfg = 0x0003;
				return PlayMultiChStart(DMA_2CH, DMA_24Bit);

#ifdef _MODEL_CM6632A_

			case 3: // 4ch 16bits
				s_TerminalClusterTable[0].channel_no = 4;
				s_TerminalClusterTable[0].channel_cfg = 0x0033;
				return PlayMultiChStart(DMA_4CH, DMA_16Bit);
		
			case 4: // 4ch 24bits
				s_TerminalClusterTable[0].channel_no = 4;
				s_TerminalClusterTable[0].channel_cfg = 0x0033;
				return PlayMultiChStart(DMA_4CH, DMA_24Bit);

			case 5: // 6ch 16bits
				s_TerminalClusterTable[0].channel_no = 6;
				s_TerminalClusterTable[0].channel_cfg = 0x003F;
				return PlayMultiChStart(DMA_6CH, DMA_16Bit);
		
			case 6: // 6ch 24bits
				s_TerminalClusterTable[0].channel_no = 6;
				s_TerminalClusterTable[0].channel_cfg = 0x003F;
				return PlayMultiChStart(DMA_6CH, DMA_24Bit);

			case 7: // 8ch 16bits
				s_TerminalClusterTable[0].channel_no = 8;
				s_TerminalClusterTable[0].channel_cfg = 0x063F;
				return PlayMultiChStart(DMA_8CH, DMA_16Bit);
		
			case 8: // 8ch 24bits
				s_TerminalClusterTable[0].channel_no = 8;
				s_TerminalClusterTable[0].channel_cfg = 0x063F;
				return PlayMultiChStart(DMA_8CH, DMA_24Bit);

#endif
		}
	}
	else
	{
		switch(alt)
		{
			case 1: // 2ch 16bits
				s_TerminalClusterTable[0].channel_no = 2;
				s_TerminalClusterTable[0].channel_cfg = 0x0003;
				return PlayMultiChStart(DMA_2CH, DMA_16Bit);
		}
	}

	return FALSE;
}

static BOOL PlaySpdifOpen(BYTE alt)
{
	for(g_Index = 0; g_Index < CLOCK_SOURCE_DSCR_NUM; ++g_Index)
	{
		if(EP_SPDIF_PLAY == s_ClockSourceTable[g_Index].pInterface->pEndpoints[0].number)
			break;
	}

	if(CLOCK_SOURCE_DSCR_NUM == g_Index)
	{
		return FALSE;
	}

	SetDmaFreq(EP_SPDIF_PLAY, s_CurrentClock[g_Index]);

	if(g_UsbIsHighSpeed)
	{
		switch(alt)
		{
			case 1: // 2ch 16bits
				s_TerminalClusterTable[1].channel_no = 2;
				s_TerminalClusterTable[1].channel_cfg = 0x0003;
				return PlaySpdifStart(DMA_16Bit);
		
			case 2: // 2ch 24bits
				s_TerminalClusterTable[1].channel_no = 2;
				s_TerminalClusterTable[1].channel_cfg = 0x0003;
				return PlaySpdifStart(DMA_24Bit);
				
			case 3: // 2ch 16bits non-pcm
				s_TerminalClusterTable[1].channel_no = 2;
				s_TerminalClusterTable[1].channel_cfg = 0x0003;
				return PlaySpdifStart(DMA_16Bit | NON_PCM);
		}
	}
	else
	{
		switch(alt)
		{
			case 1: // 2ch 16bits
				s_TerminalClusterTable[1].channel_no = 2;
				s_TerminalClusterTable[1].channel_cfg = 0x0003;
				return PlaySpdifStart(DMA_16Bit);
				
			case 2: // 2ch 16bits non-pcm
				s_TerminalClusterTable[1].channel_no = 2;
				s_TerminalClusterTable[1].channel_cfg = 0x0003;
				return PlaySpdifStart(DMA_16Bit | NON_PCM);
		}
	}

	return FALSE;
}

static BOOL PlayTwoChOpen(BYTE alt)
{
	for(g_Index = 0; g_Index < CLOCK_SOURCE_DSCR_NUM; ++g_Index)
	{
		if(EP_TWO_CH_PLAY == s_ClockSourceTable[g_Index].pInterface->pEndpoints[0].number)
			break;
	}

	if(CLOCK_SOURCE_DSCR_NUM == g_Index)
	{
		return FALSE;
	}

	SetDmaFreq(EP_TWO_CH_PLAY, s_CurrentClock[g_Index]);

	if(g_UsbIsHighSpeed)
	{
		switch(alt)
		{
			case 1: // 2ch 16bits
				s_TerminalClusterTable[2].channel_no = 2;
				s_TerminalClusterTable[2].channel_cfg = 0x0003;
				return PlayTwoChStart(DMA_16Bit);
		
			case 2: // 2ch 24bits
				s_TerminalClusterTable[2].channel_no = 2;
				s_TerminalClusterTable[2].channel_cfg = 0x0003;
				return PlayTwoChStart(DMA_24Bit);

			case 3: // 2ch 32bits
				s_TerminalClusterTable[2].channel_no = 2;
				s_TerminalClusterTable[2].channel_cfg = 0x0003;
				return PlayTwoChStart(DMA_32Bit);
		}
	}
	else
	{
		if(alt == 1)
		{
			s_TerminalClusterTable[2].channel_no = 2;
			s_TerminalClusterTable[2].channel_cfg = 0x0003;
			return PlayTwoChStart(DMA_16Bit);
		}
	}

	return FALSE;
}

static BOOL RecMultiChOpen(BYTE alt)
{
	for(g_Index = 0; g_Index < CLOCK_SOURCE_DSCR_NUM; ++g_Index)
	{
		if(EP_MULTI_CH_REC == s_ClockSourceTable[g_Index].pInterface->pEndpoints[0].number)
			break;
	}

	if(CLOCK_SOURCE_DSCR_NUM == g_Index)
	{
		return FALSE;
	}

	SetDmaFreq(EP_MULTI_CH_REC, s_CurrentClock[g_Index]);

	if(g_UsbIsHighSpeed)
	{
		switch(alt)
		{
			case 1: // 2ch 16bits
				return RecMultiChStart(DMA_2CH, DMA_16Bit);
		
			case 2: // 2ch 24bits
				return RecMultiChStart(DMA_2CH, DMA_24Bit);
		}
	}
	else
	{
		if(alt == 1)
		{
			return RecMultiChStart(DMA_2CH, DMA_16Bit);
		}
	}

	return FALSE;
}

static BOOL RecTwoChOpen(BYTE alt)
{
	for(g_Index = 0; g_Index < CLOCK_SOURCE_DSCR_NUM; ++g_Index)
	{
		if(EP_TWO_CH_REC == s_ClockSourceTable[g_Index].pInterface->pEndpoints[0].number)
			break;
	}

	if(CLOCK_SOURCE_DSCR_NUM == g_Index)
	{
		return FALSE;
	}

	SetDmaFreq(EP_TWO_CH_REC, s_CurrentClock[g_Index]);

	if(g_UsbIsHighSpeed)
	{
		switch(alt)
		{
			case 1: // 2ch 16bits
				return RecTwoChStart(DMA_16Bit);
		
			case 2: // 2ch 24bits
				return RecTwoChStart(DMA_24Bit);
		}
	}
	else
	{
		if(alt == 1)
		{
			return RecTwoChStart(DMA_16Bit);
		}
	}

	return FALSE;
}

static BOOL RecSpdifOpen(BYTE alt)
{
	for(g_Index = 0; g_Index < CLOCK_SOURCE_DSCR_NUM; ++g_Index)
	{
		if(EP_SPDIF_REC == s_ClockSourceTable[g_Index].pInterface->pEndpoints[0].number)
			break;
	}

	if(CLOCK_SOURCE_DSCR_NUM == g_Index)
	{
		return FALSE;
	}

	SetDmaFreq(EP_SPDIF_REC, s_CurrentClock[g_Index]);

	if(g_UsbIsHighSpeed)
	{
		switch(alt)
		{
			case 1: // 2ch 16bits
				return RecSpdifStart(DMA_16Bit);
		
			case 2: // 2ch 24bits
				return RecSpdifStart(DMA_24Bit);
		}
	}
	else
	{
		if(alt == 1)
		{
			return RecSpdifStart(DMA_16Bit);
		}
	}

	return FALSE;
}

static BOOL HandleControlCmnd()
{
	switch(AudioUnitTable[g_UsbRequest.index_H])
	{
	case AC_IT:
		if(g_UsbRequest.value_H == 4) // Cluster Control
		{
			return HandleTerminalCluster();
		}
		else if(g_UsbRequest.value_H == 2) // Connector Control
		{
			return HandleTerminalConnector();
		}
		break;
	
	case AC_OT:
		if(g_UsbRequest.value_H == 2) // Connector Control
		{
			return HandleTerminalConnector();
		}
		break;
	
	case AC_FEATURE:
		if(g_UsbRequest.value_H == 1) // Mute Control
		{
			return HandleMute(FALSE);
		}
		else if(g_UsbRequest.value_H == 2) // Volume Control
		{
			return HandleVolume(FALSE);
		}
		else if(g_UsbRequest.value_H == 7) // Auto Gain Control 
		{
			return HandleBoost(FALSE);	
		}
	  	break;

	case AC_CLKSRC:
		if(g_UsbRequest.value_H == 1) // Frequency Control
		{
			return HandleClockFrequency(FALSE);
		}
		else if(g_UsbRequest.value_H == 2) // Valid Control
		{
			return HandleClockValid();
		}
	  	break;
	}
 
	return FALSE;
}

static BOOL HandleControlDataOut()
{
	switch(AudioUnitTable[g_UsbRequest.index_H])
	{
	case AC_FEATURE:
		if(g_UsbRequest.value_H == 1) // Mute Control
		{
			return HandleMute(TRUE);
		}
		else if(g_UsbRequest.value_H == 2) // Volume Control
		{
			return HandleVolume(TRUE);
		}
		else if(g_UsbRequest.value_H == 7) // Auto Gain Control 
		{
			return HandleBoost(TRUE);	
		}	
	  	break;

	case AC_CLKSRC:
		if(g_UsbRequest.value_H == 1) // Frequency Control
		{
			return HandleClockFrequency(TRUE);
		}
	  	break;
	}

	return FALSE;
}

static BOOL StreamSetInterface(USB_INTERFACE* pInterface)
{
	if(pInterface->endpointNumbers != 1)
		return FALSE;

	if(g_UsbRequest.value_L)
	{
		switch(pInterface->pEndpoints[0].number)
		{
		case EP_MULTI_CH_PLAY:
			return PlayMultiChOpen(g_UsbRequest.value_L);

		case EP_SPDIF_PLAY:
			return PlaySpdifOpen(g_UsbRequest.value_L);

		case EP_TWO_CH_PLAY:
			return PlayTwoChOpen(g_UsbRequest.value_L);

		case EP_TWO_CH_REC:
			return RecTwoChOpen(g_UsbRequest.value_L);

		case EP_MULTI_CH_REC:
			return RecMultiChOpen(g_UsbRequest.value_L);

		case EP_SPDIF_REC:
			return RecSpdifOpen(g_UsbRequest.value_L);

		default:
			return FALSE;
		}
	}
	else
	{
		switch(pInterface->pEndpoints[0].number)
		{
		case EP_MULTI_CH_PLAY:
			s_TerminalClusterTable[0].channel_no = 0;
			s_TerminalClusterTable[0].channel_cfg = 0;
			return PlayMultiChStop();

		case EP_SPDIF_PLAY:
			s_TerminalClusterTable[1].channel_no = 0;
			s_TerminalClusterTable[1].channel_cfg = 0;
			return PlaySpdifStop();

		case EP_TWO_CH_PLAY:
			s_TerminalClusterTable[2].channel_no = 0;
			s_TerminalClusterTable[2].channel_cfg = 0;
			return PlayTwoChStop();

		case EP_TWO_CH_REC:
			return RecTwoChStop();

		case EP_MULTI_CH_REC:
			return RecMultiChStop();

		case EP_SPDIF_REC:
			return RecSpdifStop();

		default:
			return FALSE;
		}
	}

	return TRUE;
}


static BOOL HandleStreamCmnd(USB_INTERFACE* pInterface)
{
	if(pInterface->endpointNumbers != 1)
		return FALSE;

	/* Get CUR request and CN = 0, ID = 0 */
	if((CMD_CURRENT == g_UsbRequest.request) && (g_UsbRequest.value_L == 0) && (g_UsbRequest.index_H == 0))
	{
		switch(g_UsbRequest.value_H)
		{
			case 1:	// Active setting Control
				if(pInterface->pAltSetting)
				{
					SetUsbCtrlData(pInterface->pAltSetting, 1);
					return TRUE;
				}
	
			case 2:	// Valid setting Control
				g_TempByte1 = pInterface->maxAltSettingNumber;
				g_TempWord1 = 0;

				for(g_Index = 0; g_Index <= g_TempByte1; ++g_Index)
				{
					g_TempWord1 |= (1<<g_Index);
				}

				g_UsbCtrlData[0] = 2;
				g_UsbCtrlData[1] = LSB(g_TempWord1);
				g_UsbCtrlData[2] = MSB(g_TempWord1);

				SetUsbCtrlData(g_UsbCtrlData, 3);
				return TRUE;
	
			case 3:	// Data Format Control
				g_UsbCtrlData[0] = 1;
				g_UsbCtrlData[1] = 0;
				g_UsbCtrlData[2] = 0;
				g_UsbCtrlData[3] = 0;
	
				SetUsbCtrlData(g_UsbCtrlData, 4);
				return TRUE;
		}
	}

	return FALSE;
}

static BOOL HandleStreamDataOut()
{
	return FALSE;
}

