#include "pch.h"
#include "RecentFilesStore.h"

#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>

#include <algorithm>
#include <chrono>
#include <sstream>
#include <string>

namespace winrt::DocRedactorApp
{
    static int64_t NowUnixSeconds()
    {
        using namespace std::chrono;
        return duration_cast<seconds>(
            system_clock::now().time_since_epoch()).count();
    }

    // ---- JSON helpers (hand-rolled for our exact schema) ------------------

    // Escape backslashes and double-quotes for embedding in a JSON string.
    // We don't handle every JSON escape — paths from Windows StorageFile.Path()
    // only contain backslashes plus normal filename chars; nothing exotic.
    static std::wstring JsonEscape(winrt::hstring const& s)
    {
        std::wstring out;
        out.reserve(s.size() + 8);
        for (wchar_t c : std::wstring_view{ s })
        {
            switch (c)
            {
            case L'\\': out += L"\\\\"; break;
            case L'"':  out += L"\\\""; break;
            case L'\n': out += L"\\n";  break;
            case L'\r': out += L"\\r";  break;
            case L'\t': out += L"\\t";  break;
            default:    out += c;       break;
            }
        }
        return out;
    }

    // Inverse of JsonEscape. Reads from `pos` in `s` until it sees an
    // unescaped closing quote; advances `pos` past the close quote.
    static std::wstring JsonUnescape(std::wstring const& s, size_t& pos)
    {
        std::wstring out;
        while (pos < s.size() && s[pos] != L'"')
        {
            if (s[pos] == L'\\' && pos + 1 < s.size())
            {
                wchar_t next = s[pos + 1];
                switch (next)
                {
                case L'\\': out += L'\\'; break;
                case L'"':  out += L'"';  break;
                case L'n':  out += L'\n'; break;
                case L'r':  out += L'\r'; break;
                case L't':  out += L'\t'; break;
                default:    out += next;  break;
                }
                pos += 2;
            }
            else
            {
                out += s[pos];
                ++pos;
            }
        }
        if (pos < s.size()) ++pos;  // skip closing quote
        return out;
    }

    winrt::hstring RecentFilesStore::Serialize(std::vector<RecentFile> const& entries)
    {
        std::wstringstream ss;
        ss << L"[";
        for (size_t i = 0; i < entries.size(); ++i)
        {
            if (i > 0) ss << L",";
            ss << L"{\"path\":\"" << JsonEscape(entries[i].Path)
                << L"\",\"openedAt\":" << entries[i].OpenedAt << L"}";
        }
        ss << L"]";
        return winrt::hstring{ ss.str() };
    }

    std::vector<RecentFile> RecentFilesStore::Parse(winrt::hstring const& json)
    {
        std::vector<RecentFile> result;
        std::wstring s{ json };

        // Tiny state machine: find each `{"path":"...","openedAt":N}` and pull
        // the two fields. Order-tolerant (works even if openedAt comes first),
        // whitespace-tolerant. Anything unexpected aborts parsing and returns
        // what we've collected so far — graceful degradation on corruption.
        size_t pos = 0;
        auto skipWs = [&]() { while (pos < s.size() && (s[pos] == L' ' || s[pos] == L'\t' || s[pos] == L'\n' || s[pos] == L'\r')) ++pos; };

        skipWs();
        if (pos >= s.size() || s[pos] != L'[') return result;
        ++pos;

        while (pos < s.size())
        {
            skipWs();
            if (pos >= s.size()) break;
            if (s[pos] == L']') break;
            if (s[pos] == L',') { ++pos; continue; }
            if (s[pos] != L'{') break;
            ++pos;

            RecentFile entry{};
            entry.OpenedAt = 0;
            bool hadPath = false;
            bool hadOpenedAt = false;

            while (pos < s.size() && s[pos] != L'}')
            {
                skipWs();
                if (s[pos] == L',') { ++pos; skipWs(); }
                if (s[pos] != L'"') break;
                ++pos;

                // Read key.
                std::wstring key = JsonUnescape(s, pos);
                skipWs();
                if (pos >= s.size() || s[pos] != L':') break;
                ++pos;
                skipWs();

                if (key == L"path")
                {
                    if (pos >= s.size() || s[pos] != L'"') break;
                    ++pos;
                    entry.Path = winrt::hstring{ JsonUnescape(s, pos) };
                    hadPath = true;
                }
                else if (key == L"openedAt")
                {
                    // Parse signed integer.
                    std::wstring numBuf;
                    if (pos < s.size() && (s[pos] == L'-' || s[pos] == L'+'))
                    {
                        numBuf += s[pos];
                        ++pos;
                    }
                    while (pos < s.size() && s[pos] >= L'0' && s[pos] <= L'9')
                    {
                        numBuf += s[pos];
                        ++pos;
                    }
                    try { entry.OpenedAt = std::stoll(numBuf); hadOpenedAt = true; }
                    catch (...) { /* leave as 0 */ }
                }
                else
                {
                    // Unknown key — skip until next , or }
                    while (pos < s.size() && s[pos] != L',' && s[pos] != L'}') ++pos;
                }
                skipWs();
            }

            if (pos < s.size() && s[pos] == L'}') ++pos;

            if (hadPath && hadOpenedAt)
            {
                result.push_back(std::move(entry));
            }
        }

        return result;
    }

