#include "pch.h"
#include "ReviewPage.xaml.h"
#if __has_include("ReviewPage.g.cpp")
#include "ReviewPage.g.cpp"
#endif

#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Pickers.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/DocRedactorEngine.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.Shapes.h>
#include <winrt/Microsoft.UI.Xaml.Navigation.h>
#include <winrt/Microsoft.UI.Xaml.Documents.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Microsoft.UI.Text.h>
#include <winrt/Microsoft.UI.h>

#include <Shobjidl.h>
#include "App.xaml.h"
#include "AppSettings.h"

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Storage;
using namespace Windows::Storage::Pickers;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Navigation;

namespace winrt::DocRedactorApp::implementation
{
    ReviewPage::ReviewPage()
    {
        InitializeComponent();

        MatchesList().SelectionChanged(
            { this, &ReviewPage::MatchesList_SelectionChanged });
    }

    winrt::Windows::Foundation::IAsyncAction ReviewPage::OnNavigatedTo(NavigationEventArgs const& e)
    {
        auto path = unbox_value_or<hstring>(e.Parameter(), L"");
        FilePathText().Text(path);
        m_inputFilePath = path;

        if (path.empty())
        {
            co_return;
        }

        try
        {
            auto file = co_await StorageFile::GetFileFromPathAsync(path);
            m_inputFile = file;

            auto parser = winrt::DocRedactorEngine::XpsParser{};
            m_parseResult = co_await parser.ParseAsync(file);
            auto segments = m_parseResult.Segments();

            auto detector = winrt::DocRedactorEngine::PiiDetector{};
            auto rawMatches = co_await detector.DetectAsync(segments);

            // Filter raw matches against the user's detection flag settings.
            // Categories disabled in Settings get dropped before display.
            uint32_t enabledFlags = AppSettings::GetDetectionFlags();
            // DIAGNOSTIC: log the actual flag value.
            {
                wchar_t buf[64];
                swprintf_s(buf, L"[ReviewPage] enabledFlags = 0x%02X (raw count=%u)\n",
                    enabledFlags, rawMatches.Size());
                OutputDebugStringW(buf);
            }

            auto filteredMatches = winrt::single_threaded_vector<winrt::DocRedactorEngine::PiiMatch>();
            for (uint32_t i = 0; i < rawMatches.Size(); ++i)
            {
                auto m = rawMatches.GetAt(i);
                uint32_t cat = static_cast<uint32_t>(m.Category());
                if ((enabledFlags & cat) != 0)
                {
                    filteredMatches.Append(m);
                }
            }

            auto segmentCount = segments.Size();
            auto matchCount = filteredMatches.Size();

            std::wstring summary = L"Found ";
            summary += std::to_wstring(matchCount);
            summary += L" PII match(es) across ";
            summary += std::to_wstring(segmentCount);
            summary += L" text segment(s).";
            ResultSummaryText().Text(winrt::hstring{ summary });

            if (matchCount > 0)
            {
                m_matchViewModels = winrt::single_threaded_observable_vector<winrt::DocRedactorApp::PiiMatchViewModel>();
                for (uint32_t i = 0; i < matchCount; ++i)
                {
                    auto vm = winrt::make<implementation::PiiMatchViewModel>(filteredMatches.GetAt(i));

                    auto weakSelf = get_weak();
                    vm.PropertyChanged([weakSelf](auto const&, Microsoft::UI::Xaml::Data::PropertyChangedEventArgs const& args)
                        {
                            if (args.PropertyName() == L"ShouldMask")
                            {
                                if (auto strongSelf = weakSelf.get())
                                {
                                    strongSelf->UpdateRedactButtonState();
                                    strongSelf->RefreshSegmentDisplayText();
                                }
                            }
                        });

                    m_matchViewModels.Append(vm);
                }

                MatchesList().ItemsSource(m_matchViewModels);

                PlaceholderBorder().Visibility(Visibility::Collapsed);
                MatchesBorder().Visibility(Visibility::Visible);

                UpdateRedactButtonState();

                RenderPreview();
            }
        }
        catch (winrt::hresult_error const& ex)
        {
            std::wstring errorText = L"Error: ";
            errorText += std::wstring{ ex.message() };
            ResultSummaryText().Text(winrt::hstring{ errorText });
        }
    }

