#include "Md2Rtf.h"
#include <windows.h>
#include <string>
#include <sstream>
#include <vector>
#include <cstdio>
#include <cstring>
#include <cstdlib>

#define SCI_GETLENGTH 2006
#define SCI_GETCHARAT 2007
#define SCI_GETSTYLEAT 2010
#define SCI_GETLINECOUNT 2154
#define SCI_POSITIONFROMLINE 2167
#define SCI_LINELENGTH 2350

#define SCE_MARKDOWN_DEFAULT 0
#define SCE_MARKDOWN_LINE_BEGIN 1
#define SCE_MARKDOWN_STRONG1 2
#define SCE_MARKDOWN_STRONG2 3
#define SCE_MARKDOWN_EM1 4
#define SCE_MARKDOWN_EM2 5
#define SCE_MARKDOWN_HEADER1 6
#define SCE_MARKDOWN_HEADER2 7
#define SCE_MARKDOWN_HEADER3 8
#define SCE_MARKDOWN_HEADER4 9
#define SCE_MARKDOWN_HEADER5 10
#define SCE_MARKDOWN_HEADER6 11
#define SCE_MARKDOWN_PRECHAR 12
#define SCE_MARKDOWN_ULIST_ITEM 13
#define SCE_MARKDOWN_OLIST_ITEM 14
#define SCE_MARKDOWN_BLOCKQUOTE 15
#define SCE_MARKDOWN_STRIKEOUT 16
#define SCE_MARKDOWN_HRULE 17
#define SCE_MARKDOWN_LINK 18
#define SCE_MARKDOWN_CODE 19
#define SCE_MARKDOWN_CODE2 20
#define SCE_MARKDOWN_CODEBK 21
#define SCE_MARKDOWN_HDRTEXT 22

static std::string EscapeRTF(char c) {
    std::string res;
    if (c == '\\' || c == '{' || c == '}') {
        res += '\\'; res += c;
    } else if (c < 32 || c > 126) {
        char buf[10];
        snprintf(buf, sizeof(buf), "\\'%02x", (unsigned char)c);
        res += buf;
    } else {
        res += c;
    }
    return res;
}

