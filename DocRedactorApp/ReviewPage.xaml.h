#pragma once

#include "ReviewPage.g.h"

#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/DocRedactorEngine.h>
#include "PiiMatchViewModel.h"
#include <winrt/Microsoft.UI.Xaml.Shapes.h>

namespace winrt::DocRedactorApp::implementation
{
    struct ReviewPage : ReviewPageT<ReviewPage>
    {
        ReviewPage();

        winrt::Windows::Foundation::IAsyncAction OnNavigatedTo(Microsoft::UI::Xaml::Navigation::NavigationEventArgs const& e);

        void BackButton_Click(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);

        void MatchesList_SelectionChanged(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& e);

        void SegmentTextBlock_Tapped(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::Input::TappedRoutedEventArgs const& e);

        winrt::Windows::Foundation::IAsyncAction RedactButton_Click(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);

        // Re-renders the preview when the pane width changes (window resize).
        void PreviewScrollViewer_SizeChanged(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::SizeChangedEventArgs const& e);

        // Ctrl+S — invoke Redact and Save if enabled.
        void CtrlS_Invoked(
            winrt::Microsoft::UI::Xaml::Input::KeyboardAccelerator const& sender,
            winrt::Microsoft::UI::Xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args);

        // Esc — go Back. (If a ContentDialog is open, it consumes Esc first.)
        void Escape_Invoked(
            winrt::Microsoft::UI::Xaml::Input::KeyboardAccelerator const& sender,
            winrt::Microsoft::UI::Xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args);

        // F5 — re-parse and re-detect against the original file. Confirms
        // first if the user has unchecked any matches.
        winrt::Windows::Foundation::IAsyncAction F5_Invoked(
            winrt::Microsoft::UI::Xaml::Input::KeyboardAccelerator const& sender,
            winrt::Microsoft::UI::Xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args);

    private:
        winrt::Windows::Storage::StorageFile m_inputFile{ nullptr };

        // Captured at first navigation so F5 can reload without an event.
        winrt::hstring m_inputFilePath{};

        int32_t m_selectedMatchIndex{ -1 };

        bool m_isUpdatingSelection{ false };

        winrt::Windows::Foundation::Collections::IObservableVector<winrt::DocRedactorApp::PiiMatchViewModel> m_matchViewModels{ nullptr };

        // Cached parse result — preserved across SizeChanged events so we
        // re-render at new width without re-parsing the document.
        winrt::DocRedactorEngine::XpsParseResult m_parseResult{ nullptr };

        void UpdateRedactButtonState();

        // Returns true if the user has unchecked at least one match —
        // i.e. their state differs from the freshly-loaded "all checked"
        // default. Used by F5 to decide whether to confirm.
        bool HasUserEdits() const;

        // Core reload logic shared by F5 and (potentially) other triggers.
        // Re-parses m_inputFile and rebuilds the match list / preview.
        winrt::Windows::Foundation::IAsyncAction ReloadAsync();

        // Apply a new selection. -1 clears. Updates both panes (preview text
        // styling + list selected index) under the m_isUpdatingSelection
        // re-entry guard. Centers the preview on the selected match.
        void SetSelectedMatch(int32_t matchIndex);

        // Rebuild a single segment's TextBlock inline content based on the
        // current selection state. Splits the displayed segment text into
        // up to 3 Runs: pre-match (normal), match-substring (bold+accent),
        // post-match (normal). Called from RenderPreview for each segment
        // and from SetSelectedMatch when only the styling needs to refresh.
        void ApplySegmentInlines(
            winrt::Microsoft::UI::Xaml::Controls::TextBlock textBlock,
            int32_t segmentIndex);

        // Scroll PreviewScrollViewer so the segment containing the given
        // match index is vertically centered in the viewport. No-op if the
        // index is invalid or the segment hasn't been rendered yet.
        void CenterPreviewOnMatch(int32_t matchIndex);

        // Renders all pages from m_parseResult into PreviewStack at the
        // current PreviewScrollViewer width. Clears existing rendering first.
        void RenderPreview();
        void RefreshSegmentDisplayText();

        std::map<int32_t, winrt::Microsoft::UI::Xaml::Controls::TextBlock> m_segmentTextBlocks;

        winrt::Windows::Foundation::IAsyncOperation<winrt::hstring> PromptForFilenameAsync(
            winrt::hstring const& suggestedName);

        winrt::Windows::Foundation::IAsyncOperation<int32_t> PromptForOverwriteAsync(
            winrt::hstring const& existingName);

        winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Storage::StorageFile>
            ResolveOutputFileAsync(
                winrt::Windows::Storage::StorageFolder const& folder,
                winrt::hstring const& suggestedName);

        winrt::hstring ComposeSegmentDisplayText(
            int32_t segmentIndex,
            winrt::hstring const& originalText) const;
    };
}

namespace winrt::DocRedactorApp::factory_implementation
{
    struct ReviewPage : ReviewPageT<ReviewPage, implementation::ReviewPage>
    {
    };
}