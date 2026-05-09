#pragma once

#include "ReviewPage.g.h"

#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/DocRedactorEngine.h>
#include "PiiMatchViewModel.h"

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

    private:
        winrt::Windows::Storage::StorageFile m_inputFile{ nullptr };

        winrt::Windows::Foundation::Collections::IObservableVector<winrt::DocRedactorApp::PiiMatchViewModel> m_matchViewModels{ nullptr };

        void UpdateRedactButtonState();

        // Prompts the user for a filename, defaulting to suggestedName.
        // Returns the user's chosen name, or empty hstring if user cancelled.
        winrt::Windows::Foundation::IAsyncOperation<winrt::hstring> PromptForFilenameAsync(
            winrt::hstring const& suggestedName);

        // Shows an "overwrite or rename" dialog when a target file already exists.
        // Returns: 0 = overwrite, 1 = rename (caller should re-prompt), 2 = cancel.
        winrt::Windows::Foundation::IAsyncOperation<int32_t> PromptForOverwriteAsync(
            winrt::hstring const& existingName);

        // Resolves a target StorageFile in the given folder, prompting for name
        // and handling collisions interactively. Returns nullptr if user cancelled.
        winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Storage::StorageFile>
            ResolveOutputFileAsync(
                winrt::Windows::Storage::StorageFolder const& folder,
                winrt::hstring const& suggestedName);
    };
}

namespace winrt::DocRedactorApp::factory_implementation
{
    struct ReviewPage : ReviewPageT<ReviewPage, implementation::ReviewPage>
    {
    };
}