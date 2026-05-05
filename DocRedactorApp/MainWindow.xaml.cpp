#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

#include "WelcomePage.xaml.h"

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

    void MainWindow::SettingsButton_Click(
        IInspectable const& /*sender*/,
        RoutedEventArgs const& /*e*/)
    {
        OutputDebugStringW(L"[DocRedactor] Settings clicked - flyout not yet implemented.\n");
    }
}