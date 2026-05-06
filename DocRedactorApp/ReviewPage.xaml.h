#pragma once

#include "ReviewPage.g.h"

namespace winrt::DocRedactorApp::implementation
{
    struct ReviewPage : ReviewPageT<ReviewPage>
    {
        ReviewPage();

        // Called by the framework when this page becomes the Frame's content.
        // We use this to receive the file path navigation parameter.
        winrt::Windows::Foundation::IAsyncAction OnNavigatedTo(Microsoft::UI::Xaml::Navigation::NavigationEventArgs const& e);

        void BackButton_Click(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
    };
}

namespace winrt::DocRedactorApp::factory_implementation
{
    struct ReviewPage : ReviewPageT<ReviewPage, implementation::ReviewPage>
    {
    };
}