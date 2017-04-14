#ifndef _CODEC_H_
#define _CODEC_H_

#include "usb.h"
#include "config.h"

//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------
#define MUTE_DSCR_NUM 5
#define BOOST_DSCR_NUM 1

#ifdef _MODEL_CM6632A_
#define VOLUME_DSCR_NUM 12
#else
#define VOLUME_DSCR_NUM 6
#endif

//-----------------------------------------------------------------------------
// Type Definition
//-----------------------------------------------------------------------------
typedef struct
{
	BYTE   id;
	BYTE   ch;
	WORD   min;
	WORD   max;
	WORD   res;
	BYTE   type;
} VOLUME_DSCR;

typedef struct
{
	BYTE id;
	BYTE ch;		
} BOOST_DSCR;	

//-----------------------------------------------------------------------------
// Public Variable
//-----------------------------------------------------------------------------
extern BOOL g_IsAudioClass20;

extern BYTE code g_MuteTable[MUTE_DSCR_NUM] ;
extern VOLUME_DSCR code g_VolumeTable[VOLUME_DSCR_NUM];
extern BOOST_DSCR code g_BoostTable[BOOST_DSCR_NUM];

extern BOOL g_SpdifLockStatus;
extern BYTE idata g_SpdifRateStatus;

extern USB_INTERFACE code g_Audio20InterfaceAudioCtrl;
extern USB_INTERFACE code g_Audio20InterfaceSpeaker;
extern USB_INTERFACE code g_Audio20InterfaceSpdifOut;
extern USB_INTERFACE code g_Audio20InterfaceHeadphone;
extern USB_INTERFACE code g_Audio20InterfaceLineIn;
extern USB_INTERFACE code g_Audio20InterfaceMicIn;
extern USB_INTERFACE code g_Audio20InterfaceSpdifIn;

extern USB_INTERFACE code g_Audio10InterfaceAudioCtrl;
extern USB_INTERFACE code g_Audio10InterfaceSpeaker;
extern USB_INTERFACE code g_Audio10InterfaceSpdifOut;
extern USB_INTERFACE code g_Audio10InterfaceHeadphone;
extern USB_INTERFACE code g_Audio10InterfaceLineIn;
extern USB_INTERFACE code g_Audio10InterfaceMicIn;
extern USB_INTERFACE code g_Audio10InterfaceSpdifIn;

//-----------------------------------------------------------------------------
// Public Function
//-----------------------------------------------------------------------------
extern void AudioInit();
extern void AudioStop();
extern void AudioProcess();

extern void SetRecordMute(BOOL mute);

extern void ControlByteToFreq(BYTE value);
extern BYTE FreqToControlByte();

extern BYTE GetDmaFreq(BYTE endpoint);
extern BOOL GetCurrentBoost(BYTE index);
extern BOOL GetCurrentMute(BYTE index);
extern WORD GetCurrentVol(BYTE index);

extern void SetDmaFreq(BYTE endpoint, BYTE freq);
extern void SetCurrentBoost(BYTE index, BOOL boost);
extern void SetCurrentMute(BYTE index, BOOL mute);
extern void SetCurrentVol(BYTE index, WORD vol);

extern BOOL PlayMultiChStart(BYTE ch, BYTE format);
extern BOOL PlaySpdifStart(BYTE format);
extern BOOL PlayTwoChStart(BYTE format);
extern BOOL RecMultiChStart(BYTE ch, BYTE format);
extern BOOL RecTwoChStart(BYTE format);
extern BOOL RecSpdifStart(BYTE format);

extern BOOL PlayMultiChStop();
extern BOOL PlaySpdifStop();
extern BOOL PlayTwoChStop();
extern BOOL RecMultiChStop();
extern BOOL RecTwoChStop();
extern BOOL RecSpdifStop();

#endif

