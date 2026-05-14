#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

#include "WelcomePage.xaml.h"
#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Windows.System.h>
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
        Microsoft::UI::Xaml::Input::KeyboardAccelerator commaAccel;
        commaAccel.Modifiers(Windows::System::VirtualKeyModifiers::Control);
        commaAccel.Key(static_cast<Windows::System::VirtualKey>(188));
        commaAccel.Invoked({ this, &MainWindow::CtrlComma_Invoked });
        RootGrid().KeyboardAccelerators().Append(commaAccel);
        ContentFrame().Navigate(winrt::xaml_typename<DocRedactorApp::WelcomePage>());
    }

    winrt::Windows::Foundation::IAsyncAction MainWindow::SettingsButton_Click(
        IInspectable const& sender,
        RoutedEventArgs const& /*e*/)
    {
        auto button = sender.as<Microsoft::UI::Xaml::Controls::Button>();
        co_await ShowSettingsDialogAsync(button.XamlRoot());
    }

    winrt::Windows::Foundation::IAsyncAction MainWindow::ShowSettingsDialogAsync(
        Microsoft::UI::Xaml::XamlRoot xamlRoot)
    {
        // Build the dialog content.
        auto settingsContent = winrt::make<implementation::SettingsContent>();

        // Create the modal dialog. Unlike Flyout, ContentDialog handles focus
        // correctly when a child dialog (e.g., FolderPicker) opens — its
        // dismissal isn't tied to focus loss to OS windows.
        Microsoft::UI::Xaml::Controls::ContentDialog dialog;
        dialog.Content(settingsContent);
        dialog.XamlRoot(xamlRoot);

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

        co_await dialog.ShowAsync();
    }

    void MainWindow::CtrlComma_Invoked(
        Microsoft::UI::Xaml::Input::KeyboardAccelerator const& /*sender*/,
        Microsoft::UI::Xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args)
    {
        // Mark handled so the accelerator doesn't bubble (no parent would
        // care, but it's good hygiene).
        args.Handled(true);

        // Fire-and-forget the async dialog. Pulling XamlRoot from RootGrid
        // since we have no Button sender to ask.
        ShowSettingsDialogAsync(RootGrid().XamlRoot());
    }

    void MainWindow::CtrlW_Invoked(
        Microsoft::UI::Xaml::Input::KeyboardAccelerator const& /*sender*/,
        Microsoft::UI::Xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args)
    {
        args.Handled(true);

        // Single-window app: closing the window exits the process.
        // Note: Ctrl+W does NOT close open dialogs — the accelerator lives on
        // RootGrid, but ContentDialogs render in the popup layer outside that
        // tree, so the keystroke never reaches us while a dialog is up. Use
        // Esc or the dialog's Cancel/Close button to dismiss dialogs.
        this->Close();
    }
}