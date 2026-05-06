#include "pch.h"
#include "ReviewPage.xaml.h"
#if __has_include("ReviewPage.g.cpp")
#include "ReviewPage.g.cpp"
#endif

#include <winrt/Windows.Storage.h>
#include <winrt/DocRedactorEngine.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Navigation.h>

using namespace winrt;
using namespace Windows::Foundation;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Navigation;

namespace winrt::DocRedactorApp::implementation
{
    ReviewPage::ReviewPage()
    {
        InitializeComponent();
    }

    winrt::Windows::Foundation::IAsyncAction ReviewPage::OnNavigatedTo(NavigationEventArgs const& e)
    {
        auto path = unbox_value_or<hstring>(e.Parameter(), L"");
        FilePathText().Text(path);

        if (path.empty())
        {
            co_return;
        }

        try
        {
            auto file = co_await winrt::Windows::Storage::StorageFile::GetFileFromPathAsync(path);

            auto parser = winrt::DocRedactorEngine::XpsParser{};
            auto segments = co_await parser.ParseAsync(file);

            auto count = segments.Size();
            std::wstring resultText = L"Found ";
            resultText += std::to_wstring(count);
            resultText += L" segment(s).";

            if (count > 0)
            {
                resultText += L"\nFirst: ";
                resultText += std::wstring{ segments.GetAt(0).Text() };
            }

            ParseResultText().Text(winrt::hstring{ resultText });
        }
        catch (winrt::hresult_error const& ex)
        {
            std::wstring errorText = L"Error: ";
            errorText += std::wstring{ ex.message() };
            ParseResultText().Text(winrt::hstring{ errorText });
        }
    }

    void ReviewPage::BackButton_Click(
        IInspectable const& /*sender*/,
        RoutedEventArgs const& /*e*/)
    {
        // Walk up the visual tree to find the hosting Frame, then go back.
        // In WinUI 3, Frame() on a Page returns the parent Frame if any.
        if (auto frame = Frame())
        {
            if (frame.CanGoBack())
            {
                frame.GoBack();
            }
        }
    }
}