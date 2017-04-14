#include "usb.h"
#include "hid.h"
#include "midi.h"
#include "audio.h"
#include "peripheral.h"
#include "config.h"
#include "timer.h"

//-----------------------------------------------------------------------------
// External Variables
//-----------------------------------------------------------------------------
extern BYTE code Audio20DeviceDscr;
extern BYTE code Audio20DeviceQualDscr;
extern BYTE code Audio10DeviceDscr;
extern BYTE code Audio10DeviceQualDscr;
extern BYTE code Audio20HighSpeedConfigDscr;
extern BYTE code Audio20FullSpeedConfigDscr;
extern BYTE code Audio10HighSpeedConfigDscr;
extern BYTE code Audio10FullSpeedConfigDscr;
extern BYTE code StringDscr;

//-----------------------------------------------------------------------------
// Static Variables
//-----------------------------------------------------------------------------
static BOOL s_RecordMute = FALSE;
static BOOL s_PlayMute = FALSE;

//-----------------------------------------------------------------------------
// Build up USB device
//-----------------------------------------------------------------------------
static void HandleConfigured();
static void HandleDeconfigured();
static BOOL HandleVendorCmnd();
static BOOL HandleVendorDataOut();

static USB_INTERFACE* code s_Audio20Interfaces[] = 
{
	&g_Audio20InterfaceAudioCtrl,
	&g_Audio20InterfaceSpeaker,
	&g_Audio20InterfaceSpdifOut,
	&g_Audio20InterfaceHeadphone,
	&g_Audio20InterfaceMicIn,
	&g_Audio20InterfaceLineIn,
	&g_Audio20InterfaceSpdifIn,
	&g_InterfaceHid,

#ifdef _MIDI_
	&g_InterfaceMidiCtrl,
	&g_InterfaceMidiStream
#endif
};

static USB_DEVICE code s_Audio20HighSpeedDevice = 
{
	&Audio20DeviceDscr,
	&Audio20DeviceQualDscr,
	&Audio20HighSpeedConfigDscr,
	&Audio20FullSpeedConfigDscr,
	&StringDscr,

#ifdef _MIDI_
	10,
#else
	8,
#endif
	s_Audio20Interfaces,
	
	HandleConfigured,
	HandleDeconfigured,

	HandleVendorCmnd,
	HandleVendorDataOut
};

static USB_DEVICE code s_Audio20FullSpeedDevice = 
{
	&Audio20DeviceDscr,
#ifdef _FULL_SPEED_ONLY_
	NULL,
#else
	&Audio20DeviceQualDscr,
#endif	
	&Audio20FullSpeedConfigDscr,
#ifdef _FULL_SPEED_ONLY_
	NULL,
#else
	&Audio20HighSpeedConfigDscr,
#endif	
	&StringDscr,

#ifdef _MIDI_
	10,
#else
	8,
#endif
	s_Audio20Interfaces,
	
	HandleConfigured,
	HandleDeconfigured,

	HandleVendorCmnd,
	HandleVendorDataOut
};

#ifdef _MODEL_CM6601A_

static USB_INTERFACE* code s_Audio10Interfaces[] = 
{
	&g_Audio10InterfaceAudioCtrl,
	&g_Audio10InterfaceSpdifOut,
	&g_Audio10InterfaceSpdifIn,
	&g_Audio10InterfaceAudioCtrl,
	&g_Audio10InterfaceSpeaker,
	&g_Audio10InterfaceLineIn,
	&g_InterfaceHid,

#ifdef _MIDI_
	&g_InterfaceMidiCtrl,
	&g_InterfaceMidiStream,
#endif
};

#else

static USB_INTERFACE* code s_Audio10Interfaces[] = 
{
	&g_Audio10InterfaceAudioCtrl,
	&g_Audio10InterfaceSpdifOut,
	&g_Audio10InterfaceSpdifIn,
	&g_Audio10InterfaceAudioCtrl,
	&g_Audio10InterfaceSpeaker,
	&g_Audio10InterfaceLineIn,
	&g_Audio10InterfaceAudioCtrl,
	&g_Audio10InterfaceHeadphone,
	&g_Audio10InterfaceMicIn,
	&g_InterfaceHid,

#ifdef _MIDI_
	&g_InterfaceMidiCtrl,
	&g_InterfaceMidiStream,
#endif
};

#endif