    winrt::Windows::Foundation::IAsyncAction ReviewPage::RedactButton_Click(
        IInspectable const& /*sender*/,
        RoutedEventArgs const& /*e*/)
    {
        if (m_inputFile == nullptr || m_matchViewModels == nullptr || m_matchViewModels.Size() == 0)
        {
            co_return;
        }

        // Filter to checked matches.
        auto keptMatches = winrt::single_threaded_vector<winrt::DocRedactorEngine::PiiMatch>();
        for (uint32_t i = 0; i < m_matchViewModels.Size(); ++i)
        {
            auto vm = m_matchViewModels.GetAt(i);
            if (vm.ShouldMask())
            {
                keptMatches.Append(vm.Match());
            }
        }

        if (keptMatches.Size() == 0)
        {
            StatusBar().Severity(InfoBarSeverity::Warning);
            StatusBar().Title(L"Nothing to redact");
            StatusBar().Message(L"All matches are unchecked. Check at least one match before redacting.");
            StatusBar().IsOpen(true);
            co_return;
        }

        RedactButton().IsEnabled(false);
        auto originalContent = RedactButton().Content();
        RedactButton().Content(box_value(L"Redacting..."));

        try
        {
            // Build "<name>_redacted<.ext>" from input file name.
            auto buildSuggestedName = [](winrt::hstring const& origName) -> winrt::hstring {
                std::wstring n{ origName };
                size_t dotPos = n.rfind(L'.');
                if (dotPos != std::wstring::npos)  n.insert(dotPos, L"_redacted");
                else                                n += L"_redacted";
                return winrt::hstring{ n };
                };

            StorageFile outputFile{ nullptr };
            auto saveMode = AppSettings::GetSaveMode();

            if (saveMode == AppSettings::SaveMode::Ask)
            {
                // Mode A: FileSavePicker handles its own naming and collision detection.
                FileSavePicker picker;
                picker.SuggestedStartLocation(PickerLocationId::DocumentsLibrary);
                picker.SuggestedFileName(buildSuggestedName(m_inputFile.Name()));
                picker.FileTypeChoices().Insert(L"OpenXPS Document",
                    winrt::single_threaded_vector<winrt::hstring>({ L".oxps" }));
                picker.FileTypeChoices().Insert(L"XPS Document",
                    winrt::single_threaded_vector<winrt::hstring>({ L".xps" }));

                HWND hwnd = implementation::App::MainWindowHandle();
                auto initWithWindow = picker.as<::IInitializeWithWindow>();
                initWithWindow->Initialize(hwnd);

                outputFile = co_await picker.PickSaveFileAsync();

                if (outputFile == nullptr)
                {
                    RedactButton().Content(originalContent);
                    RedactButton().IsEnabled(true);
                    co_return;
                }
            }
            else if (saveMode == AppSettings::SaveMode::NextTo)
            {
                // Mode B: prompt for name + handle collisions in source's parent folder.
                auto parentFolder = co_await m_inputFile.GetParentAsync();
                if (parentFolder == nullptr)
                {
                    StatusBar().Severity(InfoBarSeverity::Error);
                    StatusBar().Title(L"Cannot save next to original");
                    StatusBar().Message(L"The source file's parent folder is not accessible. Please change save mode in Settings.");
                    StatusBar().IsOpen(true);
                    RedactButton().Content(originalContent);
                    RedactButton().IsEnabled(true);
                    co_return;
                }
                outputFile = co_await ResolveOutputFileAsync(
                    parentFolder,
                    buildSuggestedName(m_inputFile.Name()));
            }
            else // SaveMode::Fixed
            {
                auto fixedFolderPath = AppSettings::GetFixedSaveFolder();
                if (fixedFolderPath.empty())
                {
                    StatusBar().Severity(InfoBarSeverity::Error);
                    StatusBar().Title(L"No fixed folder configured");
                    StatusBar().Message(L"Set a folder in Settings, or change save mode to 'Ask each time' or 'Save next to original'.");
                    StatusBar().IsOpen(true);
                    RedactButton().Content(originalContent);
                    RedactButton().IsEnabled(true);
                    co_return;
                }

                auto fixedFolder = co_await StorageFolder::GetFolderFromPathAsync(fixedFolderPath);
                outputFile = co_await ResolveOutputFileAsync(
                    fixedFolder,
                    buildSuggestedName(m_inputFile.Name()));
            }

            // User cancelled the name/collision dialog.
            if (outputFile == nullptr)
            {
                RedactButton().Content(originalContent);
                RedactButton().IsEnabled(true);
                co_return;
            }

            // Common path: redact + report.
            auto redactor = winrt::DocRedactorEngine::Redactor{};
            bool ok = co_await redactor.RedactAsync(m_inputFile, outputFile, keptMatches.GetView());

            if (ok)
            {
                std::wstring msg = L"Redacted document saved as: ";
                msg += std::wstring{ outputFile.Name() };
                msg += L" (";
                msg += std::to_wstring(keptMatches.Size());
                msg += L" of ";
                msg += std::to_wstring(m_matchViewModels.Size());
                msg += L" matches redacted)";

                StatusBar().Severity(InfoBarSeverity::Success);
                StatusBar().Title(L"Redaction complete");
                StatusBar().Message(winrt::hstring{ msg });
                StatusBar().IsOpen(true);
            }
            else
            {
                StatusBar().Severity(InfoBarSeverity::Error);
                StatusBar().Title(L"Redaction failed");
                StatusBar().Message(L"The XPS modifier could not write the output file. The original is unchanged.");
                StatusBar().IsOpen(true);
            }
        }
        catch (winrt::hresult_error const& ex)
        {
            std::wstring errorText = L"Error: ";
            errorText += std::wstring{ ex.message() };

            StatusBar().Severity(InfoBarSeverity::Error);
            StatusBar().Title(L"Redaction error");
            StatusBar().Message(winrt::hstring{ errorText });
            StatusBar().IsOpen(true);
        }

        RedactButton().Content(originalContent);
        RedactButton().IsEnabled(true);
    }

