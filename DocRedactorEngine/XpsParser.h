#pragma once

#include "XpsParser.g.h"
#include "TextSegment.g.h"
#include "PiiMatch.g.h"
#include "PiiDetector.g.h"
#include "Redactor.g.h"

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

    struct PiiMatch : PiiMatchT<PiiMatch>
    {
        PiiMatch() = default;

        PiiMatch(
            winrt::hstring text,
            int32_t category,
            winrt::hstring categoryName,
            int32_t segmentIndex,
            int32_t positionInSegment,
            int32_t length,
            int32_t pageIndex,
            float originX, float originY,
            float width, float height)
            : m_text(std::move(text))
            , m_category(category)
            , m_categoryName(std::move(categoryName))
            , m_segmentIndex(segmentIndex)
            , m_positionInSegment(positionInSegment)
            , m_length(length)
            , m_pageIndex(pageIndex)
            , m_originX(originX), m_originY(originY)
            , m_width(width), m_height(height)
        {
        }

        winrt::hstring Text() const noexcept { return m_text; }
        int32_t Category() const noexcept { return m_category; }
        winrt::hstring CategoryName() const noexcept { return m_categoryName; }
        int32_t SegmentIndex() const noexcept { return m_segmentIndex; }
        int32_t PositionInSegment() const noexcept { return m_positionInSegment; }
        int32_t Length() const noexcept { return m_length; }
        int32_t PageIndex() const noexcept { return m_pageIndex; }
        float OriginX() const noexcept { return m_originX; }
        float OriginY() const noexcept { return m_originY; }
        float Width() const noexcept { return m_width; }
        float Height() const noexcept { return m_height; }

    private:
        winrt::hstring m_text;
        int32_t m_category{ 0 };
        winrt::hstring m_categoryName;
        int32_t m_segmentIndex{ 0 };
        int32_t m_positionInSegment{ 0 };
        int32_t m_length{ 0 };
        int32_t m_pageIndex{ 0 };
        float m_originX{ 0 }, m_originY{ 0 };
        float m_width{ 0 }, m_height{ 0 };
    };

    struct PiiDetector : PiiDetectorT<PiiDetector>
    {
        PiiDetector() = default;

        winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Foundation::Collections::IVectorView<winrt::DocRedactorEngine::PiiMatch>> DetectAsync(winrt::Windows::Foundation::Collections::IVectorView<winrt::DocRedactorEngine::TextSegment> segments);
    };

    struct Redactor : RedactorT<Redactor>
    {
        Redactor() = default;

        winrt::Windows::Foundation::IAsyncOperation<bool> RedactAsync(winrt::Windows::Storage::StorageFile inputFile, winrt::Windows::Storage::StorageFile outputFile, winrt::Windows::Foundation::Collections::IVectorView<winrt::DocRedactorEngine::PiiMatch> matches);
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

    struct PiiMatch : PiiMatchT<PiiMatch, implementation::PiiMatch>
    {
    };

    struct PiiDetector : PiiDetectorT<PiiDetector, implementation::PiiDetector>
    {
    };

    struct Redactor : RedactorT<Redactor, implementation::Redactor>
    {
    };
}