#pragma once
// =============================================================================
// XpsModifierCore.h - XPS document redaction with dynamic font matching
// =============================================================================
// Wrapped in dlp:: namespace. Original at Common/XpsModifier.h - this is a
// salvaged copy with the following adjustments only:
//   - Renamed include guard XPS_MODIFIER_H -> XPS_MODIFIER_CORE_H
//   - Wrapped class and free functions in namespace dlp { ... }
//   - Changed #include "DlpShared.h" -> "PiiShared.h"
// Logic body is byte-for-byte unchanged from the salvage.
// =============================================================================

#ifndef XPS_MODIFIER_CORE_H
#define XPS_MODIFIER_CORE_H

#include <windows.h>
#include <string>
#include <vector>
#include <map>
#include <xpsobjectmodel_1.h>
#include <shlwapi.h>
#include "PiiShared.h"

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shlwapi.lib")

namespace dlp
{

// =============================================================================
// MaskPiiText - Produce a masked version of detected PII
// =============================================================================
inline std::wstring MaskPiiText(const std::wstring& original, DWORD category)
{
    std::wstring result = original;
    const wchar_t MASK_CHAR = L'*';

    switch (category)
    {
    case PII_EMAIL:
    {
        size_t atPos = result.find(L'@');
        if (atPos != std::wstring::npos && atPos > 1) {
            for (size_t i = 1; i < atPos; ++i) result[i] = MASK_CHAR;
        }
        break;
    }
    case PII_PHONE:
    {
        bool firstInGroup = true;
        for (size_t i = 0; i < result.size(); ++i) {
            if (iswdigit(result[i])) {
                if (firstInGroup) firstInGroup = false;
                else result[i] = MASK_CHAR;
            }
            else firstInGroup = true;
        }
        break;
    }
    case PII_SSN:
    {
        // Show first digit + last 4 digits (e.g. "4**-**-7823").
        int digitCount = 0;
        for (auto ch : result) if (iswdigit(ch)) ++digitCount;

        if (digitCount < 5) break;  // not enough digits to do partial mask

        int seen = 0;
        for (auto& ch : result) {
            if (!iswdigit(ch)) continue;
            if (seen >= 1 && seen < digitCount - 4) ch = MASK_CHAR;
            ++seen;
        }
        break;
    }
    case PII_DATE_OF_BIRTH:
    {
        // Mask M/D, keep 4-digit year. Walk digit-groups; any group of exactly
        // 4 digits is preserved (the year), every other group is fully masked.
        size_t i = 0;
        while (i < result.size()) {
            if (!iswdigit(result[i])) { ++i; continue; }
            size_t groupStart = i;
            while (i < result.size() && iswdigit(result[i])) ++i;
            size_t groupLen = i - groupStart;
            if (groupLen != 4) {
                for (size_t j = groupStart; j < i; ++j) result[j] = MASK_CHAR;
            }
        }
        break;
    }
    case PII_CREDIT_CARD:
    {
        int digitCount = 0;
        for (auto ch : result) if (iswdigit(ch)) ++digitCount;
        int toMask = digitCount - 4, masked = 0;
        for (auto& ch : result) {
            if (iswdigit(ch) && masked < toMask) { ch = MASK_CHAR; ++masked; }
        }
        break;
    }
    case PII_IP_ADDRESS:
    {
        int dotCount = 0;
        for (size_t i = 0; i < result.size(); ++i) {
            if (result[i] == L'.') ++dotCount;
            else if (dotCount >= 2 && iswdigit(result[i])) result[i] = MASK_CHAR;
        }
        break;
    }
    default:
    {
        size_t len = result.size();
        if (len > 2) {
            for (size_t i = len / 5; i < len - len / 5; ++i)
                if (iswalnum(result[i])) result[i] = MASK_CHAR;
        }
        break;
    }
    }
    return result;
}

// =============================================================================
// XpsModifier class
// =============================================================================
class XpsModifier
{
public:
    static bool ApplyRedactions(const std::wstring& inputPath,
        const std::wstring& outputPath,
        const PiiItem* items,
        DWORD itemCount,
        bool /*deleteText*/ = false)
    {
        HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
        bool needUninit = SUCCEEDED(hr);

        IXpsOMObjectFactory1* factory = nullptr;
        hr = CoCreateInstance(__uuidof(XpsOMObjectFactory), NULL,
            CLSCTX_INPROC_SERVER, __uuidof(IXpsOMObjectFactory1), (void**)&factory);
        if (FAILED(hr)) { if (needUninit) CoUninitialize(); return false; }

        IXpsOMPackage1* package = nullptr;
        hr = factory->CreatePackageFromFile1(inputPath.c_str(), FALSE, &package);
        if (FAILED(hr)) { factory->Release(); if (needUninit) CoUninitialize(); return false; }

        // Cache to store dynamically injected fonts so we don't bloat the document
        std::map<std::wstring, IXpsOMFontResource*> fontCache;

        IXpsOMDocumentSequence* docSeq = nullptr;
        if (SUCCEEDED(package->GetDocumentSequence(&docSeq)) && docSeq) {
            IXpsOMDocumentCollection* docs = nullptr;
            if (SUCCEEDED(docSeq->GetDocuments(&docs)) && docs) {
                UINT32 docCount = 0; docs->GetCount(&docCount);
                for (UINT32 d = 0; d < docCount; ++d) {
                    IXpsOMDocument* doc = nullptr; docs->GetAt(d, &doc);
                    if (doc) {
                        IXpsOMPageReferenceCollection* pageRefs = nullptr;
                        doc->GetPageReferences(&pageRefs);
                        if (pageRefs) {
                            UINT32 pageCount = 0; pageRefs->GetCount(&pageCount);
                            for (UINT32 p = 0; p < pageCount; ++p) {
                                IXpsOMPageReference* pageRef = nullptr; pageRefs->GetAt(p, &pageRef);
                                if (pageRef) {
                                    IXpsOMPage* page = nullptr; pageRef->GetPage(&page);
                                    if (page) {
                                        IXpsOMVisualCollection* visuals = nullptr;
                                        page->GetVisuals(&visuals);
                                        if (visuals) {
                                            ProcessVisuals(visuals, items, itemCount, factory, fontCache);
                                            visuals->Release();
                                        }
                                        page->Release();
                                    }
                                    pageRef->Release();
                                }
                            }
                            pageRefs->Release();
                        }
                        doc->Release();
                    }
                }
                docs->Release();
            }
            docSeq->Release();
        }

        hr = package->WriteToFile(outputPath.c_str(), NULL, 0, FALSE);

        for (auto& pair : fontCache) pair.second->Release();
        package->Release();
        factory->Release();
        if (needUninit) CoUninitialize();

        return SUCCEEDED(hr);
    }

private:
    // -------------------------------------------------------------------------
    // Cryptographic De-obfuscator & TTF Binary Parser
    // -------------------------------------------------------------------------
    static std::wstring ExtractFontFamilyName(IXpsOMFontResource* fontRes) {
        IOpcPartUri* uri = nullptr;
        if (FAILED(fontRes->GetPartName(&uri)) || !uri) return L"";
        BSTR bstrUri; uri->GetAbsoluteUri(&bstrUri);
        std::wstring uriStr = bstrUri;
        SysFreeString(bstrUri);
        uri->Release();

        bool isObfuscated = (uriStr.find(L".odttf") != std::wstring::npos || uriStr.find(L".odttc") != std::wstring::npos);

        IStream* stream = nullptr;
        if (FAILED(fontRes->GetStream(&stream)) || !stream) return L"";

        STATSTG stat; stream->Stat(&stat, STATFLAG_NONAME);
        size_t size = stat.cbSize.QuadPart;
        if (size < 1024) { stream->Release(); return L""; }

        std::vector<uint8_t> data(size);
        ULONG read = 0;
        stream->Read(data.data(), (ULONG)size, &read);
        stream->Release();

        if (isObfuscated) {
            size_t slash = uriStr.find_last_of(L'/');
            size_t dot = uriStr.find_last_of(L'.');
            std::wstring guidStr = uriStr.substr(slash + 1, dot - slash - 1);
            std::wstring hexStr;
            for (wchar_t c : guidStr) if (c != L'-') hexStr += c;
            if (hexStr.length() == 32) {
                uint8_t guidBytes[16];
                for (int i = 0; i < 16; ++i) {
                    std::wstring byteStr = hexStr.substr(i * 2, 2);
                    guidBytes[i] = (uint8_t)wcstoul(byteStr.c_str(), nullptr, 16);
                }
                uint8_t xorKey[16];
                for (int i = 0; i < 16; ++i) xorKey[i] = guidBytes[15 - i];
                for (int i = 0; i < 32; ++i) data[i] ^= xorKey[i % 16];
            }
        }

        uint16_t numTables = (data[4] << 8) | data[5];
        size_t nameTableOffset = 0;
        for (int i = 0; i < numTables; ++i) {
            size_t off = 12 + i * 16;
            if (off + 16 > size) break;
            if (data[off] == 'n' && data[off + 1] == 'a' && data[off + 2] == 'm' && data[off + 3] == 'e') {
                nameTableOffset = (data[off + 8] << 24) | (data[off + 9] << 16) | (data[off + 10] << 8) | data[off + 11];
                break;
            }
        }

        if (nameTableOffset > 0 && nameTableOffset + 6 <= size) {
            uint16_t count = (data[nameTableOffset + 2] << 8) | data[nameTableOffset + 3];
            uint16_t stringOffset = (data[nameTableOffset + 4] << 8) | data[nameTableOffset + 5];
            size_t storageOffset = nameTableOffset + stringOffset;
            for (int i = 0; i < count; ++i) {
                size_t recOff = nameTableOffset + 6 + i * 12;
                if (recOff + 12 > size) break;
                uint16_t platformID = (data[recOff] << 8) | data[recOff + 1];
                uint16_t nameID = (data[recOff + 6] << 8) | data[recOff + 7];
                if (platformID == 3 && nameID == 1) {
                    uint16_t length = (data[recOff + 8] << 8) | data[recOff + 9];
                    uint16_t offset = (data[recOff + 10] << 8) | data[recOff + 11];
                    if (storageOffset + offset + length <= size) {
                        std::wstring familyName;
                        for (int j = 0; j < length; j += 2) {
                            wchar_t ch = (data[storageOffset + offset + j] << 8) | data[storageOffset + offset + j + 1];
                            familyName += ch;
                        }
                        return familyName;
                    }
                }
            }
        }
        return L"";
    }

