#include <string.h>

#include "SDCard.h"
#include "Emulator.h"
#include "Keyboard/ps2Keyboard.h"
#include "Emulator/z80snapshot.h"

using namespace zx;

#define FILE_COLUMNS 3

uint8_t _fileColumnWidth = DEBUG_COLUMNS / FILE_COLUMNS;
uint8_t _selectedFile = 0;
uint8_t _fileCount;

typedef TCHAR FileName[_MAX_LFN + 1];
FileName* _fileNames = (FileName*) _buffer16K_2;

bool _loadingSnapshot = false;

void GetFileCoord(uint8_t fileIndex, uint8_t* x, uint8_t* y)
{
	*x = fileIndex / (DEBUG_ROWS - 1);
	*y = 1 + fileIndex % (DEBUG_ROWS - 1);
}

TCHAR* TruncateFileName(TCHAR* fileName)
{
	int maxLength = DEBUG_COLUMNS / FILE_COLUMNS - 1;
	TCHAR* result = (TCHAR*) _buffer16K_1;
	strncpy(result, fileName, maxLength);

	result[maxLength - 1] = '\0';
	TCHAR* extension = strrchr(result, '.');
	if (extension != nullptr)
	{
		*extension = '\0';
	}

	return result;
}

void SetSelection(uint8_t selectedFile)
{
	if (_fileCount == 0)
	{
		return;
	}

	_selectedFile = selectedFile;

	uint8_t x, y;
	GetFileCoord(selectedFile, &x, &y);
	DebugScreen.PrintAt(x, y, "\x10"); // ►

	FRESULT fr;

	// Show screenshot for the selected file
	fr = f_mount(&SDFatFS, (TCHAR const*) SDPath, 1);
	if (fr == FR_OK)
	{
		FIL file;
		bool scrFileFound = false;

		TCHAR* fileName = _fileNames[selectedFile];

		// Try to open file with the same name and .SCR extension
		TCHAR scrFileName[_MAX_LFN + 1];
		strncpy(scrFileName, fileName, _MAX_LFN + 1);
		TCHAR* extension = strrchr(scrFileName, '.');
		if (extension != nullptr)
		{
			strncpy(extension, ".scr", 4);
			fr = f_open(&file, fileName, FA_READ | FA_OPEN_EXISTING);
			if (fr == FR_OK)
			{
				LoadScreenshot(&file, _buffer16K_1);
				f_close(&file);
				scrFileFound = true;
			}
		}

		if (!scrFileFound)
		{
			fr = f_open(&file, fileName, FA_READ | FA_OPEN_EXISTING);
			if (fr == FR_OK)
			{
				LoadScreenFromZ80Snapshot(&file, _buffer16K_1);
				f_close(&file);
			}
		}

		f_mount(&SDFatFS, (TCHAR const*) SDPath, 0);
	}
}

void loadSnapshot(const TCHAR* fileName)
{
	FRESULT fr = f_mount(&SDFatFS, (TCHAR const*) SDPath, 1);
	if (fr == FR_OK)
	{
		FIL file;
		fr = f_open(&file, fileName, FA_READ | FA_OPEN_EXISTING);
		LoadZ80Snapshot(&file, _buffer16K_1, _buffer16K_2);
		f_close(&file);

		f_mount(&SDFatFS, (TCHAR const*) SDPath, 0);
	}
}

bool loadSnapshotSetup()
{
	DebugScreen.SetAttribute(0x3F10); // white on blue
	DebugScreen.Clear();

	showTitle("Load snapshot. ENTER, ESC, \x18, \x19, \x1A, \x1B"); // ↑, ↓, →, ←

	FRESULT fr = f_mount(&SDFatFS, (TCHAR const*) SDPath, 1);
	if (fr != FR_OK)
	{
		return false;
	}

	DIR folder;
	FILINFO fileInfo;
	uint8_t maxFileCount = (DEBUG_ROWS - 1) * FILE_COLUMNS;
	_fileCount = 0;
	bool result = true;

	fr = f_findfirst(&folder, &fileInfo, (TCHAR const*) "/",
			(TCHAR const*) "*.z80");

	if (fr == FR_OK)
	{
		for (int fileIndex = 0; fileIndex < maxFileCount && fileInfo.fname[0];
				fileIndex++)
		{
			strncpy(_fileNames[fileIndex], fileInfo.fname, _MAX_LFN + 1);
			_fileCount++;

			fr = f_findnext(&folder, &fileInfo);
			if (fr != FR_OK)
			{
				result = false;
				break;
			}
		}
	}
	else
	{
		result = false;
	}

	for (int y = 1; y < DEBUG_ROWS; y++)
	{
		DebugScreen.PrintAt(_fileColumnWidth, y, "\xB3"); // │
		DebugScreen.PrintAt(_fileColumnWidth * 2 + 1, y, "\xB3"); // │
	}

	uint8_t x, y;
	for (int fileIndex = 0; fileIndex < _fileCount; fileIndex++)
	{
		GetFileCoord(fileIndex, &x, &y);
		DebugScreen.PrintAt(x + 1, y, TruncateFileName(_fileNames[fileIndex]));
	}

	SetSelection(_selectedFile);

	// Unmount file system
	f_mount(&SDFatFS, (TCHAR const*) SDPath, 0);

	if (result)
	{
		_loadingSnapshot = true;
	}

	return result;
}

bool loadSnapshotLoop()
{
	if (!_loadingSnapshot)
	{
		return false;
	}

	int32_t scanCode = Ps2_GetScancode();
	if (scanCode == 0 || (scanCode & 0xFF00) != 0xF000)
	{
		return true;
	}

	uint8_t previousSelection = _selectedFile;

	scanCode = ((scanCode & 0xFF0000) >> 8 | (scanCode & 0xFF));
	switch (scanCode)
	{
	case KEY_UPARROW:
		if (_selectedFile > 0)
		{
			_selectedFile--;
		}
		break;

	case KEY_DOWNARROW:
		if (_selectedFile < _fileCount)
		{
			_selectedFile++;
		}
		break;

	case KEY_ENTER:
	case KEY_KP_ENTER:
		loadSnapshot(_fileNames[_selectedFile]);
		_loadingSnapshot = false;
		return false;

	case KEY_ESC:
		_loadingSnapshot = false;
		return false;
	}

	if (previousSelection == _selectedFile)
	{
		return true;
	}

	uint8_t x, y;
	GetFileCoord(previousSelection, &x, &y);
	DebugScreen.PrintAt(x, y, " ");

	SetSelection(_selectedFile);

	return true;
}