    // ---- Public API ------------------------------------------------------

    std::vector<RecentFile> RecentFilesStore::Load()
    {
        try
        {
            auto local = Windows::Storage::ApplicationData::Current().LocalSettings();
            auto values = local.Values();
            auto raw = values.Lookup(kSettingsKey);
            if (raw == nullptr) return {};

            auto json = winrt::unbox_value_or<winrt::hstring>(raw, L"");
            if (json.empty()) return {};

            auto entries = Parse(json);

            // Sort descending by OpenedAt — defensive in case stored order
            // was corrupted, also handy if Bump's caller forgot.
            std::sort(entries.begin(), entries.end(),
                [](RecentFile const& a, RecentFile const& b) {
                    return a.OpenedAt > b.OpenedAt;
                });

            if (entries.size() > kMaxEntries)
            {
                entries.resize(kMaxEntries);
            }
            return entries;
        }
        catch (...)
        {
            return {};
        }
    }

    void RecentFilesStore::Save(std::vector<RecentFile> const& entries)
    {
        try
        {
            auto local = Windows::Storage::ApplicationData::Current().LocalSettings();
            local.Values().Insert(kSettingsKey, winrt::box_value(Serialize(entries)));
        }
        catch (...) { /* best effort */ }
    }

    void RecentFilesStore::Bump(winrt::hstring const& path)
    {
        if (path.empty()) return;

        auto entries = Load();

        // Remove existing entry with same path (case-insensitive on Windows).
        std::wstring pathLower{ path };
        std::transform(pathLower.begin(), pathLower.end(), pathLower.begin(), ::towlower);

        entries.erase(std::remove_if(entries.begin(), entries.end(),
            [&](RecentFile const& e) {
                std::wstring existing{ e.Path };
                std::transform(existing.begin(), existing.end(), existing.begin(), ::towlower);
                return existing == pathLower;
            }), entries.end());

        // Insert at front with current timestamp.
        entries.insert(entries.begin(), RecentFile{ path, NowUnixSeconds() });

        // Cap.
        if (entries.size() > kMaxEntries)
        {
            entries.resize(kMaxEntries);
        }

        Save(entries);
    }

    void RecentFilesStore::Remove(winrt::hstring const& path)
    {
        if (path.empty()) return;
        auto entries = Load();

        std::wstring pathLower{ path };
        std::transform(pathLower.begin(), pathLower.end(), pathLower.begin(), ::towlower);

        entries.erase(std::remove_if(entries.begin(), entries.end(),
            [&](RecentFile const& e) {
                std::wstring existing{ e.Path };
                std::transform(existing.begin(), existing.end(), existing.begin(), ::towlower);
                return existing == pathLower;
            }), entries.end());

        Save(entries);
    }

    void RecentFilesStore::Clear()
    {
        try
        {
            auto local = Windows::Storage::ApplicationData::Current().LocalSettings();
            local.Values().Remove(kSettingsKey);
        }
        catch (...) {}
    }
}