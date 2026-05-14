#include "pch.h"
#include "AppSettings.h"

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.h>

namespace winrt::DocRedactorApp::AppSettings
{
    namespace
    {
        // Key names used in LocalSettings. Centralized so renames stay consistent.
        constexpr wchar_t KEY_SAVE_MODE[] = L"SaveMode";
        constexpr wchar_t KEY_FIXED_SAVE_FOLDER[] = L"FixedSaveFolder";
        constexpr wchar_t KEY_DETECTION_FLAGS[] = L"DetectionFlags";

        // String values for SaveMode persistence.
        constexpr wchar_t SAVE_MODE_ASK[] = L"Ask";
        constexpr wchar_t SAVE_MODE_NEXT_TO[] = L"NextTo";
        constexpr wchar_t SAVE_MODE_FIXED[] = L"Fixed";

        // Get the LocalSettings.Values map. Single helper; called by every
        // get/set so we always go through the same code path.
        winrt::Windows::Foundation::Collections::IPropertySet GetValues()
        {
            return winrt::Windows::Storage::ApplicationData::Current().LocalSettings().Values();
        }
    }

    // -------------------------------------------------------------------------
    SaveMode GetSaveMode()
    {
        auto values = GetValues();
        if (auto entry = values.TryLookup(KEY_SAVE_MODE))
        {
            auto str = winrt::unbox_value_or<winrt::hstring>(entry, L"");
            if (str == SAVE_MODE_NEXT_TO) return SaveMode::NextTo;
            if (str == SAVE_MODE_FIXED)   return SaveMode::Fixed;
            // Fall through to default
        }
        return SaveMode::Ask;
    }

    void SetSaveMode(SaveMode mode)
    {
        auto values = GetValues();
        winrt::hstring str;
        switch (mode)
        {
        case SaveMode::NextTo: str = SAVE_MODE_NEXT_TO; break;
        case SaveMode::Fixed:  str = SAVE_MODE_FIXED;   break;
        case SaveMode::Ask:
        default:               str = SAVE_MODE_ASK;    break;
        }
        values.Insert(KEY_SAVE_MODE, winrt::box_value(str));
    }

    // -------------------------------------------------------------------------
    winrt::hstring GetFixedSaveFolder()
    {
        auto values = GetValues();
        if (auto entry = values.TryLookup(KEY_FIXED_SAVE_FOLDER))
        {
            return winrt::unbox_value_or<winrt::hstring>(entry, L"");
        }
        return L"";
    }

    void SetFixedSaveFolder(winrt::hstring const& path)
    {
        auto values = GetValues();
        values.Insert(KEY_FIXED_SAVE_FOLDER, winrt::box_value(path));
    }

    // -------------------------------------------------------------------------
    uint32_t GetDetectionFlags()
    {
        auto values = GetValues();
        if (auto entry = values.TryLookup(KEY_DETECTION_FLAGS))
        {
            uint32_t stored = winrt::unbox_value_or<uint32_t>(entry, DefaultDetectionFlags);
            if ((stored & DefaultDetectionFlags) != DefaultDetectionFlags)
            {
                stored |= DefaultDetectionFlags;
                values.Insert(KEY_DETECTION_FLAGS, winrt::box_value(stored));
            }
            return stored;
        }
        return DefaultDetectionFlags;
    }

    void SetDetectionFlags(uint32_t flags)
    {
        auto values = GetValues();
        values.Insert(KEY_DETECTION_FLAGS, winrt::box_value(flags));
    }

    // -------------------------------------------------------------------------
    bool IsCategoryEnabled(uint32_t categoryFlag)
    {
        return (GetDetectionFlags() & categoryFlag) != 0;
    }
}