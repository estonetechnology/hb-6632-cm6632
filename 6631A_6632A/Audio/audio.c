#include "audio.h"
#include "timer.h"
#include "peripheral.h"
#include "usb.h"
//#include "hid.h"
#include "config.h"

//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------
#define I2C_ADDR_1792 0x98
#define I2C_ADDR_8741 0x34
#define I2C_ADDR_4382 0x30
#define I2C_ADDR_4398 0x9E
#define I2C_ADDR_5346 0x9A

#define SPI_CS0 0x3E
#define SPI_CS1 0x3D
#define SPI_CS2 0x3B
#define SPI_CS3 0x37

#define I2S_CODEC_16bit 0x40
#define I2S_CODEC_24bit 0x50

#define PLAYBACK_ROUTING_2CH 0xE4
#define PLAYBACK_ROUTING_4CH 0xD8
#define PLAYBACK_ROUTING_6CH 0xE4
#define PLAYBACK_ROUTING_8CH 0xE4

#define IS_PLAYING(index) (s_bPlaybackStartCount & (1 << index))

enum
{
	VOL_1792 = 0,
	VOL_8741,
	VOL_4382,
	VOL_4398,
	VOL_5346
};

enum	// DMA ID
{
	PLAY_8CH,
	PLAY_SPDIF,
	PLAY_2CH,
	REC_8CH,
	REC_2CH,
	REC_SPDIF
};

//-----------------------------------------------------------------------------
// Type Definition
//-----------------------------------------------------------------------------
typedef struct
{
	BYTE chip;
	BYTE map;
} CODEC_VOLUME_CONTROL; 

//-----------------------------------------------------------------------------
// Global Variables
//-----------------------------------------------------------------------------
BOOL g_IsAudioClass20;
int g_PlayMode = 0;

//-----------------------------------------------------------------------------
// Volume Variables
//-----------------------------------------------------------------------------

#ifdef _MODEL_CM6632A_

VOLUME_DSCR code g_VolumeTable[VOLUME_DSCR_NUM] = 
{
	{13,1,0x8100,0,0x0100,VOL_4382},{13,2,0x8100,0,0x0100,VOL_4382},  // -127db ~ 0db, step 1 db
	{13,3,0x8100,0,0x0100,VOL_4382},{13,4,0x8100,0,0x0100,VOL_4382}, 
	{13,5,0x8100,0,0x0100,VOL_4382},{13,6,0x8100,0,0x0100,VOL_4382},
	{13,7,0x8100,0,0x0100,VOL_4382},{13,8,0x8100,0,0x0100,VOL_4382},
	{15,1,0x8100,0,0x0080,VOL_4398},{15,2,0x8100,0,0x0080,VOL_4398},  // -127db ~ 0db, step 0.5 db
	{16,1,0xF400,0x0C00,0x0080,VOL_5346},{16,2,0xF400,0x0C00,0x0080,VOL_5346}  // -12db ~ 12db, step 0.5 db
};

static CODEC_VOLUME_CONTROL code s_CodecVolumeTable[VOLUME_DSCR_NUM] = 
{
 	{I2C_ADDR_4382, 0x07}, {I2C_ADDR_4382, 0x08}, {I2C_ADDR_4382, 0x0A}, {I2C_ADDR_4382, 0x0B}, 
 	{I2C_ADDR_4382, 0x0D}, {I2C_ADDR_4382, 0x0E}, {I2C_ADDR_4382, 0x10}, {I2C_ADDR_4382, 0x11},
 	{I2C_ADDR_4398, 0x05}, {I2C_ADDR_4398, 0x06},
 	{I2C_ADDR_5346, 0x08}, {I2C_ADDR_5346, 0x07}
};

static WORD idata s_CurrentVolume[VOLUME_DSCR_NUM] = 
{
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0,
	0, 0
};

#else

VOLUME_DSCR code g_VolumeTable[VOLUME_DSCR_NUM] = 
{
	{13,1,0x8800,0,0x0080,VOL_1792},{13,2,0x8800,0,0x0080,VOL_1792},  // -120db ~ 0db, step 0.5 db
	{15,1,0x8800,0,0x0080,VOL_8741},{15,2,0x8800,0,0x0080,VOL_8741},  // -120db ~ 0db, step 0.5 db
	{16,1,0xF400,0x0C00,0x0080,VOL_5346},{16,2,0xF400,0x0C00,0x0080,VOL_5346}  // -12db ~ 12db, step 0.5 db
};

static CODEC_VOLUME_CONTROL code s_CodecVolumeTable[VOLUME_DSCR_NUM] = 
{
 	{I2C_ADDR_1792, 0x10}, {I2C_ADDR_1792, 0x11},
 	{I2C_ADDR_8741, 0x00}, {I2C_ADDR_8741, 0x02},
 	{I2C_ADDR_5346, 0x08}, {I2C_ADDR_5346, 0x07}
};

static WORD idata s_CurrentVolume[VOLUME_DSCR_NUM] = 
{
	0, 0,
	0, 0,
	0, 0
};

#endif

//-----------------------------------------------------------------------------
// Mute Variables
//-----------------------------------------------------------------------------
BYTE code g_MuteTable[MUTE_DSCR_NUM] = {13, 14, 15, 16, 17};
static BYTE idata s_CurrentMute = 0;
static BOOL s_RecordMute = FALSE;

//-----------------------------------------------------------------------------
// Boost Variables
//-----------------------------------------------------------------------------
BOOST_DSCR code g_BoostTable[BOOST_DSCR_NUM] = {{16, 0}};
static BYTE idata s_CurrentBoost = 0x01;

//-----------------------------------------------------------------------------
// S/PDIF-in Variables
//-----------------------------------------------------------------------------
BOOL g_SpdifLockStatus = FALSE;
BYTE idata g_SpdifRateStatus = DMA_48000;

//-----------------------------------------------------------------------------
// Static Variables
//-----------------------------------------------------------------------------
static BYTE s_bPlaybackStartCount;

#define SAMPLING_RATE_NUM 10
static BYTE code s_SamplingRateTable[SAMPLING_RATE_NUM][5] =
{
	{0x00, 0x7D, 0x00, DMA_32000, 32},
	{0x00, 0xAC, 0x44, DMA_44100, 44},
	{0x00, 0xBB, 0x80, DMA_48000, 48},
	{0x00, 0xFA, 0x00, DMA_64000, 64},
	{0x01, 0x58, 0x88, DMA_88200, 88},
	{0x01, 0x77, 0x00, DMA_96000, 96},
	{0x02, 0xB1, 0x10, DMA_176400, 176},
	{0x02, 0xEE, 0x00, DMA_192000, 192},
	{0x00, 0x1F, 0x40, DMA_8000, 8},
	{0x00, 0x3E, 0x80, DMA_16000, 16}
};

#ifdef _MCU_FEEDBACK_

typedef struct
{
	WORD mid;
	BYTE end;
} FEEDBACK;

static FEEDBACK code s_FeedbackTable[SAMPLING_RATE_NUM] =
{
	{0x0400, 0x00},	// 32k
	{0x0583, 0x33},	// 44.1k
	{0x0600, 0x00},	// 48k
	{0x0800, 0x00},	// 64k
	{0x0B06, 0x66},	// 88.2k
	{0x0C00, 0x00},	// 96k
	{0x160C, 0xCD},	// 176.4k
	{0x1800, 0x00},	// 192k
	{0x0100, 0x00},	// 8k
	{0x0200, 0x00}	// 16k
};

static WORD s_MultiChCount, s_SpdifCount, s_TwoChCount;
static WORD s_MultiChCountOld, s_SpdifCountOld, s_TwoChCountOld;
static WORD idata s_MultiChThreshold, s_SpdifThreshold, s_TwoChThreshold;
static BYTE idata s_MultiChFeedbackRatio, s_SpdifFeedbackRatio, s_TwoChFeedbackRatio;
static BOOL s_MultiChFeedbackStart, s_SpdifFeedbackStart, s_TwoChFeedbackStart;

static BYTE code s_uFrameThresholdTable[80] = 
{
//	32	44	48	64	88	96	176	192	8	16
	192,	176,	192,	192,	176,	192,	176,	192,	192,	192,	// 2ch 16bit
	192,	176,	192,	192,	176,	192,	176,	192,	192,	192,	// 2ch 24/32bit

	192,	176,	192,	192,	176,	192,	176,	192,	192,	192,	// 4ch 16bit
	192,	176,	192,	192,	176,	192,	176,	192,	192,	192,	// 4ch 24/32bit

	144,	132,	144,	144,	132,	144,	132,	144,	144,	144,	// 6ch 16bit
	144,	132,	144,	144,	132,	144,	132,	144,	144,	144,	// 6ch 24/32bit

	192,	176,	192,	192,	176,	192,	176,	192,	192,	192,	// 8ch 16bit
	192,	176,	192,	192,	176,	192,	176,	192,	192,	192	// 8ch 24/32bit
};

static BYTE code s_mSecondThresholdTable[80] = 
{
//	32	44	48	64	88	96	176	192	8	16
	192,	176,	192,	192,	176,	192,	176,	192,	192,	192,	// 2ch 16bit
	192,	176,	192,	192,	176,	192,	176,	192,	192,	192,	// 2ch 24/32bit

	192,	176,	192,	192,	176,	192,	176,	192,	192,	192,	// 4ch 16bit
	192,	176,	192,	192,	176,	192,	176,	192,	192,	192,	// 4ch 24/32bit

	144,	132,	144,	144,	132,	144,	132,	144,	144,	144,	// 6ch 16bit
	144,	132,	144,	144,	132,	144,	132,	144,	144,	144,	// 6ch 24/32bit

	192,	176,	192,	192,	176,	192,	176,	192,	192,	192,	// 8ch 16bit
	192,	176,	192,	192,	176,	192,	176,	192,	192,	192	// 8ch 24/32bit
};

#elif defined(_FEEDBACK_)

static BYTE code s_uFrameThresholdTable[80] = 
{
//	32	44	48	64	88	96	176	192	8	16
	208,	208,	208,	208,	208,	208,	208,	208,	208,	208,	// 2ch 16bit
	208,	208,	208,	208,	208,	208,	208,	208,	208,	208,	// 2ch 24/32bit

	208,	208,	208,	208,	208,	208,	208,	208,	208,	208,	// 4ch 16bit
	208,	208,	208,	208,	208,	208,	208,	208,	208,	208,	// 4ch 24/32bit

	208,	208,	208,	208,	208,	208,	208,	208,	208,	208,	// 6ch 16bit
	208,	208,	208,	208,	208,	208,	208,	208,	208,	208,	// 6ch 24/32bit

	208,	208,	208,	208,	208,	208,	208,	208,	208,	208,	// 8ch 16bit
	208,	208,	208,	208,	208,	208,	208,	208,	208,	208	// 8ch 24/32bit
};

static BYTE code s_mSecondThresholdTable[80] = 
{
//	32	44	48	64	88	96	176	192	8	16
	208,	208,	208,	208,	208,	208,	208,	208,	208,	208,	// 2ch 16bit
	208,	208,	208,	208,	208,	208,	208,	208,	208,	208,	// 2ch 24/32bit

	208,	208,	208,	208,	208,	208,	208,	208,	208,	208,	// 4ch 16bit
	208,	208,	208,	208,	208,	208,	208,	208,	208,	208,	// 4ch 24/32bit

	208,	208,	208,	208,	208,	208,	208,	208,	208,	208,	// 6ch 16bit
	208,	208,	208,	208,	208,	208,	208,	208,	208,	208,	// 6ch 24/32bit

	208,	208,	208,	208,	208,	208,	208,	208,	208,	208,	// 8ch 16bit
	208,	208,	208,	208,	208,	208,	208,	208,	208,	208	// 8ch 24/32bit
};

#else

static BYTE code s_uFrameThresholdTable[80] = 
{
//	32	44	48	64	88	96	176	192	8	16
	16,	22,	24,	32,	44,	48,	88,	96,	0,	0,	// 2ch 16bit
	32,	44,	48,	64,	88,	96,	176,	192,	0,	0,	// 2ch 24/32bit

	32,	44,	48,	64,	88,	96,	176,	192,	0,	0,	// 4ch 16bit
	64,	88,	96,	128,	176,	192,	176,	192,	0,	0,	// 4ch 24/32bit

	48,	66,	72,	96,	132,	144,	132,	144,	0,	0,	// 6ch 16bit
	96,	132,	144,	96,	132,	144,	132,	144,	0,	0,	// 6ch 24/32bit

	64,	88,	96,	128,	176,	192,	176,	192,	0,	0,	// 8ch 16bit
	128,	176,	192,	128,	176,	192,	176,	192,	0,	0	// 8ch 24/32bit
};

static BYTE code s_mSecondThresholdTable[80] = 
{
//	32	44	48	64	88	96	176	192	8	16
	64,	88,	96,	128,	176,	192,	0,	0,	64,	64,	// 2ch 16bit
	128,	176,	192,	144,	192,	208,	0,	0,	128,	128,	// 2ch 24/32bit

	128,	176,	192,	144,	192,	208,	0,	0,	128,	128,	// 4ch 16bit
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	// 4ch 24/32bit

	192,	148,	160,	0,	0,	0,	0,	0,	192,	192,	// 6ch 16bit
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	// 6ch 24/32bit

	144,	192,	208,	0,	0,	0,	0,	0,	192,	192,	// 8ch 16bit
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0	// 8ch 24/32bit
};

#endif

#ifdef _I2C_SLAVE_SUPPORT_

static BYTE idata s_I2cSlaveMap = 0;
static BYTE idata g_I2cSlaveIntStatus = 0;	// bit0: playback audio format
static BYTE idata g_I2cSlaveIntMask = 0;	// bit0: playback audio format

#endif

//-----------------------------------------------------------------------------
// Code
//-----------------------------------------------------------------------------

#ifdef _EXT_CLK_SI5351_

static BOOL s_ChangeClock = FALSE;

#define CLK_GEN_ADDR 0xC0

#define Si5351ClkDisable 0x03
#define Si5351ClkReset 0xB1

#define Si5351Clk0Ctrl 0x10
#define Si5351Clk1Ctrl 0x11
#define Si5351Clk2Ctrl 0x12
#define Si5351Clk3Ctrl 0x13
#define Si5351Clk4Ctrl 0x14
#define Si5351Clk5Ctrl 0x15
#define Si5351Clk6Ctrl 0x16
#define Si5351Clk7Ctrl 0x17
#define Si5351DisableState 0x18