    void ReviewPage::UpdateRedactButtonState()
    {
        if (m_matchViewModels == nullptr || m_matchViewModels.Size() == 0)
        {
            RedactButton().IsEnabled(false);
            return;
        }

        bool anyChecked = false;
        for (uint32_t i = 0; i < m_matchViewModels.Size(); ++i)
        {
            if (m_matchViewModels.GetAt(i).ShouldMask())
            {
                anyChecked = true;
                break;
            }
        }

        RedactButton().IsEnabled(anyChecked);
    }

    winrt::Windows::Foundation::IAsyncOperation<winrt::hstring> ReviewPage::PromptForFilenameAsync(
        winrt::hstring const& suggestedName)
    {
        // Build the input TextBox programmatically.
        Microsoft::UI::Xaml::Controls::TextBox input;
        input.Text(suggestedName);
        input.SelectAll();
        input.AcceptsReturn(false);

        // Wrap in a stack so we can add an explanatory label above.
        Microsoft::UI::Xaml::Controls::StackPanel stack;
        stack.Spacing(8);

        Microsoft::UI::Xaml::Controls::TextBlock label;
        label.Text(L"Enter a filename for the redacted document:");
        label.TextWrapping(TextWrapping::Wrap);

        stack.Children().Append(label);
        stack.Children().Append(input);

        Microsoft::UI::Xaml::Controls::ContentDialog dialog;
        dialog.Title(box_value(L"Save redacted document"));
        dialog.Content(stack);
        dialog.PrimaryButtonText(L"Save");
        dialog.CloseButtonText(L"Cancel");
        dialog.DefaultButton(Microsoft::UI::Xaml::Controls::ContentDialogButton::Primary);
        dialog.XamlRoot(this->XamlRoot());

        auto result = co_await dialog.ShowAsync();

        if (result == Microsoft::UI::Xaml::Controls::ContentDialogResult::Primary)
        {
            co_return input.Text();
        }
        co_return winrt::hstring{};  // empty = cancelled
    }

    winrt::Windows::Foundation::IAsyncOperation<int32_t> ReviewPage::PromptForOverwriteAsync(
        winrt::hstring const& existingName)
    {
        std::wstring msg = L"A file named \"";
        msg += std::wstring{ existingName };
        msg += L"\" already exists in this location. What would you like to do?";

        Microsoft::UI::Xaml::Controls::ContentDialog dialog;
        dialog.Title(box_value(L"File already exists"));
        dialog.Content(box_value(winrt::hstring{ msg }));
        dialog.PrimaryButtonText(L"Overwrite");
        dialog.SecondaryButtonText(L"Rename...");
        dialog.CloseButtonText(L"Cancel");
        dialog.DefaultButton(Microsoft::UI::Xaml::Controls::ContentDialogButton::Secondary);
        dialog.XamlRoot(this->XamlRoot());

        auto result = co_await dialog.ShowAsync();

        using R = Microsoft::UI::Xaml::Controls::ContentDialogResult;
        switch (result)
        {
        case R::Primary:    co_return 0;  // Overwrite
        case R::Secondary:  co_return 1;  // Rename
        default:            co_return 2;  // Cancel (or close button)
        }
    }

    winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Storage::StorageFile>
        ReviewPage::ResolveOutputFileAsync(
            winrt::Windows::Storage::StorageFolder const& folder,
            winrt::hstring const& suggestedName)
    {
        winrt::hstring currentName = suggestedName;

        while (true)
        {
            // 1. Prompt for filename.
            currentName = co_await PromptForFilenameAsync(currentName);
            if (currentName.empty())
            {
                // User cancelled the name prompt.
                co_return nullptr;
            }

            // 2. Check if a file with that name already exists in the target folder.
            auto existing = co_await folder.TryGetItemAsync(currentName);
            if (existing == nullptr)
            {
                // No conflict - create and return.
                co_return co_await folder.CreateFileAsync(
                    currentName,
                    Windows::Storage::CreationCollisionOption::FailIfExists);
            }

            // 3. Conflict: ask the user what to do.
            int32_t choice = co_await PromptForOverwriteAsync(currentName);
            if (choice == 0)
            {
                // Overwrite.
                co_return co_await folder.CreateFileAsync(
                    currentName,
                    Windows::Storage::CreationCollisionOption::ReplaceExisting);
            }
            else if (choice == 1)
            {
                // Rename - loop back, prompt again with current name as default
                // so user can edit incrementally.
                continue;
            }
            else
            {
                // Cancel.
                co_return nullptr;
            }
        }
    }

