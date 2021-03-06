// TestSerial.cpp : main project file.
#include "stdafx.h"
#include <stdio.h>
#include <stdint.h>
#include <windows.h>

HANDLE hCom;
DCB dcb;
BOOL ConnectState;
int err, delta_ptr, com_ptr, graph_ptr, buffsize;

LPSTR MyByte;
int pc;
OVERLAPPED ReadOS, WriteOS;
BOOL errflag;
COMMTIMEOUTS TimeOuts;
int packs, fatal, rxerror, breaks, overruns, rxovers, rxparities, frames,
pends, overbuff;

char* _Input;
uint16_t _InputSize = 10;
char* _Output;
uint16_t _OutputSize = 10;


BOOL NEAR WriteCommBlock(LPSTR lpByte, DWORD dwBytesToWrite)
{
	BOOL        fWriteStat;
	DWORD       dwBytesWritten;
	DWORD       dwErrorFlags;
	DWORD	   dwError;
	//DWORD       dwBytesSent=0;
	COMSTAT     ComStat;

	fWriteStat = WriteFile(hCom, lpByte, dwBytesToWrite,
		&dwBytesWritten, NULL);

	// Note that normally the code will not execute the following
	// because the driver caches write operations. Small I/O requests
	// (up to several thousand bytes) will normally be accepted
	// immediately and WriteFile will return true even though an
	// overlapped operation was specified

	if (!fWriteStat)
	{
		if (GetLastError() == ERROR_IO_PENDING)
		{
			//Form1->Memo3->Lines->Add("Pending");
			// ErrorInformation();
			// We should wait for the completion of the write operation
			// so we know if it worked or not

			// This is only one way to do this. It might be beneficial to
			// place the write operation in a separate thread
			// so that blocking on completion will not negatively
			// affect the responsiveness of the UI

			// If the write takes too long to complete, this
			// function will timeout according to the
			// CommTimeOuts.WriteTotalTimeoutMultiplier variable.
			// This code logs the timeout but does not retry
			// the write.

			while (!GetOverlappedResult(hCom, &WriteOS, &dwBytesWritten, TRUE))
			{
				dwError = GetLastError();
				if (dwError == ERROR_IO_INCOMPLETE)
				{
					// normal result if not finished
					//dwBytesSent += dwBytesWritten;
					continue;
				}
				else
				{
					// an error occurred, try to recover
					//ErrorInformation();
					ClearCommError(hCom, &dwErrorFlags, &ComStat);
					if (dwErrorFlags > 0)
					{

					}
					break;
				}
			}

			//dwBytesSent += dwBytesWritten;

			// if(dwBytesSent != dwBytesToWrite);
		}
		else
		{
			// some other error occurred
			ClearCommError(hCom, &dwErrorFlags, &ComStat);
			if (dwErrorFlags > 0)
			{

			}
			return (FALSE);
		}
	}
	return (TRUE);

} // end of WriteCommBlock()
int NEAR ReadCommBlock(LPSTR lpszBlock, int nMaxLength)
{
	BOOL       fReadStat;
	COMSTAT    ComStat;
	DWORD      dwErrorFlags;
	DWORD      dwLength;
	DWORD      dwError;

	// only try to read number of bytes in queue
	ClearCommError(hCom, &dwErrorFlags, &ComStat);
	if ((DWORD)nMaxLength <= ComStat.cbInQue)
		dwLength = nMaxLength;
	else
		dwLength = ComStat.cbInQue;

	buffsize = ComStat.cbInQue;
	if (buffsize>102390)
		overbuff++;
	if (dwLength > 0)
	{
		fReadStat = ReadFile(hCom, lpszBlock,
			dwLength, &dwLength, NULL);

		if (!fReadStat)
		{
			if (GetLastError() == ERROR_IO_PENDING)
			{
				pends++;
				while (!GetOverlappedResult(hCom, &ReadOS, &dwLength, TRUE))
				{
					dwError = GetLastError();
					if (dwError == ERROR_IO_INCOMPLETE)
						// normal result if not finished
						continue;
					else
					{
						// an error occurred, try to recover
						dwLength = 0;
						rxerror++;
						ClearCommError(hCom, &dwErrorFlags, &ComStat);
						if (dwErrorFlags > 0)
						{
							//DetectError(dwErrorFlags);
						}
						break;
					}

				}

			}
			else
			{
				// some other error occurred
				dwLength = 0;
				rxerror++;
				ClearCommError(hCom, &dwErrorFlags, &ComStat);
				if (dwErrorFlags > 0)
				{
					//DetectError(dwErrorFlags);
				}
			}
		}
	}

	return (dwLength);
}

