#include "pch.h"
#include "PiiMatchViewModel.h"
#if __has_include("PiiMatchViewModel.g.cpp")
#include "PiiMatchViewModel.g.cpp"
#endif

namespace winrt::DocRedactorApp::implementation
{
    PiiMatchViewModel::PiiMatchViewModel(winrt::DocRedactorEngine::PiiMatch const& match)
        : m_match(match)
        , m_shouldMask(true)
    {
        std::wstring desc = L"[";
        desc += std::wstring{ match.CategoryName() };
        desc += L"] \"";
        desc += std::wstring{ match.Text() };
        desc += L"\"  (page ";
        desc += std::to_wstring(match.PageIndex());
        desc += L", segment ";
        desc += std::to_wstring(match.SegmentIndex());
        desc += L")";

        m_description = winrt::hstring{ desc };
    }

    void PiiMatchViewModel::ShouldMask(bool value)
    {
        if (m_shouldMask != value)
        {
            m_shouldMask = value;
            RaisePropertyChanged(L"ShouldMask");
        }
    }

    void PiiMatchViewModel::RaisePropertyChanged(winrt::hstring const& propertyName)
    {
        m_propertyChanged(*this, winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventArgs(propertyName));
    }
}