    winrt::hstring ReviewPage::ComposeSegmentDisplayText(
        int32_t segmentIndex,
        winrt::hstring const& originalText) const
    {
        if (m_matchViewModels == nullptr || m_matchViewModels.Size() == 0)
        {
            return originalText;
        }

        // Collect all checked matches that belong to this segment, sorted by
        // start position. Each match is paired with its category so we can
        // apply the right per-category masking algorithm.
        struct MaskSpan
        {
            int32_t pos;
            int32_t len;
            int32_t category;
        };
        std::vector<MaskSpan> spans;

        for (uint32_t i = 0; i < m_matchViewModels.Size(); ++i)
        {
            auto vm = m_matchViewModels.GetAt(i);
            auto match = vm.Match();

            if (!vm.ShouldMask()) continue;
            if (match.SegmentIndex() != segmentIndex) continue;

            spans.push_back({
                match.PositionInSegment(),
                match.Length(),
                match.Category()
                });
        }

        if (spans.empty())
        {
            return originalText;
        }

        // Process spans in descending position order. By rewriting from the
        // back, earlier spans' positions remain valid even if the replacement
        // string changes length (currently it never does, but the descending
        // order is robust against future per-category algorithms that might).
        std::sort(spans.begin(), spans.end(),
            [](MaskSpan const& a, MaskSpan const& b) { return a.pos > b.pos; });

        std::wstring result{ originalText };
        for (auto const& span : spans)
        {
            int32_t start = span.pos;
            int32_t end = start + span.len;

            // Defensive bounds.
            if (start < 0) start = 0;
            if (end > static_cast<int32_t>(result.size()))
            {
                end = static_cast<int32_t>(result.size());
            }
            if (end <= start) continue;

            // Pull out the matched substring, run it through the engine's
            // category-specific masker (same algorithm as the redactor),
            // splice the result back in.
            winrt::hstring origSubstring{
                result.substr(start, end - start) };

            winrt::hstring maskedSubstring =
                winrt::DocRedactorEngine::PiiDetector::MaskText(
                    origSubstring, span.category);

            std::wstring maskedStr{ maskedSubstring };
            result.replace(start, end - start, maskedStr);
        }

        return winrt::hstring{ result };
    }

    void ReviewPage::ApplySegmentInlines(
        Microsoft::UI::Xaml::Controls::TextBlock textBlock,
        int32_t segmentIndex)
    {
        // Resolve the displayed (possibly masked) text for this segment using
        // the existing composition logic. ComposeSegmentDisplayText keeps the
        // SAME LENGTH as the original — each per-category masker preserves
        // character count — so positions computed against the original text
        // stay valid against the displayed text.
        if (m_parseResult == nullptr) return;

        auto segments = m_parseResult.Segments();
        if (segmentIndex < 0 || static_cast<uint32_t>(segmentIndex) >= segments.Size())
        {
            return;
        }

        auto seg = segments.GetAt(segmentIndex);
        winrt::hstring displayText = ComposeSegmentDisplayText(segmentIndex, seg.Text());

        // Figure out whether the selected match falls inside this segment.
        // If yes, capture its start/end so we can split the text into
        // pre/match/post Runs.
        int32_t hlStart = -1;
        int32_t hlEnd = -1;
        if (m_selectedMatchIndex >= 0
            && m_matchViewModels != nullptr
            && static_cast<uint32_t>(m_selectedMatchIndex) < m_matchViewModels.Size())
        {
            auto selectedMatch = m_matchViewModels.GetAt(m_selectedMatchIndex).Match();
            if (selectedMatch.SegmentIndex() == segmentIndex)
            {
                hlStart = selectedMatch.PositionInSegment();
                hlEnd = hlStart + selectedMatch.Length();

                // Clamp to displayText bounds defensively.
                int32_t dispLen = static_cast<int32_t>(std::wstring_view{ displayText }.size());
                if (hlStart < 0) hlStart = 0;
                if (hlEnd > dispLen) hlEnd = dispLen;
                if (hlEnd <= hlStart) { hlStart = -1; hlEnd = -1; }
            }
        }

        // Clear and rebuild this TextBlock's inline content.
        textBlock.Inlines().Clear();

        std::wstring text{ displayText };

        if (hlStart < 0)
        {
            // No highlight in this segment — single normal Run.
            Microsoft::UI::Xaml::Documents::Run run;
            run.Text(displayText);
            textBlock.Inlines().Append(run);
        }
        else
        {
            // Pre-match Run (may be empty).
            if (hlStart > 0)
            {
                Microsoft::UI::Xaml::Documents::Run pre;
                pre.Text(winrt::hstring{ text.substr(0, hlStart) });
                textBlock.Inlines().Append(pre);
            }

            // Match Run (bold + accent color).
            Microsoft::UI::Xaml::Documents::Run hl;
            hl.Text(winrt::hstring{ text.substr(hlStart, hlEnd - hlStart) });
            hl.FontWeight(Microsoft::UI::Text::FontWeights::Bold());
            try
            {
                auto brush = Application::Current().Resources()
                    .Lookup(box_value(L"AccentTextFillColorPrimaryBrush"))
                    .try_as<Microsoft::UI::Xaml::Media::Brush>();
                if (brush != nullptr)
                {
                    hl.Foreground(brush);
                }
                // If brush resolution fails for any reason, leave Foreground
                // unset — the Bold weight alone still distinguishes the match.
            }
            catch (...)
            {
                // Resource not present in current theme; bold-only is fine.
            }
            textBlock.Inlines().Append(hl);

            // Post-match Run (may be empty).
            if (hlEnd < static_cast<int32_t>(text.size()))
            {
                Microsoft::UI::Xaml::Documents::Run post;
                post.Text(winrt::hstring{ text.substr(hlEnd) });
                textBlock.Inlines().Append(post);
            }
        }
    }

