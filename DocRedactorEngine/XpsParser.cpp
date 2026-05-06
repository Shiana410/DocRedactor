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
        // Yield to a thread-pool thread so we're a real async operation.
        co_await winrt::resume_background();

        // STUB: returns one fake segment naming the file we received.
        // Proves the App <-> Engine projection works end-to-end before
        // the real XPS parser lands in Step 15B.
        auto segments = winrt::single_threaded_vector<winrt::DocRedactorEngine::TextSegment>();

        winrt::hstring fileName = file.Name();
        std::wstring stubText = L"[stub] Parsed file: ";
        stubText += fileName;

        segments.Append(
            winrt::make<implementation::TextSegment>(
                winrt::hstring{ stubText },
                /*pageIndex*/ 0,
                /*originX*/ 0.0f, /*originY*/ 0.0f,
                /*width*/ 100.0f, /*height*/ 12.0f,
                /*fontSize*/ 12.0f));

        co_return segments.GetView();
    }
}