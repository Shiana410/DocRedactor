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
#include <winrt/Microsoft.UI.Xaml.Navigation.h>

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
            auto file = co_await StorageFile::GetFileFromPathAsync(path);
            m_inputFile = file;

            auto parser = winrt::DocRedactorEngine::XpsParser{};
            auto segments = co_await parser.ParseAsync(file);

            auto detector = winrt::DocRedactorEngine::PiiDetector{};
            auto rawMatches = co_await detector.DetectAsync(segments);

            // Filter raw matches against the user's detection flag settings.
            // Categories disabled in Settings get dropped before display.
            uint32_t enabledFlags = AppSettings::GetDetectionFlags();
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

                    // Subscribe to ShouldMask changes so we can recompute button enable state.
                    // We capture a weak ref to ourselves to avoid a retain cycle.
                    auto weakSelf = get_weak();
                    vm.PropertyChanged([weakSelf](auto const&, Microsoft::UI::Xaml::Data::PropertyChangedEventArgs const& args)
                        {
                            if (args.PropertyName() == L"ShouldMask")
                            {
                                if (auto strongSelf = weakSelf.get())
                                {
                                    strongSelf->UpdateRedactButtonState();
                                }
                            }
                        });

                    m_matchViewModels.Append(vm);
                }

                MatchesList().ItemsSource(m_matchViewModels);

                PlaceholderBorder().Visibility(Visibility::Collapsed);
                MatchesBorder().Visibility(Visibility::Visible);

                UpdateRedactButtonState();
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