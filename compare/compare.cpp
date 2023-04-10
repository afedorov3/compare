// compare.cpp : Defines the entry point for the console application.
//
#include <windows.h>

// C RunTime Header Files
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <tchar.h>
#ifdef _UNICODE
#include <io.h>
#include <fcntl.h>
#endif /* _UNICODE */

#include "compare.h"

VOID CALLBACK cbperctimer(PVOID lpParam, BOOLEAN TimerOrWaitFired)
{
	*(BOOL*)lpParam = TRUE;
}

int _tmain(int argc, _TCHAR* argv[])
{
	INT nRet = -(ERROR_GEN_FAILURE);
	INT i;
	BYTE *pBuf1 = NULL;
	BYTE *pBuf2 = NULL;
	LPCTSTR szFile1 = NULL;
	LPCTSTR szFile2 = NULL;
	LPCTSTR szOptarg = NULL;
	TCHAR uOp;
	HANDLE hFile1 = INVALID_HANDLE_VALUE;
	HANDLE hFile2 = INVALID_HANDLE_VALUE;
	HANDLE hIO[2];
	LARGE_INTEGER nCmpLen = { 0 };
	LARGE_INTEGER nCmpLen1 = { 0 };
	LARGE_INTEGER nCmpLen2 = { 0 };
	LARGE_INTEGER nRead = { 0 };
	LARGE_INTEGER nOffset1 = { 0 };
	LARGE_INTEGER nOffset2 = { 0 };
	LARGE_INTEGER nSize1 = { 0 };
	LARGE_INTEGER nSize2 = { 0 };
	LARGE_INTEGER nDiffs = { 0 };
	BOOL bPrintPerc = TRUE;
	HANDLE perctimer = INVALID_HANDLE_VALUE;
	const DWORD percinterval = 1000 / 10; // 10 per second
	BOOL bWholeFile = FALSE;
	BOOL bComplete = FALSE;
	DWORD dwErr = ERROR_SUCCESS;
	DWORD dwRead1, dwRead2, dwIndex;
	OVERLAPPED ovl1, ovl2;
	SYSTEMTIME t1, t2;

	bPrintMap = FALSE;
	bQuiet = FALSE;
	ZeroMemory(&ovl1, sizeof(OVERLAPPED));
	ZeroMemory(&ovl2, sizeof(OVERLAPPED));
	nPercCur = nPercPrev = -1.0;
	nRangeStart.QuadPart = nRangeEnd.QuadPart = -1LL;
	bBR = FALSE;

	argv++;
	argc--;
#ifdef _UNICODE
	_setmode(_fileno(stdout), _O_WTEXT);
#endif /* _UNICODE */

	for ( i = 0; i < argc; i++ ) {
		if (argv[i]) {
			if ( *argv[i] == _T('-') || *argv[i] == _T('/') ) {
				uOp = tolower(argv[i][1]);
				if (!uOp) {
					if (!bQuiet)
						printcmderror(argv, argc, i);
					return -ERROR_BAD_ARGUMENTS;
				}
				if (argv[i][2]) {
					if (!bQuiet)
						printcmderror(argv, argc, i);
					return -ERROR_BAD_ARGUMENTS;
				}
				if ((i < (argc - 1)) && argv[i+1])
					szOptarg = argv[i+1];
				else
					szOptarg = NULL;
				switch (uOp) {
					case _T('-'):
					case _T('/'):
						uOp = 0;
						break;
					case _T('h'):
					case _T('?'):
						usage();
						return ERROR_SUCCESS;
					case _T('q'):
						bQuiet = TRUE;
					case _T('p'):
						bPrintPerc = FALSE;
						break;
					case _T('m'):
						bPrintMap = TRUE;
						break;
					case _T('f'):
						nOffset1 = getnum(szOptarg);
						i++;
						if (nOffset1.QuadPart == -1LL) {
							if (!bQuiet)
								printcmderror(argv, argc, i);
							return -ERROR_BAD_ARGUMENTS;
						}
						break;
					case _T('l'):
						nCmpLen = getnum(szOptarg);
						i++;
						if (nCmpLen.QuadPart == -1LL) {
							if (!bQuiet)
								printcmderror(argv, argc, i);
							return -ERROR_BAD_ARGUMENTS;
						}
						break;
					case _T('s'):
						nOffset2 = getnum(szOptarg);
						i++;
						if (nOffset2.QuadPart == -1LL) {
							if (!bQuiet)
								printcmderror(argv, argc, i);
							return -ERROR_BAD_ARGUMENTS;
						}
						break;
					default:
						if (!bQuiet)
							printcmderror(argv, argc, i);
						return -ERROR_BAD_ARGUMENTS;
				}
				if (!uOp) {
					i++;
					break;
				}
			} else
				break;
		}
	}

	if (i >= (argc - 1)) {
		usage();
		return -ERROR_BAD_ARGUMENTS;
	}

	szFile1 = argv[i++];
	szFile2 = argv[i++];

	hFile1 = CreateFile(szFile1, GENERIC_READ, FILE_SHARE_READ, NULL,
		OPEN_EXISTING, FILE_FLAG_OVERLAPPED|FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (hFile1 == INVALID_HANDLE_VALUE) {
		dwErr = GetLastError();
		if (!bQuiet) {
			_ftprintf_s(stderr, _T("Error opening file1: "));
			printerror(dwErr);
		}
		goto cleanup;
	}
	hFile2 = CreateFile(szFile2, GENERIC_READ, FILE_SHARE_READ, NULL,
		OPEN_EXISTING, FILE_FLAG_OVERLAPPED|FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (hFile2 == INVALID_HANDLE_VALUE) {
		dwErr = GetLastError();
		if (!bQuiet) {
			_ftprintf_s(stderr, _T("Error opening file2: "));
			printerror(dwErr);
		}
		goto cleanup;
	}

	pBuf1 = new BYTE[FBUFSIZ];
	pBuf2 = new BYTE[FBUFSIZ];
	if (pBuf1 == NULL || pBuf2 == NULL) {
		dwErr = GetLastError();
		if (!bQuiet) {
			_ftprintf_s(stderr, _T("Error allocating buffer: "));
			printerror(dwErr);
		}
		goto cleanup;
	}
	if (!GetFileSizeEx(hFile1, &nSize1)) {
		dwErr = GetLastError();
		if (!bQuiet) {
			_ftprintf_s(stderr, _T("File1 GetFileSizeEx error: "));
			printerror(dwErr);
		}
		goto cleanup;
	}
	if (!GetFileSizeEx(hFile2, &nSize2)) {
		dwErr = GetLastError();
		if (!bQuiet) {
			_ftprintf_s(stderr, _T("File2 GetFileSizeEx error: "));
			printerror(dwErr);
		}
		goto cleanup;
	}
	if (nOffset1.QuadPart == 0 && nOffset2.QuadPart == 0 && nCmpLen.QuadPart == 0)
		bWholeFile = TRUE;
	if (!bQuiet) {
		_tprintf_s(_T("Comparing %s:\n  %s,\n  size "),
			(bWholeFile?_T("files"):_T("fragments of")), szFile1);
		printsize(nSize1);
		_tprintf_s(_T(", offset 0x%llX (%llu bytes)\nand\n  %s,\n  size "), nOffset1.QuadPart,
			nOffset1.QuadPart, szFile2);
		printsize(nSize2);
		_tprintf_s(_T(", offset 0x%llX (%llu bytes)\n"), nOffset2.QuadPart, nOffset2.QuadPart);
	}
	if (nSize1.QuadPart == 0 && nSize2.QuadPart == 0) {
		if (!bQuiet)
			_tprintf_s(_T("Both files are empty, nothing to compare.\n"));
		nRet = 0;
		goto cleanup;
	} else if (nSize1.QuadPart == 0 || nSize2.QuadPart == 0) {
		if (!bQuiet)
			_tprintf_s(_T("%s is empty and other is not, files are different.\n"),
			(nSize1.QuadPart == 0?_T("File1"):_T("File2")));
		nRet = 1;
		goto cleanup;
	}
	if (nOffset1.QuadPart >= nSize1.QuadPart && nOffset2.QuadPart >= nSize2.QuadPart) {
		if (!bQuiet)
			_tprintf_s(_T("Both offsets are greater than files size, comparing from 0.\n"));
		nOffset1.QuadPart = 0;
		nOffset2.QuadPart = 0;
	} else if (nOffset1.QuadPart >= nSize1.QuadPart || nOffset2.QuadPart >= nSize2.QuadPart) {
		if (!bQuiet)
			_tprintf_s(_T("%s offset is greater than file size, %s are different.\n"),
			(nOffset1.QuadPart >= nSize1.QuadPart?_T("File1"):_T("File2")),
			(bWholeFile?_T("files"):_T("fragments")));
		nRet = 1;
		goto cleanup;
	}
	nCmpLen1.QuadPart = nSize1.QuadPart - nOffset1.QuadPart;
	nCmpLen2.QuadPart = nSize2.QuadPart - nOffset2.QuadPart;
	if (nCmpLen.QuadPart != 0) {
		if (nCmpLen.QuadPart > nCmpLen1.QuadPart &&
			nCmpLen.QuadPart > nCmpLen2.QuadPart) {
			nCmpLen.QuadPart = min(nCmpLen1.QuadPart, nCmpLen2.QuadPart);
			if (!bQuiet)
				_tprintf_s(_T("Comparing length is greater than both file sizes, comparing first %llu bytes.\n"), nCmpLen.QuadPart);
		} else if (nCmpLen.QuadPart > nCmpLen1.QuadPart ||
			nCmpLen.QuadPart > nCmpLen2.QuadPart) {
			if (!bQuiet)
				_tprintf_s(_T("Comparing length is greater than %s size, fragments are different.\n"),
					(nCmpLen.QuadPart > nCmpLen1.QuadPart)?_T("file1"):_T("file2"));
			return 1;
		} else if (!bQuiet)
			_tprintf_s(_T("Comparing first %llu bytes.\n"), nCmpLen.QuadPart);
	} else {
		nCmpLen.QuadPart = min(nCmpLen1.QuadPart, nCmpLen2.QuadPart);
		if (nCmpLen1.QuadPart != nCmpLen2.QuadPart) {
			if (!bQuiet)
				_tprintf_s(_T("%s have different size, comparing first %llu bytes.\n"),
					(bWholeFile?_T("Files"):_T("Fragments")), nCmpLen.QuadPart);
			bWholeFile = FALSE;
		}
	}

	hIO[0] = ovl1.hEvent = CreateEvent(NULL, TRUE, TRUE, NULL);
	hIO[1] = ovl2.hEvent = CreateEvent(NULL, TRUE, TRUE, NULL);
	if (ovl1.hEvent == NULL || ovl2.hEvent == NULL) {
		dwErr = GetLastError();
		if (!bQuiet) {
			_ftprintf_s(stderr, _T("CreateEvent error: "));
			printerror(dwErr);
		}
		goto cleanup;
	}

	if (!bQuiet && bPrintMap)
		_tprintf_s(_T("Differences map:\n"));

	ovl1.OffsetHigh = nOffset1.HighPart;
	ovl1.Offset = nOffset1.LowPart;
	ovl2.OffsetHigh = nOffset2.HighPart;
	ovl2.Offset = nOffset2.LowPart;

	if (!bQuiet) {
		if (bPrintPerc && !CreateTimerQueueTimer(&perctimer, NULL, cbperctimer, &bPrintPerc, 0, percinterval, WT_EXECUTEINTIMERTHREAD)) {
			perctimer = INVALID_HANDLE_VALUE;
			bPrintPerc = FALSE;
		}
		GetSystemTime(&t1);
	}
	while (!bComplete) {
		ReadFile(hFile1, pBuf1, FBUFSIZ, NULL, &ovl1);
		dwErr = GetLastError();
		if (dwErr != ERROR_IO_PENDING) {
			if (!bQuiet) {
				_ftprintf_s(stderr, _T("\nError reading file1: "));
				printerror(dwErr);
			}
			goto cleanup;
		}
		ReadFile(hFile2, pBuf2, FBUFSIZ, NULL, &ovl2);
		dwErr = GetLastError();
		if (dwErr != ERROR_IO_PENDING) {
			if (!bQuiet) {
				_ftprintf_s(stderr, _T("\nError reading file2: "));
				printerror(dwErr);
			}
			goto cleanup;
		}
		dwErr = WaitForMultipleObjects(2, hIO, TRUE, INFINITE);
		if (dwErr == WAIT_FAILED) {
			dwErr = GetLastError();
			if (!bQuiet) {
				_ftprintf_s(stderr, _T("\nWait I/O complete failed: "));
				printerror(dwErr);
			}
			goto cleanup;
		} else if (dwErr == WAIT_ABANDONED_0) {
			dwErr = ERROR_CANCELLED;
			break;
		}
		if (!GetOverlappedResult(hFile1, &ovl1, &dwRead1, FALSE)) {
			dwErr = GetLastError();
			if (dwErr == ERROR_HANDLE_EOF) {
				dwErr = ERROR_SUCCESS;
				bComplete = TRUE;
				break;
			} else {
				if (!bQuiet) {
					_ftprintf_s(stderr, _T("\nError reading file1: "));
					printerror(dwErr);
				}
				goto cleanup;
			}
		}
		if (!GetOverlappedResult(hFile2, &ovl2, &dwRead2, FALSE)) {
			dwErr = GetLastError();
			if (dwErr == ERROR_HANDLE_EOF) {
				dwErr = ERROR_SUCCESS;
				bComplete = TRUE;
				break;
			} else {
				if (!bQuiet) {
					_ftprintf_s(stderr, _T("\nError reading file2: "));
					printerror(dwErr);
				}
				goto cleanup;
			}
		}
			
		if (dwRead1 < FBUFSIZ || dwRead2 < FBUFSIZ) {
			bComplete = TRUE;
			dwRead1 = min(dwRead1, dwRead2);
		}
		if (dwRead1 > (nCmpLen.QuadPart - nRead.QuadPart)) {
			bComplete = TRUE;
			dwRead1 = (DWORD)(nCmpLen.QuadPart - nRead.QuadPart);
		}
						
		dwIndex = 0;
		while(dwIndex < dwRead1) {
			if (pBuf1[dwIndex] != pBuf2[dwIndex]) {
				nDiffs.QuadPart++;
				if (bPrintMap && !bQuiet) {
					if (bBR)
						_puttc(_T('\r'), stdout);
					if (nOffset1.QuadPart != nOffset2.QuadPart)
						_tprintf_s(_T("%016llX %02X|%02X %016llX\n"),
							nOffset1.QuadPart + nRead.QuadPart + dwIndex,
							pBuf1[dwIndex], pBuf2[dwIndex],
							nOffset2.QuadPart + nRead.QuadPart + dwIndex);
					else
						_tprintf_s(_T("%016llX %02X|%02X\n"),
							nOffset1.QuadPart + nRead.QuadPart + dwIndex,
							pBuf1[dwIndex], pBuf2[dwIndex]);
				}
				nRangeEnd.QuadPart = nRead.QuadPart + dwIndex;
				if (nRangeStart.QuadPart == -1LL)
					nRangeStart.QuadPart = nRangeEnd.QuadPart;
			}
			dwIndex++;
		}

		nRead.QuadPart += dwRead1;
			
		if (bPrintPerc)
			printperc(nRead, nCmpLen), bPrintPerc = FALSE;

		if (bComplete)
			break;

		ovl1.OffsetHigh = nRead.HighPart;
		ovl1.Offset = nRead.LowPart;
		ovl2.OffsetHigh = nRead.HighPart;
		ovl2.Offset = nRead.LowPart;
	}	// read cycle

	if (!bQuiet) {
		GetSystemTime(&t2);

		if (perctimer != INVALID_HANDLE_VALUE) {
			DeleteTimerQueueTimer(NULL, perctimer, NULL), perctimer = INVALID_HANDLE_VALUE;
			printperc(nCmpLen, nCmpLen); // 100%
		}

		if (bBR)
			_puttc(_T('\n'), stdout);
		if (nDiffs.QuadPart == 0) {
			_tprintf_s(_T("%s are identical\n"), (bWholeFile?_T("Files"):_T("Fragments")));
		} else {
			_tprintf_s(_T("Found %llu difference"), nDiffs.QuadPart);
			if (nRangeStart.QuadPart != nRangeEnd.QuadPart) {
				if (nOffset1.QuadPart != nOffset2.QuadPart)	{
					_tprintf(_T("s in range\nfrom 0x%llX / 0x%llX (%0.3f%% / %0.3f%%)\n  to 0x%llX / 0x%llX (%0.3f%% / %0.3f%%).\n"),
						nOffset1.QuadPart + nRangeStart.QuadPart,
						nOffset2.QuadPart + nRangeStart.QuadPart,
						((double)(nOffset1.QuadPart + nRangeStart.QuadPart) / (double)(nSize1.QuadPart - 1)) * 100,
						((double)(nOffset2.QuadPart + nRangeStart.QuadPart) / (double)(nSize2.QuadPart - 1)) * 100,
						nOffset1.QuadPart + nRangeEnd.QuadPart,
						nOffset2.QuadPart + nRangeEnd.QuadPart,
						((double)(nOffset1.QuadPart + nRangeEnd.QuadPart) / (double)(nSize1.QuadPart - 1)) * 100,
						((double)(nOffset2.QuadPart + nRangeEnd.QuadPart) / (double)(nSize2.QuadPart - 1)) * 100);
				} else if (bWholeFile) {
					_tprintf(_T("s in range\nfrom 0x%llX (%0.3f%%)\n  to 0x%llX (%0.3f%%).\n"),
						nOffset1.QuadPart + nRangeStart.QuadPart,
						((double)(nOffset1.QuadPart + nRangeStart.QuadPart) / (double)(nSize1.QuadPart - 1)) * 100,
						nOffset1.QuadPart + nRangeEnd.QuadPart,
						((double)(nOffset1.QuadPart + nRangeEnd.QuadPart) / (double)(nSize1.QuadPart - 1)) * 100);
				} else {
					_tprintf(_T("s in range\nfrom 0x%llX (%0.3f%%)\n  to 0x%llX (%0.3f%% / %0.3f%%).\n"),
						nOffset1.QuadPart + nRangeStart.QuadPart,
						((double)(nOffset1.QuadPart + nRangeStart.QuadPart) / (double)(nSize1.QuadPart - 1)) * 100,
						nOffset1.QuadPart + nRangeEnd.QuadPart,
						((double)(nOffset1.QuadPart + nRangeEnd.QuadPart) / (double)(nSize1.QuadPart - 1)) * 100,
						((double)(nOffset1.QuadPart + nRangeEnd.QuadPart) / (double)(nSize2.QuadPart - 1)) * 100);
				}
			} else {
				if (bPrintMap) {
					_tprintf(_T(".\n"));
				} else {
					if (nOffset1.QuadPart != nOffset2.QuadPart)
						_tprintf(_T(" at 0x%llX/0x%llX.\n"),
							nOffset1.QuadPart + nRangeStart.QuadPart,
							nOffset2.QuadPart + nRangeStart.QuadPart);
					else
						_tprintf(_T(" at 0x%llX.\n"), nOffset1.QuadPart + nRangeStart.QuadPart);
				}
			}
		}
		printstat(t1, t2, nCmpLen);
	}

	nRet = (nDiffs.QuadPart == 0?(bWholeFile?0:1):2);

cleanup:
	if (dwErr != ERROR_SUCCESS) {
		nRet = 3;
		if (!bQuiet)
			_ftprintf_s(stderr, _T("Comparing is not complete.\n"));
	}
	if (ovl1.hEvent != NULL)
		CloseHandle(ovl1.hEvent);
	if (ovl2.hEvent != NULL)
		CloseHandle(ovl2.hEvent);
	if (pBuf1 != NULL)
		delete[] pBuf1;
	if (pBuf2 != NULL)
		delete[] pBuf2;
	if (hFile1 != INVALID_HANDLE_VALUE) {
		CancelIo (hFile1);
		CloseHandle(hFile1);
	}
	if (hFile2 != INVALID_HANDLE_VALUE) {
		CancelIo (hFile2);
		CloseHandle(hFile2);
	}

	return nRet;
}

LARGE_INTEGER getnum(LPCTSTR szValue)
{
	LARGE_INTEGER nRet;
	BYTE nRadix = 10;
	LPTSTR szEndPtr = NULL;

	nRet.QuadPart = -1LL;

	while(*szValue != _T('\0')) {
		if (*szValue == _T('0'))
			nRadix = 8;
		else if (isxdigit(*szValue))
			break;
		else if (tolower(*szValue) == _T('x') && nRadix == 8)
			nRadix = 16;
		else
			return nRet;
		szValue++;
	}

	nRet.QuadPart = _tcstoui64(szValue, &szEndPtr, nRadix);
	if (szEndPtr && *szEndPtr != _T('\0')) {
		nRet.QuadPart = -1LL;
		return nRet;
	}

	return nRet;
}

void printperc(LARGE_INTEGER nPos, LARGE_INTEGER nSize)
{
	nPercCur = (double)nPos.QuadPart / (double)nSize.QuadPart * (double)100.0;
	if ((nPercCur != nPercPrev)) {
		bBR = TRUE;
		_tprintf_s(_T("\r%.1f%%"), nPercCur);
		nPercPrev = nPercCur;
	}
}

void printsize(LARGE_INTEGER nSize)
{
	LPCTSTR szSuff = NULL;
	double fSize = 0.0;

	if (nSize.QuadPart >= 1024) {
		if (nSize.QuadPart >= 1099511627776ULL) {
		  szSuff = _T("TiB");
		  fSize = ((double)nSize.QuadPart) / 1099511627776ULL;
		} else if (nSize.QuadPart >= 1073741824ULL) {
		  szSuff = _T("GiB");
		  fSize = ((double)nSize.QuadPart) / 1073741824ULL;
		} else if (nSize.QuadPart >= 1048576ULL) {
		  szSuff = _T("MiB");
		  fSize = ((double)nSize.QuadPart) / 1048576ULL;
		} else {
		  szSuff = _T("KiB");
		  fSize = ((double)nSize.QuadPart) / 1024ULL;
		}
		_tprintf_s(_T("%.2f%s (%llu bytes)"), fSize, szSuff, nSize.QuadPart);
	} else
		_tprintf_s(_T("%llu bytes"), nSize.QuadPart);
}

DWORD printerror(DWORD dwErr)
{
	if(dwErr != 0) {
		LPTSTR lpBuffer = NULL;
		DWORD num;
		num = FormatMessage(
					FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS,
					NULL,
					dwErr,
					MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT),
					(LPTSTR)&lpBuffer,
					0,
					NULL);
		if (num > 0) {
			_ftprintf_s(stderr, _T("%s"), lpBuffer);
			LocalFree(lpBuffer);
		}
	}
	return dwErr;
}

void printcmderror(LPTSTR* argv, int argc, int arg)
{
	if(arg < argc) {
		arg++;
		LPTSTR lpBuffer = NULL;
		DWORD num;
		DWORD_PTR pArgs[] = {(DWORD_PTR)arg,
							 (DWORD_PTR)(arg==1?_T("st"):arg==2?_T("nd"):arg==3?_T("d"):_T("th")),
							 (DWORD_PTR)argv[arg - 1]};
		num =  FormatMessage(
					FORMAT_MESSAGE_FROM_STRING | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_ARGUMENT_ARRAY,
					_T("Command line error at %1!d!%2 argument: %3"),
					0,
					MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT),
					(LPTSTR)&lpBuffer,
					0,
					(va_list*)pArgs);
		if (num > 0)
			_ftprintf_s(stderr, _T("%s\n"), lpBuffer);
		LocalFree(lpBuffer);
	}
}

