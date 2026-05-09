#pragma once
// =============================================================================
// PiiShared.h - PII category constants and shared PII item type
// =============================================================================
// Trimmed-down salvage of Common/DlpShared.h. We keep:
//   - PII_* category bitmask constants (used by PiiCore regex scanner)
//   - MAX_PII_TEXT / MAX_PII_ITEMS size limits (used by PiiItem buffer sizing)
//   - PiiItem struct (used by XpsModifier as input for redaction)
//
// We deliberately omit the printer-driver constants (DLP_SPOOL_FOLDER,
// DlpJobConfig, etc.) from the original DlpShared.h — they're not applicable
// to our packaged-app architecture.
// =============================================================================

#include <windows.h>

namespace dlp
{
    // PII category flags - matched by ScanForPii regex patterns.
    constexpr DWORD PII_EMAIL = 0x01;
    constexpr DWORD PII_PHONE = 0x02;
    constexpr DWORD PII_SSN = 0x04;
    constexpr DWORD PII_CREDIT_CARD = 0x08;
    constexpr DWORD PII_IP_ADDRESS = 0x10;
    constexpr DWORD PII_DATE_OF_BIRTH = 0x20;

    // Size limits matching original DlpShared.h
    constexpr size_t MAX_PII_TEXT = 256;
    constexpr size_t MAX_PII_ITEMS = 512;

    // A single detected PII hit, formatted for redaction input.
    // Mirrors the original PiiItem struct from DlpShared.h - the salvaged
    // XpsModifier code expects this exact shape (fixed-size buffers).
    struct PiiItem
    {
        wchar_t  text[MAX_PII_TEXT];     // The matched PII string
        DWORD    category;               // PII_* bitmask
        BOOL     shouldMask;             // TRUE = mask this match, FALSE = skip it
        int      pageIndex;              // 0-based page number where found
        double   x, y, width, height;    // Bounding box (1/96 inch units)
    };
}