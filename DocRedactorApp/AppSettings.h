#pragma once

#include <winrt/base.h>
#include <cstdint>

namespace winrt::DocRedactorApp::AppSettings
{
    // ---------------------------------------------------------------------
    // Save mode — controls what happens when the user clicks "Redact and Save".
    // ---------------------------------------------------------------------
    enum class SaveMode : int32_t
    {
        Ask = 0,        // Show FileSavePicker on each redact (default)
        NextTo = 1,     // Auto-save as <input>_redacted.oxps in source folder
        Fixed = 2,      // Auto-save to FixedSaveFolder
    };

    // Read the current save mode from LocalSettings. Returns Ask if unset.
    SaveMode GetSaveMode();

    // Persist a new save mode. Subsequent GetSaveMode calls (this run or a
    // future run) will return this value.
    void SetSaveMode(SaveMode mode);

    // ---------------------------------------------------------------------
    // Fixed save folder — only used when SaveMode == Fixed.
    // ---------------------------------------------------------------------
    // Returns the persisted folder path, or empty string if unset.
    winrt::hstring GetFixedSaveFolder();

    void SetFixedSaveFolder(winrt::hstring const& path);

    // ---------------------------------------------------------------------
    // PII detection category flags — uint32 bitmask of dlp::PII_* values.
    // Default = 0x3F (all six categories enabled).
    // ---------------------------------------------------------------------
    // PII detection category flags. Values match dlp::PII_* in the engine,
    // but defined here so the App doesn't need to include engine headers.
    constexpr uint32_t Pii_Email = 0x01;
    constexpr uint32_t Pii_Phone = 0x02;
    constexpr uint32_t Pii_Ssn = 0x04;
    constexpr uint32_t Pii_CreditCard = 0x08;
    constexpr uint32_t Pii_IpAddress = 0x10;
    constexpr uint32_t Pii_DateOfBirth = 0x20;

    constexpr uint32_t DefaultDetectionFlags = 0x3F;  // All six enabled

    uint32_t GetDetectionFlags();
    void SetDetectionFlags(uint32_t flags);

    // Convenience: check whether a specific category is enabled.
    bool IsCategoryEnabled(uint32_t categoryFlag);
}