/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinUtil.h"

extern "C" {
#include "../ext/mujs/regexp.h"
}

#include "wingui/UIModels.h"

#include "Settings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "ProgressUpdateUI.h"
#include "TextSelection.h"
#include "TextSearch.h"

#define SkipWhitespace(c) for (; str::IsWs(*(c)); (c)++)
// ignore spaces between CJK glyphs but not between Latin, Greek, Cyrillic, etc. letters
// cf. http://code.google.com/p/sumatrapdf/issues/detail?id=959
#define isnoncjkwordchar(c) (isWordChar(c) && (unsigned short)(c) < 0x2E80)

static void markAllPagesNonSkip(Vec<bool>& pagesToSkip) {
    for (size_t i = 0; i < pagesToSkip.size(); i++) {
        pagesToSkip[i] = false;
    }
}
TextSearch::TextSearch(EngineBase* engine) : TextSelection(engine) {
    nPages = engine->PageCount();
    pagesToSkip.SetSize(nPages);
    markAllPagesNonSkip(pagesToSkip);
}

TextSearch::~TextSearch() {
    Clear();
}

void TextSearch::Clear() {
    str::FreePtr(&findText);
    str::FreePtr(&anchor);
    str::FreePtr(&lastText);
    if (regexProg) {
        regfree(regexProg);
        regexProg = nullptr;
    }
    regexValid = false;
    Reset();
}

void TextSearch::Reset() {
    pageText = nullptr;
    TextSelection::Reset();
}

int TextSearch::GetCurrentPageNo() const {
    return findPage;
}

// note: the result might not be a valid page number!
int TextSearch::GetSearchHitStartPageNo() const {
    return searchHitStartAt;
}

void TextSearch::SetText(const WCHAR* text) {
    const bool needsWordBoundaries = wholeWord;
    // search text starting with a single space enables the 'Match word start'
    // and search text ending in a single space enables the 'Match word end' option
    // (that behavior already "kind of" exists without special treatment, but
    // usually is not quite what a user expects, so let's try to be cleverer)
    this->matchWordStart = needsWordBoundaries || (text[0] == ' ' && text[1] != ' ');
    this->matchWordEnd = needsWordBoundaries || (str::EndsWith(text, L" ") && !str::EndsWith(text, L"  "));

    if (!useRegex && text[0] == ' ') {
        text++;
    }

    // don't reset anything if the search text and options haven't changed at all
    if (str::Eq(this->lastText, text) && lastMatchCase == matchCase && lastWholeWord == wholeWord &&
        lastUseRegex == useRegex) {
        return;
    }

    this->Clear();
    this->lastText = str::Dup(text);
    this->findText = str::Dup(text);
    this->lastMatchCase = matchCase;
    this->lastWholeWord = wholeWord;
    this->lastUseRegex = useRegex;

    // extract anchor string (the first word or the first symbol) for faster searching
    if (isnoncjkwordchar(*text)) {
        const WCHAR* end;
        for (end = text; isnoncjkwordchar(*end); end++) {
            ;
        }
        anchor = str::Dup(text, end - text);
    }
    // Adobe Reader also matches certain hard-to-type Unicode
    // characters when searching for easy-to-type homoglyphs
    // cf. https://web.archive.org/web/20140201013717/http://forums.fofou.org:80/sumatrapdf/topic?id=2432337&comments=3
    else if (*text == '-' || *text == '\'' || *text == '"') {
        anchor = nullptr;
    } else {
        anchor = str::Dup(text, 1);
    }

    if (str::Len(this->findText) >= INT_MAX) {
        this->findText[(unsigned)INT_MAX - 1] = '\0';
    }
    if (str::EndsWith(this->findText, L" ")) {
        this->findText[str::Len(this->findText) - 1] = '\0';
    }

    if (useRegex) {
        TempStr patternUtf8 = ToUtf8Temp(this->findText);
        int regexFlags = REG_NEWLINE;
        if (!matchCase) {
            regexFlags |= REG_ICASE;
        }
        regexProg = regcomp(patternUtf8, regexFlags, nullptr);
        regexValid = regexProg != nullptr;
    }

    markAllPagesNonSkip(pagesToSkip);
}

