# DocRedactor

A Windows desktop application for detecting and redacting personally identifiable information (PII) from XPS and OpenXPS documents.

## About

DocRedactor is a personal project exploring native Windows app development with the modern Microsoft UI stack. The application ingests XPS/OXPS documents, scans them for sensitive data across six PII categories, and produces a redacted copy with sensitive content masked while preserving the original layout.

The redaction engine includes a real XPS document parser (via Windows' XPS Object Model API), a regex-based PII detector, and a document modifier that handles XPS's font obfuscation scheme — reversing the GUID-XOR-encoded font headers, parsing TTF name tables to identify the original font family, and dynamically substituting a system font that contains the masking glyph.

## Tech stack

- **Language:** C++ (C++/WinRT)
- **UI framework:** WinUI 3
- **Runtime:** Windows App SDK
- **Packaging:** MSIX (packaged desktop app)
- **Build tooling:** Visual Studio 2026, MSBuild, MIDLRT
- **Target platform:** Windows 10 (1809+) and Windows 11, x64

## Architecture

The solution consists of two C++ projects with a shared salvage layer:

- **DocRedactorApp** — the WinUI 3 packaged desktop application. Contains UI, navigation, file pickers, drag-drop, and user-facing logic. Built on a `Window` → `Frame` → `Page` shell pattern with a custom title bar.

- **DocRedactorEngine** — a Windows Runtime Component hosting all redaction logic. Exposes five runtimeclasses to the App via the WinRT projection layer: `XpsParser`, `TextSegment`, `PiiDetector`, `PiiMatch`, and `Redactor`. Heavy work happens on background thread-pool threads via `co_await winrt::resume_background()`.

- **Common/** — original salvaged C++ headers from an earlier port-monitor incarnation of the project. Wrapped in a `dlp::` namespace and included into the engine as `XpsCore.h`, `PiiCore.h`, and `XpsModifierCore.h`. The redaction logic itself is unchanged from the salvaged code; the engine adds WinRT projection on top.

The App-Engine split is deliberate: keeping redaction logic in a separate component makes it independently testable, forces clean API boundaries between presentation and processing, and physically separates COM-heavy native work from XAML rendering.

## Pipeline

StorageFile → XpsParser.ParseAsync()  → IVectorView<TextSegment>
↓
PiiDetector.DetectAsync(segments)
↓
IVectorView<PiiMatch>
↓
ReviewPage shows findings → user confirms → Redactor.RedactAsync()
↓
Modified .oxps written to user-chosen path

PII detection covers six categories (email, phone, SSN, credit card, IP address, date of birth) with category-aware masking strategies — emails preserve the domain, phones preserve the first digit per group, credit cards preserve the last four digits, etc.

## Current state

**Working:**
- WinUI 3 application shell with custom title bar and Frame-based navigation
- File selection via picker and drag-drop with file-type validation
- Real XPS/OXPS parsing extracting positioned text runs from documents
- PII regex detection across six categories with full segment provenance
- Match display showing category, matched text, and source location
- Redaction with dynamic font matching: detects the original embedded font, reverses XPS GUID-XOR obfuscation to read the TTF name table, maps to a Windows system font, and injects the substitute font for redacted text
- Output to user-chosen `.oxps` destination via FileSavePicker
- Status feedback via WinUI InfoBar (success/error)

**In progress (toward v1.0):**
- Per-match toggles so users can deselect false positives before redacting
- Settings flyout for save preferences and PII category enablement
- Side-by-side page preview with bidirectional list-selection highlighting

## Roadmap

- [x] **v0.9** — Engine pipeline complete: parse → detect → redact, list-only review UI, FileSavePicker output
- [ ] **v1.0** — Per-match toggles, settings flyout (save mode + category toggles), side-by-side page preview, persisted preferences via `LocalSettings`
- [ ] **v1.1** — PDF support via PDFium or `Windows.Data.Pdf` (text extraction with bounding boxes)
- [ ] **v1.2** — Batch mode for processing multiple files
- [ ] **v1.3** — Keyboard shortcuts (Ctrl+O, Ctrl+S, page navigation), recent files list
- [ ] **v1.4** — PII list filtering, toast notifications after save, High Contrast theme support
- [ ] **v2.0** — Document-centric review: render full pages with inline PII highlighting and click-to-toggle

## Building

Requirements:
- Visual Studio 2026 (or later) with the **Desktop development with C++** workload and the **Universal Windows Platform development** workload
- Windows 11 SDK 10.0.22621 or later
- C++/WinRT Visual Studio Extension

Steps:
1. Clone: `git clone https://github.com/Shiana410/DocRedactor.git`
2. Open `DocRedactor.sln` in Visual Studio
3. Set platform to `x64`, set `DocRedactorApp` as the startup project
4. Build → Build Solution

Each developer generates their own code-signing dev cert on first build — Visual Studio handles this automatically.

## Backstory

This project began as a Print Support App-based DLP virtual printer that intercepted print jobs system-wide. After hitting deployment constraints around hardware-bound printer association, the architecture was rebuilt around explicit user-driven document selection. The XPS parsing, PII detection, and document modification code from the original incarnation survived the pivot intact and now lives under `Common/`, wrapped into the engine via `dlp::` namespace headers.

## License

Not yet licensed. MIT under consideration for v1.0.
