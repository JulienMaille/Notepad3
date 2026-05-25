#include "Md2Rtf.h"
#include <windows.h>
#include <string>
#include <sstream>
#include <vector>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include "Scintilla.h"
#include "SciLexer.h"


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

    long long length = SendMessage(hwndSci, SCI_GETLENGTH, 0, 0);
    if (length <= 0) return nullptr;

    // Force lexing of the entire document to ensure styles are available
    SendMessage(hwndSci, SCI_COLOURISE, 0, -1);

    long long lineCount = SendMessage(hwndSci, SCI_GETLINECOUNT, 0, 0);
    if (lineCount <= 0) return nullptr;

    std::vector<char> styledText(static_cast<size_t>(length) * 2 + 2, 0);
    Sci_TextRange tr;
    tr.chrg.cpMin = 0;
    tr.chrg.cpMax = static_cast<Sci_PositionCR>(length);
    tr.lpstrText = styledText.data();
    SendMessage(hwndSci, SCI_GETSTYLEDTEXT, 0, (LPARAM)&tr);

    auto GetCharAt = [&](long long pos) -> char {
        return styledText[static_cast<size_t>(pos) * 2];
    };
    auto GetStyleAt = [&](long long pos) -> int {
        return (unsigned char)styledText[static_cast<size_t>(pos) * 2 + 1];
    };

    std::ostringstream rtf;
    rtf << "{\\rtf1\\ansi\\ansicpg65001\\deff0\\nouicompat{\\fonttbl{\\f0\\fnil\\fcharset0 Calibri;}{\\f1\\fmodern\\fcharset0 Consolas;}}\n";
    rtf << "{\\colortbl ;\\red0\\green0\\blue255;\\red128\\green128\\blue128;\\red214\\green51\\blue132;\\red240\\green240\\blue240;}\n";

    bool paragraphOpen = false;
    bool inListBlock = false;
    bool inLinkUrl = false;

    for (long long i = 0; i < lineCount; ++i) {
        long long start = SendMessage(hwndSci, SCI_POSITIONFROMLINE, (WPARAM)i, 0);
        long long len = SendMessage(hwndSci, SCI_LINELENGTH, (WPARAM)i, 0);

        std::string lineRTF = "";
        bool isEmpty = true;

        int headerLevel = 0;
        bool isList = false;
        bool isCode = false;
        bool isHrule = false;


        bool b = false, it = false, s = false, c = false, l = false;


        bool isCodeBlockLine = false;
        for (long long j = 0; j < len; ++j) {
            int st = GetStyleAt(start + j);
            if (st == SCE_MARKDOWN_CODEBK) {
                isCodeBlockLine = true;
                break;
            }
        }

        bool leadingSpace = !isCodeBlockLine;
        int leadingSpacesCount = 0;

        for (long long j = 0; j < len; ++j) {
            long long pos = start + j;
            char ch = GetCharAt(pos);
            if (ch == '\r' || ch == '\n') continue;

            if (leadingSpace) {
                if (ch == ' ') {
                    leadingSpacesCount += 1;
                    continue;
                } else if (ch == '\t') {
                    leadingSpacesCount += 4;
                    continue;
                }
            }
            leadingSpace = false;
            isEmpty = false;

            // Remove manual isInlineCode toggle, let Scintilla lexer handle SCE_MARKDOWN_CODE
            // But we might want to skip the backtick character if it's the inline code marker.
            // If style is SCE_MARKDOWN_CODE and ch == '`', should we skip it?
            // Actually, showing the backtick in the preview is fine or we can hide it.
            // Many previews hide the markdown syntax. We can hide it.
            int style = GetStyleAt(pos);

            bool hideChar = false;
            if (style == SCE_MARKDOWN_LINK) {
                if (ch == '[') hideChar = true;
                else if (ch == ']') hideChar = true;
                else if (ch == '(') { inLinkUrl = true; hideChar = true; }
                else if (ch == ')') { inLinkUrl = false; hideChar = true; }
                else if (inLinkUrl) hideChar = true;
            } else {
                inLinkUrl = false;
            }

            if ((style == SCE_MARKDOWN_CODE || style == SCE_MARKDOWN_CODE2) && ch == '`') {
                hideChar = true;
            }


            if (style >= SCE_MARKDOWN_HEADER1 && style <= SCE_MARKDOWN_HEADER6) {
                headerLevel = style - SCE_MARKDOWN_HEADER1 + 1;
                if (ch == '#') continue;
                if (ch == ' ' && j == headerLevel) continue;
            }
            if (style == SCE_MARKDOWN_ULIST_ITEM || style == SCE_MARKDOWN_OLIST_ITEM) {
                isList = true;
            }
            // Fallback manual detection for sub-lists that the lexer might miss
            if (lineRTF.empty() && (ch == '-' || ch == '*' || ch == '+')) {
                if (j + 1 < len) {
                    char nextCh = GetCharAt(pos + 1);
                    if (nextCh == ' ') {
                        isList = true;
                    }
                }
            }
            if (lineRTF.empty() && ch >= '0' && ch <= '9') {
                // Peek forward to see if it's "1. "
                for (long long k = 1; j + k < len && k < 5; ++k) {
                    char nextCh = GetCharAt(pos + k);
                    if (nextCh == '.') {
                        if (j + k + 1 < len) {
                            char spaceCh = GetCharAt(pos + k + 1);
                            if (spaceCh == ' ') {
                                isList = true;
                            }
                        }
                        break;
                    } else if (nextCh < '0' || nextCh > '9') {
                        break;
                    }
                }
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
            bool nextC = (style == SCE_MARKDOWN_CODE || style == SCE_MARKDOWN_CODE2);
            bool nextL = (style == SCE_MARKDOWN_LINK);

            if (nextB != b) { lineRTF += (nextB ? "\\b " : "\\b0 "); b = nextB; }
            if (nextI != it) { lineRTF += (nextI ? "\\i " : "\\i0 "); it = nextI; }
            if (nextS != s) { lineRTF += (nextS ? "\\strike " : "\\strike0 "); s = nextS; }
            if (nextC != c) { lineRTF += (nextC ? "\\f1\\fs20\\cf3\\chcbpat4 " : "\\chcbpat0\\cf0\\f0\\fs22 "); c = nextC; }
            if (nextL != l) { lineRTF += (nextL ? "\\cf1\\ul " : "\\ulnone\\cf0 "); l = nextL; }

            if ((style == SCE_MARKDOWN_STRONG1 || style == SCE_MARKDOWN_STRONG2) && (ch == '*' || ch == '_')) continue;
            if ((style == SCE_MARKDOWN_EM1 || style == SCE_MARKDOWN_EM2) && (ch == '*' || ch == '_')) continue;
            if (style == SCE_MARKDOWN_STRIKEOUT && ch == '~') continue;

            if (style == SCE_MARKDOWN_HRULE && (ch == '-' || ch == '*' || ch == '_')) continue;

            if (!hideChar) { lineRTF += EscapeRTF(ch); }
        }

        if (b) { lineRTF += "\\b0 "; }
        if (it) { lineRTF += "\\i0 "; }
        if (s) { lineRTF += "\\strike0 "; }
        if (c) { lineRTF += "\\chcbpat0\\cf0\\f0 "; }
        if (l) { lineRTF += "\\ulnone\\cf0 "; }

        if (isEmpty) {
            if (paragraphOpen) {
                rtf << "\\chcbpat0\\cf0\\f0\\fs22\\par\n";
                paragraphOpen = false;
            }
            // rtf << "\\pard\\sa200\\f0\\fs22\\lang9\\par\n";
            continue;
        }

        if (isHrule) {
            inListBlock = false;
            if (paragraphOpen) { rtf << "\\chcbpat0\\cf0\\f0\\fs22\\par\n"; paragraphOpen = false; }
            rtf << "\\pard\\sa200\\cf2\\f0\\fs22 ________________________________________________________________________________\\cf0\\par\n";
            continue;
        }

        if (headerLevel > 0) {
            inListBlock = false;
            if (paragraphOpen) { rtf << "\\chcbpat0\\cf0\\f0\\fs22\\par\n"; paragraphOpen = false; }
            int fs = 48 - (headerLevel * 4);
            if (headerLevel == 1 || headerLevel == 2) {
                rtf << "\\pard\\brdrb\\brdrs\\brdrw15\\brsp20\\sa200\\cf0\\b\\fs" << fs << " " << lineRTF << "\\par\n";
            } else {
                rtf << "\\pard\\sa200\\cf0\\b\\fs" << fs << " " << lineRTF << "\\par\n";
            }
            continue;
        }

        if (isCode) {
            if (paragraphOpen) { rtf << "\\chcbpat0\\cf0\\f0\\fs22\\par\n"; paragraphOpen = false; }
            rtf << "\\pard\\sa200\\cf2\\f1\\fs20 " << lineRTF << "\\cf0\\par\n";
            continue;
        }

        int listIndent = 360 + (leadingSpacesCount / 2) * 360;
        if (isList) {
            inListBlock = true;
            if (paragraphOpen) { rtf << "\\chcbpat0\\cf0\\f0\\fs22\\par\n"; paragraphOpen = false; }
            rtf << "\\pard\\li" << listIndent << "\\sa200\\cf0\\f0\\fs22\\lang9 " << lineRTF;
            paragraphOpen = true;
            continue;
        }

        if (!paragraphOpen) {
            if (inListBlock) {
                rtf << "\\pard\\li" << listIndent << "\\sa200\\cf0\\f0\\fs22\\lang9 " << lineRTF;
            } else {
                rtf << "\\pard\\sa200\\cf0\\f0\\fs22\\lang9 " << lineRTF;
            }
            paragraphOpen = true;
        } else {
            rtf << "\\line " << lineRTF;
        }
    }

    if (paragraphOpen) {
                rtf << "\\chcbpat0\\cf0\\f0\\fs22\\par\n";
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