void TextSearch::SetMatchCase(bool newMatchCase) {
    if (matchCase == newMatchCase) {
        return;
    }
    this->matchCase = newMatchCase;

    markAllPagesNonSkip(pagesToSkip);
}

void TextSearch::SetWholeWord(bool newWholeWord) {
    if (wholeWord == newWholeWord) {
        return;
    }
    this->wholeWord = newWholeWord;

    markAllPagesNonSkip(pagesToSkip);
}

void TextSearch::SetUseRegex(bool newUseRegex) {
    if (useRegex == newUseRegex) {
        return;
    }
    this->useRegex = newUseRegex;

    markAllPagesNonSkip(pagesToSkip);
}

void TextSearch::SetDirection(TextSearch::Direction direction) {
    bool fwd = TextSearch::Direction::Forward == direction;
    if (fwd == forward) {
        return;
    }
    forward = fwd;
    if (findText) {
        int n = (int)str::Len(findText);
        if (fwd) {
            findIndex += n;
        } else {
            findIndex -= n;
        }
    }
}

void TextSearch::SetLastResult(TextSelection* sel) {
    CopySelection(sel);

    AutoFreeWStr selection(ExtractText(" "));
    str::NormalizeWSInPlace(selection);
    SetText(selection);

    searchHitStartAt = findPage = std::min(startPage, endPage);
    findPage = std::max(startPage, endPage);
    findIndex = (findPage == endPage ? endGlyph : startGlyph);
    pageText = engine->GetTextForPage(findPage);
    forward = true;
}

static WCHAR CharToLower2(WCHAR c) {
    WCHAR buf[1] = {c};
    CharLowerBuffW(buf, 1);
    return buf[0];
}

static inline WCHAR CharToLower(WCHAR c) {
    // fast path that hopefully will be inlined
    if (c >= 'a' && c <= 'z') {
        return c;
    }
    if (c >= '0' && c <= '9') {
        return c;
    }
    if (c >= 'A' && c <= 'Z') {
        return c + 32;
    }
    return CharToLower2(c);
}

static int Utf8ByteOffsetToWideIndex(const char* utf8Text, int byteOffset) {
    if (!utf8Text || byteOffset <= 0) {
        return 0;
    }
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, utf8Text, byteOffset, nullptr, 0);
    if (wideLen < 0) {
        return 0;
    }
    return wideLen;
}

static int WideIndexToUtf8ByteOffset(const WCHAR* wideText, int wideOffset) {
    if (!wideText || wideOffset <= 0) {
        return 0;
    }
    int byteLen = WideCharToMultiByte(CP_UTF8, 0, wideText, wideOffset, nullptr, 0, nullptr, nullptr);
    if (byteLen < 0) {
        return 0;
    }
    return byteLen;
}

bool TextSearch::MatchWholeWord(const WCHAR* start, const WCHAR* end, const WCHAR* pageStart,
                                const WCHAR* pageEnd) const {
    if (start > pageStart && isWordChar(start[-1]) && isWordChar(start[0])) {
        return false;
    }
    if (end < pageEnd && end > pageStart && isWordChar(end[-1]) && isWordChar(end[0])) {
        return false;
    }
    return true;
}

static const char* NextUtf8Char(const char* text) {
    return text + std::max(utf8RuneLen((const u8*)text), 1);
}