static USB_DEVICE code s_Audio10HighSpeedDevice = 
{
	&Audio10DeviceDscr,
	&Audio10DeviceQualDscr,
	&Audio10HighSpeedConfigDscr,
	&Audio10FullSpeedConfigDscr,
	&StringDscr,

#ifdef _MODEL_CM6601A_

#ifdef _MIDI_
	9,
#else
	7,
#endif

#else

#ifdef _MIDI_
	12,
#else
	10,
#endif

#endif
	s_Audio10Interfaces,

	HandleConfigured,
	HandleDeconfigured,

	HandleVendorCmnd,
	HandleVendorDataOut
};

static USB_DEVICE code s_Audio10FullSpeedDevice = 
{
	&Audio10DeviceDscr,
#ifdef _FULL_SPEED_ONLY_
	NULL,
#else
	&Audio10DeviceQualDscr,
#endif	
	&Audio10FullSpeedConfigDscr,
#ifdef _FULL_SPEED_ONLY_
	NULL,
#else
	&Audio10HighSpeedConfigDscr,
#endif	
	&StringDscr,

#ifdef _MODEL_CM6601A_

#ifdef _MIDI_
	9,
#else
	7,
#endif

#else

#ifdef _MIDI_
	12,
#else
	10,
#endif

#endif
	s_Audio10Interfaces,

	HandleConfigured,
	HandleDeconfigured,

	HandleVendorCmnd,
	HandleVendorDataOut
};

//-----------------------------------------------------------------------------
// Peripheral Callback Functions
//-----------------------------------------------------------------------------
void HandleGpio()
{
	g_InputReport.source |= bmBIT0;
	SendInputReport();
}

void HandleGpi()
{
	g_TempByte1 = PERI_ReadByte(GPI_DATA);
	g_InputReport.button = g_TempByte1 & 0x07;

	if(g_TempByte1 & bmBIT3)  // Record Mute
	{
		s_RecordMute = !s_RecordMute;
		SetRecordMute(s_RecordMute);			
	}

	g_InputReport.source |= bmBIT1;
	SendInputReport();
}

//-----------------------------------------------------------------------------
// USB Callback Functions
//-----------------------------------------------------------------------------
void HandleUsbReset()
{
#ifdef _MODEL_CM6601A_

	g_IsAudioClass20 = FALSE;

	if(g_UsbIsHighSpeed)
	{
		RegisterUsbDevice(&s_Audio10HighSpeedDevice);
	}
	else
	{
		RegisterUsbDevice(&s_Audio10FullSpeedDevice);
	}

#else

#ifdef _AUDIO_CLASS_20_ONLY_

	g_IsAudioClass20 = TRUE;

	if(g_UsbIsHighSpeed)
	{
		RegisterUsbDevice(&s_Audio20HighSpeedDevice);
	}
	else
	{
		RegisterUsbDevice(&s_Audio20FullSpeedDevice);
	}

#else

	if(PERI_ReadByte(GPI_DATA) & bmBIT4)
	{
		g_IsAudioClass20 = TRUE;

		if(g_UsbIsHighSpeed)
		{
			RegisterUsbDevice(&s_Audio20HighSpeedDevice);
		}
		else
		{
			RegisterUsbDevice(&s_Audio20FullSpeedDevice);
		}
	}
	else
	{
		g_IsAudioClass20 = FALSE;

		if(g_UsbIsHighSpeed)
		{
			RegisterUsbDevice(&s_Audio10HighSpeedDevice);
		}
		else
		{
			RegisterUsbDevice(&s_Audio10FullSpeedDevice);
		}
	}

#endif

#endif

	HidInit();

#ifdef _MIDI_
	MidiInit();
#endif

#ifdef _I2C_SLAVE_SUPPORT_  
        PERI_WriteByte(I2C_SLAVE_CTRL, 0x88);	// Enable data sync.
#endif
}

void HandleUsbResume()
{
	if(g_UsbConfiguration)
	{
		PERI_WriteByte(PAD_GP22_GP23_CTRL, 0x3B);   //XPWDN/XRSTO output enable, close the anti-pop relay
		AudioInit();
	}
}	

void HandleUsbSuspend()
{
	if(g_UsbConfiguration)
	{
		AudioStop();	
		PERI_WriteByte(PAD_GP22_GP23_CTRL, 0xBB);   //XPWDN/XRSTO output disable, open the anti-pop relay
	}
}

void HandleInt4Complete()
{
	g_InputReport.source = 0;
}

