#include "pch.h"
#include "ReviewPage.xaml.h"
#if __has_include("ReviewPage.g.cpp")
#include "ReviewPage.g.cpp"
#endif

#include <winrt/Windows.Storage.h>
#include <winrt/DocRedactorEngine.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
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

            // Step 1: parse the document into text segments.
            auto parser = winrt::DocRedactorEngine::XpsParser{};
            auto segments = co_await parser.ParseAsync(file);

            // Step 2: scan those segments for PII.
            auto detector = winrt::DocRedactorEngine::PiiDetector{};
            auto matches = co_await detector.DetectAsync(segments);

            auto segmentCount = segments.Size();
            auto matchCount = matches.Size();

            // Update summary line: matches first (the headline), segments second (context).
            std::wstring summary = L"Found ";
            summary += std::to_wstring(matchCount);
            summary += L" PII match(es) across ";
            summary += std::to_wstring(segmentCount);
            summary += L" text segment(s).";
            ResultSummaryText().Text(winrt::hstring{ summary });

            if (matchCount > 0)
            {
                // Populate the ListView with one entry per match.
                MatchesList().Items().Clear();
                for (uint32_t i = 0; i < matchCount; ++i)
                {
                    auto m = matches.GetAt(i);

                    std::wstring line = L"[";
                    line += std::wstring{ m.CategoryName() };
                    line += L"] \"";
                    line += std::wstring{ m.Text() };
                    line += L"\"  (page ";
                    line += std::to_wstring(m.PageIndex());
                    line += L", segment ";
                    line += std::to_wstring(m.SegmentIndex());
                    line += L")";

                    auto tb = TextBlock();
                    tb.Text(winrt::hstring{ line });
                    tb.IsTextSelectionEnabled(true);
                    tb.TextWrapping(TextWrapping::Wrap);
                    tb.FontFamily(Media::FontFamily(L"Consolas"));
                    tb.Padding(ThicknessHelper::FromUniformLength(4));

                    MatchesList().Items().Append(tb);
                }

                PlaceholderBorder().Visibility(Visibility::Collapsed);
                MatchesBorder().Visibility(Visibility::Visible);
            }
            // else: no PII found, leave placeholder visible. Summary tells the user
            // we did look — "Found 0 PII match(es) across N text segment(s)."
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