#pragma once

#include "App.xaml.g.h"

namespace winrt::DocRedactorApp::implementation
{
    struct App : AppT<App>
    {
        App();

        void OnLaunched(Microsoft::UI::Xaml::LaunchActivatedEventArgs const&);

        // Static accessors so other pages can reach the main window's HWND
        // without needing to resolve App's projected type.
        static winrt::Microsoft::UI::Xaml::Window MainWindow() noexcept { return s_window; }
        static HWND MainWindowHandle() noexcept { return s_hwnd; }

    private:
        winrt::Microsoft::UI::Xaml::Window window{ nullptr };

        // Static cache populated in OnLaunched after MainWindow is created.
        // Avoids the WinRT projection / get_self machinery that doesn't work
        // for XAML-derived App classes.
        static inline winrt::Microsoft::UI::Xaml::Window s_window{ nullptr };
        static inline HWND s_hwnd{ nullptr };
    };
}