#define Si5351Clk0Freq 0x2A
#define Si5351Clk1Freq 0x32
#define Si5351Clk2Freq 0x3A
#define Si5351Clk3Freq 0x42
#define Si5351Clk4Freq 0x4A
#define Si5351Clk5Freq 0x52
#define Si5351Clk6Freq 0x5A
#define Si5351Clk7Freq 0x5B

static code BYTE s_ClkSetup1[8] = {0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80};
static code BYTE s_ClkSetup2[2] = {0xAA, 0xAA};

static code BYTE s_MSNA[8] = {0x0C, 0x35, 0x00, 0x0F, 0xB1, 0x00, 0x0A, 0x5B};	// 884.736M
static code BYTE s_MSNB[8] = {0x3D, 0x09, 0x00, 0x0E, 0x41, 0x00, 0x30, 0xB7};	// 812.8512M

static code BYTE s_CLK6_7[2] = {0x12, 0x12};	// 884.736M/18 = 49.152M, 812.8512M/18 = 45.1584M

static code BYTE s_CLK_6M[4] = {0x00, 0x01, 0x00, 0x46};	// BCLK 6.144M
static code BYTE s_CLK_12M[4] = {0x00, 0x01, 0x00, 0x22};	// MCLK 12.288M
static code BYTE s_CLK_24M[4] = {0x00, 0x01, 0x00, 0x10};	// MCLK 24.576M
static code BYTE s_CLK_48K[4] = {0x00, 0x01, 0x42, 0x3E};	// LRCK 48K
static code BYTE s_CLK_96K[4] = {0x00, 0x01, 0x32, 0x3E};	// LRCK 96K
static code BYTE s_CLK_192K[4] = {0x00, 0x01, 0x22, 0x3E};// LRCK 96K

static code BYTE s_SourcePLL_A = 0x4F;
static code BYTE s_SourcePLL_B = 0x6F;
static code BYTE s_EnableClk = 0xD9;
static code BYTE s_DisableClk = 0xFF;
static code BYTE s_ResetClock = 0xAC;

void SetMultiChClock(BYTE freq)
{
	//I2cMasterRead(CLK_GEN_ADDR, Si5351ClkDisable, &g_TempByte1, 1|I2C_FAST_MODE);
	//g_TempByte1 |= (bmBIT1|bmBIT2|bmBIT5);
	//I2cMasterWrite(CLK_GEN_ADDR, Si5351ClkDisable, &g_TempByte1, 1|I2C_FAST_MODE);

	switch(freq)
	{
		case DMA_44100:
			I2cMasterWrite(CLK_GEN_ADDR, Si5351Clk1Ctrl, &s_SourcePLL_B, 1|I2C_FAST_MODE);
			I2cMasterWrite(CLK_GEN_ADDR, Si5351Clk2Ctrl, &s_SourcePLL_B, 1|I2C_FAST_MODE);
			I2cMasterWrite(CLK_GEN_ADDR, Si5351Clk5Ctrl, &s_SourcePLL_B, 1|I2C_FAST_MODE);
			I2cMasterWrite(CLK_GEN_ADDR, Si5351Clk1Freq, &s_CLK_12M, 4|I2C_FAST_MODE);
			I2cMasterWrite(CLK_GEN_ADDR, Si5351Clk2Freq, &s_CLK_48K, 4|I2C_FAST_MODE);
			I2cMasterWrite(CLK_GEN_ADDR, Si5351Clk5Freq, &s_CLK_6M, 4|I2C_FAST_MODE);
			break;

		case DMA_48000:
			I2cMasterWrite(CLK_GEN_ADDR, Si5351Clk1Ctrl, &s_SourcePLL_A, 1|I2C_FAST_MODE);
			I2cMasterWrite(CLK_GEN_ADDR, Si5351Clk2Ctrl, &s_SourcePLL_A, 1|I2C_FAST_MODE);
			I2cMasterWrite(CLK_GEN_ADDR, Si5351Clk5Ctrl, &s_SourcePLL_A, 1|I2C_FAST_MODE);
			I2cMasterWrite(CLK_GEN_ADDR, Si5351Clk1Freq, &s_CLK_12M, 4|I2C_FAST_MODE);
			I2cMasterWrite(CLK_GEN_ADDR, Si5351Clk2Freq, &s_CLK_48K, 4|I2C_FAST_MODE);
			I2cMasterWrite(CLK_GEN_ADDR, Si5351Clk5Freq, &s_CLK_6M, 4|I2C_FAST_MODE);
			break;

		case DMA_88200:
			I2cMasterWrite(CLK_GEN_ADDR, Si5351Clk1Ctrl, &s_SourcePLL_B, 1|I2C_FAST_MODE);
			I2cMasterWrite(CLK_GEN_ADDR, Si5351Clk2Ctrl, &s_SourcePLL_B, 1|I2C_FAST_MODE);
			I2cMasterWrite(CLK_GEN_ADDR, Si5351Clk5Ctrl, &s_SourcePLL_B, 1|I2C_FAST_MODE);
			I2cMasterWrite(CLK_GEN_ADDR, Si5351Clk1Freq, &s_CLK_24M, 4|I2C_FAST_MODE);
			I2cMasterWrite(CLK_GEN_ADDR, Si5351Clk2Freq, &s_CLK_96K, 4|I2C_FAST_MODE);
			I2cMasterWrite(CLK_GEN_ADDR, Si5351Clk5Freq, &s_CLK_6M, 4|I2C_FAST_MODE);
			break;

		case DMA_96000:
			I2cMasterWrite(CLK_GEN_ADDR, Si5351Clk1Ctrl, &s_SourcePLL_A, 1|I2C_FAST_MODE);
			I2cMasterWrite(CLK_GEN_ADDR, Si5351Clk2Ctrl, &s_SourcePLL_A, 1|I2C_FAST_MODE);
			I2cMasterWrite(CLK_GEN_ADDR, Si5351Clk5Ctrl, &s_SourcePLL_A, 1|I2C_FAST_MODE);
			I2cMasterWrite(CLK_GEN_ADDR, Si5351Clk1Freq, &s_CLK_24M, 4|I2C_FAST_MODE);
			I2cMasterWrite(CLK_GEN_ADDR, Si5351Clk2Freq, &s_CLK_96K, 4|I2C_FAST_MODE);
			I2cMasterWrite(CLK_GEN_ADDR, Si5351Clk5Freq, &s_CLK_6M, 4|I2C_FAST_MODE);
			break;

		case DMA_176400:
			I2cMasterWrite(CLK_GEN_ADDR, Si5351Clk1Ctrl, &s_SourcePLL_B, 1|I2C_FAST_MODE);
			I2cMasterWrite(CLK_GEN_ADDR, Si5351Clk2Ctrl, &s_SourcePLL_B, 1|I2C_FAST_MODE);
			I2cMasterWrite(CLK_GEN_ADDR, Si5351Clk5Ctrl, &s_SourcePLL_B, 1|I2C_FAST_MODE);
			I2cMasterWrite(CLK_GEN_ADDR, Si5351Clk1Freq, &s_CLK_24M, 4|I2C_FAST_MODE);
			I2cMasterWrite(CLK_GEN_ADDR, Si5351Clk2Freq, &s_CLK_192K, 4|I2C_FAST_MODE);
			I2cMasterWrite(CLK_GEN_ADDR, Si5351Clk5Freq, &s_CLK_12M, 4|I2C_FAST_MODE);
			break;

		case DMA_192000:
			I2cMasterWrite(CLK_GEN_ADDR, Si5351Clk1Ctrl, &s_SourcePLL_A, 1|I2C_FAST_MODE);
			I2cMasterWrite(CLK_GEN_ADDR, Si5351Clk2Ctrl, &s_SourcePLL_A, 1|I2C_FAST_MODE);
			I2cMasterWrite(CLK_GEN_ADDR, Si5351Clk5Ctrl, &s_SourcePLL_A, 1|I2C_FAST_MODE);
			I2cMasterWrite(CLK_GEN_ADDR, Si5351Clk1Freq, &s_CLK_24M, 4|I2C_FAST_MODE);
			I2cMasterWrite(CLK_GEN_ADDR, Si5351Clk2Freq, &s_CLK_192K, 4|I2C_FAST_MODE);
			I2cMasterWrite(CLK_GEN_ADDR, Si5351Clk5Freq, &s_CLK_12M, 4|I2C_FAST_MODE);
	}

	I2cMasterWrite(CLK_GEN_ADDR, Si5351ClkReset, &s_ResetClock, 1|I2C_FAST_MODE);

	//g_TempByte1 &= ~(bmBIT1|bmBIT2|bmBIT5);
	//I2cMasterWrite(CLK_GEN_ADDR, Si5351ClkDisable, &g_TempByte1, 1|I2C_FAST_MODE);
}

void ClockGenInit()
{
	I2cMasterWrite(CLK_GEN_ADDR, Si5351Clk0Ctrl, &s_ClkSetup1, 8|I2C_FAST_MODE);
	I2cMasterWrite(CLK_GEN_ADDR, Si5351DisableState, &s_ClkSetup2, 2|I2C_FAST_MODE);
	SetMultiChClock(GetDmaFreq(EP_MULTI_CH_PLAY));
	I2cMasterWrite(CLK_GEN_ADDR, Si5351ClkDisable, &s_EnableClk, 1|I2C_FAST_MODE);
}

void ClockGenDisable()
{
	I2cMasterWrite(CLK_GEN_ADDR, Si5351ClkDisable, &s_DisableClk, 1|I2C_FAST_MODE);
}

#endif

void ControlByteToFreq(BYTE value)
{
	BYTE index;

	for(index = 0; index<SAMPLING_RATE_NUM; ++index)	
	{	
		if(value == s_SamplingRateTable[index][3])
			break;
	}
    
	if(SAMPLING_RATE_NUM == index)
	{
		index = 2; // If not find, set 48K.
	}

	g_UsbCtrlData[0] = s_SamplingRateTable[index][2];
	g_UsbCtrlData[1] = s_SamplingRateTable[index][1];
	g_UsbCtrlData[2] = s_SamplingRateTable[index][0];
	g_UsbCtrlData[3] = 0;
}

BYTE FreqToControlByte()
{
	BYTE index;

	for(index = 0; index<SAMPLING_RATE_NUM; ++index)
	{
		if((g_UsbCtrlData[0]==s_SamplingRateTable[index][2]) && 
				(g_UsbCtrlData[1]==s_SamplingRateTable[index][1]) &&
		   			(g_UsbCtrlData[2]==s_SamplingRateTable[index][0]))
			break;
	}

	if(SAMPLING_RATE_NUM == index)
	{
		index = 2; // If not find, set 48K.
	}

	return s_SamplingRateTable[index][3];
}

BYTE GetDmaFreq(BYTE endpoint)
{
	switch(endpoint)
	{
		case EP_MULTI_CH_PLAY:
			return PERI_ReadByte(DMA_PLAY_8CH_L) & FREQ_MASK;

		case EP_SPDIF_PLAY:
			return PERI_ReadByte(DMA_PLAY_SPDIF) & FREQ_MASK;

		case EP_TWO_CH_PLAY:
			return PERI_ReadByte(DMA_PLAY_2CH) & FREQ_MASK;

		case EP_MULTI_CH_REC:
			return PERI_ReadByte(DMA_REC_8CH_L) & FREQ_MASK;

		case EP_TWO_CH_REC:
			return PERI_ReadByte(DMA_REC_2CH) & FREQ_MASK;

		case EP_SPDIF_REC:
			return PERI_ReadByte(DMA_REC_SPDIF) & FREQ_MASK;

		default:
			return DMA_48000;
	}
}

void SetDmaFreq(BYTE endpoint, BYTE freq)
{
	switch(endpoint)
	{
		case EP_MULTI_CH_PLAY:
			if(freq != GetDmaFreq(EP_MULTI_CH_PLAY))	// Frequency changes
			{
#ifdef _I2C_SLAVE_SUPPORT_
				g_I2cSlaveIntStatus |= bmBIT0;  

 				if(!(g_I2cSlaveIntMask & bmBIT0))
					PERI_WriteByte(GPIO_DATA_L, PERI_ReadByte(GPIO_DATA_L) | bmBIT7);
#endif          

#ifdef _EXT_CLK_SI5351_
				s_ChangeClock = TRUE;
#endif          
			}

			PERI_WriteByte(DMA_PLAY_8CH_L, (PERI_ReadByte(DMA_PLAY_8CH_L)&(~FREQ_MASK))|freq);
			break;

		case EP_SPDIF_PLAY:
			PERI_WriteByte(DMA_PLAY_SPDIF, (PERI_ReadByte(DMA_PLAY_SPDIF)&(~FREQ_MASK))|freq);
			break;

		case EP_TWO_CH_PLAY:
			PERI_WriteByte(DMA_PLAY_2CH, (PERI_ReadByte(DMA_PLAY_2CH)&(~FREQ_MASK))|freq);
			break;

		case EP_MULTI_CH_REC:
			PERI_WriteByte(DMA_REC_8CH_L, (PERI_ReadByte(DMA_REC_8CH_L)&(~FREQ_MASK))|freq);
			break;

		case EP_TWO_CH_REC:
			PERI_WriteByte(DMA_REC_2CH, (PERI_ReadByte(DMA_REC_2CH)&(~FREQ_MASK))|freq);
			break;

		case EP_SPDIF_REC:
			PERI_WriteByte(DMA_REC_SPDIF, (PERI_ReadByte(DMA_REC_SPDIF)&(~FREQ_MASK))|freq);
	}
}

static BYTE GetSamplingRate(BYTE freq)
{
	for(g_Index = 0; g_Index<SAMPLING_RATE_NUM; ++g_Index)
	{
		if(freq == s_SamplingRateTable[g_Index][3])
			break;
	}

	return s_SamplingRateTable[g_Index][4];
}

static void PlaybackResetRef()
{
	s_bPlaybackStartCount = 0;
	PERI_WriteByte(GPIO_DATA_H, PERI_ReadByte(GPIO_DATA_H) | bmBIT5);
}	

static void PlaybackAddRef(BYTE index)
{
	s_bPlaybackStartCount |= (bmBIT0 << index);
}	

