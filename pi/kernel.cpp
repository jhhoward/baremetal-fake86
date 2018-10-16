//
// kernel.cpp
//
// Circle - A C++ bare metal environment for Raspberry Pi
// Copyright (C) 2014-2015  R. Stange <rsta2@o2online.de>
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
#include "kernel.h"
#include <circle/usb/usbkeyboard.h>
#include <circle/string.h>
#include "../src/fake86/fake86.h"
#include "../src/fake86/video.h"

#define PARTITION	"emmc1-1"
#define FILENAME	"circle.txt"

static const char FromKernel[] = "kernel";
CKernel *CKernel::s_pThis = 0;
u8 CKernel::s_InputBuffer[6];

CKernel::CKernel (void)
:	m_Screen (256, 256, true),//(m_Options.GetWidth (), m_Options.GetHeight (), true),
	m_Timer (&m_Interrupt),
	m_Logger (m_Options.GetLogLevel (), &m_Timer),
	m_DWHCI (&m_Interrupt, &m_Timer)
	//m_EMMC (&m_Interrupt, &m_Timer, &m_ActLED)
{
	s_pThis = this;
	m_ActLED.Blink (5);	// show we are alive
}

CKernel::~CKernel (void)
{
}

void log(const char* message, ...)
{
	va_list myargs;
	va_start(myargs, message);
	CLogger::Get()->WriteV(FromKernel, LogNotice, message, myargs);
	va_end(myargs);
}

boolean CKernel::Initialize (void)
{
	boolean bOK = TRUE;
	
	m_pFrameBuffer = new CBcmFrameBuffer (OUTPUT_DISPLAY_WIDTH, OUTPUT_DISPLAY_HEIGHT, 8);
    
	m_pFrameBuffer->SetPalette (0, 0x0000);  // black
	m_pFrameBuffer->SetPalette (1, 0x0010);  // blue
	m_pFrameBuffer->SetPalette (2, 0x8000);  // red
	m_pFrameBuffer->SetPalette (3, 0x8010);  // magenta
	m_pFrameBuffer->SetPalette (4, 0x0400);  // green
	m_pFrameBuffer->SetPalette (5, 0x0410);  // cyan
	m_pFrameBuffer->SetPalette (6, 0x8400);  // yellow
	m_pFrameBuffer->SetPalette (7, 0x8410);  // white
	m_pFrameBuffer->SetPalette (8, 0x0000);  // black
	m_pFrameBuffer->SetPalette (9, 0x001F);  // bright blue
	m_pFrameBuffer->SetPalette (10, 0xF800); // bright red
	m_pFrameBuffer->SetPalette (11, 0xF81F); // bright magenta
	m_pFrameBuffer->SetPalette (12, 0x07E0); // bright green
	m_pFrameBuffer->SetPalette (13, 0x07FF); // bright cyan
	m_pFrameBuffer->SetPalette (14, 0xFFE0); // bright yellow
	m_pFrameBuffer->SetPalette (15, 0xFFFF); // bright white

	if (!m_pFrameBuffer->Initialize()) {
		return FALSE;
	}
	
	if (bOK)
	{
		bOK = m_Screen.Initialize ();
	}

	if (bOK)
	{
		bOK = m_Serial.Initialize (115200);
	}

	if (bOK)
	{
		CDevice *pTarget = m_DeviceNameService.GetDevice (m_Options.GetLogDevice (), FALSE);
		if (pTarget == 0)
		{
			//pTarget = &m_Screen;
		}

		bOK = m_Logger.Initialize (pTarget);
	}

	if (bOK)
	{
		bOK = m_Interrupt.Initialize ();
	}

	if (bOK)
	{
		bOK = m_Timer.Initialize ();
	}

	if (bOK)
	{
		//bOK = m_EMMC.Initialize ();
	}
	
	initfake86();

	return bOK;
}

