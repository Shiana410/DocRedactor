#pragma once

#include "WelcomePage.g.h"
#include "RecentFilesStore.h"

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

        // Ctrl+O — equivalent to clicking the Open File button.
        void CtrlO_Invoked(
            winrt::Microsoft::UI::Xaml::Input::KeyboardAccelerator const& sender,
            winrt::Microsoft::UI::Xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args);

        // Recent-files panel handlers.
        winrt::Windows::Foundation::IAsyncAction RecentsList_ItemClick(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::Controls::ItemClickEventArgs const& e);

        void ClearRecentsButton_Click(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);

        // Called from constructor and after each successful open / clear.
        void RefreshRecentsList();

        // Called whenever the page becomes the current frame target — keeps
        // the recents list fresh after navigating back from ReviewPage.
        void OnNavigatedTo(
            winrt::Microsoft::UI::Xaml::Navigation::NavigationEventArgs const& e);

    private:
        // Helper used by both the picker and the drop handler to navigate
        // to ReviewPage with the selected file's path as parameter.
        // Also bumps the recents list.
        void NavigateToReview(winrt::hstring const& path);

        // Show the "unsupported file" InfoBar with a custom message and
        // schedule auto-dismissal after a few seconds.
        winrt::Windows::Foundation::IAsyncAction ShowUnsupportedFileMessage(
            winrt::hstring const& fileExtension);

        // Build one row's visual for the RecentsList. Pulled out so the
        // refresh function stays readable.
        winrt::Microsoft::UI::Xaml::FrameworkElement BuildRecentRow(
            winrt::DocRedactorApp::RecentFile const& entry);
    };
}

namespace winrt::DocRedactorApp::factory_implementation
{
    struct WelcomePage : WelcomePageT<WelcomePage, implementation::WelcomePage>
    {
    };
}