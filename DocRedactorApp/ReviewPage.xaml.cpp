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

            // Update summary line
            std::wstring summary = L"Found ";
            summary += std::to_wstring(count);
            summary += L" segment(s).";
            ResultSummaryText().Text(winrt::hstring{ summary });

            if (count > 0)
            {
                // Populate the ListView with one entry per segment
                SegmentsList().Items().Clear();
                for (uint32_t i = 0; i < count; ++i)
                {
                    auto seg = segments.GetAt(i);

                    std::wstring line = L"[page ";
                    line += std::to_wstring(seg.PageIndex());
                    line += L"] \"";
                    line += std::wstring{ seg.Text() };
                    line += L"\"";

                    auto tb = TextBlock();
                    tb.Text(winrt::hstring{ line });
                    tb.IsTextSelectionEnabled(true);
                    tb.TextWrapping(TextWrapping::Wrap);
                    tb.FontFamily(Media::FontFamily(L"Consolas"));
                    tb.Padding(ThicknessHelper::FromUniformLength(4));

                    SegmentsList().Items().Append(tb);
                }

                // Swap visibility: hide placeholder, show segments
                PlaceholderBorder().Visibility(Visibility::Collapsed);
                SegmentsBorder().Visibility(Visibility::Visible);
            }
            // else: leave placeholder visible, summary still says "Found 0 segment(s)."
        }
        catch (winrt::hresult_error const& ex)
        {
            std::wstring errorText = L"Error: ";
            errorText += std::wstring{ ex.message() };
            ResultSummaryText().Text(winrt::hstring{ errorText });
        }
    }

    void ReviewPage::BackButton_Click(
        IInspectable const& /*sender*/,
        RoutedEventArgs const& /*e*/)
    {
        if (auto frame = Frame())
        {
            if (frame.CanGoBack())
            {
                frame.GoBack();
            }
        }
    }
}