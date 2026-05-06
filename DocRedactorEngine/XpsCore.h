#pragma once
// =============================================================================
// XpsCore.h  –  XPS/OXPS Document text extractor with coordinate mapping
// =============================================================================
// Wrapped in dlp:: namespace so the class XpsParser doesn't collide with
// the runtimeclass winrt::DocRedactorEngine::XpsParser.
// =============================================================================

#ifndef XPS_CORE_H
#define XPS_CORE_H

#include <windows.h>
#include <xpsobjectmodel_1.h>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

namespace dlp
{

struct XpsTextRun
{
    std::wstring text;
    double       originX   = 0;
    double       originY   = 0;
    double       fontSize  = 10;
    double       width     = 0;
    double       height    = 0;
    int          pageIndex = 0;
    std::wstring fontUri;
};

struct XpsDocument
{
    std::vector<XpsTextRun> textRuns;
    int                     pageCount = 0;
    std::wstring            filePath;
};

class XpsParser
{
public:
    static bool Parse(const std::wstring& xpsFilePath, XpsDocument& doc)
    {
        doc.filePath = xpsFilePath;
        doc.textRuns.clear();
        doc.pageCount = 0;

        HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
        bool needUninit = SUCCEEDED(hr);

        IXpsOMObjectFactory1* factory1 = nullptr;
        hr = CoCreateInstance(
            __uuidof(XpsOMObjectFactory),
            NULL,
            CLSCTX_INPROC_SERVER,
            __uuidof(IXpsOMObjectFactory1),
            (void**)&factory1);

        if (FAILED(hr) || !factory1)
        {
            OutputDebugStringW(L"[DLP] Failed to create IXpsOMObjectFactory1\n");
            if (needUninit) CoUninitialize();
            return false;
        }

        IXpsOMPackage1* package1 = nullptr;
        hr = factory1->CreatePackageFromFile1(xpsFilePath.c_str(), FALSE, &package1);
        if (FAILED(hr) || !package1)
        {
            wchar_t msg[256];
            swprintf_s(msg, L"[DLP] CreatePackageFromFile1 failed: 0x%08X\n", hr);
            OutputDebugStringW(msg);
            factory1->Release();
            if (needUninit) CoUninitialize();
            return false;
        }

        IXpsOMPackage* package = nullptr;
        package1->QueryInterface(__uuidof(IXpsOMPackage), (void**)&package);
        package1->Release();

        if (!package)
        {
            factory1->Release();
            if (needUninit) CoUninitialize();
            return false;
        }

        IXpsOMDocumentSequence* docSeq = nullptr;
        package->GetDocumentSequence(&docSeq);
        if (!docSeq)
        {
            package->Release();
            factory1->Release();
            if (needUninit) CoUninitialize();
            return false;
        }

        IXpsOMDocumentCollection* docs = nullptr;
        docSeq->GetDocuments(&docs);
        UINT32 docCount = 0;
        if (docs) docs->GetCount(&docCount);

        int globalPageIdx = 0;

        for (UINT32 d = 0; d < docCount; ++d)
        {
            IXpsOMDocument* xpsDoc = nullptr;
            docs->GetAt(d, &xpsDoc);
            if (!xpsDoc) continue;

            IXpsOMPageReferenceCollection* pageRefs = nullptr;
            xpsDoc->GetPageReferences(&pageRefs);
            UINT32 pageCount = 0;
            if (pageRefs) pageRefs->GetCount(&pageCount);

            for (UINT32 p = 0; p < pageCount; ++p)
            {
                IXpsOMPageReference* pageRef = nullptr;
                pageRefs->GetAt(p, &pageRef);
                if (!pageRef) continue;

                IXpsOMPage* page = nullptr;
                pageRef->GetPage(&page);
                if (!page) { pageRef->Release(); continue; }

                IXpsOMVisualCollection* visuals = nullptr;
                page->GetVisuals(&visuals);
                if (visuals)
                {
                    ExtractVisualsRecursive(visuals, doc, globalPageIdx);
                    visuals->Release();
                }

                doc.pageCount = globalPageIdx + 1;
                page->Release();
                pageRef->Release();
                ++globalPageIdx;
            }

            if (pageRefs) pageRefs->Release();
            xpsDoc->Release();
        }

        if (docs) docs->Release();
        docSeq->Release();
        package->Release();
        factory1->Release();
        if (needUninit) CoUninitialize();

        return !doc.textRuns.empty() || doc.pageCount > 0;
    }

private:
    static void ExtractVisualsRecursive(IXpsOMVisualCollection* visuals,
                                         XpsDocument& doc, int pageIdx)
    {
        UINT32 count = 0;
        visuals->GetCount(&count);

        for (UINT32 i = 0; i < count; ++i)
        {
            IXpsOMVisual* visual = nullptr;
            visuals->GetAt(i, &visual);
            if (!visual) continue;

            IXpsOMGlyphs* glyphs = nullptr;
            if (SUCCEEDED(visual->QueryInterface(__uuidof(IXpsOMGlyphs), (void**)&glyphs))
                && glyphs)
            {
                ExtractGlyphs(glyphs, doc, pageIdx);
                glyphs->Release();
            }

            IXpsOMCanvas* canvas = nullptr;
            if (SUCCEEDED(visual->QueryInterface(__uuidof(IXpsOMCanvas), (void**)&canvas))
                && canvas)
            {
                IXpsOMVisualCollection* childVisuals = nullptr;
                canvas->GetVisuals(&childVisuals);
                if (childVisuals)
                {
                    ExtractVisualsRecursive(childVisuals, doc, pageIdx);
                    childVisuals->Release();
                }
                canvas->Release();
            }

            visual->Release();
        }
    }