    void ReviewPage::RenderPreview()
    {
        // Clear any previous rendering.
        m_segmentTextBlocks.clear();
        PreviewStack().Children().Clear();

        if (m_parseResult == nullptr)
        {
            // No data to render. Restore placeholder.
            TextBlock placeholder;
            placeholder.Text(L"Page preview will appear here");
            placeholder.Foreground(
                Application::Current().Resources()
                .Lookup(box_value(L"TextFillColorTertiaryBrush"))
                .as<Microsoft::UI::Xaml::Media::Brush>());
            placeholder.HorizontalAlignment(HorizontalAlignment::Center);
            placeholder.Margin(ThicknessHelper::FromLengths(0, 40, 0, 0));
            PreviewStack().Children().Append(placeholder);
            return;
        }

        auto pages = m_parseResult.Pages();
        auto segments = m_parseResult.Segments();

        if (pages.Size() == 0)
        {
            return;
        }

        double paneWidth = PreviewScrollViewer().ActualWidth();
        if (paneWidth <= 0)
        {
            paneWidth = PreviewBorder().ActualWidth();
        }
        if (paneWidth <= 0)
        {
            return;
        }

        // Build a lookup from page index to that page's segments.
        // We use the original IVectorView index (NOT just per-page index) because
        // PiiMatch.SegmentIndex() is a global index into the segments vector.
        struct IndexedSegment
        {
            int32_t globalIndex;
            winrt::DocRedactorEngine::TextSegment segment;
        };

        std::map<int32_t, std::vector<IndexedSegment>> segmentsByPage;
        for (uint32_t i = 0; i < segments.Size(); ++i)
        {
            auto seg = segments.GetAt(i);
            segmentsByPage[seg.PageIndex()].push_back({
                static_cast<int32_t>(i), seg });
        }

        // Render each page.
        for (uint32_t i = 0; i < pages.Size(); ++i)
        {
            auto page = pages.GetAt(i);
            double pageWidth = static_cast<double>(page.Width());
            double pageHeight = static_cast<double>(page.Height());

            if (pageWidth <= 0 || pageHeight <= 0)
            {
                continue;
            }

            constexpr double scale = 1.0;
            double scaledWidth = pageWidth * scale;
            double scaledHeight = pageHeight * scale;

            Canvas canvas;
            canvas.Width(scaledWidth);
            canvas.Height(scaledHeight);
            canvas.Background(
                Application::Current().Resources()
                .Lookup(box_value(L"SolidBackgroundFillColorBaseBrush"))
                .as<Microsoft::UI::Xaml::Media::Brush>());

            // Position each segment on this page. The segment's TextBlock holds
            // either the original text or a partially-masked version, depending on
            // which matches in this segment are currently checked.
            auto it = segmentsByPage.find(page.PageIndex());
            if (it != segmentsByPage.end())
            {
                for (auto const& indexed : it->second)
                {
                    auto const& seg = indexed.segment;

                    TextBlock tb;
                    // Fixed UI display size — independent of the document's
                    // authored font size. The actual redaction in
                    // Redactor::RedactAsync preserves the original sizes;
                    // this only affects on-screen preview.
                    tb.FontSize(14.0);
                    // Monospace font so column-aligned content (whitespace-
                    // padded values in form-style documents) lines up in
                    // the preview.
                    tb.FontFamily(Microsoft::UI::Xaml::Media::FontFamily{ L"Consolas" });

                    // Stash the segment's global index in Tag so the Tapped
                    // handler can recover it without an enclosing capture.
                    tb.Tag(box_value(indexed.globalIndex));

                    // Wire up tap-to-select on the preview text.
                    tb.Tapped({ this, &ReviewPage::SegmentTextBlock_Tapped });

                    // Populate the TextBlock's inline content (handles both
                    // the masked-text display and selection highlighting).
                    ApplySegmentInlines(tb, indexed.globalIndex);

                    double left = seg.OriginX() * scale;
                    double top = (seg.OriginY() - seg.FontSize()) * scale;

                    Canvas::SetLeft(tb, left);
                    Canvas::SetTop(tb, top);

                    canvas.Children().Append(tb);

                    // Remember the TextBlock so we can re-render its text when
                    // a checkbox toggles without rebuilding the whole canvas.
                    m_segmentTextBlocks[indexed.globalIndex] = tb;
                }
            }

            // Wrap in a Border to give each page a visual frame.
            Border pageBorder;
            pageBorder.Background(
                Application::Current().Resources()
                .Lookup(box_value(L"SolidBackgroundFillColorBaseBrush"))
                .as<Microsoft::UI::Xaml::Media::Brush>());
            pageBorder.BorderBrush(
                Application::Current().Resources()
                .Lookup(box_value(L"ControlStrokeColorDefaultBrush"))
                .as<Microsoft::UI::Xaml::Media::Brush>());
            pageBorder.BorderThickness(ThicknessHelper::FromUniformLength(1));
            pageBorder.CornerRadius(Microsoft::UI::Xaml::CornerRadiusHelper::FromUniformRadius(4));
            pageBorder.Padding(ThicknessHelper::FromUniformLength(8));
            pageBorder.HorizontalAlignment(HorizontalAlignment::Stretch);
            pageBorder.Child(canvas);

            // Canvas itself stays at its native (document) size, top-left
            // anchored inside the Border. Wide windows show empty space on
            // the right of the Canvas; narrow windows trigger the ScrollViewer.
            canvas.HorizontalAlignment(HorizontalAlignment::Left);
            canvas.VerticalAlignment(VerticalAlignment::Top);

            PreviewStack().Children().Append(pageBorder);
        }
    }