    // -------------------------------------------------------------------------
    // System Font Mapper
    // -------------------------------------------------------------------------
    static std::wstring MapFontToSystemFile(const std::wstring& familyName) {
        if (familyName.find(L"Times") != std::wstring::npos) return L"C:\\Windows\\Fonts\\times.ttf";
        if (familyName.find(L"Arial") != std::wstring::npos) return L"C:\\Windows\\Fonts\\arial.ttf";
        if (familyName.find(L"Calibri") != std::wstring::npos) return L"C:\\Windows\\Fonts\\calibri.ttf";
        if (familyName.find(L"Consolas") != std::wstring::npos) return L"C:\\Windows\\Fonts\\consola.ttf";
        if (familyName.find(L"Courier") != std::wstring::npos) return L"C:\\Windows\\Fonts\\cour.ttf";
        if (familyName.find(L"Segoe") != std::wstring::npos) return L"C:\\Windows\\Fonts\\segoeui.ttf";
        if (familyName.find(L"Tahoma") != std::wstring::npos) return L"C:\\Windows\\Fonts\\tahoma.ttf";
        return L"C:\\Windows\\Fonts\\arial.ttf";
    }

    // -------------------------------------------------------------------------
    // Dynamic Font Injector
    // -------------------------------------------------------------------------
    static IXpsOMFontResource* CreateDynamicFontResource(IXpsOMObjectFactory1* factory, const std::wstring& sysPath, size_t indexId) {
        IStream* fontStream = nullptr;
        if (FAILED(SHCreateStreamOnFileW(sysPath.c_str(), STGM_READ, &fontStream))) return nullptr;

        wchar_t uriStr[128];
        swprintf_s(uriStr, L"/Resources/Fonts/DynamicRedact_%zu.ttf", indexId);

        IOpcPartUri* fontUri = nullptr;
        factory->CreatePartUri(uriStr, &fontUri);

        IXpsOMFontResource* fontRes = nullptr;
        factory->CreateFontResource(fontStream, XPS_FONT_EMBEDDING_NORMAL, fontUri, FALSE, &fontRes);

        if (fontUri) fontUri->Release();
        if (fontStream) fontStream->Release();
        return fontRes;
    }

