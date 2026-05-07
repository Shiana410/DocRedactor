#pragma once
// =============================================================================
// PiiShared.h - PII category constants from DlpShared.h
// =============================================================================
// Trimmed-down salvage of Common/DlpShared.h. We only need the PII_* category
// bitmask values for the engine's regex detection. Printer driver constants
// from the original file (DLP_SPOOL_FOLDER, DlpJobConfig, etc.) are not
// applicable to our packaged-app architecture and are intentionally omitted.
// =============================================================================

#include <windows.h>

namespace dlp
{
    // PII category flags - matched by ScanForPii regex patterns.
    // Values match Common/DlpShared.h to keep salvaged code compatible.
    constexpr DWORD PII_EMAIL         = 0x01;
    constexpr DWORD PII_PHONE         = 0x02;
    constexpr DWORD PII_SSN           = 0x04;
    constexpr DWORD PII_CREDIT_CARD   = 0x08;
    constexpr DWORD PII_IP_ADDRESS    = 0x10;
    constexpr DWORD PII_DATE_OF_BIRTH = 0x20;
}