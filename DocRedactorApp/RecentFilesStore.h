#pragma once

#include <winrt/base.h>
#include <vector>
#include <cstdint>

namespace winrt::DocRedactorApp
{
    struct RecentFile
    {
        winrt::hstring Path;
        int64_t OpenedAt;  // Unix epoch seconds
    };

    struct RecentFilesStore
    {
        // Loads the full list, sorted by OpenedAt descending. Empty list
        // if nothing's stored or the JSON is malformed.
        static std::vector<RecentFile> Load();

        // Saves the full list. Caller is responsible for sorting/capping
        // before calling; this just persists what it's given.
        static void Save(std::vector<RecentFile> const& entries);

        // Records that `path` was just opened. Bumps OpenedAt if present,
        // inserts new if not. Re-sorts and caps at 10. Persists immediately.
        static void Bump(winrt::hstring const& path);

        // Removes one path from the list. No-op if not present.
        static void Remove(winrt::hstring const& path);

        // Wipes everything.
        static void Clear();

        // Max entries kept. Anything beyond this gets pruned by Bump.
        static constexpr size_t kMaxEntries = 10;

    private:
        // Storage key inside ApplicationData.Current().LocalSettings().
        static constexpr wchar_t kSettingsKey[] = L"RecentFiles";

        // Serialize/parse — hand-rolled for our simple 2-field schema.
        static winrt::hstring Serialize(std::vector<RecentFile> const& entries);
        static std::vector<RecentFile> Parse(winrt::hstring const& json);
    };
}