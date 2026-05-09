#pragma once

#include "PiiMatchViewModel.g.h"

#include <winrt/DocRedactorEngine.h>
#include <winrt/Microsoft.UI.Xaml.Data.h>

namespace winrt::DocRedactorApp::implementation
{
    struct PiiMatchViewModel : PiiMatchViewModelT<PiiMatchViewModel>
    {
        PiiMatchViewModel() = default;

        explicit PiiMatchViewModel(winrt::DocRedactorEngine::PiiMatch const& match);

        winrt::DocRedactorEngine::PiiMatch Match() const noexcept { return m_match; }

        bool ShouldMask() const noexcept { return m_shouldMask; }
        void ShouldMask(bool value);

        winrt::hstring Description() const { return m_description; }

        // INotifyPropertyChanged implementation
        winrt::event_token PropertyChanged(winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const& handler)
        {
            return m_propertyChanged.add(handler);
        }

        void PropertyChanged(winrt::event_token const& token) noexcept
        {
            m_propertyChanged.remove(token);
        }

    private:
        winrt::DocRedactorEngine::PiiMatch m_match{ nullptr };
        bool m_shouldMask{ true };
        winrt::hstring m_description;

        winrt::event<winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventHandler> m_propertyChanged;

        void RaisePropertyChanged(winrt::hstring const& propertyName);
    };
}

namespace winrt::DocRedactorApp::factory_implementation
{
    struct PiiMatchViewModel : PiiMatchViewModelT<PiiMatchViewModel, implementation::PiiMatchViewModel>
    {
    };
}