#pragma once
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

char* ConvertMarkdownToRTF(HWND hwndSci);
void FreeMarkdownRTF(char* rtf);

#ifdef __cplusplus
}
#endif
