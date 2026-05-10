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

        winrt::Windows::Foundation::IAsyncAction RedactButton_Click(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);

        // Re-renders the preview when the pane width changes (window resize).
        void PreviewScrollViewer_SizeChanged(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::SizeChangedEventArgs const& e);

    private:
        winrt::Windows::Storage::StorageFile m_inputFile{ nullptr };

        winrt::Windows::Foundation::Collections::IObservableVector<winrt::DocRedactorApp::PiiMatchViewModel> m_matchViewModels{ nullptr };

        // Cached parse result — preserved across SizeChanged events so we
        // re-render at new width without re-parsing the document.
        winrt::DocRedactorEngine::XpsParseResult m_parseResult{ nullptr };

        void UpdateRedactButtonState();

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