#include "md2rtf.h"
#include <string>
#include <sstream>
#include <vector>
#include <cstdio>
#include <cstring>
#include <cstdlib>

class MarkdownToRTF {
public:
    static std::string Convert(const std::string& markdown) {
        std::ostringstream rtf;
        rtf << "{\\rtf1\\ansi\\ansicpg65001\\deff0\\nouicompat{\\fonttbl{\\f0\\fnil\\fcharset0 Calibri;}{\\f1\\fmodern\\fcharset0 Consolas;}}\n";
        rtf << "{\\colortbl ;\\red0\\green0\\blue255;\\red128\\green128\\blue128;}\n";
        rtf << "\\pard\\sa200\\sl276\\slmult1\\f0\\fs22\\lang9\n";

        bool inCodeBlock = false;
        std::istringstream iss(markdown);
        std::string line;

        while (std::getline(iss, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            if (line.size() >= 3 && line.substr(0, 3) == "```") {
                inCodeBlock = !inCodeBlock;
                if (inCodeBlock) {
                    rtf << "\\pard\\sa200\\sl276\\slmult1\\f1\\fs20\\cf2 ";
                } else {
                    rtf << "\\par\\pard\\sa200\\sl276\\slmult1\\f0\\fs22\\cf0\n";
                }
                continue;
            }

            if (inCodeBlock) {
                rtf << EscapeRTF(line) << "\\line\n";
                continue;
            }

            size_t headerLevel = 0;
            while (headerLevel < line.size() && line[headerLevel] == '#') {
                headerLevel++;
            }
            if (headerLevel > 0 && headerLevel <= 6 && headerLevel < line.size() && line[headerLevel] == ' ') {
                int fs = 48 - ((int)headerLevel * 4);
                rtf << "{\\pard\\sa200\\sl276\\slmult1\\b\\fs" << fs << " " << ParseLine(line.substr(headerLevel + 1)) << "\\par}\n";
                continue;
            }

            if (line.size() >= 2 && (line.substr(0, 2) == "- " || line.substr(0, 2) == "* ")) {
                rtf << "{\\pntext\\f0 \\'B7\\tab}{\\*\\pn\\pnlvlblt\\pnf0\\pnindent0{\\pntxtb \\'B7}}";
                rtf << "\\fi-360\\li360\\sa200\\sl276\\slmult1 " << ParseLine(line.substr(2)) << "\\par\n";
                continue;
            }

            if (line.empty() || line.find_first_not_of(" \t") == std::string::npos) {
                rtf << "\\par\n";
            } else {
                rtf << ParseLine(line) << "\\par\n";
            }
        }

        rtf << "}";
        return rtf.str();
    }

private:
    static std::string EscapeRTF(const std::string& text) {
        std::string res;
        for (char c : text) {
            if (c == '\\' || c == '{' || c == '}') {
                res += '\\';
                res += c;
            } else if (c < 32 || c > 126) {
                char buf[10];
                snprintf(buf, sizeof(buf), "\\'%02x", (unsigned char)c);
                res += buf;
            } else {
                res += c;
            }
        }
        return res;
    }

    static std::string ParseLine(const std::string& line) {
        std::string res;
        bool b = false, i = false, s = false, c = false;
        for (size_t pos = 0; pos < line.size(); ++pos) {
            if (pos + 1 < line.size() && line[pos] == '*' && line[pos+1] == '*') {
                b = !b;
                res += b ? "{\\b " : "}";
                pos++;
            }
            else if (pos + 1 < line.size() && line[pos] == '_' && line[pos+1] == '_') {
                b = !b;
                res += b ? "{\\b " : "}";
                pos++;
            }
            else if (line[pos] == '*' || line[pos] == '_') {
                i = !i;
                res += i ? "{\\i " : "}";
            }
            else if (pos + 1 < line.size() && line[pos] == '~' && line[pos+1] == '~') {
                s = !s;
                res += s ? "{\\strike " : "}";
                pos++;
            }
            else if (line[pos] == '`') {
                c = !c;
                res += c ? "{\\f1\\cf2 " : "}";
            }
            else {
                res += EscapeRTF(std::string(1, line[pos]));
            }
        }
        return res;
    }
};

extern "C" char* ConvertMarkdownToRTF(const char* markdown, size_t len) {
    if (!markdown) return nullptr;
    std::string md(markdown, len);
    std::string rtf = MarkdownToRTF::Convert(md);
#ifdef _WIN32
    return _strdup(rtf.c_str());
#else
    return strdup(rtf.c_str());
#endif
}

extern "C" void FreeMarkdownRTF(char* rtf) {
    if (rtf) {
        free(rtf);
    }
}