void HandleInt15Complete()
{

}

//-----------------------------------------------------------------------------
// USB Requests Handler Functions
//-----------------------------------------------------------------------------
static void HandleConfigured()
{
	PERI_WriteByte(PAD_GP22_GP23_CTRL, 0x3B);	// XPWDN/XRSTO output enable, close the anti-pop relay
	AudioInit();
}

static void HandleDeconfigured()
{
	PERI_WriteByte(GPIO_DATA_H, PERI_ReadByte(GPIO_DATA_H) & ~bmBIT5);
}

static BOOL HandleVendorCmnd()
{
	if(g_UsbRequest.type == 0x40)	// Vendor Write
	{
		if(g_UsbRequest.request == 1)      // Register Write
		{
			if(g_UsbCtrlLength <= USB_CTRL_BUF_SIZE)
			{
				SetUsbCtrlData(g_UsbCtrlData, g_UsbCtrlLength);
				return TRUE;
			}
		}
		else if(g_UsbRequest.request == 7)      // XRAM Write
		{
			if(g_UsbCtrlLength <= USB_CTRL_BUF_SIZE)
			{
				SetUsbCtrlData(g_UsbCtrlData, g_UsbCtrlLength);
				return TRUE;
			}
		}
	}
	else if(g_UsbRequest.type == 0xC0)	// Vendor Read
	{
		if(g_UsbRequest.request == 2) // Register Read
		{
			g_TempWord1 = (g_UsbRequest.value_H<<8) | g_UsbRequest.value_L;    // Register address

			for(g_Index=0; g_Index<USB_CTRL_BUF_SIZE; ++g_Index)
			{
				g_UsbCtrlData[g_Index] = PERI_ReadByte(g_TempWord1+g_Index);
			}

			SetUsbCtrlData(g_UsbCtrlData, USB_CTRL_BUF_SIZE);
			return TRUE;
		}
		else if(g_UsbRequest.request == 4) // Flash Read
		{
			g_TempWord1 = (g_UsbRequest.value_H<<8) | g_UsbRequest.value_L;   // Flash address

			for(g_Index=0; g_Index<USB_CTRL_BUF_SIZE; ++g_Index)
			{
				g_UsbCtrlData[g_Index] = FlashRead(g_TempWord1+g_Index);
			}

			SetUsbCtrlData(g_UsbCtrlData, USB_CTRL_BUF_SIZE);
			return TRUE;
		}
		else if(g_UsbRequest.request == 8)      // XRAM Read
		{
			g_TempByte1 = g_UsbRequest.value_L;    // Register address

			for(g_Index=0; g_Index<USB_CTRL_BUF_SIZE; ++g_Index)
			{
				g_UsbCtrlData[g_Index] = *((BYTE xdata *)(g_TempByte1+g_Index));
			}

			SetUsbCtrlData(g_UsbCtrlData, USB_CTRL_BUF_SIZE);
			return TRUE;
		}
	}

	return FALSE;
}

static BOOL HandleVendorDataOut()
{
	if(g_UsbRequest.type == 0x40)	// Vendor Write
	{
		if(g_UsbRequest.request == 1)      // Register Write
		{
			g_TempWord1 = (g_UsbRequest.value_H<<8) | g_UsbRequest.value_L;    // Register address

			for(g_Index=0; g_Index<g_UsbCtrlLength; ++g_Index)
			{
				PERI_WriteByte(g_TempWord1+g_Index, g_UsbCtrlData[g_Index]);
			}

			return TRUE;
		}
		else if(g_UsbRequest.request == 7)      // XRAM Write
		{
			g_TempByte1 = g_UsbRequest.value_L;    // Register address

			for(g_Index=0; g_Index<g_UsbCtrlLength; ++g_Index)
			{
				*((BYTE xdata *)(g_TempByte1+g_Index)) = g_UsbCtrlData[g_Index];
			}
	
			return TRUE;
		}
	}

	return FALSE;
}

