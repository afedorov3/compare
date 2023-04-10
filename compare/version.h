
#define CMP_VERSION_FILEVERSION 2,1,0,0
#define CMP_VERSION_DISPLAY     "2.1.0.0"
#define CMP_DATE_AUTHOR         "2012 AF"
#ifdef _DEBUG
#ifdef _WIN64
#define CMP_VERSION_DISPLAY_FULL CMP_VERSION_DISPLAY " (debug, x64)"
#else
#define CMP_VERSION_DISPLAY_FULL CMP_VERSION_DISPLAY " (debug)"
#endif
#else
#ifdef _WIN64
#define CMP_VERSION_DISPLAY_FULL CMP_VERSION_DISPLAY " (x64)"
#else
#define CMP_VERSION_DISPLAY_FULL CMP_VERSION_DISPLAY
#endif
#endif

#ifdef _WIN64
#define CMP_FILE_NAME "compare.exe"
#else
#define CMP_FILE_NAME "compare32.exe"
#endif