bool TextSearch::FindTextInPageRegex(int pageNo, TextSearch::PageAndOffset* finalGlyph) {
    if (!regexValid || !regexProg || !pageText) {
        return false;
    }

    int pageLen = (int)str::Len(pageText);
    const WCHAR* pageStart = pageText;
    const WCHAR* pageEnd = pageText + pageLen;
    int wideSearchStart = findIndex;
    if (wideSearchStart < 0) {
        wideSearchStart = 0;
    } else if (wideSearchStart > pageLen) {
        wideSearchStart = pageLen;
    }
    TempStr pageUtf8 = ToUtf8Temp(pageText);
    int searchStartBytes = WideIndexToUtf8ByteOffset(pageText, wideSearchStart);

    if (forward) {
        const char* cur = pageUtf8 + searchStartBytes;
        while (*cur) {
            Resub match{};
            int result = regexecAt(regexProg, pageUtf8, cur, &match, 0);
            if (result != 0) {
                return false;
            }
            const char* found = match.sub[0].sp;
            const char* foundEnd = match.sub[0].ep;
            if (!found || !foundEnd) {
                return false;
            }
            if (foundEnd <= found) {
                if (!*found) {
                    return false;
                }
                cur = NextUtf8Char(found);
                continue;
            }
            int foundByteOffset = (int)(found - pageUtf8);
            int foundEndByteOffset = (int)(foundEnd - pageUtf8);
            int foundWideOffset = Utf8ByteOffsetToWideIndex(pageUtf8, foundByteOffset);
            int foundEndWideOffset = Utf8ByteOffsetToWideIndex(pageUtf8, foundEndByteOffset);
            const WCHAR* foundWide = pageStart + foundWideOffset;
            const WCHAR* foundWideEnd = pageStart + foundEndWideOffset;
            if (!wholeWord || MatchWholeWord(foundWide, foundWideEnd, pageStart, pageEnd)) {
                int off = foundWideOffset;
                searchHitStartAt = pageNo;
                StartAt(pageNo, off);
                SelectUpTo(pageNo, foundEndWideOffset);
                findIndex = foundEndWideOffset;
                if (finalGlyph) {
                    *finalGlyph = {pageNo, foundEndWideOffset};
                }
                return true;
            }
            cur = NextUtf8Char(found);
        }
        return false;
    }

    const char* cur = pageUtf8;
    const char* searchLimit = pageUtf8 + searchStartBytes;
    PageAndOffset best = {-1, -1};
    while (cur < searchLimit) {
        Resub match{};
        int result = regexecAt(regexProg, pageUtf8, cur, &match, 0);
        if (result != 0) {
            break;
        }
        const char* found = match.sub[0].sp;
        const char* foundEnd = match.sub[0].ep;
        if (!found || !foundEnd || found >= searchLimit) {
            break;
        }
        if (foundEnd <= found) {
            if (!*found) {
                break;
            }
            cur = NextUtf8Char(found);
            continue;
        }
        int foundByteOffset = (int)(found - pageUtf8);
        int foundEndByteOffset = (int)(foundEnd - pageUtf8);
        int foundWideOffset = Utf8ByteOffsetToWideIndex(pageUtf8, foundByteOffset);
        int foundEndWideOffset = Utf8ByteOffsetToWideIndex(pageUtf8, foundEndByteOffset);
        const WCHAR* foundWide = pageStart + foundWideOffset;
        const WCHAR* foundWideEnd = pageStart + foundEndWideOffset;
        if (!wholeWord || MatchWholeWord(foundWide, foundWideEnd, pageStart, pageEnd)) {
            best = {foundWideOffset, foundEndWideOffset};
        }
        cur = NextUtf8Char(found);
    }

    if (best.page < 0) {
        return false;
    }

    int off = best.page;
    int endOff = best.offset;
    searchHitStartAt = pageNo;
    StartAt(pageNo, off);
    SelectUpTo(pageNo, endOff);
    findIndex = off;
    if (finalGlyph) {
        *finalGlyph = {pageNo, endOff};
    }
    return true;
}

