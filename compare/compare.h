#ifndef _COMPARE_H
#define _COMPARE_H

#define FBUFSIZ	131072

LARGE_INTEGER getnum(LPCTSTR szValue);
void printperc(LARGE_INTEGER nPos, LARGE_INTEGER nSize);
void printsize(LARGE_INTEGER nSize);
DWORD printerror(DWORD dwErr);
void printcmderror(LPTSTR* argv, int argc, int arg);
void printstat(SYSTEMTIME t1, SYSTEMTIME t2, LARGE_INTEGER len);
void usage();

double nPercCur, nPercPrev;
BOOL bPrintMap;
BOOL bQuiet;
BOOL bBR;
LARGE_INTEGER nRangeStart;
LARGE_INTEGER nRangeEnd;

#endif /* _COMPARE_H */