TShutdownMode CKernel::Run (void)
{
	m_Logger.Write (FromKernel, LogNotice, "Compile time: " __DATE__ " " __TIME__);
	/*
	m_Logger.Write (FromKernel, LogNotice, "Compile time: " __DATE__ " " __TIME__);

	// Mount file system
	CDevice *pPartition = m_DeviceNameService.GetDevice (PARTITION, TRUE);
	if (pPartition == 0)
	{
		m_Logger.Write (FromKernel, LogPanic, "Partition not found: %s", PARTITION);
	}

	if (!m_FileSystem.Mount (pPartition))
	{
		m_Logger.Write (FromKernel, LogPanic, "Cannot mount partition: %s", PARTITION);
	}

	// Show contents of root directory
	TDirentry Direntry;
	TFindCurrentEntry CurrentEntry;
	unsigned nEntry = m_FileSystem.RootFindFirst (&Direntry, &CurrentEntry);
	for (unsigned i = 0; nEntry != 0; i++)
	{
		if (!(Direntry.nAttributes & FS_ATTRIB_SYSTEM))
		{
			CString FileName;
			FileName.Format ("%-14s", Direntry.chTitle);

			m_Screen.Write ((const char *) FileName, FileName.GetLength ());

			if (i % 5 == 4)
			{
				m_Screen.Write ("\n", 1);
			}
		}

		nEntry = m_FileSystem.RootFindNext (&Direntry, &CurrentEntry);
	}
	m_Screen.Write ("\n", 1);

	// Create file and write to it
	unsigned hFile = m_FileSystem.FileCreate (FILENAME);
	if (hFile == 0)
	{
		m_Logger.Write (FromKernel, LogPanic, "Cannot create file: %s", FILENAME);
	}

	for (unsigned nLine = 1; nLine <= 5; nLine++)
	{
		CString Msg;
		Msg.Format ("Hello File! (Line %u)\n", nLine);

		if (m_FileSystem.FileWrite (hFile, (const char *) Msg, Msg.GetLength ()) != Msg.GetLength ())
		{
			m_Logger.Write (FromKernel, LogError, "Write error");
			break;
		}
	}

	if (!m_FileSystem.FileClose (hFile))
	{
		m_Logger.Write (FromKernel, LogPanic, "Cannot close file");
	}

	// Reopen file, read it and display its contents
	hFile = m_FileSystem.FileOpen (FILENAME);
	if (hFile == 0)
	{
		m_Logger.Write (FromKernel, LogPanic, "Cannot open file: %s", FILENAME);
	}

	char Buffer[100];
	unsigned nResult;
	while ((nResult = m_FileSystem.FileRead (hFile, Buffer, sizeof Buffer)) > 0)
	{
		if (nResult == FS_ERROR)
		{
			m_Logger.Write (FromKernel, LogError, "Read error");
			break;
		}

		m_Screen.Write (Buffer, nResult);
	}
	
	if (!m_FileSystem.FileClose (hFile))
	{
		m_Logger.Write (FromKernel, LogPanic, "Cannot close file");
	}*/

	CUSBKeyboardDevice *pKeyboard = (CUSBKeyboardDevice *) m_DeviceNameService.GetDevice ("ukbd1", FALSE);
	
	if(pKeyboard)
	{
		//pKeyboard->RegisterKeyStatusHandlerRaw (KeyStatusHandlerRaw);
			pKeyboard->RegisterKeyPressedHandler (KeyPressedHandler);

	}
	
	char screentext[80*25+1];
	int cursorX, cursorY;
	int firstRow = 5;
	int count = 0;
	
	while(simulatefake86())
	{
		if(pKeyboard)
			pKeyboard->UpdateLEDs ();

		uint32_t* palettePtr;
		int paletteSize;
		
		getactivepalette((uint8_t**)&palettePtr, &paletteSize);
		
		for(int n = 0; n < paletteSize; n++)
		{
			m_pFrameBuffer->SetPalette (n, palettePtr[n]);
		}
		m_pFrameBuffer->UpdatePalette();
		
		drawfake86((uint8_t*) m_pFrameBuffer->GetBuffer());
		//m_Screen.CursorMove(firstRow, 0);
		/*count++;
		
		if(count >= 100)
		{
			dumptextscreen(screentext, &cursorX, &cursorY);
			//m_Screen.CursorMove(firstRow + cursorY, cursorX);
			m_Logger.Write(FromKernel, LogNotice, screentext);
			count = 0;
			m_Timer.MsDelay (100);
		}*/
		
		for(int n = 0; n < 6; n++)
		{
			if(s_InputBuffer[n] != 0)
			{
				handlekeydown(s_InputBuffer[n]);
				s_InputBuffer[n] = 0;
			}
		}
	}

	return ShutdownHalt;
}

void CKernel::KeyStatusHandlerRaw (unsigned char ucModifiers, const unsigned char RawKeys[6])
{
	for(int n = 0; n < 6; n++)
	{
		s_InputBuffer[n] = RawKeys[n];
	}
	//assert (s_pThis != 0);
    //
	//CString Message;
	//Message.Format ("Key status (modifiers %02X)", (unsigned) ucModifiers);
    //
	//for (unsigned i = 0; i < 6; i++)
	//{
	//	if (RawKeys[i] != 0)
	//	{
	//		CString KeyCode;
	//		KeyCode.Format (" %02X", (unsigned) RawKeys[i]);
    //
	//		Message.Append (KeyCode);
	//	}
	//}

}

void CKernel::KeyPressedHandler (const char *pString)
{
	//assert (s_pThis != 0);
	for(int n = 0; n < 6; n++)
	{
		if(pString[n])
		{
			s_InputBuffer[n] = pString[n];
		}
	}
	//s_pThis->m_Screen.Write (pString, strlen (pString));
}

