#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

#include "WelcomePage.xaml.h"
#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>
#include "SettingsContent.xaml.h"

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Interop;
using namespace winrt::Windows::Foundation;

namespace winrt::DocRedactorApp::implementation
{
    MainWindow::MainWindow()
    {
        InitializeComponent();
        ContentFrame().Navigate(winrt::xaml_typename<DocRedactorApp::WelcomePage>());
    }

    winrt::Windows::Foundation::IAsyncAction MainWindow::SettingsButton_Click(
        IInspectable const& sender,
        RoutedEventArgs const& /*e*/)
    {
        auto button = sender.as<Microsoft::UI::Xaml::Controls::Button>();

        // Build the dialog content.
        auto settingsContent = winrt::make<implementation::SettingsContent>();

        // Create the modal dialog. Unlike Flyout, ContentDialog handles focus
        // correctly when a child dialog (e.g., FolderPicker) opens — its
        // dismissal isn't tied to focus loss to OS windows.
        Microsoft::UI::Xaml::Controls::ContentDialog dialog;
        dialog.Content(settingsContent);
        dialog.XamlRoot(button.XamlRoot());

        // We use SettingsContent's own Save/Cancel buttons, so suppress the
        // ContentDialog's default action buttons.
        dialog.PrimaryButtonText(L"");
        dialog.SecondaryButtonText(L"");
        dialog.CloseButtonText(L"");

        // Subscribe to the UserControl's RequestClose event. When it fires
        // (because user clicked Save or Cancel), close the dialog.
        auto settingsContentImpl = settingsContent.as<DocRedactorApp::SettingsContent>();
        settingsContentImpl.RequestClose([dialog](IInspectable const&, bool /*saved*/)
            {
                dialog.Hide();
            });

        // Show the dialog and wait for it to close. ShowAsync returns the
        // ContentDialogResult but we don't need it - SettingsContent already
        // persisted what needed persisting before firing RequestClose.
        co_await dialog.ShowAsync();
    }
}