static void PlaybackReleaseRef(BYTE index)
{
	s_bPlaybackStartCount &= ~(bmBIT0 << index);
	
	if(s_bPlaybackStartCount == 0)
	{
		PERI_WriteByte(GPIO_DATA_H, PERI_ReadByte(GPIO_DATA_H) | bmBIT5);
	}
}	

static BYTE GetSpdifInSamplingRate()
{
	switch(PERI_ReadByte(SPDIF_IN_STATUS_3) & 0x0F)
	{
		case SPDIF_STATUS_44100:
			return DMA_44100;

		case SPDIF_STATUS_32000:
			return DMA_32000;

		case SPDIF_STATUS_88200:
			return DMA_88200;

		case SPDIF_STATUS_96000:
			return DMA_96000;

		case SPDIF_STATUS_64000:
			return DMA_64000;

		case SPDIF_STATUS_176400:
			return DMA_176400;

		case SPDIF_STATUS_192000:
			return DMA_192000;

		case SPDIF_STATUS_48000:
		default:
			return DMA_48000;
	}	
}

static void HandleSpdifIn()
{
	static BYTE code s_ClockRateChangeNotify[6] = {0, 1, 0, 1, 0, 23};
	static BYTE code s_ClockValidChangeNotify[6] = {0, 1, 0, 2, 0, 23};

	g_TempByte1 = PERI_ReadByte(SPDIF_CTRL_1);

	if(g_TempByte1 & bmBIT3)	// Sense
	{
		if(g_TempByte1 & bmBIT4)	// Lock
		{
			if(!g_SpdifLockStatus)
			{
				g_SpdifLockStatus = TRUE;
				SendInt15Data(s_ClockValidChangeNotify, 6);
			}
			else if(g_SpdifRateStatus != GetSpdifInSamplingRate())
			{
				g_SpdifRateStatus = GetSpdifInSamplingRate();
				SendInt15Data(s_ClockRateChangeNotify, 6);
			}
		}
		else		// No Lock
		{
			if(g_SpdifLockStatus)
			{
				g_SpdifLockStatus = FALSE;
				SendInt15Data(s_ClockValidChangeNotify, 6);
			}

			if((PERI_ReadByte(SPDIF_CTRL_2) & bmBIT0) == bmBIT0)
				PERI_WriteByte(SPDIF_CTRL_2, 0);
			else
				PERI_WriteByte(SPDIF_CTRL_2, bmBIT0);							
		}
	}
	else
	{
		if(g_SpdifLockStatus)
		{
			g_SpdifLockStatus = FALSE;
			SendInt15Data(s_ClockValidChangeNotify, 6);
		}
	}

	if(g_SpdifLockStatus && (GetDmaFreq(EP_SPDIF_REC)==g_SpdifRateStatus))
	{
		PERI_WriteByte(RECORD_ROUTING_L, PERI_ReadByte(RECORD_ROUTING_L) & ~bmBIT3);
	}
	else
	{
		PERI_WriteByte(RECORD_ROUTING_L, PERI_ReadByte(RECORD_ROUTING_L) | bmBIT3);
	}
}

static void CodecReset()
{
	PERI_WriteByte(MISC_FUNCTION_CTRL, PERI_ReadByte(MISC_FUNCTION_CTRL) | bmBIT2); //set high reset codec

#ifdef _EXT_CLK_SI5351_
	PERI_WriteWord(I2S_PLAY_8CH_L, MCLK_TRI_STATE|BCLK_LRCK_128|I2S_SLAVE|MCLK_LRCK_256|I2S_MODE|I2S_48000);
	ClockGenInit();
#endif
}

static void CodecInit()
{
#ifdef _MODEL_CM6632A_

	g_TempByte1 = 0xC0;
	I2cMasterWrite(I2C_ADDR_4398, 0x08, &g_TempByte1, 1|I2C_FAST_MODE);	//set to Control Port Mode
	g_TempByte1 = 0x40;
	I2cMasterWrite(I2C_ADDR_4398, 0x08, &g_TempByte1, 1|I2C_FAST_MODE);	//set to Power on

	I2cMasterRead(I2C_ADDR_4398, 0x08, &g_TempByte1, 1|I2C_FAST_MODE);
	if(g_TempByte1 == 0x40)
		PERI_WriteByte(GPIO_DATA_H, PERI_ReadByte(GPIO_DATA_H) | bmBIT2);  // For debug 

	g_TempByte1 = 0x81;
	I2cMasterWrite(I2C_ADDR_4382, 1, &g_TempByte1, 1|I2C_FAST_MODE);
	g_TempByte1 = 0x94;
	I2cMasterWrite(I2C_ADDR_4382, 3, &g_TempByte1, 1|I2C_FAST_MODE);
	g_TempByte1 = 0x01;
	I2cMasterWrite(I2C_ADDR_4382, 4, &g_TempByte1, 1|I2C_FAST_MODE);
	g_TempByte1 = 0x10;
	I2cMasterWrite(I2C_ADDR_4382, 2, &g_TempByte1, 1|I2C_FAST_MODE);	//set I2S mode
	g_TempByte1 = 0x80;
	I2cMasterWrite(I2C_ADDR_4382, 1, &g_TempByte1, 1|I2C_FAST_MODE);

#else

	// Delay about 0.45 ms for PCM1792 stable after MCLK startup
	for(g_TempWord1=0; g_TempWord1<300; g_TempWord1++);

	g_TempByte1 = 0x80;
	I2cMasterWrite(I2C_ADDR_1792, 0x12, &g_TempByte1, 1|I2C_FAST_MODE);

	I2cMasterRead(I2C_ADDR_1792, 0x12, &g_TempByte1, 1|I2C_FAST_MODE);
	if(g_TempByte1 == 0x80)
		PERI_WriteByte(GPIO_DATA_H, PERI_ReadByte(GPIO_DATA_H) | bmBIT4);  // For debug 

	g_TempByte1= bmBIT0;// Enable volume ramp
	I2cMasterWrite(I2C_ADDR_8741, 0x04<<1, &g_TempByte1, 1|I2C_FAST_MODE);

#endif

	g_TempByte1 = 0;	// Power on CS-5346
	I2cMasterWrite(I2C_ADDR_5346, 2, &g_TempByte1, 1|I2C_FAST_MODE);
}

void AudioInit()
{
	CodecReset();

	PERI_WriteByte(I2S_PLAY_SPDIF_H, bmBIT4);	// Mute unused I2S MCLK
#ifdef _MODEL_CM6601A_
	PERI_WriteByte(I2S_PLAY_2CH_H, bmBIT4);	// Mute unused I2S MCLK
	PERI_WriteByte(I2S_REC_2CH_H, bmBIT4);	// Mute unused I2S MCLK
#endif

	PERI_WriteByte(DMA_FIFO_FLUSH, bmBIT6);	// Flush FIFO automatically when FIFO is full

	PERI_WriteByte(PLAYBACK_ROUTING_L, MULTI_CH_PLAY_ROUTE_I2S|TWO_CH_PLAY_ROUTE_I2S|SPDIF_PLAY_ROUTE_SPDIF); 
	PERI_WriteByte(RECORD_ROUTING_L, MULTI_CH_REC_ROUTE_I2S|TWO_CH_REC_ROUTE_I2S|SPDIF_REC_ROUTE_SPDIF);

#ifdef _EXT_OSC_45_49_
	PERI_WriteByte(CLOCK_ROUTE, 0xCD); // Enable XD6, XD7 external ocsilator, discard packet if CRC error
	PERI_WriteByte(OSC_CTRL, 0x03); // OSC is 49.152/45.1584 MHz
	PERI_WriteByte(PLL_CTRL, 0x26);	// Turn off PLL-A, PLL-B, PLL-3. Turn on PLL-1, PLL-2
#elif defined(_EXT_OSC_22_24_)
	PERI_WriteByte(CLOCK_ROUTE, 0xCD); // Enable XD6, XD7 external ocsilator, discard packet if CRC error
	PERI_WriteByte(OSC_CTRL, 0x02); // OSC is 24.576/22.5792 MHz
	PERI_WriteByte(PLL_CTRL, 0x26);	// Turn off PLL-A, PLL-B, PLL-3. Turn on PLL-1, PLL-2
#else
	PERI_WriteByte(CLOCK_ROUTE, 0xC1); // Discard packet if CRC error
#endif

#ifdef _FEEDBACK_
	if(g_IsAudioClass20)
		PERI_WriteByte(FEEDBACK_CTRL_L, 0x00);
	else
		PERI_WriteByte(FEEDBACK_CTRL_L, 0xC0);

	PERI_WriteWord(FEEDBACK_CTRL2_L, 0x0249);	// Feedback 1/32 sample, period 32ms

	PERI_WriteByte(PLL_ADJUST_PERIOD, 0x00);

#ifdef _MCU_FEEDBACK_
	s_MultiChFeedbackStart = FALSE;
	s_SpdifFeedbackStart = FALSE;
	s_TwoChFeedbackStart = FALSE;

	PERI_WriteByte(MISC_FUNCTION_CTRL, PERI_ReadByte(MISC_FUNCTION_CTRL) | bmBIT6); //set playback sync. to SOF
	USB_HANDSHAKE3 = 0x1D;	// Enable 32ms SOF interrupt and MCU feedback control
#endif

#else
	if(g_IsAudioClass20)
		PERI_WriteByte(FEEDBACK_CTRL_L, 0x3F);
	else
		PERI_WriteByte(FEEDBACK_CTRL_L, 0xBF);

	PERI_WriteByte(PLL_ADJUST_PERIOD, 32);	// PLL adjust period 32ms
	PERI_WriteByte(MISC_FUNCTION_CTRL, PERI_ReadByte(MISC_FUNCTION_CTRL) | bmBIT6); // Play sync. to SOF
	PERI_WriteByte(PLL_THRES_LIMIT_H, 0x07);	// Disable threshold limitation
#endif

	PlaybackResetRef();
	CodecInit();

	//
	// Configure initial audio parameters
	//
	if(s_RecordMute)
		SetRecordMute(s_RecordMute);

	for(g_Index=0; g_Index<MUTE_DSCR_NUM; ++g_Index)
		SetCurrentMute(g_Index, (s_CurrentMute>>g_Index)&bmBIT0);

	for(g_Index=0; g_Index<BOOST_DSCR_NUM; ++g_Index)
		SetCurrentBoost(g_Index, (s_CurrentBoost>>g_Index)&bmBIT0);

	for(g_Index=0; g_Index<VOLUME_DSCR_NUM; ++g_Index)
		SetCurrentVol(g_Index, s_CurrentVolume[g_Index]);
}

#ifdef _MCU_FEEDBACK_

