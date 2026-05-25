with open("src/Md2Rtf.cpp", "r") as f:
    content = f.read()

# Current logic strips backticks:
#             if (ch == '`') {
#                 isInlineCode = !isInlineCode;
#                 continue;
#             }

# We need to distinguish inline code markers from backticks inside codeblocks.
# For inline code blocks marked by Scintilla, the style itself is `SCE_MARKDOWN_CODE` or `SCE_MARKDOWN_CODE2`.
# Wait, if we use the lexer style, do we even need the manual `isInlineCode` toggle?
# Let's look at lexer styles:
#             bool nextC = isInlineCode || (style == SCE_MARKDOWN_CODE || style == SCE_MARKDOWN_CODE2);
# Scintilla already sets `SCE_MARKDOWN_CODE` or `SCE_MARKDOWN_CODE2` for the backticks AND the text inside!
# Wait! Let's verify.
# Ah, if Scintilla sets it for inline code, why did we add `isInlineCode` toggle?
# Because someone reported inline code wasn't highlighted correctly if the lexer was weird?
# Wait, no, I added `isInlineCode` toggle in earlier steps because I thought Scintilla's lexer didn't style inline code!
# Let's remove `isInlineCode` toggle entirely. Scintilla lexer's `SCE_MARKDOWN_CODE` handles inline code natively.

old_inline_toggle = """            if (ch == '`') {
                isInlineCode = !isInlineCode;
                continue;
            }"""

new_inline_toggle = """            // Remove manual isInlineCode toggle, let Scintilla lexer handle SCE_MARKDOWN_CODE
            // But we might want to skip the backtick character if it's the inline code marker.
            // If style is SCE_MARKDOWN_CODE and ch == '`', should we skip it?
            // Actually, showing the backtick in the preview is fine or we can hide it.
            // Many previews hide the markdown syntax. We can hide it.
            if ((style == SCE_MARKDOWN_CODE || style == SCE_MARKDOWN_CODE2) && ch == '`') {
                // To hide the backtick in inline code
                continue;
            }"""

content = content.replace(old_inline_toggle, new_inline_toggle)
content = content.replace("bool isInlineCode = false;", "")
content = content.replace("bool nextC = isInlineCode || (style == SCE_MARKDOWN_CODE || style == SCE_MARKDOWN_CODE2);", "bool nextC = (style == SCE_MARKDOWN_CODE || style == SCE_MARKDOWN_CODE2);")

with open("src/Md2Rtf.cpp", "w") as f:
    f.write(content)
