#pragma once
// =============================================================================
// PiiCore.h - Regex-based PII scanner
// =============================================================================
// Wrapped in dlp:: namespace to keep these symbols out of the global namespace.
// Original at Common/PiiDetect.h - this is a salvaged copy with namespace and
// include adjustments only; logic body is unchanged.
// =============================================================================

#ifndef PII_CORE_H
#define PII_CORE_H

#include <string>
#include <vector>
#include <regex>
#include "PiiShared.h"

namespace dlp
{

struct PiiMatch
{
    std::wstring text;
    DWORD        category;
    size_t       pos;       // character offset in source string
    size_t       len;
};

inline void ScanForPii(const std::wstring& src, std::vector<PiiMatch>& out)
{
    struct Pattern { DWORD cat; std::wregex re; };

    // Patterns run in priority order: most-specific first. Once a character
    // range is claimed by an earlier (higher-priority) match, later patterns
    // cannot match into that range. This prevents Phone from eating the
    // first 10 digits of a 16-digit credit card, etc.
    //
    // Order rationale:
    //   CreditCard — long, structured, distinctive (13-19 digits)
    //   SSN        — exact 9 digits with two separators
    //   IpAddress  — 4 dotted octets with range validation
    //   DateOfBirth — date-shaped triplet
    //   Email      — has @, very low false-positive rate
    //   Phone      — last resort; loosely shaped, would over-match without
    //                the upstream claims protecting structured PII
    static const Pattern patterns[] =
    {
        // Credit card (13-19 digits with optional spaces/dashes every 4)
        { PII_CREDIT_CARD,
          std::wregex(LR"(\b(?:\d[ \-]?){13,18}\d\b)") },
          // SSN (123-45-6789 or 123 45 6789)
          { PII_SSN,
            std::wregex(LR"(\b\d{3}[\-\s]?\d{2}[\-\s]?\d{4}\b)") },
            // IPv4
            { PII_IP_ADDRESS,
              std::wregex(LR"(\b(?:25[0-5]|2[0-4]\d|[01]?\d\d?)(?:\.(?:25[0-5]|2[0-4]\d|[01]?\d\d?)){3}\b)") },
              // Date of birth (MM/DD/YYYY, DD-MM-YYYY, YYYY-MM-DD)
              { PII_DATE_OF_BIRTH,
                std::wregex(LR"(\b(?:\d{1,2}[/\-]\d{1,2}[/\-]\d{2,4}|\d{4}[/\-]\d{1,2}[/\-]\d{1,2})\b)") },
                // Email (RFC 5321 simplified)
                { PII_EMAIL,
                  std::wregex(LR"([a-zA-Z0-9._%+\-]+@[a-zA-Z0-9.\-]+\.[a-zA-Z]{2,})",
                              std::regex::icase) },
                              // US phone (various formats)
                              { PII_PHONE,
                                std::wregex(LR"((\+?1[\s\-\.]?)?\(?\d{3}\)?[\s\-\.]?\d{3}[\s\-\.]?\d{4})") },
    };

    // Claimed character ranges. Each pair is [start, end) — exclusive end.
    // After each pattern's matches are committed, their ranges are added here.
    std::vector<std::pair<size_t, size_t>> claimed;

    // Helper: does [start, end) overlap any existing claim?
    auto overlapsClaim = [&claimed](size_t start, size_t end) -> bool {
        for (auto const& c : claimed)
        {
            // Two ranges overlap iff start < c.end AND c.start < end.
            if (start < c.second && c.first < end) return true;
        }
        return false;
        };

    for (auto& p : patterns)
    {
        try
        {
            auto beginIt = std::wsregex_iterator(src.begin(), src.end(), p.re);
            auto endIt = std::wsregex_iterator();
            for (auto it = beginIt; it != endIt; ++it)
            {
                size_t matchStart = static_cast<size_t>(it->position());
                size_t matchLen = it->str().size();
                size_t matchEnd = matchStart + matchLen;

                // Skip if a higher-priority category already claimed any part
                // of this character range.
                if (overlapsClaim(matchStart, matchEnd))
                {
                    continue;
                }

                PiiMatch m;
                m.text = it->str();
                m.category = p.cat;
                m.pos = matchStart;
                m.len = matchLen;
                out.push_back(m);

                claimed.emplace_back(matchStart, matchEnd);
            }
        }
        catch (std::regex_error const&)
        {
            // If a regex throws (malformed pattern), skip the category
            // gracefully. Production logging would go here.
        }
    }
}

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

} // namespace dlp

#endif // PII_CORE_H