// try to match "findText" from "start" with whitespace tolerance
// (ignore all whitespace except after alphanumeric characters)
TextSearch::PageAndOffset TextSearch::MatchEnd(const WCHAR* start) const {
    const WCHAR *match = findText, *end = start;
    const PageAndOffset notFound = {-1, -1};
    int currentPage = findPage;
    const WCHAR* currentPageText = pageText;
    bool lookingAtWs;

    if (matchWordStart && start > pageText && isWordChar(start[-1]) && isWordChar(start[0])) {
        return notFound;
    }

    if (!match) {
        return notFound;
    }

    while (*match) {
        if (!*end) {
            return notFound;
        }
        /* Going from page n to page n+1 is a space, too.*/
        lookingAtWs = (!*end && (currentPage < nPages)) || str::IsWs(*end);
        bool isMatch = false;
        if (matchCase) {
            isMatch = *match == *end;
        } else {
            WCHAR matchLower = CharToLower(*match);
            WCHAR matchEnd = CharToLower(*end);
            isMatch = matchLower == matchEnd;
        }
        if (isMatch) {
            /* characters are identical */;
        } else if (str::IsWs(*match) && lookingAtWs) {
            /* treat all whitespace as identical and end of page as whitespace.
               The end of the document is NOT seen as whitespace */
            ;
            // TODO: Adobe Reader seems to have a more extensive list of
            //       normalizations - is there an easier way?
        } else if (*match == '-' && (0x2010 <= *end && *end <= 0x2014)) {
            /* make HYPHEN-MINUS also match HYPHEN, NON-BREAKING HYPHEN,
               FIGURE DASH, EN DASH and EM DASH (but not the other way around) */
            ;
        } else if (*match == '\'' && (0x2018 <= *end && *end <= 0x201b)) {
            /* make APOSTROPHE also match LEFT/RIGHT SINGLE QUOTATION MARK */;
        } else if (*match == '"' && (0x201c <= *end && *end <= 0x201f)) {
            /* make QUOTATION MARK also match LEFT/RIGHT DOUBLE QUOTATION MARK */;
        } else {
            return notFound;
        }
        match++;
        // We might get here either ...
        if (*end) {
            // ... because there's a genuine match -> consider next character in next loop iteration
            end++;
        } else {
            // ... or because we were looking at whitespace in the pattern and we were at a page break
            // -> skip to next page
            ++currentPage;
            end = currentPageText = engine->GetTextForPage(currentPage);
        }
        // treat "??" and "? ?" differently, since '?' could have been a word
        // character that's just missing an encoding (and '?' is the replacement
        // character); cf. http://code.google.com/p/sumatrapdf/issues/detail?id=1574
        if (*match && !isnoncjkwordchar(*(match - 1)) && (*(match - 1) != '?' || *match != '?') ||
            lookingAtWs && str::IsWs(*(match - 1))) {
            SkipWhitespace(match);
            SkipWhitespace(end);
            while ((!*end) && (currentPage < nPages)) {
                // treat page break as whitespace, too
                ++currentPage;
                end = currentPageText = engine->GetTextForPage(currentPage);
                SkipWhitespace(end);
            }
        }
    }
    if (matchWordEnd && end > currentPageText && isWordChar(end[-1]) && isWordChar(end[0])) {
        return notFound;
    }

    int off = (int)(end - currentPageText);
    return {currentPage, off};
}

static const WCHAR* GetNextIndex(const WCHAR* base, int offset, bool forward) {
    const WCHAR* c = base + offset + (forward ? 0 : -1);
    if (c < base || !*c) {
        return nullptr;
    }
    return c;
}

bool TextSearch::FindTextInPage(int pageNo, TextSearch::PageAndOffset* finalGlyph) {
    if (str::IsEmpty(findText)) {
        return false;
    }
    if (!pageNo) {
        pageNo = findPage;
    }
    // According to my analysis of 69912675c766b6325f38036913dcf0505a00be36, when we
    // get here with pageNo != 0 the findText has already been set so I didn't add
    // a findText = engine->GetTextForPage(findPage) here.
    findPage = pageNo;
    if (useRegex) {
        return FindTextInPageRegex(pageNo, finalGlyph);
    }

    const WCHAR* found;
    PageAndOffset fg;
    do {
        if (!anchor) {
            found = GetNextIndex(pageText, findIndex, forward);
        } else if (forward) {
            const WCHAR* s = pageText + findIndex;
            if (matchCase) {
                found = StrStr(s, anchor);
            } else {
                found = StrStrI(s, anchor);
            }
        } else {
            found = StrRStrI(pageText, pageText + findIndex, anchor);
        }
        if (!found) {
            return false;
        }
        findIndex = (int)(found - pageText) + (forward ? 1 : 0);
        fg = MatchEnd(found);
    } while (fg.page <= 0);

    int offset = (int)(found - pageText);
    searchHitStartAt = pageNo;
    StartAt(pageNo, offset);
    SelectUpTo(fg.page, fg.offset);
    findIndex = forward ? fg.offset : offset;

    // try again if the found text is completely outside the page's mediabox
    if (result.len == 0) {
        return FindTextInPage(pageNo, finalGlyph);
    }

    if (finalGlyph) {
        *finalGlyph = fg;
    }
    return true;
}

