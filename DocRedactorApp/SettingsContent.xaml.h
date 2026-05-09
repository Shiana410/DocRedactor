#pragma once

#include "SettingsContent.g.h"

namespace winrt::DocRedactorApp::implementation
{
    struct SettingsContent : SettingsContentT<SettingsContent>
    {
        SettingsContent();

        winrt::Windows::Foundation::IAsyncAction BrowseFolderButton_Click(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);

        void SaveButton_Click(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);

        void CancelButton_Click(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);

        // RequestClose event - fires on Save/Cancel so parent can dismiss the flyout.
        // Boolean arg: true if Save was clicked, false if Cancel.
        winrt::event_token RequestClose(winrt::Windows::Foundation::EventHandler<bool> const& handler)
        {
            return m_requestClose.add(handler);
        }

        void RequestClose(winrt::event_token const& token) noexcept
        {
            m_requestClose.remove(token);
        }

    private:
        winrt::hstring m_pickedFolder;

        winrt::event<winrt::Windows::Foundation::EventHandler<bool>> m_requestClose;

        void LoadSettingsIntoControls();
    };
}

namespace winrt::DocRedactorApp::factory_implementation
{
    struct SettingsContent : SettingsContentT<SettingsContent, implementation::SettingsContent>
    {
    };
}