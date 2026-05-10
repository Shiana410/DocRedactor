#include "pch.h"
#include "XpsParser.h"
#if __has_include("XpsParser.g.cpp")
#include "XpsParser.g.cpp"
#endif
#if __has_include("TextSegment.g.cpp")
#include "TextSegment.g.cpp"
#endif
#if __has_include("PageInfo.g.cpp")
#include "PageInfo.g.cpp"
#endif
#if __has_include("XpsParseResult.g.cpp")
#include "XpsParseResult.g.cpp"
#endif
#if __has_include("PiiMatch.g.cpp")
#include "PiiMatch.g.cpp"
#endif
#if __has_include("PiiDetector.g.cpp")
#include "PiiDetector.g.cpp"
#endif
#if __has_include("Redactor.g.cpp")
#include "Redactor.g.cpp"
#endif

#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.h>

#include "XpsCore.h"
#include "PiiCore.h"
#include "XpsModifierCore.h"

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace winrt::DocRedactorEngine::implementation
{
    int32_t XpsParser::MyProperty()
    {
        throw hresult_not_implemented();
    }

    void XpsParser::MyProperty(int32_t /* value */)
    {
        throw hresult_not_implemented();
    }

    winrt::hstring XpsParser::Greeting()
    {
        return L"Hello from DocRedactorEngine";
    }

    winrt::Windows::Foundation::IAsyncOperation<winrt::DocRedactorEngine::XpsParseResult> XpsParser::ParseAsync(winrt::Windows::Storage::StorageFile file)
    {
        winrt::hstring filePath = file.Path();

        co_await winrt::resume_background();

        dlp::XpsDocument xpsDoc;
        bool ok = dlp::XpsParser::Parse(std::wstring{ filePath }, xpsDoc);

        auto segments = winrt::single_threaded_vector<winrt::DocRedactorEngine::TextSegment>();
        auto pages = winrt::single_threaded_vector<winrt::DocRedactorEngine::PageInfo>();

        if (ok)
        {
            // Convert text segments
            for (auto const& run : xpsDoc.textRuns)
            {
                segments.Append(
                    winrt::make<implementation::TextSegment>(
                        winrt::hstring{ run.text },
                        run.pageIndex,
                        static_cast<float>(run.originX),
                        static_cast<float>(run.originY),
                        static_cast<float>(run.width),
                        static_cast<float>(run.height),
                        static_cast<float>(run.fontSize)));
            }

            // Convert page info
            for (auto const& pi : xpsDoc.pages)
            {
                pages.Append(
                    winrt::make<implementation::PageInfo>(
                        pi.pageIndex,
                        static_cast<float>(pi.width),
                        static_cast<float>(pi.height)));
            }
        }

        co_return winrt::make<implementation::XpsParseResult>(segments.GetView(), pages.GetView());
    }

    winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Foundation::Collections::IVectorView<winrt::DocRedactorEngine::PiiMatch>> PiiDetector::DetectAsync(winrt::Windows::Foundation::Collections::IVectorView<winrt::DocRedactorEngine::TextSegment> segments)
    {
        // Snapshot segment data on the UI thread before yielding, since IVectorView
        // is a projected WinRT object that we don't want to touch from another thread.
        struct SegmentSnapshot
        {
            std::wstring text;
            int32_t pageIndex;
            float originX, originY, width, height;
        };

        std::vector<SegmentSnapshot> snapshot;
        snapshot.reserve(segments.Size());
        for (uint32_t i = 0; i < segments.Size(); ++i)
        {
            auto seg = segments.GetAt(i);
            snapshot.push_back({
                std::wstring{ seg.Text() },
                seg.PageIndex(),
                seg.OriginX(), seg.OriginY(),
                seg.Width(), seg.Height()
                });
        }

        // Yield to a thread-pool thread so regex scanning doesn't block the UI.
        co_await winrt::resume_background();

        // Build the result vector. We populate per-segment so each match
        // remembers which segment it came from — critical for redaction later.
        auto matches = winrt::single_threaded_vector<winrt::DocRedactorEngine::PiiMatch>();

        for (size_t segIdx = 0; segIdx < snapshot.size(); ++segIdx)
        {
            auto const& seg = snapshot[segIdx];

            // Run the salvaged regex scanner against this segment's text.
            std::vector<dlp::PiiMatch> rawMatches;
            dlp::ScanForPii(seg.text, rawMatches);

            // Convert each raw match into a projected PiiMatch with
            // segment provenance attached.
            for (auto const& rm : rawMatches)
            {
                matches.Append(
                    winrt::make<implementation::PiiMatch>(
                        winrt::hstring{ rm.text },
                        static_cast<int32_t>(rm.category),
                        winrt::hstring{ dlp::CategoryName(rm.category) },
                        static_cast<int32_t>(segIdx),
                        static_cast<int32_t>(rm.pos),
                        static_cast<int32_t>(rm.len),
                        seg.pageIndex,
                        seg.originX, seg.originY,
                        seg.width, seg.height));
            }
        }

        co_return matches.GetView();
    }

    winrt::hstring PiiDetector::MaskText(winrt::hstring const& original, int32_t category)
    {
        std::wstring input{ original };
        std::wstring masked = dlp::MaskPiiText(input, static_cast<DWORD>(category));
        return winrt::hstring{ masked };
    }

    winrt::Windows::Foundation::IAsyncOperation<bool> Redactor::RedactAsync(winrt::Windows::Storage::StorageFile inputFile, winrt::Windows::Storage::StorageFile outputFile, winrt::Windows::Foundation::Collections::IVectorView<winrt::DocRedactorEngine::PiiMatch> matches)
    {
        // Snapshot everything we need from projected objects on the calling thread,
        // before yielding. After co_await we'll be on a thread-pool thread and
        // shouldn't touch the projected vector view directly.

        winrt::hstring inputPath = inputFile.Path();
        winrt::hstring outputPath = outputFile.Path();

        // Convert the matches collection to dlp::PiiItem array.
        // dlp::PiiItem expects fixed-size wchar buffers; we copy with truncation safety.
        std::vector<dlp::PiiItem> items;
        items.reserve(matches.Size());

        for (uint32_t i = 0; i < matches.Size(); ++i)
        {
            auto m = matches.GetAt(i);

            dlp::PiiItem item{};  // zero-init: empty text buffer, all fields default

            // Copy match text into the fixed buffer with size limit and null termination.
            std::wstring matchText{ m.Text() };
            wcsncpy_s(item.text, dlp::MAX_PII_TEXT, matchText.c_str(), _TRUNCATE);

            item.category = static_cast<DWORD>(m.Category());
            item.shouldMask = TRUE;  // Phase 3: mask everything by default.
            // Phase 4 may add user toggles.
            item.pageIndex = m.PageIndex();
            item.x = static_cast<double>(m.OriginX());
            item.y = static_cast<double>(m.OriginY());
            item.width = static_cast<double>(m.Width());
            item.height = static_cast<double>(m.Height());

            items.push_back(item);
        }

        // Yield to a thread-pool thread for the COM-heavy redaction work.
        co_await winrt::resume_background();

        // Run the salvaged redactor.
        bool ok = dlp::XpsModifier::ApplyRedactions(
            std::wstring{ inputPath },
            std::wstring{ outputPath },
            items.data(),
            static_cast<DWORD>(items.size()),
            /*deleteText*/ false);

        co_return ok;
    }
}