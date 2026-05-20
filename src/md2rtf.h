#pragma once
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

char* ConvertMarkdownToRTF(const char* markdown, size_t len);
void FreeMarkdownRTF(char* rtf);

#ifdef __cplusplus
}
#endif
