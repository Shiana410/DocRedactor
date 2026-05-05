#pragma once
// =============================================================================
// DlpShared.h  –  Types shared across all DLP Virtual Printer components
// =============================================================================
// Used by: BackgroundTask, ForegroundUI
// =============================================================================

#include <windows.h>
#include <stdint.h>

// Output folders
#define DLP_SPOOL_FOLDER        L"C:\\ProgramData\\DlpVirtualPrinter\\Spool"
#define DLP_OUTPUT_FOLDER       L"C:\\ProgramData\\DlpVirtualPrinter\\Output"
#define DLP_LOG_FOLDER          L"C:\\ProgramData\\DlpVirtualPrinter\\Logs"

// Virtual printer name (the printer we associate with via Add-PrintSupportAppAssociation)
#define DLP_PRINTER_NAME        L"DLP Secure Printer"

// ─────────────────────────────────────────────────────────────────────────────
// PII category flags
// ─────────────────────────────────────────────────────────────────────────────
#define PII_EMAIL           0x01
#define PII_PHONE           0x02
#define PII_SSN             0x04
#define PII_CREDIT_CARD     0x08
#define PII_IP_ADDRESS      0x10
#define PII_DATE_OF_BIRTH   0x20

// ─────────────────────────────────────────────────────────────────────────────
// Size limits
// ─────────────────────────────────────────────────────────────────────────────
#define MAX_PII_TEXT        256
#define MAX_PII_ITEMS       512
#define MAX_PROCESS_NAME    128

// ─────────────────────────────────────────────────────────────────────────────
// A single detected PII hit
// (Background task fills these, foreground UI lets user toggle shouldMask,
//  background task applies mask based on the choices)
// ─────────────────────────────────────────────────────────────────────────────
struct PiiItem
{
    wchar_t  text[MAX_PII_TEXT];     // The matched PII string
    DWORD    category;               // PII_* bitmask
    BOOL     shouldMask;             // User sets TRUE/FALSE in UI
    int      pageIndex;              // 0-based page number where found
    double   x, y, width, height;    // Bounding box in XPS coordinate space (1/96 inch)
};

// ─────────────────────────────────────────────────────────────────────────────
// Job routing options (what to do after masking)
// ─────────────────────────────────────────────────────────────────────────────
#define DLP_ROUTE_PDF           0x01   // Save as PDF
#define DLP_ROUTE_PHYSICAL      0x02   // Forward to a physical printer
#define DLP_ROUTE_BOTH          0x03   // Both

struct DlpJobConfig
{
    DWORD   routingMode;                    // DLP_ROUTE_*
    wchar_t targetPrinter[128];             // Physical printer name (if routing)
    wchar_t outputPath[MAX_PATH];           // PDF output path (if saving)
};