void HandleIsoFeedback()
{
	if(USB_HANDSHAKE1 & bmBIT4)
	{
		USB_HANDSHAKE2 = bmBIT4;	// Clear interrupt status

		s_MultiChCount = PERI_ReadWord(MULTI_CH_FIFO_REMAIN_L);
		s_SpdifCount = PERI_ReadWord(SPDIF_FIFO_REMAIN_L);
		s_TwoChCount = PERI_ReadWord(TWO_CH_FIFO_REMAIN_L);

		if((!s_MultiChFeedbackStart) && IS_PLAYING(PLAY_8CH))
		{
			g_TempWord1 = (MULTI_CH_PLAYBACK_COUNT[1] << 8) | MULTI_CH_PLAYBACK_COUNT[0];

			if(s_MultiChCountOld != g_TempWord1)
			{
				s_MultiChFeedbackStart = TRUE;
				s_MultiChCountOld = s_MultiChCount;

				if((!g_IsAudioClass20) && g_UsbIsHighSpeed)
				{
					s_MultiChThreshold = s_MultiChCount + 64;
				}
			}
		}

		if((!s_SpdifFeedbackStart) && IS_PLAYING(PLAY_SPDIF))
		{
			g_TempWord1 = (SPDIF_PLAYBACK_COUNT[1] << 8) | SPDIF_PLAYBACK_COUNT[0];

			if(s_SpdifCountOld != g_TempWord1)
			{
				s_SpdifFeedbackStart = TRUE;
				s_SpdifCountOld = s_SpdifCount;

				if((!g_IsAudioClass20) && g_UsbIsHighSpeed)
				{
					s_SpdifThreshold = s_SpdifCount + 64;
				}
			}
		}

		if((!s_TwoChFeedbackStart) && IS_PLAYING(PLAY_2CH))
		{
			g_TempWord1 = (TWO_CH_PLAYBACK_COUNT[1] << 8) | TWO_CH_PLAYBACK_COUNT[0];

			if(s_TwoChCountOld != g_TempWord1)
			{
				s_TwoChFeedbackStart = TRUE;
				s_TwoChCountOld = s_TwoChCount;

				if((!g_IsAudioClass20) && g_UsbIsHighSpeed)
				{
					s_TwoChThreshold = s_TwoChCount + 64;
				}
			}
		}

		if(s_MultiChFeedbackStart)
		{
			//g_InputReport.reserved1 = MSB(s_MultiChCount);
			//g_InputReport.reserved2 = LSB(s_MultiChCount);
			//g_InputReport.offset_high = MSB(s_MultiChThreshold);
			//g_InputReport.offset_low = LSB(s_MultiChThreshold);

			g_Index = PERI_ReadByte(DMA_PLAY_8CH_L) >> 3;

			if(s_MultiChCount > s_MultiChThreshold)
			{
				g_TempWord1 = s_MultiChCount - s_MultiChThreshold;

				if(s_MultiChCount < s_MultiChCountOld)
					g_TempWord1 >>= 5;

				s_MultiChCountOld = s_MultiChCount;

				if(s_MultiChFeedbackRatio)
				{
					g_TempWord1 /= s_MultiChFeedbackRatio;
				}

				if(g_TempWord1)
				{
					if(g_TempWord1 > 2)
						g_TempWord1 = 2;

					//g_InputReport.source = LSB(g_TempWord1) | bmBIT7;

					g_TempWord1 = s_FeedbackTable[g_Index].mid - g_TempWord1;
				}
			}
			else
			{
				g_TempWord1 = s_MultiChThreshold - s_MultiChCount;

				if(s_MultiChCount > s_MultiChCountOld)
					g_TempWord1 >>= 5;

				s_MultiChCountOld = s_MultiChCount;

				if(s_MultiChFeedbackRatio)
				{
					g_TempWord1 /= s_MultiChFeedbackRatio;
				}

				if(g_TempWord1)
				{
					if(g_TempWord1 > 2)
						g_TempWord1 = 2;

					//g_InputReport.source = LSB(g_TempWord1);

					g_TempWord1 = s_FeedbackTable[g_Index].mid + g_TempWord1;
				}
			}

			if(g_TempWord1)
			{
				if(g_IsAudioClass20 && g_UsbIsHighSpeed)
				{
					MULTI_CH_FEEDBACK_DATA[0] = s_FeedbackTable[g_Index].end;
					MULTI_CH_FEEDBACK_DATA[1] = LSB(g_TempWord1);
					MULTI_CH_FEEDBACK_DATA[2] = MSB(g_TempWord1);
				}
				else
				{
					MULTI_CH_FEEDBACK_DATA[0] = (s_FeedbackTable[g_Index].end << 3);
					g_TempWord1 = ((g_TempWord1<<3) | ((s_FeedbackTable[g_Index].end & 0xE0) >> 5));
					MULTI_CH_FEEDBACK_DATA[1] = LSB(g_TempWord1);
					MULTI_CH_FEEDBACK_DATA[2] = MSB(g_TempWord1);
				}

				MULTI_CH_FEEDBACK_DATA[3] = 0;
				USB_HANDSHAKE4 = bmBIT0;
			}
			else
			{
				USB_HANDSHAKE4 = bmBIT3;	// Reset feedback data
			}

			//SendInt4Data((BYTE*)(&g_InputReport), INPUT_REPORT_SIZE);
		}

		if(s_SpdifFeedbackStart)
		{
			//g_InputReport.reserved1 = MSB(s_SpdifCount);
			//g_InputReport.reserved2 = LSB(s_SpdifCount);
			//g_InputReport.offset_high = MSB(s_SpdifThreshold);
			//g_InputReport.offset_low = LSB(s_SpdifThreshold);

			g_Index = PERI_ReadByte(DMA_PLAY_SPDIF) >> 3;

			if(s_SpdifCount > s_SpdifThreshold)
			{
				g_TempWord1 = s_SpdifCount - s_SpdifThreshold;

				if(s_SpdifCount < s_SpdifCountOld)
					g_TempWord1 >>= 5;

				s_SpdifCountOld = s_SpdifCount;

				if(s_SpdifFeedbackRatio)
				{
					g_TempWord1 /= s_SpdifFeedbackRatio;
				}

				if(g_TempWord1)
				{
					if(g_TempWord1 > 2)
						g_TempWord1 = 2;

					//g_InputReport.source = LSB(g_TempWord1) | bmBIT7;

					g_TempWord1 = s_FeedbackTable[g_Index].mid - g_TempWord1;
				}
			}
			else
			{
				g_TempWord1 = s_SpdifThreshold - s_SpdifCount;

				if(s_SpdifCount > s_SpdifCountOld)
					g_TempWord1 >>= 5;

				s_SpdifCountOld = s_SpdifCount;

				if(s_SpdifFeedbackRatio)
				{
					g_TempWord1 /= s_SpdifFeedbackRatio;
				}

				if(g_TempWord1)
				{
					if(g_TempWord1 > 2)
						g_TempWord1 = 2;

					//g_InputReport.source = LSB(g_TempWord1);

					g_TempWord1 = s_FeedbackTable[g_Index].mid + g_TempWord1;
				}
			}

			if(g_TempWord1)
			{
				if(g_IsAudioClass20 && g_UsbIsHighSpeed)
				{
					SPDIF_FEEDBACK_DATA[0] = s_FeedbackTable[g_Index].end;
					SPDIF_FEEDBACK_DATA[1] = LSB(g_TempWord1);
					SPDIF_FEEDBACK_DATA[2] = MSB(g_TempWord1);
				}
				else
				{
					SPDIF_FEEDBACK_DATA[0] = (s_FeedbackTable[g_Index].end << 3);
					g_TempWord1 = ((g_TempWord1<<3) | ((s_FeedbackTable[g_Index].end & 0xE0) >> 5));
					SPDIF_FEEDBACK_DATA[1] = LSB(g_TempWord1);
					SPDIF_FEEDBACK_DATA[2] = MSB(g_TempWord1);
				}

				SPDIF_FEEDBACK_DATA[3] = 0;
				USB_HANDSHAKE4 = bmBIT1;
			}
			else
			{
				USB_HANDSHAKE4 = bmBIT4;	// Reset feedback data
			}

			//SendInt4Data((BYTE*)(&g_InputReport), INPUT_REPORT_SIZE);
		}

		if(s_TwoChFeedbackStart)
		{
			//g_InputReport.reserved1 = MSB(s_TwoChCount);
			//g_InputReport.reserved2 = LSB(s_TwoChCount);
			//g_InputReport.offset_high = MSB(s_TwoChThreshold);
			//g_InputReport.offset_low = LSB(s_TwoChThreshold);

			g_Index = PERI_ReadByte(DMA_PLAY_2CH) >> 3;

			if(s_TwoChCount > s_TwoChThreshold)
			{
				g_TempWord1 = s_TwoChCount - s_TwoChThreshold;

				if(s_TwoChCount < s_TwoChCountOld)
					g_TempWord1 >>= 5;

				s_TwoChCountOld = s_TwoChCount;

				if(s_TwoChFeedbackRatio)
				{
					g_TempWord1 /= s_TwoChFeedbackRatio;
				}

				if(g_TempWord1)
				{
					if(g_TempWord1 > 2)
						g_TempWord1 = 2;

					//g_InputReport.source = LSB(g_TempWord1) | bmBIT7;

					g_TempWord1 = s_FeedbackTable[g_Index].mid - g_TempWord1;
				}
			}
			else
			{
				g_TempWord1 = s_TwoChThreshold - s_TwoChCount;

				if(s_TwoChCount > s_TwoChCountOld)
					g_TempWord1 >>= 5;

				s_TwoChCountOld = s_TwoChCount;

				if(s_TwoChFeedbackRatio)
				{
					g_TempWord1 /= s_TwoChFeedbackRatio;
				}

				if(g_TempWord1)
				{
					if(g_TempWord1 > 2)
						g_TempWord1 = 2;

					//g_InputReport.source = LSB(g_TempWord1);

					g_TempWord1 = s_FeedbackTable[g_Index].mid + g_TempWord1;
				}
			}

			if(g_TempWord1)
			{
				if(g_IsAudioClass20 && g_UsbIsHighSpeed)
				{
					TWO_CH_FEEDBACK_DATA[0] = s_FeedbackTable[g_Index].end;
					TWO_CH_FEEDBACK_DATA[1] = LSB(g_TempWord1);
					TWO_CH_FEEDBACK_DATA[2] = MSB(g_TempWord1);
				}
				else
				{
					TWO_CH_FEEDBACK_DATA[0] = (s_FeedbackTable[g_Index].end << 3);
					g_TempWord1 = ((g_TempWord1<<3) | ((s_FeedbackTable[g_Index].end & 0xE0) >> 5));
					TWO_CH_FEEDBACK_DATA[1] = LSB(g_TempWord1);
					TWO_CH_FEEDBACK_DATA[2] = MSB(g_TempWord1);
				}

				TWO_CH_FEEDBACK_DATA[3] = 0;
				USB_HANDSHAKE4 = bmBIT2;
			}
			else
			{
				USB_HANDSHAKE4 = bmBIT5;	// Reset feedback data
			}

			//SendInt4Data((BYTE*)(&g_InputReport), INPUT_REPORT_SIZE);
		}
	}
}

#endif

#ifdef _I2C_SLAVE_SUPPORT_

void HandleI2cSlave()
{
	g_TempByte1 = PERI_ReadByte(I2C_SLAVE_STATUS);

	if(g_TempByte1 & bmBIT3)	// Write transaction
	{
		s_I2cSlaveMap = PERI_ReadByte(I2C_SLAVE_MAP);
		g_TempByte1 = (g_TempByte1 >> 1) & 0x03;

		if(g_TempByte1 == 2)      // MAP + 2 bytes
		{
			if(s_I2cSlaveMap == 0xFD)	//set interrupt mask 
			{           
				g_I2cSlaveIntMask = PERI_ReadByte(I2C_SLAVE_DATA_0);                
			}
			else
			{
				PERI_WriteByte(s_I2cSlaveMap++, PERI_ReadByte(I2C_SLAVE_DATA_0));
				PERI_WriteByte(s_I2cSlaveMap++, PERI_ReadByte(I2C_SLAVE_DATA_1));
			}
		}
		else if(g_TempByte1 == 1) // MAP + 1 byte
		{
			if(s_I2cSlaveMap == 0xFD)	//set interrupt mask 
			{           
				g_I2cSlaveIntMask = PERI_ReadByte(I2C_SLAVE_DATA_0);                
			}
			else
			{
				PERI_WriteByte(s_I2cSlaveMap++, PERI_ReadByte(I2C_SLAVE_DATA_0));
			}
		}
	}
	else if(g_TempByte1 & bmBIT4)	// Read transaction
	{
		if(s_I2cSlaveMap == 0xFE)	//get interrupt status 
		{           
			PERI_WriteByte(I2C_SLAVE_DATA_0, g_I2cSlaveIntStatus); 
			PERI_WriteByte(I2C_SLAVE_DATA_1, 0);
		}
		else if(s_I2cSlaveMap == 0xFD)	//get interrupt mask 
		{           
			PERI_WriteByte(I2C_SLAVE_DATA_0, g_I2cSlaveIntMask); 
			PERI_WriteByte(I2C_SLAVE_DATA_1, 0);
		}
		else if(s_I2cSlaveMap == 0x00)	//get audio format of speaker  
		{    
			PERI_WriteByte(I2C_SLAVE_DATA_0, PERI_ReadByte(DMA_PLAY_8CH_L)); 
			PERI_WriteByte(I2C_SLAVE_DATA_1, PERI_ReadByte(DMA_PLAY_8CH_H));

			g_I2cSlaveIntStatus &= ~bmBIT0;

			if(!(g_I2cSlaveIntMask & bmBIT0))
				PERI_WriteByte(GPIO_DATA_L, PERI_ReadByte(GPIO_DATA_L) & ~bmBIT7);
		}
		else
		{
			PERI_WriteByte(I2C_SLAVE_DATA_0, PERI_ReadByte(s_I2cSlaveMap++));
			PERI_WriteByte(I2C_SLAVE_DATA_1, PERI_ReadByte(s_I2cSlaveMap++));
		}
	}

	PERI_WriteByte(I2C_SLAVE_CTRL, 0x89);	// Ack processing complete
	PERI_WriteByte(I2C_SLAVE_STATUS, 0x38);	// Clear status flag
}

#endif

void AudioProcess()
{
#ifdef _MCU_FEEDBACK_
	HandleIsoFeedback();
#endif

#ifdef _I2C_SLAVE_SUPPORT_
	if(g_I2cSlaveRequest)
	{
		g_I2cSlaveRequest = FALSE;
		HandleI2cSlave();
	}
#endif
	
	if(g_Tick10ms > 30)
	{
		g_Tick10ms = 0;

		HandleSpdifIn();

		if(s_bPlaybackStartCount)
		{
			g_TempByte1 = PERI_ReadByte(GPIO_DATA_H);

			if(g_TempByte1 & bmBIT5)
			{
				PERI_WriteByte(GPIO_DATA_H, g_TempByte1 & ~bmBIT5);
				//PERI_WriteByte(GPIO_DATA_H, g_TempByte1 & ~bmBIT4);	/*test*/ 
			}
			else
			{
				PERI_WriteByte(GPIO_DATA_H, g_TempByte1 | bmBIT5);
				//PERI_WriteByte(GPIO_DATA_H, g_TempByte1 | bmBIT4);	/*test*/ 
			}
		}
	}
}

#ifdef _MODEL_CM6632A_

static void ConvertToAnalogDAC4382()
{
	signed short sword;

	if(g_TempWord1 == 0x8000)
	{
		g_TempByte1 = 0xFF;
	}
	else
	{
		sword = (signed short)(g_TempWord1);

		sword >>= 8;
		sword = -sword;

		g_TempByte1 = (BYTE)(sword);
	}
}

static void ConvertToAnalogDAC4398()
{
	signed short sword;

	if(g_TempWord1 == 0x8000)
	{
		g_TempByte1 = 0xFF;
	}
	else
	{
		sword = (signed short)(g_TempWord1);

		sword >>= 7;
		sword = -sword;

		g_TempByte1 = (BYTE)(sword);
	}
}

#else

static void ConvertToAnalogDAC1792()
{
	signed short sword;
	
	if(g_TempWord1 == 0x8000)
	{
		g_TempByte1 = 0;
	}
	else
	{
		sword = (signed short)(g_TempWord1);

		sword >>= 7;
		if(sword < -240)
			sword = -240;

		g_TempByte1 = (BYTE)(sword + 255);
	}
}

static void ConvertToAnalogDAC8741()
{
	if(g_TempWord1 == 0x8000)
	{
		g_TempWord1 = 0x3FF;
	}
	else
	{
		g_TempWord1 = -((signed short)(g_TempWord1));
		g_TempWord1 >>= 5;
	}
}

#endif

static void ConvertToAnalogDAC5346()
{
	if(g_TempWord1 == 0x8000)
	{
		g_TempByte1 = 0x28;  // Minimum -12dB
	}
	else
	{
		g_TempByte1 = (g_TempWord1 >> 7) & 0x3F;
	}
}

WORD GetCurrentVol(BYTE index)
{
	return s_CurrentVolume[index];
}

BOOL GetCurrentBoost(BYTE index)
{
	return (s_CurrentBoost >> index) & bmBIT0;
}

BOOL GetCurrentMute(BYTE index)
{
	return (s_CurrentMute >> index) & bmBIT0;
}

void SetCurrentBoost(BYTE index, BOOL boost)
{
	if(boost)
	{
		s_CurrentBoost = s_CurrentBoost | (bmBIT0 << index);
		g_TempByte1 = 0x18;
	}	
	else
	{
		s_CurrentBoost = s_CurrentBoost & (~(bmBIT0 << index));
		g_TempByte1 = 0x1C;
	}	

	I2cMasterWrite(I2C_ADDR_5346, 9, &g_TempByte1, 1|I2C_FAST_MODE);
}	

