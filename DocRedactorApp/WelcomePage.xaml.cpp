#include "pch.h"
#include "WelcomePage.xaml.h"
#if __has_include("WelcomePage.g.cpp")
#include "WelcomePage.g.cpp"
#endif

namespace winrt::DocRedactorApp::implementation
{
    WelcomePage::WelcomePage()
    {
        InitializeComponent();
    }
}