//-----------------------------------------------------------------------------
// Main Process
//-----------------------------------------------------------------------------
static void GpioInit()
{
	PERI_WriteByte(0xF4, 0x33);	// I2C master and slave pull up
	PERI_WriteByte(PAD_GP24_GP25_CTRL, 0xB8);	// Flash driving current 2mA for improving jitter

	PERI_WriteByte(GPIO_DIRECTION_L, PERI_ReadByte(GPIO_DIRECTION_L) | bmBIT7); //GPIO7 output mode
	PERI_WriteByte(GPIO_DIRECTION_H, 0xFF); //set GPIO 8.9.10.11.12.13.14.15 output mode	
	PERI_WriteByte(GPI_INT_MASK, bmBIT5 | bmBIT3 | bmBIT2 | bmBIT1 | bmBIT0);

	PERI_WriteByte(GPIO_DATA_H, 0x00);	 //GPIO 8.9 use to select ADC CS5381 sampling rate range 00: 2KHz ~ 54KHz	
	
	//add ben gpio12 低电平
	PERI_WriteByte(GPIO_DATA_H, PERI_ReadByte(GPIO_DATA_H) & (~bmBIT4));
}

extern int g_PlayMode;
static void PowerOnReset()
{
#ifdef _REMOTE_WAKEUP_
	MCU_EXIST_REPORT |= bmBIT2;
#endif

#ifdef _MIDI_
	SCON = 0x50;		// setup serial port control
	TMOD = 0x21;	// Timer1 auto reload, Timer0 16 bits timer
	TH1 = 0xFC;		// For 31250 BAUD @48MHz
	TH0 = 0x63;		// For 10ms time tick
	TL0 = 0xC0;
	TCON = 0x51;		// INT1 to level triger, Timer1 Timer0 run

	IE = 0x17;		// Timer0, INT0, INT1, UART interrupt enable
	IP = 0x15;		// INT0, INT1, UART interrupt high priority
#else
	TMOD = 0x01;	// Timer0 16 bits timer
	TH0 = 0x63;		// For 10ms time tick
	TL0 = 0xC0;
	TCON = 0x11;		// INT1 to level triger, Timer0 run

	IE = 0x07;		// Timer0, INT0, INT1 interrupt enable
	IP = 0x05;		// INT0, INT1 interrupt high priority
#endif

	EA  = 1; // Enable interrupts

	while(!PERI_ReadByte(GPIO_DIRECTION_H));	// Wait until peripheral interface ready

	GpioInit();
	UsbInit();

	g_IsAudioClass20 = TRUE;
	RegisterUsbDevice(&s_Audio20HighSpeedDevice);
	
	//add ben
	g_PlayMode = 0;
	s_PlayMute = FALSE;
}	

void main()
{
	PowerOnReset();

	// Main Loop
	while(TRUE)
	{
		if(g_GpiRequest)
		{
			g_GpiRequest = FALSE;
			HandleGpi();
		}

		if(g_GpioRequest)
		{
			g_GpioRequest = FALSE;
			HandleGpio();
		}

		UsbProcess();

		if(g_UsbIsActive)
		{
			//add ben
			if (s_PlayMute == FALSE)
			{
				g_Tick500ms_Switch = 1;
				if(g_Tick500ms > 100)
				{
					g_Tick500ms_Switch = 0;
					g_Tick500ms = 0;
					s_PlayMute = TRUE;
				}					
			}
			else
			{
				g_Tick500ms_Switch = 0;	
				g_Tick500ms = 0;			
			}	
		}	
		else
		{
				g_Tick500ms_Switch = 0;	
				g_Tick500ms = 0;
				s_PlayMute = FALSE;	
		}	
		
		if (s_PlayMute == FALSE)
		{
			PERI_WriteByte(GPIO_DATA_H, PERI_ReadByte(GPIO_DATA_H) & (~bmBIT4));	
		}	
		else
		{
			//add ben gpio12 高电平
			PERI_WriteByte(GPIO_DATA_H, PERI_ReadByte(GPIO_DATA_H) | bmBIT4);				
		}	
		
		if(g_UsbIsActive)
		{
			AudioProcess();
#ifdef _MIDI_
			MidiProcess();
#endif
		}
	}
}

//-----------------------------------------------------------------------------
// For debugging
//-----------------------------------------------------------------------------
#ifdef _DEBUG_

#include "uart.h"

void PutChar(BYTE c)
{
	if(!TxQueueIsFull())
	{
		PutIntoTxQueue(c);
	}
}

void PrintString(BYTE *p)
{
	while(*p != '\0')
	{
		PutChar(*p++);
	}
}

static BYTE code s_HexAscii[16] = "0123456789ABCDEF";
void PrintByte(BYTE i)
{
	PutChar(s_HexAscii[(i>>4) & 0x0F]);
	PutChar(s_HexAscii[i & 0x0F]);
}

#endif

