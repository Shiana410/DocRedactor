#pragma once

#include "MainWindow.g.h"

namespace winrt::DocRedactorApp::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();

        winrt::Windows::Foundation::IAsyncAction SettingsButton_Click(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);

        // Ctrl+, — opens the Settings dialog from anywhere in the window.
        void CtrlComma_Invoked(
            winrt::Microsoft::UI::Xaml::Input::KeyboardAccelerator const& sender,
            winrt::Microsoft::UI::Xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args);

        // Ctrl+W — closes the window (exits the app since this is single-window).
        void CtrlW_Invoked(
            winrt::Microsoft::UI::Xaml::Input::KeyboardAccelerator const& sender,
            winrt::Microsoft::UI::Xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args);

    private:
        // Core Settings-opening logic, shared between the gear button and
        // the Ctrl+, accelerator. Takes a XamlRoot directly so callers don't
        // need a Button sender to pull it from.
        winrt::Windows::Foundation::IAsyncAction ShowSettingsDialogAsync(
            winrt::Microsoft::UI::Xaml::XamlRoot xamlRoot);
    };
}

namespace winrt::DocRedactorApp::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}