    // -------------------------------------------------------------------------
    // Tree Traversal
    // -------------------------------------------------------------------------
    static void ProcessVisuals(IXpsOMVisualCollection* visuals, const PiiItem* items, DWORD itemCount,
        IXpsOMObjectFactory1* factory, std::map<std::wstring, IXpsOMFontResource*>& fontCache)
    {
        UINT32 count = 0; visuals->GetCount(&count);
        for (UINT32 i = 0; i < count; ++i) {
            IXpsOMVisual* visual = nullptr; visuals->GetAt(i, &visual);
            if (!visual) continue;

            IXpsOMGlyphs* glyphs = nullptr;
            if (SUCCEEDED(visual->QueryInterface(__uuidof(IXpsOMGlyphs), (void**)&glyphs)) && glyphs) {
                LPWSTR text = nullptr;
                if (SUCCEEDED(glyphs->GetUnicodeString(&text)) && text) {
                    std::wstring strText = text;
                    bool modified = false;

                    for (DWORD j = 0; j < itemCount; ++j) {
                        if (!items[j].shouldMask) continue;
                        std::wstring target = items[j].text;
                        std::wstring replacement = MaskPiiText(target, items[j].category);
                        size_t pos = 0;
                        while ((pos = strText.find(target, pos)) != std::wstring::npos) {
                            strText.replace(pos, target.length(), replacement);
                            pos += replacement.length();
                            modified = true;
                        }
                    }

                    if (modified) {
                        IXpsOMGlyphsEditor* editor = nullptr;
                        if (SUCCEEDED(glyphs->GetGlyphsEditor(&editor)) && editor) {
                            editor->SetUnicodeString(strText.c_str());
                            editor->SetGlyphIndices(0, NULL);
                            editor->ApplyEdits();
                            editor->Release();
                        }

                        IXpsOMFontResource* origFont = nullptr;
                        if (SUCCEEDED(glyphs->GetFontResource(&origFont)) && origFont) {
                            std::wstring familyName = ExtractFontFamilyName(origFont);
                            std::wstring sysPath = MapFontToSystemFile(familyName);

                            if (fontCache.find(sysPath) == fontCache.end()) {
                                fontCache[sysPath] = CreateDynamicFontResource(factory, sysPath, fontCache.size());
                            }
                            if (fontCache[sysPath]) {
                                glyphs->SetFontResource(fontCache[sysPath]);
                            }
                            origFont->Release();
                        }
                    }
                    CoTaskMemFree(text);
                }
                glyphs->Release();
            }

            IXpsOMCanvas* canvas = nullptr;
            if (SUCCEEDED(visual->QueryInterface(__uuidof(IXpsOMCanvas), (void**)&canvas)) && canvas) {
                IXpsOMVisualCollection* childVisuals = nullptr;
                canvas->GetVisuals(&childVisuals);
                if (childVisuals) {
                    ProcessVisuals(childVisuals, items, itemCount, factory, fontCache);
                    childVisuals->Release();
                }
                canvas->Release();
            }

            visual->Release();
        }
    }
};

} // namespace dlp

#endif // XPS_MODIFIER_CORE_H