void printstat(SYSTEMTIME t1, SYSTEMTIME t2, LARGE_INTEGER len)
{
	FILETIME ft1, ft2;
	ULARGE_INTEGER u1, u2;

    SystemTimeToFileTime(&t1, &ft1);
	SystemTimeToFileTime(&t2, &ft2);
	u1.HighPart = ft1.dwHighDateTime;
	u1.LowPart = ft1.dwLowDateTime;
	u2.HighPart = ft2.dwHighDateTime;
	u2.LowPart = ft2.dwLowDateTime;
	u2.QuadPart -= u1.QuadPart;
	u2.QuadPart /= 10000;				// msec
	double speed = 0.0;

	if (u2.QuadPart >= 100) {			// 0.1s
		speed = 1000.0 / (double)u2.QuadPart * len.QuadPart;
	}
	_tprintf(_T("time spent: "));
	if (u2.QuadPart >= 60000) {			// 1m
		if (u2.QuadPart >= 86400000) {	// 1d
			_tprintf(_T("%llud"), u2.QuadPart / 86400000);
			u2.QuadPart %= 86400000;
		}
		if (u2.QuadPart >= 3600000) {	// 1h
			_tprintf(_T("%lluh"), u2.QuadPart / 3600000);
			u2.QuadPart %= 3600000;
		}
		if (u2.QuadPart >= 60000) {		// 1m
			_tprintf(_T("%llum"), u2.QuadPart / 60000);
			u2.QuadPart %= 60000;
		}
		if (u2.QuadPart >= 1000) {		// 1s
			_tprintf(_T("%llus"), u2.QuadPart / 1000);
		}
	} else {
		_tprintf(_T("%llu.%03llus"), u2.QuadPart / 1000, u2.QuadPart % 1000);
	}
	if (speed > 0.0)
		_tprintf(_T(", average speed %0.3fMiB/s\n"), speed / 1048576.0);
	else
		_tprintf(_T("\n"));
}

void usage()
{
	_tprintf(_T("Compare v2.1")
#ifdef _WIN64
			_T(" [x64]")
#endif
			_T("\nUsage: compare [opts] file1 file2 [> Output_File]\n")
			_T("options:\n")
			_T("  -f: First file offset(dec, 0xNUM = hex, 0NUM = oct).\n")
			_T("  -l: Length to compare(dec, 0xNUM = hex, 0NUM = oct).\n")
			_T("  -m: Print differences map (very slow, in case of\n")
			_T("      output redirection make very big files).\n")
			_T("  -q: don't print anything\n")
			_T("  -p: don't print completion percent\n")
			_T("  -s: Second file offset(dec, 0xNUM = hex, 0NUM = oct).\n")
			_T("return values:\n")
			_T("  0: files are identical\n")
			_T("  1: fragments are identical\n")
			_T("  2: files/fragments are different\n")
			_T("  3: error occured\n"));
}
