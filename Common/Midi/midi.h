#ifndef _MIDI_H_
#define _MIDI_H_

#include "usb.h"

//-----------------------------------------------------------------------------
// Public Variable
//-----------------------------------------------------------------------------
extern USB_INTERFACE code g_InterfaceMidiCtrl;
extern USB_INTERFACE code g_InterfaceMidiStream;

//-----------------------------------------------------------------------------
// Public Function
//-----------------------------------------------------------------------------
extern void MidiInit();
extern void MidiProcess();

#endif