int InitComPort(LPCTSTR MyPort, unsigned int baudrate, unsigned char databits, unsigned char parity, unsigned char stopbits)
{
	hCom = CreateFile(MyPort,
		GENERIC_READ | GENERIC_WRITE,
		0,	  /* comm devices must be opened w/exclusive-access */
		NULL, /* no security attrs */
		OPEN_EXISTING, /* comm devices must use OPEN_EXISTING */
		NULL,//FILE_FLAG_OVERLAPPED,//NULL,    /* overlapped I/O */
		NULL  /* hTemplate must be NULL for comm devices */
	);
	if (hCom == INVALID_HANDLE_VALUE)
	{
		return 1;
	}

	dcb.DCBlength = sizeof(DCB);

	if (!GetCommState(hCom, &dcb))
		return 1;

	dcb.BaudRate = baudrate;
	dcb.fBinary = true;
	dcb.fOutxCtsFlow = false;
	dcb.fOutxDsrFlow = FALSE; //выключаем режим слежения за сигналом DSR
	dcb.fDtrControl = DTR_CONTROL_DISABLE; //отключаем использование линии DTR
	dcb.fDsrSensitivity = FALSE; //отключаем восприимчивость драйвера к состоянию линии DSR
	dcb.fNull = FALSE; //разрешить приём нулевых байтов
	dcb.fRtsControl = RTS_CONTROL_DISABLE; //отключаем использование линии RTS
	dcb.fAbortOnError = FALSE; //отключаем остановку всех операций чтения/записи при ошибке
	dcb.ByteSize = databits;    //Number of bits/byte, 4-8
	dcb.Parity = parity;        //0-4=None,Odd,Even,Mark,Space
	dcb.StopBits = stopbits;    //0,1,2 = 1, 1.5, 2


	if (!SetCommState(hCom, &dcb))
		return 1;

	TimeOuts.ReadIntervalTimeout = 0;
	TimeOuts.ReadTotalTimeoutMultiplier = 0;
	TimeOuts.ReadTotalTimeoutConstant = 0;
	TimeOuts.WriteTotalTimeoutMultiplier = 0;
	TimeOuts.WriteTotalTimeoutConstant = 0;

	if (!SetCommTimeouts(hCom, &TimeOuts))
		return 1;

	if (!SetupComm(hCom, 102400, 102400))
	{
		return 1;
	}
	// purge any information in the buffer
	PurgeComm(hCom, PURGE_TXCLEAR | PURGE_RXCLEAR);

	// set up for overlapped I/O
	WriteOS.Offset = 0 ;
	WriteOS.OffsetHigh = 0 ;
	ReadOS.Offset = 0 ;
	ReadOS.OffsetHigh = 0 ;
	ConnectState = true;
	return 0;
}

unsigned char SendBuffer() { 
	uint16_t iTempBytes = 0;

	iTempBytes = WriteCommBlock(_Output, _OutputSize);
	printf("TX[%d]\n", iTempBytes); 

	return 0; 
}

unsigned char ReceiveBuffer() { 
	uint16_t iTempBytes = 0;
	//int iRepeats = 0;

	uint8_t curChar = 0x00;
	//uint8_t  packetSize = 0x00;
	//uint8_t  tempCRC = 0xFF;

	iTempBytes = ReadCommBlock(_Input, _InputSize);
	//if (iTempBytes != 0x00) {
	printf("RX[%d][", iTempBytes);
	for (uint8_t i = 0; i < _InputSize; ++i) {
		printf("%2hhX ", _Input[i]);
	}
	printf("]\n");
	
		//search synchronous byte 0x03
	//}

	return 0; 
}

DWORD WINAPI COMWriteData(void *Parameter)
{
	OVERLAPPED olOverlapped;
	DWORD iTemp, iSignal;
	bool fRes = false;
	olOverlapped.hEvent = CreateEvent(NULL, true, true, NULL);

	if (olOverlapped.hEvent == NULL) {
		// error creating overlapped event handle
		printf("error creating overlapped event handle\n");
		return FALSE;
	}

	while (true)
	{
		Sleep(50);
		if (SendBuffer() == 0)
		{
			if (GetLastError() != ERROR_IO_PENDING) {
				//printf("WriteFile failed, but isn't delayed. Report error and abort.\n");
				// WriteFile failed, but isn't delayed. Report error and abort.
				fRes = FALSE;
			}
			else {
				iSignal = WaitForSingleObject(olOverlapped.hEvent, INFINITE);
				switch (iSignal)
				{
					case WAIT_OBJECT_0:
						if (!GetOverlappedResult(hCom, &olOverlapped, &iTemp, true)) {
							fRes = false;
						}
						else {
							fRes = true;
						}
						break;
					default:
						fRes = false;
						break;
				}
			}
		}
		else
		{
			// WriteFile completed immediately.
			fRes = TRUE;
			printf("Write error\n");
		}
	}

	if (olOverlapped.hEvent != NULL) {
		CloseHandle(olOverlapped.hEvent);
	}

	return fRes;

	//fclose(FSBUFFER);
}

DWORD WINAPI COMReadData(void *Parameter)
{
	COMSTAT csStatus;
	OVERLAPPED olOverlapped;
	DWORD iMask, iSignal, iTemp, iAppearedBytes;

	olOverlapped.hEvent = CreateEvent(NULL, true, true, NULL);
	SetCommMask(hCom, EV_RXCHAR);
	while (true)
	{
		WaitCommEvent(hCom, &iMask, &olOverlapped);
		iSignal = WaitForSingleObject(olOverlapped.hEvent, INFINITE);
		if (iSignal == WAIT_OBJECT_0)
		{
			if (GetOverlappedResult(hCom, &olOverlapped, &iTemp, true))
			{
				if ((iMask & EV_RXCHAR) != 0)
				{
					ClearCommError(hCom, &iTemp, &csStatus);
					iAppearedBytes = csStatus.cbInQue;

						ReceiveBuffer();
						//memset(IsParamReady, 0x00, uParamsToSend * sizeof(flag));
				}
			}
		}

		Sleep(5);
	}

	return 0;
}

HANDLE hReadThread, hWriteThread;

int main()
{
	if (InitComPort(L"\\\\.\\COM2", 115200, 8, 0, 0)) {
		printf("Port Init ERROR\n");
		system("pause");
		return 0;
	}
	_Output = new char[_OutputSize];
	_Input = new char[_InputSize];

	memset(_Output, 0x00, _OutputSize);
	memset(_Input, 0x00, _InputSize);

	hReadThread = CreateThread(NULL, 0, COMReadData, 0, 0, NULL);
	hWriteThread = CreateThread( NULL, 0, COMWriteData, 0, 0, NULL );

	while (1) {};

	return 0;
}