    void ReviewPage::RefreshSegmentDisplayText()
    {
        if (m_parseResult == nullptr)
        {
            return;
        }

        auto segments = m_parseResult.Segments();

        // Walk every tracked segment TextBlock and recompute its inlines.
        // We could optimize to only refresh segments touched by the toggled
        // match, but PropertyChanged doesn't tell us which match toggled,
        // and there are typically few segments. O(n) is fine.
        for (auto& [segIdx, tb] : m_segmentTextBlocks)
        {
            if (segIdx < 0 || static_cast<uint32_t>(segIdx) >= segments.Size())
            {
                continue;
            }
            ApplySegmentInlines(tb, segIdx);
        }
    }

    void ReviewPage::PreviewScrollViewer_SizeChanged(
        IInspectable const& /*sender*/,
        Microsoft::UI::Xaml::SizeChangedEventArgs const& /*e*/)
    {
        // Re-render at new available width. Note: this fires often during
        // window drag-resize. For a v1.0 with small test files, that's fine.
        // If users redact 100-page documents, we'd debounce here.
        RenderPreview();
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

    void ReviewPage::CtrlS_Invoked(
        Microsoft::UI::Xaml::Input::KeyboardAccelerator const& /*sender*/,
        Microsoft::UI::Xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args)
    {
        args.Handled(true);

        // Honor the disabled state — pressing Ctrl+S when there's nothing to
        // redact should be a no-op, not a "nothing to redact" warning toast.
        if (!RedactButton().IsEnabled())
        {
            return;
        }

        // Fire-and-forget; RedactButton_Click manages its own UI state.
        RedactButton_Click(RedactButton(), nullptr);
    }

    void ReviewPage::Escape_Invoked(
        Microsoft::UI::Xaml::Input::KeyboardAccelerator const& /*sender*/,
        Microsoft::UI::Xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args)
    {
        args.Handled(true);
        BackButton_Click(BackButton(), nullptr);
    }

    bool ReviewPage::HasUserEdits() const
    {
        if (m_matchViewModels == nullptr) return false;
        for (uint32_t i = 0; i < m_matchViewModels.Size(); ++i)
        {
            if (!m_matchViewModels.GetAt(i).ShouldMask())
            {
                return true;
            }
        }
        return false;
    }

    winrt::Windows::Foundation::IAsyncAction ReviewPage::F5_Invoked(
        Microsoft::UI::Xaml::Input::KeyboardAccelerator const& /*sender*/,
        Microsoft::UI::Xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args)
    {
        args.Handled(true);

        if (m_inputFilePath.empty())
        {
            co_return;
        }

        // Confirm if user has unchecked anything.
        if (HasUserEdits())
        {
            Microsoft::UI::Xaml::Controls::ContentDialog dialog;
            dialog.Title(box_value(L"Reload file?"));
            dialog.Content(box_value(winrt::hstring{
                L"Reloading will re-run PII detection from scratch. Your current "
                L"checkbox selections will be lost. Continue?" }));
            dialog.PrimaryButtonText(L"Reload");
            dialog.CloseButtonText(L"Cancel");
            dialog.DefaultButton(Microsoft::UI::Xaml::Controls::ContentDialogButton::Close);
            dialog.XamlRoot(this->XamlRoot());

            auto result = co_await dialog.ShowAsync();
            if (result != Microsoft::UI::Xaml::Controls::ContentDialogResult::Primary)
            {
                co_return;
            }
        }

        co_await ReloadAsync();
    }

    winrt::Windows::Foundation::IAsyncAction ReviewPage::ReloadAsync()
    {
        // Replay the same logic as OnNavigatedTo, but using the cached path
        // so we don't need a NavigationEventArgs. Reset all UI state first
        // so we don't leak stale data if the reload fails partway through.
        m_matchViewModels = nullptr;
        m_parseResult = nullptr;
        m_segmentTextBlocks.clear();
        m_selectedMatchIndex = -1;
        PreviewStack().Children().Clear();
        MatchesList().ItemsSource(nullptr);
        MatchesBorder().Visibility(Visibility::Collapsed);
        PlaceholderBorder().Visibility(Visibility::Visible);
        ResultSummaryText().Text(L"");
        RedactButton().IsEnabled(false);

        if (m_inputFilePath.empty())
        {
            co_return;
        }

        try
        {
            auto file = co_await StorageFile::GetFileFromPathAsync(m_inputFilePath);
            m_inputFile = file;

            auto parser = winrt::DocRedactorEngine::XpsParser{};
            m_parseResult = co_await parser.ParseAsync(file);
            auto segments = m_parseResult.Segments();

            auto detector = winrt::DocRedactorEngine::PiiDetector{};
            auto rawMatches = co_await detector.DetectAsync(segments);

            uint32_t enabledFlags = AppSettings::GetDetectionFlags();
            // DIAGNOSTIC: log the actual flag value.
            {
                wchar_t buf[64];
                swprintf_s(buf, L"[ReviewPage] enabledFlags = 0x%02X (raw count=%u)\n",
                    enabledFlags, rawMatches.Size());
                OutputDebugStringW(buf);
            }
            auto filteredMatches = winrt::single_threaded_vector<winrt::DocRedactorEngine::PiiMatch>();
            for (uint32_t i = 0; i < rawMatches.Size(); ++i)
            {
                auto m = rawMatches.GetAt(i);
                uint32_t cat = static_cast<uint32_t>(m.Category());
                if ((enabledFlags & cat) != 0)
                {
                    filteredMatches.Append(m);
                }
            }

            auto segmentCount = segments.Size();
            auto matchCount = filteredMatches.Size();

            std::wstring summary = L"Found ";
            summary += std::to_wstring(matchCount);
            summary += L" PII match(es) across ";
            summary += std::to_wstring(segmentCount);
            summary += L" text segment(s).";
            ResultSummaryText().Text(winrt::hstring{ summary });

            if (matchCount > 0)
            {
                m_matchViewModels = winrt::single_threaded_observable_vector<winrt::DocRedactorApp::PiiMatchViewModel>();
                for (uint32_t i = 0; i < matchCount; ++i)
                {
                    auto vm = winrt::make<implementation::PiiMatchViewModel>(filteredMatches.GetAt(i));

                    auto weakSelf = get_weak();
                    vm.PropertyChanged([weakSelf](auto const&, Microsoft::UI::Xaml::Data::PropertyChangedEventArgs const& propArgs)
                        {
                            if (propArgs.PropertyName() == L"ShouldMask")
                            {
                                if (auto strongSelf = weakSelf.get())
                                {
                                    strongSelf->UpdateRedactButtonState();
                                    strongSelf->RefreshSegmentDisplayText();
                                }
                            }
                        });

                    m_matchViewModels.Append(vm);
                }

                MatchesList().ItemsSource(m_matchViewModels);
                PlaceholderBorder().Visibility(Visibility::Collapsed);
                MatchesBorder().Visibility(Visibility::Visible);
                UpdateRedactButtonState();
                RenderPreview();
            }

            StatusBar().Severity(InfoBarSeverity::Success);
            StatusBar().Title(L"Reloaded");
            StatusBar().Message(L"File re-parsed from disk.");
            StatusBar().IsOpen(true);
        }
        catch (winrt::hresult_error const& ex)
        {
            std::wstring errorText = L"Error: ";
            errorText += std::wstring{ ex.message() };
            ResultSummaryText().Text(winrt::hstring{ errorText });
        }
    }

    void ReviewPage::MatchesList_SelectionChanged(
        IInspectable const& /*sender*/,
        Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& /*e*/)
    {
        // Re-entry guard: if SetSelectedMatch just programmatically set the
        // SelectedIndex (e.g. because the user clicked the preview), this
        // event fires too. Skip the routing in that case to avoid loops.
        if (m_isUpdatingSelection) return;

        int32_t newIndex = MatchesList().SelectedIndex();
        SetSelectedMatch(newIndex);
    }

    void ReviewPage::SegmentTextBlock_Tapped(
        IInspectable const& sender,
        Microsoft::UI::Xaml::Input::TappedRoutedEventArgs const& e)
    {
        try
        {
            auto tb = sender.try_as<Microsoft::UI::Xaml::Controls::TextBlock>();
            if (tb == nullptr) return;

            auto tag = tb.Tag();
            int32_t segIdx = winrt::unbox_value_or<int32_t>(tag, -1);
            if (segIdx < 0) return;

            // Find matches in this segment.
            if (m_matchViewModels == nullptr || m_matchViewModels.Size() == 0)
            {
                SetSelectedMatch(-1);
                return;
            }

            // Collect indices of matches that belong to this segment.
            std::vector<uint32_t> candidates;
            for (uint32_t i = 0; i < m_matchViewModels.Size(); ++i)
            {
                auto match = m_matchViewModels.GetAt(i).Match();
                if (match.SegmentIndex() == segIdx)
                {
                    candidates.push_back(i);
                }
            }

            if (candidates.empty())
            {
                SetSelectedMatch(-1);
                return;
            }

            if (candidates.size() == 1)
            {
                SetSelectedMatch(static_cast<int32_t>(candidates[0]));
                return;
            }

            // Multiple matches in this segment — pick by character position.
            // GetPosition can throw or return invalid points if the visual
            // tree is in an unusual state (e.g. mid-relayout when the Run
            // inline structure was just rebuilt). Catch defensively and fall
            // back to selecting the first candidate.
            double clickX = 0;
            try
            {
                auto point = e.GetPosition(tb);
                clickX = point.X;
            }
            catch (...)
            {
                SetSelectedMatch(static_cast<int32_t>(candidates[0]));
                return;
            }

            double glyphWidth = 14.0 * 0.55;
            int32_t clickChar = static_cast<int32_t>(clickX / glyphWidth);

            uint32_t bestIdx = candidates[0];
            int32_t bestDistance = INT32_MAX;
            for (uint32_t i : candidates)
            {
                auto match = m_matchViewModels.GetAt(i).Match();
                int32_t mStart = match.PositionInSegment();
                int32_t mEnd = mStart + match.Length();

                int32_t distance;
                if (clickChar >= mStart && clickChar < mEnd)
                {
                    distance = 0;
                }
                else if (clickChar < mStart)
                {
                    distance = mStart - clickChar;
                }
                else
                {
                    distance = clickChar - (mEnd - 1);
                }

                if (distance < bestDistance)
                {
                    bestDistance = distance;
                    bestIdx = i;
                }
            }

            SetSelectedMatch(static_cast<int32_t>(bestIdx));
        }
        catch (winrt::hresult_error const& ex)
        {
            // Diagnostic so we can see what's failing if this still crashes.
            std::wstring dbg = L"[SegmentTextBlock_Tapped] HRESULT ";
            wchar_t buf[32];
            swprintf_s(buf, L"0x%08X: ", static_cast<uint32_t>(ex.code()));
            dbg += buf;
            dbg += std::wstring{ ex.message() };
            dbg += L"\n";
            OutputDebugStringW(dbg.c_str());
        }
        catch (...)
        {
            OutputDebugStringW(L"[SegmentTextBlock_Tapped] unknown exception\n");
        }
    }

    void ReviewPage::SetSelectedMatch(int32_t matchIndex)
    {
        // Validate.
        if (m_matchViewModels == nullptr) matchIndex = -1;
        else if (matchIndex >= 0
            && static_cast<uint32_t>(matchIndex) >= m_matchViewModels.Size())
        {
            matchIndex = -1;
        }

        // No change?
        if (matchIndex == m_selectedMatchIndex) return;

        int32_t previousIndex = m_selectedMatchIndex;
        m_selectedMatchIndex = matchIndex;

        // Update the ListView under the re-entry guard so the
        // MatchesList_SelectionChanged handler doesn't re-call us.
        m_isUpdatingSelection = true;
        try
        {
            MatchesList().SelectedIndex(matchIndex);
            if (matchIndex >= 0)
            {
                // Scroll the list to bring the selected row into view too.
                auto item = m_matchViewModels.GetAt(matchIndex);
                MatchesList().ScrollIntoView(item);
            }
        }
        catch (...) { /* defensive */ }
        m_isUpdatingSelection = false;

        // Re-render the affected segments' inlines. We touch at most two:
        // the previously-selected match's segment (to remove highlight) and
        // the newly-selected match's segment (to add highlight). If both
        // matches are in the same segment, one ApplySegmentInlines call
        // suffices.
        auto refreshSegment = [this](int32_t mi) {
            if (mi < 0 || m_matchViewModels == nullptr) return;
            if (static_cast<uint32_t>(mi) >= m_matchViewModels.Size()) return;
            int32_t segIdx = m_matchViewModels.GetAt(mi).Match().SegmentIndex();
            auto it = m_segmentTextBlocks.find(segIdx);
            if (it != m_segmentTextBlocks.end())
            {
                ApplySegmentInlines(it->second, segIdx);
            }
            };
        refreshSegment(previousIndex);
        if (matchIndex != previousIndex) refreshSegment(matchIndex);

        // Scroll the preview to center on the new selection.
        if (matchIndex >= 0)
        {
            CenterPreviewOnMatch(matchIndex);
        }
    }

    void ReviewPage::CenterPreviewOnMatch(int32_t matchIndex)
    {
        if (matchIndex < 0
            || m_matchViewModels == nullptr
            || static_cast<uint32_t>(matchIndex) >= m_matchViewModels.Size())
        {
            return;
        }

        auto match = m_matchViewModels.GetAt(matchIndex).Match();
        int32_t segIdx = match.SegmentIndex();

        auto it = m_segmentTextBlocks.find(segIdx);
        if (it == m_segmentTextBlocks.end()) return;

        auto& tb = it->second;

        // Compute the TextBlock's vertical position relative to the
        // PreviewScrollViewer's content area. We use TransformToVisual to
        // walk the visual tree: tb -> page Border -> Canvas -> PreviewStack
        // -> ScrollViewer Content. The Y component of the resulting point
        // tells us the TextBlock's offset within the scrollable region.
        try
        {
            auto transform = tb.TransformToVisual(PreviewStack());
            auto point = transform.TransformPoint(
                winrt::Windows::Foundation::Point{ 0, 0 });

            double tbTop = point.Y;
            double tbHeight = tb.ActualHeight();
            if (tbHeight <= 0) tbHeight = 14.0;  // fallback to font size

            double viewportHeight = PreviewScrollViewer().ViewportHeight();
            double targetOffset = tbTop + (tbHeight / 2.0) - (viewportHeight / 2.0);

            // Clamp to valid scroll range.
            if (targetOffset < 0) targetOffset = 0;
            double maxOffset = PreviewScrollViewer().ScrollableHeight();
            if (targetOffset > maxOffset) targetOffset = maxOffset;

            PreviewScrollViewer().ChangeView(
                nullptr,                                  // no horizontal change
                winrt::box_value(targetOffset).as<winrt::Windows::Foundation::IReference<double>>(),
                nullptr,                                  // no zoom change
                false);                                   // animated
        }
        catch (...)
        {
            // TransformToVisual can throw if the visual tree isn't fully
            // realized yet (e.g. during initial load). Defensive swallow.
        }
    }
}