/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct Reprog;

struct TextSearch : public TextSelection {
    enum class Direction : bool {
        Backward = false,
        Forward = true
    };

    explicit TextSearch(EngineBase* engine);
    ~TextSearch();

    void SetMatchCase(bool sensitive);
    void SetWholeWord(bool wholeWord);
    void SetUseRegex(bool useRegex);
    void SetDirection(Direction direction);
    void SetLastResult(TextSelection* sel);
    TextSel* FindFirst(int page, const WCHAR* text);
    TextSel* FindNext();
    TextSel* FindFirstOnPage(int page, const WCHAR* text);
    TextSel* FindNextOnPage(int page);
    bool CountMatchesPerPage(const WCHAR* text, Vec<int>& pageMatchCounts, int* totalMatches);

    int GetCurrentPageNo() const;
    int GetSearchHitStartPageNo() const;

    ProgressUpdateCb progressCb;

    // Lightweight container for page and offset within the page to use as return value of MatchEnd
    struct PageAndOffset {
        int page;
        int offset;
    };

    WCHAR* findText = nullptr;
    WCHAR* anchor = nullptr;
    int findPage = 0;
    int searchHitStartAt = 0; // when text found spans several pages, searchHitStartAt < findPage
    bool forward = true;
    bool matchCase = false;
    bool wholeWord = false;
    bool useRegex = false;
    // these two options are implicitly set when the search text begins
    // resp. ends in a single space (many users already search that way),
    // combining them yields a 'Whole words' search
    bool matchWordStart = false;
    bool matchWordEnd = false;
    bool regexValid = false;
    Reprog* regexProg = nullptr;
    int lastMatchStart = -1;
    int lastMatchEnd = -1;
    bool pageSearchDone = false;

    void SetText(const WCHAR* text);
    bool FindTextInPage(int pageNo, PageAndOffset* finalGlyph);
    bool FindStartingAtPage(int pageNo);
    PageAndOffset MatchEnd(const WCHAR* start) const;
    bool FindTextInPageRegex(int pageNo, PageAndOffset* finalGlyph);
    bool MatchWholeWord(const WCHAR* start, const WCHAR* end, const WCHAR* pageStart, const WCHAR* pageEnd) const;

    void Clear();
    void Reset();

    const WCHAR* pageText = nullptr;
    int findIndex = 0;

    WCHAR* lastText = nullptr;
    bool lastMatchCase = false;
    bool lastWholeWord = false;
    bool lastUseRegex = false;
    int nPages = 0;
    Vec<bool> pagesToSkip;
};
