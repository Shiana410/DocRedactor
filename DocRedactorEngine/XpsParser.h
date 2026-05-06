#pragma once

#include "XpsParser.g.h"
#include "TextSegment.g.h"

namespace winrt::DocRedactorEngine::implementation
{
    struct XpsParser : XpsParserT<XpsParser>
    {
        XpsParser() = default;

        int32_t MyProperty();
        void MyProperty(int32_t value);
        winrt::hstring Greeting();

        winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Foundation::Collections::IVectorView<winrt::DocRedactorEngine::TextSegment>> ParseAsync(winrt::Windows::Storage::StorageFile file);
    };

    struct TextSegment : TextSegmentT<TextSegment>
    {
        TextSegment() = default;

        TextSegment(
            winrt::hstring text,
            int32_t pageIndex,
            float originX, float originY,
            float width, float height,
            float fontSize)
            : m_text(std::move(text))
            , m_pageIndex(pageIndex)
            , m_originX(originX), m_originY(originY)
            , m_width(width), m_height(height)
            , m_fontSize(fontSize)
        {
        }

        winrt::hstring Text() const noexcept { return m_text; }
        int32_t PageIndex() const noexcept { return m_pageIndex; }
        float OriginX() const noexcept { return m_originX; }
        float OriginY() const noexcept { return m_originY; }
        float Width() const noexcept { return m_width; }
        float Height() const noexcept { return m_height; }
        float FontSize() const noexcept { return m_fontSize; }

    private:
        winrt::hstring m_text;
        int32_t m_pageIndex{ 0 };
        float m_originX{ 0 }, m_originY{ 0 };
        float m_width{ 0 }, m_height{ 0 };
        float m_fontSize{ 0 };
    };
}

namespace winrt::DocRedactorEngine::factory_implementation
{
    struct XpsParser : XpsParserT<XpsParser, implementation::XpsParser>
    {
    };

    struct TextSegment : TextSegmentT<TextSegment, implementation::TextSegment>
    {
    };
}