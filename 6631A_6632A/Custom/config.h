#ifndef _CONFIG_H_
#define _CONFIG_H_

//-----------------------------------------------------------------------------
// Configuration Switch
//-----------------------------------------------------------------------------
//#define _MIDI_

//#define _FULL_SPEED_ONLY_
//#define _AUDIO_CLASS_20_ONLY_
//#define _I2C_SLAVE_SUPPORT_

//-----------------------------------------------------------------------------
// IDs in Descriptor
//-----------------------------------------------------------------------------

#ifdef _MODEL_CM6601A_

#elif defined(_MODEL_CM6632A_)

	#define _FEEDBACK_
	#define _MCU_FEEDBACK_
	#define _EXT_OSC_22_24_
	//#define _EXT_OSC_45_49_

#else	// CM6631A

	#define _FEEDBACK_
	#define _MCU_FEEDBACK_
	//#define _EXT_OSC_22_24_
	#define _EXT_OSC_45_49_

	#define _EXT_CLK_SI5351_

#endif


#define VENDOR_ID 0x8C0D
#define PRODUCT_ID 0x1503

#define VERSION_ID 0x0413

#endif