    static void ExtractGlyphs(IXpsOMGlyphs* glyphs, XpsDocument& doc, int pageIdx)
    {
        LPWSTR unicodeStr = nullptr;
        HRESULT hr = glyphs->GetUnicodeString(&unicodeStr);
        if (FAILED(hr) || !unicodeStr || wcslen(unicodeStr) == 0)
        {
            if (unicodeStr) CoTaskMemFree(unicodeStr);
            return;
        }

        XpsTextRun run;
        run.text = unicodeStr;
        run.pageIndex = pageIdx;
        CoTaskMemFree(unicodeStr);

        XPS_POINT origin = {};
        glyphs->GetOrigin(&origin);
        run.originX = origin.x;
        run.originY = origin.y;

        FLOAT emSize = 10.0f;
        glyphs->GetFontRenderingEmSize(&emSize);
        run.fontSize = emSize;

        IXpsOMFontResource* fontRes = nullptr;
        if (SUCCEEDED(glyphs->GetFontResource(&fontRes)) && fontRes)
        {
            IOpcPartUri* partUri = nullptr;
            if (SUCCEEDED(fontRes->GetPartName(&partUri)) && partUri)
            {
                BSTR uri = nullptr;
                if (SUCCEEDED(partUri->GetAbsoluteUri(&uri)) && uri)
                {
                    run.fontUri = uri;
                    SysFreeString(uri);
                }
                partUri->Release();
            }
            fontRes->Release();
        }

        run.height = run.fontSize;
        run.width  = run.fontSize * 0.55 * run.text.length();

        {
            UINT32 indexCount = 0;
            HRESULT hrIdx = glyphs->GetGlyphIndexCount(&indexCount);
            if (SUCCEEDED(hrIdx) && indexCount > 0)
            {
                std::vector<XPS_GLYPH_INDEX> gi(indexCount);
                hrIdx = glyphs->GetGlyphIndices(&indexCount, gi.data());
                if (SUCCEEDED(hrIdx))
                {
                    double w = 0;
                    for (UINT32 k = 0; k < indexCount; ++k)
                        w += gi[k].advanceWidth;
                    if (w > 0) run.width = w;
                }
            }
        }

        doc.textRuns.push_back(std::move(run));
    }
};

} // namespace dlp

#endif // XPS_CORE_H