#pragma once

#include "WelcomePage.g.h"

namespace winrt::DocRedactorApp::implementation
{
    struct WelcomePage : WelcomePageT<WelcomePage>
    {
        WelcomePage();
    };
}

namespace winrt::DocRedactorApp::factory_implementation
{
    struct WelcomePage : WelcomePageT<WelcomePage, implementation::WelcomePage>
    {
    };
}