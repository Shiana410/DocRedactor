#include "pch.h"
#include "XpsParser.h"
#if __has_include("XpsParser.g.cpp")
#include "XpsParser.g.cpp"
#endif
#if __has_include("TextSegment.g.cpp")
#include "TextSegment.g.cpp"
#endif
#if __has_include("PiiMatch.g.cpp")
#include "PiiMatch.g.cpp"
#endif
#if __has_include("PiiDetector.g.cpp")
#include "PiiDetector.g.cpp"
#endif

#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.h>

#include "XpsCore.h"
#include "PiiCore.h"

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

    winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Foundation::Collections::IVectorView<winrt::DocRedactorEngine::TextSegment>> XpsParser::ParseAsync(winrt::Windows::Storage::StorageFile file)
    {
        // Capture the path on the UI thread before we yield, since file is a
        // projected WinRT object that we want to stop touching once we move off thread.
        winrt::hstring filePath = file.Path();

        // Yield to a thread-pool thread so the COM-heavy XPS parsing work
        // doesn't block the UI thread.
        co_await winrt::resume_background();

        // Run the salvaged XPS extractor.
        dlp::XpsDocument xpsDoc;
        bool ok = dlp::XpsParser::Parse(std::wstring{ filePath }, xpsDoc);

        // Build the projected vector. We do this even on failure (returning empty)
        // so the App side gets a well-formed empty result instead of an exception.
        auto segments = winrt::single_threaded_vector<winrt::DocRedactorEngine::TextSegment>();

        if (ok)
        {
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
        }

        co_return segments.GetView();
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
}