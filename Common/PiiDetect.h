#pragma once
// =============================================================================
// PiiDetect.h  –  Header-only PII regex engine (no external dependencies)
// =============================================================================
// Uses std::wregex (C++11). Scans aggregated text for sensitive data patterns.
// Reused and extended from the GDI hooking version.
// =============================================================================

#include <string>
#include <vector>
#include <regex>
#include "DlpShared.h"

struct PiiMatch
{
    std::wstring text;
    DWORD        category;
    size_t       pos;       // character offset in source string
    size_t       len;
};

// ---------------------------------------------------------------------------
// ScanForPii  –  Run all patterns against `src`, append matches to `out`.
// ---------------------------------------------------------------------------
inline void ScanForPii(const std::wstring& src, std::vector<PiiMatch>& out)
{
    struct Pattern { DWORD cat; std::wregex re; };

    // Compiled once (Meyer's singleton via static local)
    static const Pattern patterns[] =
    {
        // Email (RFC 5321 simplified)
        { PII_EMAIL,
          std::wregex(LR"([a-zA-Z0-9._%+\-]+@[a-zA-Z0-9.\-]+\.[a-zA-Z]{2,})",
                      std::regex::icase) },

        // US phone (various formats)
        { PII_PHONE,
          std::wregex(LR"((\+?1[\s\-\.]?)?\(?\d{3}\)?[\s\-\.]?\d{3}[\s\-\.]?\d{4})") },

        // SSN (123-45-6789 or 123 45 6789)
        { PII_SSN,
          std::wregex(LR"(\b\d{3}[\-\s]?\d{2}[\-\s]?\d{4}\b)") },

        // Credit card (13-19 digits with optional spaces/dashes every 4)
        { PII_CREDIT_CARD,
          std::wregex(LR"(\b(?:\d[ \-]?){13,18}\d\b)") },

        // IPv4
        { PII_IP_ADDRESS,
          std::wregex(LR"(\b(?:25[0-5]|2[0-4]\d|[01]?\d\d?)(?:\.(?:25[0-5]|2[0-4]\d|[01]?\d\d?)){3}\b)") },

        // Date of birth (MM/DD/YYYY, DD-MM-YYYY, YYYY-MM-DD)
        { PII_DATE_OF_BIRTH,
          std::wregex(LR"(\b(?:\d{1,2}[/\-]\d{1,2}[/\-]\d{2,4}|\d{4}[/\-]\d{1,2}[/\-]\d{1,2})\b)") },
    };

    for (auto& p : patterns)
    {
        auto begin = std::wsregex_iterator(src.begin(), src.end(), p.re);
        auto end   = std::wsregex_iterator();
        for (auto it = begin; it != end; ++it)
        {
            PiiMatch m;
            m.text     = it->str();
            m.category = p.cat;
            m.pos      = static_cast<size_t>(it->position());
            m.len      = m.text.size();
            out.push_back(m);
        }
    }
}

// ---------------------------------------------------------------------------
// CategoryName  –  human-readable label for GUI display
// ---------------------------------------------------------------------------
inline const wchar_t* CategoryName(DWORD cat)
{
    switch (cat)
    {
    case PII_EMAIL:         return L"Email Address";
    case PII_PHONE:         return L"Phone Number";
    case PII_SSN:           return L"Social Security Number";
    case PII_CREDIT_CARD:   return L"Credit Card Number";
    case PII_IP_ADDRESS:    return L"IP Address";
    case PII_DATE_OF_BIRTH: return L"Date of Birth";
    default:                return L"Sensitive Data";
    }
}

// ---------------------------------------------------------------------------
// CategoryIcon  –  emoji/symbol for compact display
// ---------------------------------------------------------------------------
inline const wchar_t* CategoryIcon(DWORD cat)
{
    switch (cat)
    {
    case PII_EMAIL:         return L"\U0001F4E7"; // envelope
    case PII_PHONE:         return L"\U0001F4DE"; // telephone
    case PII_SSN:           return L"\U0001F510"; // locked
    case PII_CREDIT_CARD:   return L"\U0001F4B3"; // credit card
    case PII_IP_ADDRESS:    return L"\U0001F310"; // globe
    case PII_DATE_OF_BIRTH: return L"\U0001F4C5"; // calendar
    default:                return L"\U000026A0"; // warning
    }
}