bool TextSearch::FindStartingAtPage(int pageNo) {
    if (str::IsEmpty(findText)) {
        return false;
    }

    int next = forward ? 1 : -1;
    while ((1 <= pageNo) && (pageNo <= nPages) && !WasCanceled(progressCb)) {
        UpdateProgress(progressCb, pageNo, nPages);

        if (pagesToSkip[pageNo - 1]) {
            pageNo += next;
            continue;
        }

        Reset();

        pageText = engine->GetTextForPage(pageNo, &findIndex);
        if (pageText) {
            if (forward) {
                findIndex = 0;
            }
            PageAndOffset r;
            if (FindTextInPage(pageNo, &r)) {
                if (forward) {
                    if (findPage != r.page) {
                        findPage = r.page;
                        pageText = engine->GetTextForPage(findPage);
                    }
                    findIndex = r.offset;
                }
                return true;
            }
            pagesToSkip[pageNo - 1] = true;
        }

        pageNo += next;
    }

    // allow for the first/last page to be included in the next search
    searchHitStartAt = findPage = forward ? nPages + 1 : 0;

    return false;
}

TextSel* TextSearch::FindFirst(int page, const WCHAR* text) {
    SetText(text);

    if (FindStartingAtPage(page)) {
        return &result;
    }
    return nullptr;
}

TextSel* TextSearch::FindNext() {
    ReportIf(!findText);
    if (!findText) {
        return nullptr;
    }

    if (WasCanceled(progressCb)) {
        return nullptr;
    }
    UpdateProgress(progressCb, findPage, nPages);

    PageAndOffset finalGlyph;
    if (FindTextInPage(findPage, &finalGlyph)) {
        if (forward) {
            findPage = finalGlyph.page;
            findIndex = finalGlyph.offset;
            pageText = engine->GetTextForPage(findPage);
        }
        return &result;
    }

    auto next = forward ? 1 : -1;
    if (FindStartingAtPage(findPage + next)) {
        return &result;
    }
    return nullptr;
}

bool TextSearch::CountMatchesPerPage(const WCHAR* text, Vec<int>& pageMatchCounts, int* totalMatches) {
    pageMatchCounts.Reset();
    pageMatchCounts.SetSize(nPages);
    for (int i = 0; i < nPages; i++) {
        pageMatchCounts[i] = 0;
    }
    if (totalMatches) {
        *totalMatches = 0;
    }
    if (!text || !*text) {
        return true;
    }

    SetText(text);
    if (!findText) {
        return true;
    }

    forward = true;
    for (int pageNo = 1; pageNo <= nPages; pageNo++) {
        if (WasCanceled(progressCb)) {
            return false;
        }
        UpdateProgress(progressCb, pageNo, nPages);

        int pageMatches = 0;
        int pageLen = 0;
        pageText = engine->GetTextForPage(pageNo, &pageLen);
        if (!pageText) {
            continue;
        }

        findPage = pageNo;
        findIndex = 0;
        for (;;) {
            PageAndOffset glyph{};
            int prevIndex = findIndex;
            if (!FindTextInPage(pageNo, &glyph)) {
                break;
            }
            pageMatches++;

            if (glyph.page != pageNo) {
                break;
            }
            pageText = engine->GetTextForPage(pageNo);
            findPage = pageNo;
            findIndex = glyph.offset;
            if (findIndex <= prevIndex) {
                findIndex = prevIndex + 1;
            }
            if (findIndex > pageLen) {
                break;
            }
        }

        pageMatchCounts[pageNo - 1] = pageMatches;
        if (totalMatches) {
            *totalMatches += pageMatches;
        }
    }
    return true;
}