void SetCurrentMute(BYTE index, BOOL mute)
{
	if(mute)
	{
		s_CurrentMute = s_CurrentMute | (bmBIT0 << index);
	}	
	else
	{
		s_CurrentMute = s_CurrentMute & (~(bmBIT0 << index));
	}	

	switch(g_MuteTable[index])
	{
		case 13:	// Speaker			
#ifdef _MODEL_CM6632A_

			if(mute)
			{
				PERI_WriteByte(GPIO_DATA_H, PERI_ReadByte(GPIO_DATA_H) | bmBIT6);  //for led display 
			}
			else
			{
				PERI_WriteByte(GPIO_DATA_H, PERI_ReadByte(GPIO_DATA_H) & (~bmBIT6));	//for led display  	
			}

			for(index=0; index<8; ++index)
			{
				I2cMasterRead(I2C_ADDR_4382, s_CodecVolumeTable[index].map, &g_TempByte1, 1|I2C_FAST_MODE);
						
				if(mute)
				{
					g_TempByte1 |= bmBIT7;
				}
				else
				{
					g_TempByte1 &= (~bmBIT7);
				}

				I2cMasterWrite(I2C_ADDR_4382, s_CodecVolumeTable[index].map, &g_TempByte1, 1|I2C_FAST_MODE);
			}

#else

			I2cMasterRead(I2C_ADDR_1792, 0x12, &g_TempByte1, 1|I2C_FAST_MODE);
					
			if(mute)
			{
				g_TempByte1 |= bmBIT0;
				PERI_WriteByte(GPIO_DATA_H, PERI_ReadByte(GPIO_DATA_H) | bmBIT6);  //for led display 
			}
			else
			{
				g_TempByte1 &= (~bmBIT0);
				PERI_WriteByte(GPIO_DATA_H, PERI_ReadByte(GPIO_DATA_H) & (~bmBIT6));	//for led display  	
			}

			I2cMasterWrite(I2C_ADDR_1792, 0x12, &g_TempByte1, 1|I2C_FAST_MODE);

#endif
			break;

		case 14:	// SPDIF Out						
			if(mute)
			{
				PERI_WriteByte(DMA_FIFO_MUTE, PERI_ReadByte(DMA_FIFO_MUTE) | bmBIT1);
			}
			else
			{
				PERI_WriteByte(DMA_FIFO_MUTE, PERI_ReadByte(DMA_FIFO_MUTE) & ~bmBIT1);
			}
			break;

		case 15:	// Headphone			
#ifdef _MODEL_CM6632A_

			if(mute)
			{
				g_TempByte1 = 0xD8;
			}
			else
			{
				g_TempByte1 = 0xC0;	    
			}					 

			I2cMasterWrite(I2C_ADDR_4398, 0x04, &g_TempByte1, 1|I2C_FAST_MODE);

#else

			if(mute)
			{
				g_TempByte1 = bmBIT3|bmBIT0;
			}
			else
			{
				g_TempByte1 = bmBIT0;	    
			}					 

			I2cMasterWrite(I2C_ADDR_8741, 0x04<<1, &g_TempByte1, 1|I2C_FAST_MODE);

#endif
			break;

		case 16:	// Mic In
			if(mute)
			{
				PERI_WriteByte(RECORD_MUTE, PERI_ReadByte(RECORD_MUTE) | bmBIT4);
			}
			else if(!s_RecordMute)
			{
				PERI_WriteByte(RECORD_MUTE, PERI_ReadByte(RECORD_MUTE) & (~bmBIT4));
			}
			break;

		case 17:	// Line In
			if(mute)
			{
				PERI_WriteByte(RECORD_MUTE, PERI_ReadByte(RECORD_MUTE) | bmBIT0);
			}
			else if(!s_RecordMute)
			{
				PERI_WriteByte(RECORD_MUTE, PERI_ReadByte(RECORD_MUTE) & (~bmBIT0));
			}
			break;
	}
}

void SetCurrentVol(BYTE index, WORD vol)
{
	s_CurrentVolume[index] = vol;
	g_TempWord1 = vol;

#ifdef _MODEL_CM6632A_

	if(g_VolumeTable[index].type == VOL_4382)
	{
		ConvertToAnalogDAC4382();
		I2cMasterWrite(s_CodecVolumeTable[index].chip, s_CodecVolumeTable[index].map, &g_TempByte1, 1|I2C_FAST_MODE);
	}
	else if(g_VolumeTable[index].type == VOL_4398)
	{
		ConvertToAnalogDAC4398();
		I2cMasterWrite(s_CodecVolumeTable[index].chip, s_CodecVolumeTable[index].map, &g_TempByte1, 1|I2C_FAST_MODE);
	}

#else

	if(g_VolumeTable[index].type == VOL_1792)
	{
		ConvertToAnalogDAC1792();
		I2cMasterWrite(s_CodecVolumeTable[index].chip, s_CodecVolumeTable[index].map, &g_TempByte1, 1|I2C_FAST_MODE);
	}
	else if(g_VolumeTable[index].type == VOL_8741)
	{
		ConvertToAnalogDAC8741();
		g_TempByte1 = g_TempWord1 & 0x1F;
		I2cMasterWrite(s_CodecVolumeTable[index].chip, s_CodecVolumeTable[index].map<<1, &g_TempByte1, 1|I2C_FAST_MODE);
		g_TempByte1 = bmBIT5 | ((g_TempWord1>>5) & 0x1F);
		I2cMasterWrite(s_CodecVolumeTable[index].chip, (s_CodecVolumeTable[index].map+1)<<1, &g_TempByte1, 1|I2C_FAST_MODE);
	}

#endif

	else if(g_VolumeTable[index].type == VOL_5346)
	{
		ConvertToAnalogDAC5346();
		I2cMasterWrite(s_CodecVolumeTable[index].chip, s_CodecVolumeTable[index].map, &g_TempByte1, 1|I2C_FAST_MODE);
	}
}

void SetRecordMute(BOOL mute)
{
	if(mute)
	{
		PERI_WriteByte(RECORD_MUTE, PERI_ReadByte(RECORD_MUTE) | bmBIT4 | bmBIT0);
		PERI_WriteByte(GPIO_DATA_H, PERI_ReadByte(GPIO_DATA_H) | bmBIT7);		
	}
	else
	{
		if(!GetCurrentMute(3))
			PERI_WriteByte(RECORD_MUTE, PERI_ReadByte(RECORD_MUTE) & (~bmBIT4));

		if(!GetCurrentMute(4))
			PERI_WriteByte(RECORD_MUTE, PERI_ReadByte(RECORD_MUTE) & (~bmBIT0));

		PERI_WriteByte(GPIO_DATA_H, PERI_ReadByte(GPIO_DATA_H) & (~bmBIT7));
	}		

	s_RecordMute = mute;
}

#ifdef _MODEL_CM6632A_
//add ben Modify bclk
// Control CS4382
BOOL PlayMultiChStart(BYTE ch, BYTE format)
{
#ifdef _I2C_SLAVE_SUPPORT_

	if((format!=(PERI_ReadByte(DMA_PLAY_8CH_L)&0x06)) || (ch!=PERI_ReadByte(DMA_PLAY_8CH_H)))
	{
		g_I2cSlaveIntStatus |= bmBIT0;        

		if(!(g_I2cSlaveIntMask & bmBIT0))
			PERI_WriteByte(GPIO_DATA_L, PERI_ReadByte(GPIO_DATA_L) | bmBIT7);
	}

#endif

	g_TempByte1 = GetDmaFreq(EP_MULTI_CH_PLAY);
	PERI_WriteByte(DMA_PLAY_8CH_L, format|g_TempByte1);

	g_Index = g_TempByte1 >> 3;
		
	// Set sampling rate
	switch(g_TempByte1)
	{
		case DMA_8000:
			//gpio
			PERI_WriteByte(GPIO_DATA_H, PERI_ReadByte(GPIO_DATA_H) | bmBIT2);  //for gpio 

			//add ben gpio12 高电平
			//g_PlayMode = 1;
			//PERI_WriteByte(GPIO_DATA_H, PERI_ReadByte(GPIO_DATA_H) | bmBIT4);
	
			g_TempWord1 = BCLK_LRCK_128|I2S_MASTER|MCLK_LRCK_256|I2S_MODE|I2S_8000;
			g_TempByte1 = 0x24;             // single speed
			break;

		case DMA_16000:
			//gpio
			PERI_WriteByte(GPIO_DATA_H, PERI_ReadByte(GPIO_DATA_H) | bmBIT2);  //for gpio 

			//add ben gpio12 高电平
			//g_PlayMode = 1;
			//PERI_WriteByte(GPIO_DATA_H, PERI_ReadByte(GPIO_DATA_H) | bmBIT4);
			
			g_TempWord1 = BCLK_LRCK_128|I2S_MASTER|MCLK_LRCK_256|I2S_MODE|I2S_16000;
			g_TempByte1 = 0x24;             // single speed
			break;

		case DMA_32000:
			//gpio
			PERI_WriteByte(GPIO_DATA_H, PERI_ReadByte(GPIO_DATA_H) | bmBIT2);  //for gpio 

			//add ben gpio12 高电平
			//g_PlayMode = 1;
			//PERI_WriteByte(GPIO_DATA_H, PERI_ReadByte(GPIO_DATA_H) | bmBIT4);
					
			g_TempWord1 = BCLK_LRCK_128|I2S_MASTER|MCLK_LRCK_256|I2S_MODE|I2S_32000;
			g_TempByte1 = 0x24;             // single speed
			break;

		case DMA_44100:
			//gpio
			PERI_WriteByte(GPIO_DATA_H, PERI_ReadByte(GPIO_DATA_H) | bmBIT2);  //for gpio 

			//add ben gpio12 高电平
			//g_PlayMode = 1;
			//PERI_WriteByte(GPIO_DATA_H, PERI_ReadByte(GPIO_DATA_H) | bmBIT4);
			
			//g_TempWord1 = BCLK_LRCK_128|I2S_MASTER|MCLK_LRCK_256|I2S_MODE|I2S_44100;
			g_TempWord1 = BCLK_LRCK_64|I2S_MASTER|MCLK_LRCK_256|I2S_MODE|I2S_44100;
			g_TempByte1 = 0x24;             // single speed
			break;

		case DMA_48000:
			//gpio
			PERI_WriteByte(GPIO_DATA_H, PERI_ReadByte(GPIO_DATA_H) | bmBIT2);  //for gpio 

			//add ben gpio12 高电平
			//g_PlayMode = 1;
			//PERI_WriteByte(GPIO_DATA_H, PERI_ReadByte(GPIO_DATA_H) | bmBIT4);
			
			//g_TempWord1 = BCLK_LRCK_128|I2S_MASTER|MCLK_LRCK_256|I2S_MODE|I2S_48000;
			g_TempWord1 = BCLK_LRCK_64|I2S_MASTER|MCLK_LRCK_256|I2S_MODE|I2S_48000;
			g_TempByte1 = 0x24;             // single speed
			break;

		case DMA_64000:
			//gpio
			PERI_WriteByte(GPIO_DATA_H, PERI_ReadByte(GPIO_DATA_H) | bmBIT2);  //for gpio 

			//add ben gpio12 高电平
			//g_PlayMode = 1;
			//PERI_WriteByte(GPIO_DATA_H, PERI_ReadByte(GPIO_DATA_H) | bmBIT4);
			
			g_TempWord1 = BCLK_LRCK_64|I2S_MASTER|MCLK_LRCK_256|I2S_MODE|I2S_64000;
			g_TempByte1 = 0x25;             // double speed
			break;

		case DMA_88200:
			//gpio
			PERI_WriteByte(GPIO_DATA_H, PERI_ReadByte(GPIO_DATA_H) | bmBIT2);  //for gpio 		

			//add ben gpio12 高电平
			//g_PlayMode = 1;
			//PERI_WriteByte(GPIO_DATA_H, PERI_ReadByte(GPIO_DATA_H) | bmBIT4);
				
			g_TempWord1 = BCLK_LRCK_64|I2S_MASTER|MCLK_LRCK_256|I2S_MODE|I2S_88200;
			g_TempByte1 = 0x25;             // double speed
			break;

		case DMA_96000:
			//gpio
			PERI_WriteByte(GPIO_DATA_H, PERI_ReadByte(GPIO_DATA_H) | bmBIT2);  //for gpio 

			//add ben gpio12 高电平
			//g_PlayMode = 1;
			//PERI_WriteByte(GPIO_DATA_H, PERI_ReadByte(GPIO_DATA_H) | bmBIT4);
			
			g_TempWord1 = BCLK_LRCK_64|I2S_MASTER|MCLK_LRCK_256|I2S_MODE|I2S_96000;
			g_TempByte1 = 0x25;             // double speed
			break;

		case DMA_176400:
			//gpio
			PERI_WriteByte(GPIO_DATA_H, PERI_ReadByte(GPIO_DATA_H) & (~bmBIT2));  //for gpio 

			//add ben gpio12 高电平
			//g_PlayMode = 1;
			//PERI_WriteByte(GPIO_DATA_H, PERI_ReadByte(GPIO_DATA_H) | bmBIT4);
			
			g_TempWord1 = BCLK_LRCK_64|I2S_MASTER|MCLK_LRCK_128|I2S_MODE|I2S_176400;
			g_TempByte1 = 0x26;             // Quad speed
			break;

		case DMA_192000:
			//gpio
			PERI_WriteByte(GPIO_DATA_H, PERI_ReadByte(GPIO_DATA_H) & (~bmBIT2));  //for gpio 

			//add ben gpio12 高电平
			//g_PlayMode = 1;
			//PERI_WriteByte(GPIO_DATA_H, PERI_ReadByte(GPIO_DATA_H) | bmBIT4);
			
			g_TempWord1 = BCLK_LRCK_64|I2S_MASTER|MCLK_LRCK_128|I2S_MODE|I2S_192000;
			g_TempByte1 = 0x26;             // Quad speed
			break;

		default:
			return FALSE;
	}

	// Set bit resolution
	switch(format & RESOLUTION_MASK)
	{
		case DMA_16Bit:
#ifdef _MCU_FEEDBACK_
			s_MultiChFeedbackRatio = 1;
#endif

			g_TempWord1 |= I2S_16Bit;
			break;

		case DMA_24Bit:
		case DMA_24Bit_32Container:
#ifdef _MCU_FEEDBACK_
			s_MultiChFeedbackRatio = 2;
#endif

			g_Index += 10;
			g_TempWord1 |= I2S_24Bit;
			break;

		case DMA_32Bit:
#ifdef _MCU_FEEDBACK_
			s_MultiChFeedbackRatio = 2;
#endif

			g_Index += 10;
			g_TempWord1 |= I2S_32Bit;
	}

	I2cMasterWrite(I2C_ADDR_4382, 0x06, &g_TempByte1, 1);
	I2cMasterWrite(I2C_ADDR_4382, 0x09, &g_TempByte1, 1);
	I2cMasterWrite(I2C_ADDR_4382, 0x0C, &g_TempByte1, 1);
	I2cMasterWrite(I2C_ADDR_4382, 0x0F, &g_TempByte1, 1);

	PERI_WriteByte(I2S_PLAY_8CH_H, PERI_ReadByte(I2S_PLAY_8CH_H) | bmBIT4); // Mute MCLK
	PERI_WriteWord(I2S_PLAY_8CH_L, g_TempWord1|MCLK_MUTE);	// Set I2S format
	PERI_WriteByte(I2S_PLAY_8CH_H, PERI_ReadByte(I2S_PLAY_8CH_H) & ~bmBIT4); // Unmute MCLK

	PERI_WriteByte(DMA_PLAY_8CH_H, ch);	// Set channel numbers
	switch(ch)
	{
		case DMA_2CH:
			PERI_WriteByte(PLAYBACK_ROUTING_H, PLAYBACK_ROUTING_2CH);
			break;

		case DMA_4CH:
#ifdef _MCU_FEEDBACK_
			s_MultiChFeedbackRatio <<= 1;
#endif

			g_Index += 20;

			PERI_WriteByte(PLAYBACK_ROUTING_H, PLAYBACK_ROUTING_4CH);
			break;

		case DMA_6CH:
#ifdef _MCU_FEEDBACK_
			s_MultiChFeedbackRatio *= 3;
#endif

			g_Index += 40;

			PERI_WriteByte(PLAYBACK_ROUTING_H, PLAYBACK_ROUTING_6CH);
			break;		

		case DMA_8CH:
#ifdef _MCU_FEEDBACK_
			s_MultiChFeedbackRatio <<= 2;
#endif

			g_Index += 60;

			PERI_WriteByte(PLAYBACK_ROUTING_H, PLAYBACK_ROUTING_8CH);
	}

	if(g_IsAudioClass20 && g_UsbIsHighSpeed)
		PERI_WriteByte(STARTING_THRESHOLD_MUTICH_L, s_uFrameThresholdTable[g_Index]);
	else
		PERI_WriteByte(STARTING_THRESHOLD_MUTICH_L, s_mSecondThresholdTable[g_Index]);

#ifdef _MCU_FEEDBACK_
	if(g_IsAudioClass20 && g_UsbIsHighSpeed)
		s_MultiChThreshold = s_uFrameThresholdTable[g_Index];
	else
		s_MultiChThreshold = s_mSecondThresholdTable[g_Index];

	USB_HANDSHAKE4 = bmBIT3;	// Reset feedback data
	s_MultiChCountOld = (MULTI_CH_PLAYBACK_COUNT[1] << 8) | MULTI_CH_PLAYBACK_COUNT[0];
#endif

	PERI_WriteByte(DMA_PLAY_8CH_L, PERI_ReadByte(DMA_PLAY_8CH_L)|DMA_START);

	PlaybackAddRef(PLAY_8CH);
	return TRUE;
}

