#ifndef __EMULATOR_H__
#define __EMULATOR_H__

#include "Display/Screen.h"
#include "Emulator/SpectrumScreen.h"
#include "fatfs.h"

using namespace Display;

#define DEBUG_COLUMNS 50
#define DEBUG_ROWS 8

extern Screen DebugScreen;
extern SpectrumScreen MainScreen;

void initializeVideo();
void startVideo();

#endif /* __EMULATOR_H__ */