extern "C" char* ConvertMarkdownToRTF(HWND hwndSci) {
    if (!hwndSci) return nullptr;

    long long lineCount = SendMessage(hwndSci, SCI_GETLINECOUNT, 0, 0);
    if (lineCount <= 0) return nullptr;

    std::ostringstream rtf;
    rtf << "{\\rtf1\\ansi\\ansicpg65001\\deff0\\nouicompat{\\fonttbl{\\f0\\fnil\\fcharset0 Calibri;}{\\f1\\fmodern\\fcharset0 Consolas;}}\n";
    rtf << "{\\colortbl ;\\red0\\green0\\blue255;\\red128\\green128\\blue128;\\red214\\green51\\blue132;\\red240\\green240\\blue240;}\n";

    bool paragraphOpen = false;

    for (long long i = 0; i < lineCount; ++i) {
        long long start = SendMessage(hwndSci, SCI_POSITIONFROMLINE, (WPARAM)i, 0);
        long long len = SendMessage(hwndSci, SCI_LINELENGTH, (WPARAM)i, 0);

        std::string lineRTF = "";
        bool isEmpty = true;

        int headerLevel = 0;
        bool isList = false;
        bool isCode = false;
        bool isHrule = false;
        bool isInlineCode = false;

        bool b = false, it = false, s = false, c = false;

        for (long long j = 0; j < len; ++j) {
            long long pos = start + j;
            char ch = (char)SendMessage(hwndSci, SCI_GETCHARAT, (WPARAM)pos, 0);
            if (ch == '\r' || ch == '\n') continue;
            isEmpty = false;

            if (ch == '`') {
                isInlineCode = !isInlineCode;
                continue;
            }

            int style = (int)SendMessage(hwndSci, SCI_GETSTYLEAT, (WPARAM)pos, 0);

            if (style >= SCE_MARKDOWN_HEADER1 && style <= SCE_MARKDOWN_HEADER6) {
                headerLevel = style - SCE_MARKDOWN_HEADER1 + 1;
                if (ch == '#') continue;
                if (ch == ' ' && j == headerLevel) continue;
            }
            if (style == SCE_MARKDOWN_ULIST_ITEM || style == SCE_MARKDOWN_OLIST_ITEM) {
                isList = true;
            }
            if (style == SCE_MARKDOWN_CODEBK) {
                isCode = true;
                if (ch == '`') continue;
            }
            if (style == SCE_MARKDOWN_HRULE) {
                isHrule = true;
                if (ch == '-' || ch == '*' || ch == '_') continue;
            }

            bool nextB = (style == SCE_MARKDOWN_STRONG1 || style == SCE_MARKDOWN_STRONG2);
            bool nextI = (style == SCE_MARKDOWN_EM1 || style == SCE_MARKDOWN_EM2);
            bool nextS = (style == SCE_MARKDOWN_STRIKEOUT);
            bool nextC = isInlineCode || (style == SCE_MARKDOWN_CODE || style == SCE_MARKDOWN_CODE2);

            if (nextB != b) { lineRTF += (nextB ? "{\\b " : "}"); b = nextB; }
            if (nextI != it) { lineRTF += (nextI ? "{\\i " : "}"); it = nextI; }
            if (nextS != s) { lineRTF += (nextS ? "{\\strike " : "}"); s = nextS; }
            if (nextC != c) { lineRTF += (nextC ? "{\\f1\\cf3\\highlight4 " : "}"); c = nextC; }

            if ((style == SCE_MARKDOWN_STRONG1 || style == SCE_MARKDOWN_STRONG2) && (ch == '*' || ch == '_')) continue;
            if ((style == SCE_MARKDOWN_EM1 || style == SCE_MARKDOWN_EM2) && (ch == '*' || ch == '_')) continue;
            if (style == SCE_MARKDOWN_STRIKEOUT && ch == '~') continue;

            if (style == SCE_MARKDOWN_HRULE && (ch == '-' || ch == '*' || ch == '_')) continue;

            lineRTF += EscapeRTF(ch);
        }

        if (b) lineRTF += "}";
        if (it) lineRTF += "}";
        if (s) lineRTF += "}";
        if (c) lineRTF += "}";

        if (isEmpty) {
            if (paragraphOpen) {
                rtf << "{\\highlight0\\cf0\\f0\\fs22  }\\par\n";
                paragraphOpen = false;
            }
            // rtf << "\\pard\\sa200\\f0\\fs22\\lang9\\par\n";
            continue;
        }

        if (isHrule) {
            if (paragraphOpen) { rtf << "{\\highlight0\\cf0\\f0\\fs22  }\\par\n"; paragraphOpen = false; }
            rtf << "{\\pard\\sa200\\f0\\fs22\\lang9\\qc \\strike \\emdash \\emdash \\emdash \\emdash \\emdash \\emdash \\emdash \\emdash \\emdash \\emdash \\par}\n";
            continue;
        }

        if (headerLevel > 0) {
            if (paragraphOpen) { rtf << "{\\highlight0\\cf0\\f0\\fs22  }\\par\n"; paragraphOpen = false; }
            int fs = 48 - (headerLevel * 4);
            if (headerLevel == 1 || headerLevel == 2) {
                rtf << "{\\pard\\brdrb\\brdrs\\brdrw15\\brsp20\\sa200\\b\\fs" << fs << " " << lineRTF << "\\par}\n";
            } else {
                rtf << "{\\pard\\sa200\\b\\fs" << fs << " " << lineRTF << "\\par}\n";
            }
            continue;
        }

        if (isCode) {
            if (paragraphOpen) { rtf << "{\\highlight0\\cf0\\f0\\fs22  }\\par\n"; paragraphOpen = false; }
            rtf << "{\\pard\\sa200\\f1\\fs20\\cf2 " << lineRTF << "\\par}\n";
            continue;
        }

        if (isList) {
            if (paragraphOpen) { rtf << "{\\highlight0\\cf0\\f0\\fs22  }\\par\n"; paragraphOpen = false; }
            rtf << "{\\pard\\li360\\sa0\\f0\\fs22 " << lineRTF << "\\par}\n";
            continue;
        }

        if (!paragraphOpen) {
            rtf << "\\pard\\sa200\\f0\\fs22\\lang9 " << lineRTF;
            paragraphOpen = true;
        } else {
            rtf << " " << lineRTF;
        }
    }

    if (paragraphOpen) {
                rtf << "{\\highlight0\\cf0\\f0\\fs22  }\\par\n";
    }

    rtf << "}";
#ifdef _WIN32
    return _strdup(rtf.str().c_str());
#else
    return strdup(rtf.str().c_str());
#endif
}

extern "C" void FreeMarkdownRTF(char* rtf) {
    if (rtf) {
        free(rtf);
    }
}