#else

// Control PCM1792
BOOL PlayMultiChStart(BYTE ch, BYTE format)
{
	//g_TempByte1 = bmBIT4;
	//I2cMasterWrite(I2C_ADDR_1792, 0x13, &g_TempByte1, 1);

#ifdef _I2C_SLAVE_SUPPORT_

	if((format!=(PERI_ReadByte(DMA_PLAY_8CH_L)&0x06)) || (ch!=PERI_ReadByte(DMA_PLAY_8CH_H)))
	{
		g_I2cSlaveIntStatus |= bmBIT0;        

		if(!(g_I2cSlaveIntMask & bmBIT0))
			PERI_WriteByte(GPIO_DATA_L, PERI_ReadByte(GPIO_DATA_L) | bmBIT7);
	}

#endif    

	g_TempByte1 = GetDmaFreq(EP_MULTI_CH_PLAY);
	PERI_WriteByte(DMA_PLAY_8CH_L, format|g_TempByte1);

	I2cMasterRead(I2C_ADDR_1792, 0x14, &g_TempByte2, 1);
	g_TempByte2 &= 0xFC;

	g_Index = g_TempByte1 >> 3;

	// Set sampling rate
	switch(g_TempByte1)
	{
		case DMA_8000:
			g_TempWord1 = BCLK_LRCK_128|MCLK_LRCK_256|I2S_MODE|I2S_8000;
			g_TempByte2 |= 0x02;             // OS = 128fs
			break;

		case DMA_16000:
			g_TempWord1 = BCLK_LRCK_128|MCLK_LRCK_256|I2S_MODE|I2S_16000;
			g_TempByte2 |= 0x02;             // OS = 128fs
			break;

		case DMA_32000:
			g_TempWord1 = BCLK_LRCK_128|MCLK_LRCK_256|I2S_MODE|I2S_32000;
			g_TempByte2 |= 0x02;             // OS = 128fs
			break;

		case DMA_44100:
			g_TempWord1 = BCLK_LRCK_128|MCLK_LRCK_256|I2S_MODE|I2S_44100;
			g_TempByte2 |= 0x02;             // OS = 128fs
			break;

		case DMA_48000:
			g_TempWord1 = BCLK_LRCK_128|MCLK_LRCK_256|I2S_MODE|I2S_48000;
			g_TempByte2 |= 0x02;             // OS = 128fs
			break;

		case DMA_64000:
			g_TempWord1 = BCLK_LRCK_64|MCLK_LRCK_256|I2S_MODE|I2S_64000;
			g_TempByte2 |= 0x00;             // OS = 64fs
			break;

		case DMA_88200:
			g_TempWord1 = BCLK_LRCK_64|MCLK_LRCK_256|I2S_MODE|I2S_88200;
			g_TempByte2 |= 0x00;             // OS = 64fs
			break;

		case DMA_96000:
			g_TempWord1 = BCLK_LRCK_64|MCLK_LRCK_256|I2S_MODE|I2S_96000;
			g_TempByte2 |= 0x00;             // OS = 64fs	
			break;

		case DMA_176400:
			g_TempWord1 = BCLK_LRCK_64|MCLK_LRCK_128|I2S_MODE|I2S_176400;
			g_TempByte2 |= 0x01;              // OS = 32fs
			break;

		case DMA_192000:
			g_TempWord1 = BCLK_LRCK_64|MCLK_LRCK_128|I2S_MODE|I2S_192000;
			g_TempByte2 |= 0x01;              // OS = 32fs
			break;

		default:
			return FALSE;
	}

#ifdef _EXT_CLK_SI5351_
	g_TempWord1 |= MCLK_TRI_STATE;
#else
	g_TempWord1 |= I2S_MASTER;
#endif

	I2cMasterRead(I2C_ADDR_1792, 0x12, &g_TempByte1, 1);
	g_TempByte1 &= 0x0F;

	// Set bit resolution
	switch(format & RESOLUTION_MASK)
	{
		case DMA_16Bit:
#ifdef _MCU_FEEDBACK_
			s_MultiChFeedbackRatio = 1;
#endif

			g_TempByte1 |= (bmBIT7 | I2S_CODEC_16bit);
			g_TempWord1 |= I2S_16Bit;
			break;

		case DMA_24Bit:
		case DMA_24Bit_32Container:
#ifdef _MCU_FEEDBACK_
			s_MultiChFeedbackRatio = 2;
#endif

			g_Index += 10;
			g_TempByte1 |= (bmBIT7 | I2S_CODEC_24bit);
			g_TempWord1 |= I2S_24Bit;
			break;

		case DMA_32Bit:
#ifdef _MCU_FEEDBACK_
			s_MultiChFeedbackRatio = 2;
#endif

			g_Index += 10;
			g_TempWord1 |= I2S_32Bit;
	}

	I2cMasterWrite(I2C_ADDR_1792, 0x12, &g_TempByte1, 1);
	I2cMasterWrite(I2C_ADDR_1792, 0x14, &g_TempByte2, 1);

	PERI_WriteByte(I2S_PLAY_8CH_H, PERI_ReadByte(I2S_PLAY_8CH_H) | bmBIT4); // Mute MCLK
	PERI_WriteWord(I2S_PLAY_8CH_L, g_TempWord1|MCLK_MUTE);	// Set I2S format
	PERI_WriteByte(I2S_PLAY_8CH_H, PERI_ReadByte(I2S_PLAY_8CH_H) & ~bmBIT4); // Unmute MCLK

	PERI_WriteByte(DMA_PLAY_8CH_H, ch);	// Set channel numbers
	switch(ch)
	{
		case DMA_2CH:
			PERI_WriteByte(PLAYBACK_ROUTING_H, PLAYBACK_ROUTING_2CH);
			break;

		case DMA_4CH:
#ifdef _MCU_FEEDBACK_
			s_MultiChFeedbackRatio <<= 1;
#endif

			g_Index += 20;

			PERI_WriteByte(PLAYBACK_ROUTING_H, PLAYBACK_ROUTING_4CH);
			break;

		case DMA_6CH:
#ifdef _MCU_FEEDBACK_
			s_MultiChFeedbackRatio *= 3;
#endif

			g_Index += 40;

			PERI_WriteByte(PLAYBACK_ROUTING_H, PLAYBACK_ROUTING_6CH);
			break;		

		case DMA_8CH:
#ifdef _MCU_FEEDBACK_
			s_MultiChFeedbackRatio <<= 2;
#endif

			g_Index += 60;

			PERI_WriteByte(PLAYBACK_ROUTING_H, PLAYBACK_ROUTING_8CH);
	}

#ifdef _EXT_CLK_SI5351_
	if(s_ChangeClock)
	{
		SetMultiChClock(GetDmaFreq(EP_MULTI_CH_PLAY));
		s_ChangeClock = FALSE;
	}
#endif

	if(g_IsAudioClass20 && g_UsbIsHighSpeed)
		PERI_WriteByte(STARTING_THRESHOLD_MUTICH_L, s_uFrameThresholdTable[g_Index]);
	else
		PERI_WriteByte(STARTING_THRESHOLD_MUTICH_L, s_mSecondThresholdTable[g_Index]);

#ifdef _MCU_FEEDBACK_
	if(g_IsAudioClass20 && g_UsbIsHighSpeed)
		s_MultiChThreshold = s_uFrameThresholdTable[g_Index];
	else
		s_MultiChThreshold = s_mSecondThresholdTable[g_Index];

	USB_HANDSHAKE4 = bmBIT3;	// Reset feedback data
	s_MultiChCountOld = (MULTI_CH_PLAYBACK_COUNT[1] << 8) | MULTI_CH_PLAYBACK_COUNT[0];
#endif

	PERI_WriteByte(DMA_PLAY_8CH_L, PERI_ReadByte(DMA_PLAY_8CH_L)|DMA_START);

	PlaybackAddRef(PLAY_8CH);
	return TRUE;
}

#endif

BOOL PlaySpdifStart(BYTE format)
{
	g_TempByte1 = GetDmaFreq(EP_SPDIF_PLAY);
	PERI_WriteByte(DMA_PLAY_SPDIF, (format&0x7F)|g_TempByte1);

	g_TempByte2 = PERI_ReadByte(SPDIF_CTRL_3) & 0xF8;

	g_Index = g_TempByte1 >> 3;

	// Set sampling rate
	switch(g_TempByte1)
	{
		case DMA_32000:
			g_TempByte2 |= SPDIF_CTRL_32000;
			break;

		case DMA_44100:
			g_TempByte2 |= SPDIF_CTRL_44100;
			break;

		case DMA_48000:
			g_TempByte2 |= SPDIF_CTRL_48000;
			break;

		case DMA_88200:
			g_TempByte2 |= SPDIF_CTRL_88200;
			break;

		case DMA_96000:
			g_TempByte2 |= SPDIF_CTRL_96000;
			break;		

		case DMA_176400:
			g_TempByte2 |= SPDIF_CTRL_176400;
			break;

		case DMA_192000:
			g_TempByte2 |= SPDIF_CTRL_192000;
			break;

		default:
			return FALSE;
	}

	PERI_WriteByte(SPDIF_CTRL_3, g_TempByte2);	

	// Set bit resolution
	switch(format & RESOLUTION_MASK)
	{
		case DMA_16Bit:
#ifdef _MCU_FEEDBACK_
			s_SpdifFeedbackRatio = 1;
#endif

			break;

		case DMA_24Bit:
		case DMA_24Bit_32Container:
		case DMA_32Bit:
#ifdef _MCU_FEEDBACK_
			s_SpdifFeedbackRatio = 2;
#endif

			g_Index += 10;
			break;
	}

	if(format & NON_PCM)
	{
		PERI_WriteByte(SPDIF_OUT0_STATUS_L, (PERI_ReadByte(SPDIF_OUT0_STATUS_L) | (bmBIT1|bmBIT5)) | bmBIT0);			
	}
	else
	{
		PERI_WriteByte(SPDIF_OUT0_STATUS_L, (PERI_ReadByte(SPDIF_OUT0_STATUS_L) & ~(bmBIT1|bmBIT5)) | bmBIT0);			
	}

	if(g_IsAudioClass20 && g_UsbIsHighSpeed)
		PERI_WriteByte(STARTING_THRESHOLD_SPDIF_L, s_uFrameThresholdTable[g_Index]);
	else
		PERI_WriteByte(STARTING_THRESHOLD_SPDIF_L, s_mSecondThresholdTable[g_Index]);

#ifdef _MCU_FEEDBACK_
	if(g_IsAudioClass20 && g_UsbIsHighSpeed)
		s_SpdifThreshold = s_uFrameThresholdTable[g_Index];
	else
		s_SpdifThreshold = s_mSecondThresholdTable[g_Index];

	USB_HANDSHAKE4 = bmBIT4;	// Reset feedback data
	s_SpdifCountOld = (SPDIF_PLAYBACK_COUNT[1] << 8) | SPDIF_PLAYBACK_COUNT[0];
#endif

	PERI_WriteByte(DMA_PLAY_SPDIF, PERI_ReadByte(DMA_PLAY_SPDIF)|DMA_START);

	PlaybackAddRef(PLAY_SPDIF);
	return TRUE;
}

