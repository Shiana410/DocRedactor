#pragma once

#include "WelcomePage.g.h"

namespace winrt::DocRedactorApp::implementation
{
    struct WelcomePage : WelcomePageT<WelcomePage>
    {
        WelcomePage();

        void DropZone_DragOver(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::DragEventArgs const& e);

        void DropZone_DragLeave(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::DragEventArgs const& e);

        winrt::Windows::Foundation::IAsyncAction DropZone_Drop(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::DragEventArgs const& e);

        winrt::Windows::Foundation::IAsyncAction OpenFileButton_Click(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);

    private:
        // Helper used by both the picker and the drop handler to navigate
        // to ReviewPage with the selected file's path as parameter.
        void NavigateToReview(winrt::hstring const& path);

        // Show the "unsupported file" InfoBar with a custom message and
        // schedule auto-dismissal after a few seconds.
        winrt::Windows::Foundation::IAsyncAction ShowUnsupportedFileMessage(
            winrt::hstring const& fileExtension);
    };
}

namespace winrt::DocRedactorApp::factory_implementation
{
    struct WelcomePage : WelcomePageT<WelcomePage, implementation::WelcomePage>
    {
    };
}