#include "pch.h"
#include "XpsParser.h"
#if __has_include("XpsParser.g.cpp")
#include "XpsParser.g.cpp"
#endif
#if __has_include("TextSegment.g.cpp")
#include "TextSegment.g.cpp"
#endif

#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.h>

#include "XpsCore.h"

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
}