#ifdef _MODEL_CM6632A_

// Control CS4398
BOOL PlayTwoChStart(BYTE format)
{
	g_TempByte1 = GetDmaFreq(EP_TWO_CH_PLAY);
	PERI_WriteByte(DMA_PLAY_2CH, format|g_TempByte1);

	g_Index = g_TempByte1 >> 3;

	// Set sampling rate
	switch(g_TempByte1)
	{
		case DMA_8000:
			g_TempWord1 = BCLK_LRCK_128|I2S_MASTER|MCLK_LRCK_256|I2S_MODE|I2S_8000;
			g_TempByte2 = 0x10;             // I2S mode, single speed
			break;

		case DMA_16000:
			g_TempWord1 = BCLK_LRCK_128|I2S_MASTER|MCLK_LRCK_256|I2S_MODE|I2S_16000;
			g_TempByte2 = 0x10;             // I2S mode, single speed
			break;

		case DMA_32000:
			g_TempWord1 = BCLK_LRCK_128|I2S_MASTER|MCLK_LRCK_256|I2S_MODE|I2S_32000;
			g_TempByte2 = 0x10;             // I2S mode, single speed
			break;

		case DMA_44100:
			g_TempWord1 = BCLK_LRCK_128|I2S_MASTER|MCLK_LRCK_256|I2S_MODE|I2S_44100;
			g_TempByte2 = 0x10;             // I2S mode, single speed
			break;

		case DMA_48000:
			g_TempWord1 = BCLK_LRCK_128|I2S_MASTER|MCLK_LRCK_256|I2S_MODE|I2S_48000;
			g_TempByte2 = 0x10;             // I2S mode, single speed
			break;

		case DMA_64000:
			g_TempWord1 = BCLK_LRCK_64|I2S_MASTER|MCLK_LRCK_256|I2S_MODE|I2S_64000;
			g_TempByte2 = 0x11;             // I2S mode, double speed
			break;

		case DMA_88200:
			g_TempWord1 = BCLK_LRCK_64|I2S_MASTER|MCLK_LRCK_256|I2S_MODE|I2S_88200;
			g_TempByte2 = 0x11;             // I2S mode, double speed
			break;

		case DMA_96000:
			g_TempWord1 = BCLK_LRCK_64|I2S_MASTER|MCLK_LRCK_256|I2S_MODE|I2S_96000;
			g_TempByte2 = 0x11;             // I2S mode, double speed
			break;

		case DMA_176400:
			g_TempWord1 = BCLK_LRCK_64|I2S_MASTER|MCLK_LRCK_128|I2S_MODE|I2S_176400;
			g_TempByte2 = 0x12;             // I2S mode, Quad speed	
			break;

		case DMA_192000:
			g_TempWord1 = BCLK_LRCK_64|I2S_MASTER|MCLK_LRCK_128|I2S_MODE|I2S_192000;
			g_TempByte2 = 0x12;             // I2S mode, Quad speed	
			break;

		default:
			return FALSE;
	}

	// Set bit resolution
	switch(format & RESOLUTION_MASK)
	{
		case DMA_16Bit:
#ifdef _MCU_FEEDBACK_
			s_TwoChFeedbackRatio = 1;
#endif

			g_TempWord1 |= I2S_16Bit;
			break;

		case DMA_24Bit:
		case DMA_24Bit_32Container:
#ifdef _MCU_FEEDBACK_
			s_TwoChFeedbackRatio = 2;
#endif

			g_Index += 10;
			g_TempWord1 |= I2S_24Bit;
			break;

		case DMA_32Bit:
#ifdef _MCU_FEEDBACK_
			s_TwoChFeedbackRatio = 2;
#endif

			g_Index += 10;
			g_TempWord1 |= I2S_32Bit;
	}

	I2cMasterWrite(I2C_ADDR_4398, 0x02, &g_TempByte2, 1);

	PERI_WriteByte(I2S_PLAY_2CH_H, PERI_ReadByte(I2S_PLAY_2CH_H) | bmBIT4); // Mute MCLK
	PERI_WriteWord(I2S_PLAY_2CH_L, g_TempWord1|MCLK_MUTE);	// Set I2S format
	PERI_WriteByte(I2S_PLAY_2CH_H, PERI_ReadByte(I2S_PLAY_2CH_H) & ~bmBIT4); // Unmute MCLK

	if(g_IsAudioClass20 && g_UsbIsHighSpeed)
		PERI_WriteByte(STARTING_THRESHOLD_2CH_L, s_uFrameThresholdTable[g_Index]);
	else
		PERI_WriteByte(STARTING_THRESHOLD_2CH_L, s_mSecondThresholdTable[g_Index]);

#ifdef _MCU_FEEDBACK_
	if(g_IsAudioClass20 && g_UsbIsHighSpeed)
		s_TwoChThreshold = s_uFrameThresholdTable[g_Index];
	else
		s_TwoChThreshold = s_mSecondThresholdTable[g_Index];

	USB_HANDSHAKE4 = bmBIT5;	// Reset feedback data
	s_TwoChCountOld = (TWO_CH_PLAYBACK_COUNT[1] << 8) | TWO_CH_PLAYBACK_COUNT[0];
#endif

	PERI_WriteByte(DMA_PLAY_2CH, PERI_ReadByte(DMA_PLAY_2CH)|DMA_START);

	PlaybackAddRef(PLAY_2CH);
	return TRUE;
}

#else

// Control WM8741
BOOL PlayTwoChStart(BYTE format)
{
	g_TempByte1 = GetDmaFreq(EP_TWO_CH_PLAY);
	PERI_WriteByte(DMA_PLAY_2CH, format|g_TempByte1);

	g_Index = g_TempByte1 >> 3;

	// Set sampling rate
	switch(g_TempByte1)
	{
		case DMA_8000:
			g_TempWord1 = BCLK_LRCK_128|I2S_MASTER|MCLK_LRCK_256|I2S_MODE|I2S_8000;
			g_TempByte2 = 0x00;             // OS = low
			break;

		case DMA_16000:
			g_TempWord1 = BCLK_LRCK_128|I2S_MASTER|MCLK_LRCK_256|I2S_MODE|I2S_16000;
			g_TempByte2 = 0x00;             // OS = low
			break;

		case DMA_32000:
			g_TempWord1 = BCLK_LRCK_128|I2S_MASTER|MCLK_LRCK_256|I2S_MODE|I2S_32000;
			g_TempByte2 = 0x00;             // OS = low
			break;

		case DMA_44100:
			g_TempWord1 = BCLK_LRCK_128|I2S_MASTER|MCLK_LRCK_256|I2S_MODE|I2S_44100;
			g_TempByte2 = 0x00;             // OS = low
			break;

		case DMA_48000:
			g_TempWord1 = BCLK_LRCK_128|I2S_MASTER|MCLK_LRCK_256|I2S_MODE|I2S_48000;
			g_TempByte2 = 0x00;             // OS = low
			break;

		case DMA_64000:
			g_TempWord1 = BCLK_LRCK_64|I2S_MASTER|MCLK_LRCK_256|I2S_MODE|I2S_64000;
			g_TempByte2 = 0x20;             // OS = medium
			break;

		case DMA_88200:
			g_TempWord1 = BCLK_LRCK_64|I2S_MASTER|MCLK_LRCK_256|I2S_MODE|I2S_88200;
			g_TempByte2 = 0x20;             // OS = medium
			break;

		case DMA_96000:
			g_TempWord1 = BCLK_LRCK_64|I2S_MASTER|MCLK_LRCK_256|I2S_MODE|I2S_96000;
			g_TempByte2 = 0x20;             // OS = medium
			break;

		case DMA_176400:
			g_TempWord1 = BCLK_LRCK_64|I2S_MASTER|MCLK_LRCK_128|I2S_MODE|I2S_176400;
			g_TempByte2 = 0x40;             // OS = high
			break;

		case DMA_192000:
			g_TempWord1 = BCLK_LRCK_64|I2S_MASTER|MCLK_LRCK_128|I2S_MODE|I2S_192000;
			g_TempByte2 = 0x40;             // OS = high
			break;

		default:
			return FALSE;
	}

	// Set bit resolution
	switch(format & RESOLUTION_MASK)
	{
		case DMA_16Bit:
#ifdef _MCU_FEEDBACK_
			s_TwoChFeedbackRatio = 1;
#endif

			g_TempByte1 = 0x08;
			g_TempWord1 |= I2S_16Bit;
			break;

		case DMA_24Bit:
		case DMA_24Bit_32Container:
#ifdef _MCU_FEEDBACK_
			s_TwoChFeedbackRatio = 2;
#endif

			g_Index += 10;
			g_TempByte1 = 0x0A;
			g_TempWord1 |= I2S_24Bit;
			break;

		case DMA_32Bit:
#ifdef _MCU_FEEDBACK_
			s_TwoChFeedbackRatio = 2;
#endif

			g_Index += 10;
			g_TempByte1 = 0x0B;
			g_TempWord1 |= I2S_32Bit;
	}

	I2cMasterWrite(I2C_ADDR_8741, 0x05<<1, &g_TempByte1, 1);
	I2cMasterWrite(I2C_ADDR_8741, 0x07<<1, &g_TempByte2, 1);

	PERI_WriteByte(I2S_PLAY_2CH_H, PERI_ReadByte(I2S_PLAY_2CH_H) | bmBIT4); // Mute MCLK
	PERI_WriteWord(I2S_PLAY_2CH_L, g_TempWord1|MCLK_MUTE);	// Set I2S format
	PERI_WriteByte(I2S_PLAY_2CH_H, PERI_ReadByte(I2S_PLAY_2CH_H) & ~bmBIT4); // Unmute MCLK

	if(g_IsAudioClass20 && g_UsbIsHighSpeed)
		PERI_WriteByte(STARTING_THRESHOLD_2CH_L, s_uFrameThresholdTable[g_Index]);
	else
		PERI_WriteByte(STARTING_THRESHOLD_2CH_L, s_mSecondThresholdTable[g_Index]);

#ifdef _MCU_FEEDBACK_
	if(g_IsAudioClass20 && g_UsbIsHighSpeed)
		s_TwoChThreshold = s_uFrameThresholdTable[g_Index];
	else
		s_TwoChThreshold = s_mSecondThresholdTable[g_Index];

	USB_HANDSHAKE4 = bmBIT5;	// Reset feedback data
	s_TwoChCountOld = (TWO_CH_PLAYBACK_COUNT[1] << 8) | TWO_CH_PLAYBACK_COUNT[0];
#endif

	PERI_WriteByte(DMA_PLAY_2CH, PERI_ReadByte(DMA_PLAY_2CH)|DMA_START);

	PlaybackAddRef(PLAY_2CH);
	return TRUE;
}

#endif

// Control CS5381
BOOL RecMultiChStart(BYTE ch, BYTE format)
{
	g_TempByte1 = GetDmaFreq(EP_MULTI_CH_REC);
	PERI_WriteByte(DMA_REC_8CH_L, format|g_TempByte1);

	g_TempWord2 = GetSamplingRate(g_TempByte1);

	if(g_IsAudioClass20 && g_UsbIsHighSpeed)
		g_TempWord2 = (g_TempWord2/8)+2;
	else
		g_TempWord2 = g_TempWord2+2;

	// Set sampling rate
	switch(g_TempByte1)
	{
		case DMA_8000:
			g_TempWord1 = BCLK_LRCK_64|I2S_MASTER|MCLK_LRCK_256|I2S_MODE|I2S_8000;
			PERI_WriteByte(GPIO_DATA_H, PERI_ReadByte(GPIO_DATA_H) & ~(bmBIT0|bmBIT1));	//GPIO 8.9 use to select ADC CS5381 sampling rate range 00: 2KHz ~ 54KHz	
			break;

		case DMA_16000:
			g_TempWord1 = BCLK_LRCK_64|I2S_MASTER|MCLK_LRCK_256|I2S_MODE|I2S_16000;
			PERI_WriteByte(GPIO_DATA_H, PERI_ReadByte(GPIO_DATA_H) & ~(bmBIT0|bmBIT1));	//GPIO 8.9 use to select ADC CS5381 sampling rate range 00: 2KHz ~ 54KHz	
			break;

		case DMA_32000:
			g_TempWord1 = BCLK_LRCK_64|I2S_MASTER|MCLK_LRCK_256|I2S_MODE|I2S_32000;
			PERI_WriteByte(GPIO_DATA_H, PERI_ReadByte(GPIO_DATA_H) & ~(bmBIT0|bmBIT1));	//GPIO 8.9 use to select ADC CS5381 sampling rate range 00: 2KHz ~ 54KHz	
			break;

		case DMA_44100:
			g_TempWord1 = BCLK_LRCK_64|I2S_MASTER|MCLK_LRCK_256|I2S_MODE|I2S_44100;
			PERI_WriteByte(GPIO_DATA_H, PERI_ReadByte(GPIO_DATA_H) & ~(bmBIT0|bmBIT1));	//GPIO 8.9 use to select ADC CS5381 sampling rate range 00: 2KHz ~ 54KHz	
			break;

		case DMA_48000:
			g_TempWord1 = BCLK_LRCK_64|I2S_MASTER|MCLK_LRCK_256|I2S_MODE|I2S_48000;
			PERI_WriteByte(GPIO_DATA_H, PERI_ReadByte(GPIO_DATA_H) & ~(bmBIT0|bmBIT1));	//GPIO 8.9 use to select ADC CS5381 sampling rate range 00: 2KHz ~ 54KHz	
			break;

		case DMA_64000:
			g_TempWord1 = BCLK_LRCK_64|I2S_MASTER|MCLK_LRCK_256|I2S_MODE|I2S_64000;
			PERI_WriteByte(GPIO_DATA_H, (PERI_ReadByte(GPIO_DATA_H) | bmBIT0) & ~bmBIT1);	//GPIO 8.9 use to select ADC CS5381 sampling rate range 01: 50KHz ~ 108KHz
			break;

		case DMA_88200:
			g_TempWord1 = BCLK_LRCK_64|I2S_MASTER|MCLK_LRCK_256|I2S_MODE|I2S_88200;
			PERI_WriteByte(GPIO_DATA_H, (PERI_ReadByte(GPIO_DATA_H) | bmBIT0) & ~bmBIT1);	//GPIO 8.9 use to select ADC CS5381 sampling rate range 01: 50KHz ~ 108KHz
			break;

		case DMA_96000:
			g_TempWord1 = BCLK_LRCK_64|I2S_MASTER|MCLK_LRCK_256|I2S_MODE|I2S_96000;
			PERI_WriteByte(GPIO_DATA_H, (PERI_ReadByte(GPIO_DATA_H) | bmBIT0) & ~bmBIT1);	//GPIO 8.9 use to select ADC CS5381 sampling rate range 01: 50KHz ~ 108KHz
			break;

		case DMA_176400:
			g_TempWord1 = BCLK_LRCK_64|I2S_MASTER|MCLK_LRCK_128|I2S_MODE|I2S_176400;
			PERI_WriteByte(GPIO_DATA_H, (PERI_ReadByte(GPIO_DATA_H) & ~bmBIT0) | bmBIT1);	//GPIO 8.9 use to select ADC CS5381 sampling rate range 10: 100KHz ~ 192KHz
			break;

		case DMA_192000:
			g_TempWord1 = BCLK_LRCK_64|I2S_MASTER|MCLK_LRCK_128|I2S_MODE|I2S_192000;
			PERI_WriteByte(GPIO_DATA_H, (PERI_ReadByte(GPIO_DATA_H) & ~bmBIT0) | bmBIT1);	//GPIO 8.9 use to select ADC CS5381 sampling rate range 10: 100KHz ~ 192KHz
			break;

		default:
			return FALSE;
	}

	// Set bit resolution
	switch(format & RESOLUTION_MASK)
	{
		case DMA_16Bit:
			g_TempWord1 |= I2S_16Bit;
			g_TempWord2 *= 4;
			break;

		case DMA_24Bit:
			g_TempWord1 |= I2S_24Bit;
			g_TempWord2 *= 6;
			break;

		case DMA_24Bit_32Container:
			g_TempWord1 |= I2S_24Bit;
			g_TempWord2 *= 8;
			break;

		case DMA_32Bit:
			g_TempWord1 |= I2S_32Bit;
			g_TempWord2 *= 8;
	}

	PERI_WriteByte(I2S_REC_8CH_H, PERI_ReadByte(I2S_REC_8CH_H) | bmBIT4); // Mute MCLK
	PERI_WriteWord(I2S_REC_8CH_L, g_TempWord1|MCLK_MUTE);	// Set I2S format
	PERI_WriteByte(I2S_REC_8CH_H, PERI_ReadByte(I2S_REC_8CH_H) & ~bmBIT4); // Unmute MCLK

	PERI_WriteByte(DMA_REC_8CH_H, ch);	// Set channel numbers
	switch(ch)
	{
		case DMA_4CH:
			g_TempWord2 *= 2;
			break;

		case DMA_6CH:
			g_TempWord2 *= 3;
			break;		

		case DMA_8CH:
			g_TempWord2 *= 4;
	}

	PERI_WriteWord(MAX_DATA_REC_8CH_L, g_TempWord2);	

	PERI_WriteByte(DMA_REC_8CH_L, PERI_ReadByte(DMA_REC_8CH_L)|DMA_START);
	return TRUE;
}

