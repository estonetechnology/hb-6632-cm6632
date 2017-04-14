#include "midi.h"
#include "uart.h"

//-----------------------------------------------------------------------------
// USB Interface Structure Definition
//-----------------------------------------------------------------------------
USB_INTERFACE code g_InterfaceMidiStream;

static USB_ENDPOINT code s_MidiEndpoints[2] = 
{
	{
		EP_BULK_OUT,
		&g_InterfaceMidiStream,

		NULL,
		NULL
	},
	
	{
		EP_BULK_IN,
		&g_InterfaceMidiStream,

		NULL,
		NULL
	}
};

USB_INTERFACE code g_InterfaceMidiCtrl = 
{
	0,
	NULL,

	0,
	NULL,

	NULL,
	NULL,

	NULL,
	NULL
};

USB_INTERFACE code g_InterfaceMidiStream = 
{
	0,
	NULL,

	2,
	s_MidiEndpoints,

	NULL,
	NULL,

	NULL,
	NULL
};

//-----------------------------------------------------------------------------
// External Variables
//-----------------------------------------------------------------------------
extern volatile BOOL g_BulkOutRequest;

//-----------------------------------------------------------------------------
// Static Variables
//-----------------------------------------------------------------------------
static BYTE idata s_UartRxByte;

static BYTE code s_MsgLenA[] = {                   3,3,3,3, 2,2,3   }; // 8x..Ex
static BYTE code s_MsgLenB[] = { 3,2,3,2, 0,0,1,0, 1,0,1,1, 1,0,1,1 }; // F0..FF
static BYTE code s_CinTbl [] = { 5,2,3 };
static BYTE code s_Cidlen [] = { 0,0,2,3, 3,1,2,3, 3,3,3,3, 2,2,3,1 };

static BYTE idata s_CurCmd = 0;
static BYTE idata s_MidiOutMsgCnt = 0;
static BYTE idata s_MidiInMsgCnt = 0;
static BYTE idata s_MidiInMsgBuf[3] = {0,0,0};

//-----------------------------------------------------------------------------
// Code
//-----------------------------------------------------------------------------
static BYTE MsgLen(BYTE cmd)
{
	return cmd < 0xf0 ? s_MsgLenA[(cmd>>4)-8] : s_MsgLenB[cmd&0x0f];
}

static BYTE Cin()
{
	if(s_CurCmd < 0xF0)
		return (s_CurCmd>>4);

	if(s_CurCmd == 0xF0) 
		return 4 + ((s_MidiInMsgBuf[s_MidiInMsgCnt-1] == 0xF7) ? s_MidiInMsgCnt : 0);

	if(s_CurCmd < 0xF8) 
		return s_CinTbl[s_MidiInMsgCnt-1];

	return 15;
}

static void DonePacket()
{
	if(!MsgLen(s_CurCmd)) // undefined codes are indicated by length 0
		return; 

	if(!(BULK_CTRL_3 & bmBIT3))	// If bulk-in fifo is not full
	{
		BULK_IN_DATA[0] = Cin();
		BULK_IN_DATA[1] = s_MidiInMsgBuf[0];
		BULK_IN_DATA[2] = s_MidiInMsgBuf[1];
		BULK_IN_DATA[3] = s_MidiInMsgBuf[2];

		//if(g_UsbIsHighSpeed)
		//	BULK_CTRL_2 = 0x7D;
		//else
			BULK_CTRL_2 = 0x5D;
	}
	
	s_MidiInMsgCnt = s_MidiInMsgBuf[0] = s_MidiInMsgBuf[1] = s_MidiInMsgBuf[2] = 0; // prepare for next message

	if(s_CurCmd < 0xF0)
		s_MidiInMsgBuf[s_MidiInMsgCnt++] = s_CurCmd;   // insert running status

	if(s_CurCmd > 0xF0) 
		s_CurCmd = 0;                  // preserve running status
}

static void PassPacket()
{
	if(!(BULK_CTRL_3 & bmBIT3))	// If bulk-in fifo is not full
	{
		BULK_IN_DATA[0] = 0x0F;
		BULK_IN_DATA[1] = s_UartRxByte;
		BULK_IN_DATA[2] = 0;
		BULK_IN_DATA[3] = 0;

		//if(g_UsbIsHighSpeed)
		//	BULK_CTRL_2 = 0x7D;
		//else
			BULK_CTRL_2 = 0x5D;
	}
}

static void ProcessMidiInByte()
{
#if 1
	if(s_UartRxByte > 0xF7)
	{ 
		if(MsgLen(s_UartRxByte)) 
			PassPacket(); 
			
		return;
	}

	if(s_UartRxByte > 0x7F && s_CurCmd == 0xF0)
	{ 
		s_MidiInMsgBuf[s_MidiInMsgCnt++] = 0xF7; 
		DonePacket(); 
	}

	if(s_UartRxByte > 0x7F)
	{
		s_CurCmd = s_UartRxByte; 
		s_MidiInMsgCnt = 0;
	}

	if(s_CurCmd)
		s_MidiInMsgBuf[s_MidiInMsgCnt++] = s_UartRxByte;
	
	if(s_CurCmd && s_MidiInMsgCnt >= MsgLen(s_CurCmd))
		DonePacket();
#else

	BULK_IN_DATA[s_MidiInMsgCnt++] = s_UartRxByte;
	if(s_MidiInMsgCnt == 4)
	{
		s_MidiInMsgCnt = 0;

		if(g_UsbIsHighSpeed)
			BULK_CTRL_2 = 0x7D;
		else
			BULK_CTRL_2 = 0x5D;
	}

#endif
}

static void ProcessMidiOutByte()
{
	if(!TxQueueIsFull())
	{
		if(s_MidiOutMsgCnt <= s_Cidlen[BULK_OUT_DATA[0]&0x0F])
		{
			PutIntoTxQueue(BULK_OUT_DATA[s_MidiOutMsgCnt]);
			s_MidiOutMsgCnt ++;
		}
		else
		{
			s_MidiOutMsgCnt = 0;
			BULK_CTRL_1 |= bmBIT0;
		}
	}
}

void MidiInit()
{
	/*if(g_UsbIsHighSpeed)
	{
		// Set Maximum packet size of Bulk endpoint to 512 bytes and flush fifo
		BULK_CTRL_1 = 0x0E; 
		BULK_CTRL_2 = 0x7A; 				
	}
	else
	{
		// Set Maximum packet size of Bulk endpoint to 64 bytes and flush fifo
		BULK_CTRL_1 = 0x0A; 
		BULK_CTRL_2 = 0x5A;		
	}*/		

	// Always set Maximum packet size to 64 bytes no matter high speed or full speed
	BULK_CTRL_1 = 0x0A;
	BULK_CTRL_2 = 0x5A;

	UartInit();
}

void MidiProcess()
{
	if(!RxQueueIsEmpty())
	{
		s_UartRxByte = GetFromRxQueue();
		ProcessMidiInByte();
	}

	if(g_BulkOutRequest)
	{
		g_BulkOutRequest = FALSE;

		if((BULK_CTRL_3&0x07) == 4)
			s_MidiOutMsgCnt = 1;
		else
			BULK_CTRL_1 |= bmBIT0;
	}

	if(s_MidiOutMsgCnt)
	{
		ProcessMidiOutByte();
	}
}

