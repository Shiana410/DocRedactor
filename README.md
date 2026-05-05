# DocRedactor

A Windows desktop application for detecting and redacting personally identifiable information (PII) from XPS documents.

## About

DocRedactor is a personal project exploring native Windows app development with the modern Microsoft UI stack. The goal is a focused desktop tool that can ingest XPS documents, scan them for sensitive data (names, addresses, phone numbers, ID numbers, and similar PII), and produce a redacted copy with that data masked or removed.

This repository represents the early scaffolding of the project. The application architecture is in place and the shell runs, but the redaction engine itself has not been implemented yet.

## Tech stack

- **Language:** C++ (C++/WinRT)
- **UI framework:** WinUI 3
- **Runtime:** Windows App SDK
- **Packaging:** MSIX (packaged desktop app)
- **Build tooling:** Visual Studio 2026, MSBuild, MIDLRT
- **Target platform:** Windows 10 (1809+) and Windows 11, x64

## Architecture

The solution consists of two C++ projects sharing a common header set:

- **DocRedactorApp** — the WinUI 3 packaged desktop application. Contains the UI, navigation, and user-facing logic. Built on a `Window` → `Frame` → `Page` shell pattern with a custom title bar.
- **DocRedactorEngine** — a Windows Runtime Component intended to host all redaction logic (XPS parsing, PII detection, document modification). Currently a stub; will grow into the substantive part of the project.
- **Common/** — shared C++ headers consumed by both projects via include path. Defines the contracts between UI and engine.

The split between App and Engine is deliberate: keeping redaction logic in a separate component makes it independently testable and forces clean API boundaries between presentation and processing.

## Current state

What works:

- WinUI 3 application shell with custom title bar and Frame-based navigation
- App-to-Engine project reference and shared header infrastructure
- Clean MSIX packaging configuration
- Builds and runs on Windows 11

What's next:

- Welcome page UI: drag-and-drop zone and file picker for selecting an XPS document
- XPS document parsing (reading text and structure from `.xps` / `.oxps` files)
- PII detection rules (regex-based first, with room to add more sophisticated detectors later)
- Document modification: producing a redacted output document preserving the original layout
- File association so users can open `.xps` files directly with DocRedactor

## Status

Active personal project. Bootstrapping took longer than expected due to some friction with the Visual Studio 2026 WinUI 3 C++ templates, but the foundation is now stable. Real feature work begins from this commit forward.