// Control CS5346
BOOL RecTwoChStart(BYTE format)
{
	g_TempByte1 = GetDmaFreq(EP_TWO_CH_REC);
	PERI_WriteByte(DMA_REC_2CH, format|g_TempByte1);

	g_TempWord2 = GetSamplingRate(g_TempByte1);

	if(g_IsAudioClass20 && g_UsbIsHighSpeed)
		g_TempWord2 = (g_TempWord2/8)+2;
	else
		g_TempWord2 = g_TempWord2+2;

	// Set sampling rate
	switch(g_TempByte1)
	{
		case DMA_8000:
			g_TempByte1 = bmBIT4;	// Single speed
			g_TempByte2 = 0;
			g_TempWord1 = BCLK_LRCK_64|I2S_MASTER|MCLK_LRCK_256|I2S_MODE|I2S_8000;
			break;

		case DMA_16000:
			g_TempByte1 = bmBIT4;	// Single speed
			g_TempByte2 = 0;
			g_TempWord1 = BCLK_LRCK_64|I2S_MASTER|MCLK_LRCK_256|I2S_MODE|I2S_16000;
			break;

		case DMA_32000:
			g_TempByte1 = bmBIT4;	// Single speed
			g_TempByte2 = 0;
			g_TempWord1 = BCLK_LRCK_64|I2S_MASTER|MCLK_LRCK_256|I2S_MODE|I2S_32000;
			break;

		case DMA_44100:
			g_TempByte1 = bmBIT4;	// Single speed
			g_TempByte2 = 0;
			g_TempWord1 = BCLK_LRCK_64|I2S_MASTER|MCLK_LRCK_256|I2S_MODE|I2S_44100;
			break;

		case DMA_48000:
			g_TempByte1 = bmBIT4;	// Single speed
			g_TempByte2 = 0;
			g_TempWord1 = BCLK_LRCK_64|I2S_MASTER|MCLK_LRCK_256|I2S_MODE|I2S_48000;
			break;

		case DMA_64000:
			g_TempByte1 = bmBIT4|bmBIT6;	// Double speed
			g_TempByte2 = 0;
			g_TempWord1 = BCLK_LRCK_64|I2S_MASTER|MCLK_LRCK_128|I2S_MODE|I2S_64000;
			break;

		case DMA_88200:
			g_TempByte1 = bmBIT4|bmBIT6;	// Double speed
			g_TempByte2 = 0;
			g_TempWord1 = BCLK_LRCK_64|I2S_MASTER|MCLK_LRCK_128|I2S_MODE|I2S_88200;
			break;

		case DMA_96000:
			g_TempByte1 = bmBIT4|bmBIT6;	// Double speed
			g_TempByte2 = 0;
			g_TempWord1 = BCLK_LRCK_64|I2S_MASTER|MCLK_LRCK_128|I2S_MODE|I2S_96000;
			break;

		case DMA_176400:
			g_TempByte1 = bmBIT4|bmBIT7;	// Quad speed
			g_TempByte2 = 0x20;	// Mclk/2
			g_TempWord1 = BCLK_LRCK_64|I2S_MASTER|MCLK_LRCK_128|I2S_MODE|I2S_176400;
			break;

		case DMA_192000:
			g_TempByte1 = bmBIT4|bmBIT7;	// Quad speed
			g_TempByte2 = 0x20;	// Mclk/2
			g_TempWord1 = BCLK_LRCK_64|I2S_MASTER|MCLK_LRCK_128|I2S_MODE|I2S_192000;
			break;

		default:
			return FALSE;
	}

	I2cMasterWrite(I2C_ADDR_5346, 4, &g_TempByte1, 1);
	I2cMasterWrite(I2C_ADDR_5346, 5, &g_TempByte2, 1);

	// Set bit resolution
	switch(format & RESOLUTION_MASK)
	{
		case DMA_16Bit:
			g_TempWord1 |= I2S_16Bit;
			g_TempWord2 *= 4;
			break;

		case DMA_24Bit:
			g_TempWord1 |= I2S_24Bit;
			g_TempWord2 *= 6;
			break;

		case DMA_24Bit_32Container:
			g_TempWord1 |= I2S_24Bit;
			g_TempWord2 *= 8;
			break;

		case DMA_32Bit:
			g_TempWord1 |= I2S_32Bit;
			g_TempWord2 *= 8;
	}

	PERI_WriteByte(I2S_REC_2CH_H, PERI_ReadByte(I2S_REC_2CH_H) | bmBIT4); // Mute MCLK
	PERI_WriteWord(I2S_REC_2CH_L, g_TempWord1|MCLK_MUTE);	// Set I2S format
	PERI_WriteByte(I2S_REC_2CH_H, PERI_ReadByte(I2S_REC_2CH_H) & ~bmBIT4); // Unmute MCLK

	PERI_WriteWord(MAX_DATA_REC_2CH_L, g_TempWord2);	

	PERI_WriteByte(DMA_REC_2CH, PERI_ReadByte(DMA_REC_2CH)|DMA_START);
	return TRUE;
}

BOOL RecSpdifStart(BYTE format)
{
	g_TempByte1 = GetDmaFreq(EP_SPDIF_REC);
	PERI_WriteByte(DMA_REC_SPDIF, format|g_TempByte1);

	g_TempWord2 = GetSamplingRate(g_TempByte1);

	if(g_SpdifLockStatus && (g_TempByte1==g_SpdifRateStatus))
	{
		PERI_WriteByte(RECORD_ROUTING_L, PERI_ReadByte(RECORD_ROUTING_L) & ~bmBIT3);
	}
	else
	{
		PERI_WriteByte(RECORD_ROUTING_L, PERI_ReadByte(RECORD_ROUTING_L) | bmBIT3);
	}

	if(g_IsAudioClass20 && g_UsbIsHighSpeed)
		g_TempWord2 = (g_TempWord2/8)+2;
	else
		g_TempWord2 = g_TempWord2+2;

	// Set sampling rate
	switch(g_TempByte1)
	{
		case DMA_32000:
			g_TempWord1 = BCLK_LRCK_128|I2S_MASTER|MCLK_LRCK_256|I2S_MODE|I2S_32000;
			break;

		case DMA_44100:
			g_TempWord1 = BCLK_LRCK_128|I2S_MASTER|MCLK_LRCK_256|I2S_MODE|I2S_44100;
			break;

		case DMA_48000:
			g_TempWord1 = BCLK_LRCK_128|I2S_MASTER|MCLK_LRCK_256|I2S_MODE|I2S_48000;
			break;

		case DMA_64000:
			g_TempWord1 = BCLK_LRCK_128|I2S_MASTER|MCLK_LRCK_256|I2S_MODE|I2S_64000;
			break;

		case DMA_88200:
			g_TempWord1 = BCLK_LRCK_64|I2S_MASTER|MCLK_LRCK_256|I2S_MODE|I2S_88200;
			break;

		case DMA_96000:
			g_TempWord1 = BCLK_LRCK_64|I2S_MASTER|MCLK_LRCK_256|I2S_MODE|I2S_96000;
			break;

		case DMA_176400:
			g_TempWord1 = BCLK_LRCK_64|I2S_MASTER|MCLK_LRCK_128|I2S_MODE|I2S_176400;
			break;

		case DMA_192000:
			g_TempWord1 = BCLK_LRCK_64|I2S_MASTER|MCLK_LRCK_128|I2S_MODE|I2S_192000;
			break;

		default:
			return FALSE;
	}

	// Set bit resolution
	switch(format & RESOLUTION_MASK)
	{
		case DMA_16Bit:
			g_TempWord1 |= I2S_16Bit;
			g_TempWord2 *= 4;
			break;

		case DMA_24Bit:
			g_TempWord1 |= I2S_24Bit;
			g_TempWord2 *= 6;
			break;

		case DMA_24Bit_32Container:
			g_TempWord1 |= I2S_24Bit;
			g_TempWord2 *= 8;
			break;

		case DMA_32Bit:
			g_TempWord1 |= I2S_32Bit;
			g_TempWord2 *= 8;
	}

	PERI_WriteByte(I2S_REC_SPDIF_H, PERI_ReadByte(I2S_REC_SPDIF_H) | bmBIT4); // Mute MCLK
	PERI_WriteWord(I2S_REC_SPDIF_L, g_TempWord1|MCLK_MUTE);	// Set I2S format
	PERI_WriteByte(I2S_REC_SPDIF_H, PERI_ReadByte(I2S_REC_SPDIF_H) & ~bmBIT4); // Unmute MCLK

	PERI_WriteWord(MAX_DATA_REC_SPDIF_L, g_TempWord2);

	PERI_WriteByte(SPDIF_IN_STATUS_0, 1);
	PERI_WriteByte(DMA_REC_SPDIF, PERI_ReadByte(DMA_REC_SPDIF)|DMA_START);
	return TRUE;
}

BOOL PlayMultiChStop()
{
	PERI_WriteByte(DMA_PLAY_8CH_L, PERI_ReadByte(DMA_PLAY_8CH_L) & ~bmBIT0);
	PlaybackReleaseRef(PLAY_8CH);	

#ifdef _MCU_FEEDBACK_
	s_MultiChFeedbackStart = FALSE;
#endif

	return TRUE;
}

BOOL PlaySpdifStop()
{
	PERI_WriteByte(SPDIF_OUT0_STATUS_L, PERI_ReadByte(SPDIF_OUT0_STATUS_L) & ~bmBIT0);			

	PERI_WriteByte(DMA_PLAY_SPDIF, PERI_ReadByte(DMA_PLAY_SPDIF) & ~bmBIT0);
	PlaybackReleaseRef(PLAY_SPDIF);	

#ifdef _MCU_FEEDBACK_
	s_SpdifFeedbackStart = FALSE;
#endif

	return TRUE;
}

BOOL PlayTwoChStop()
{
	PERI_WriteByte(DMA_PLAY_2CH, PERI_ReadByte(DMA_PLAY_2CH) & ~bmBIT0);
	PlaybackReleaseRef(PLAY_2CH);	

#ifdef _MCU_FEEDBACK_
	s_TwoChFeedbackStart = FALSE;
#endif

	return TRUE;
}

BOOL RecMultiChStop()
{
	PERI_WriteByte(DMA_REC_8CH_L, PERI_ReadByte(DMA_REC_8CH_L) & ~bmBIT0);
	return TRUE;
}

BOOL RecTwoChStop()
{
	PERI_WriteByte(DMA_REC_2CH, PERI_ReadByte(DMA_REC_2CH) & ~bmBIT0);
	return TRUE;
}

BOOL RecSpdifStop()
{
	PERI_WriteByte(SPDIF_IN_STATUS_0, 0);
	PERI_WriteByte(DMA_REC_SPDIF, PERI_ReadByte(DMA_REC_SPDIF) & ~bmBIT0);
	return TRUE;
}

void AudioStop()
{
	PlayMultiChStop();
	PlaySpdifStop();
	PlayTwoChStop();
	RecMultiChStop();
	RecTwoChStop();
	RecSpdifStop();

	PlaybackResetRef();

/*#ifdef _EXT_CLK_SI5351_
	ClockGenDisable();
#endif*/
}

