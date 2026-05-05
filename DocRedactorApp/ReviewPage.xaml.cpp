#include "pch.h"
#include "ReviewPage.xaml.h"
#if __has_include("ReviewPage.g.cpp")
#include "ReviewPage.g.cpp"
#endif

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

    void ReviewPage::OnNavigatedTo(NavigationEventArgs const& e)
    {
        // The navigation parameter is a boxed hstring containing the file path.
        // unbox_value_or returns a default if the box is null or wrong type --
        // safer than unbox_value which throws.
        auto path = winrt::unbox_value_or<winrt::hstring>(e.Parameter(), L"(no file)");
        FilePathText().Text(path);

        std::wstring msg = L"[DocRedactor] ReviewPage navigated to with path: ";
        msg += path;
        msg += L"\n";
        OutputDebugStringW(msg.c_str());
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