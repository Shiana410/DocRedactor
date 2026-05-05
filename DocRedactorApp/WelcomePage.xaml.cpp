#include "pch.h"
#include "WelcomePage.xaml.h"
#if __has_include("WelcomePage.g.cpp")
#include "WelcomePage.g.cpp"
#endif

#include "App.xaml.h"
#include <winrt/DocRedactorApp.h>

#include <winrt/Windows.ApplicationModel.DataTransfer.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Pickers.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>

#include <microsoft.ui.xaml.window.h>
#include <Shobjidl.h>

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::ApplicationModel::DataTransfer;
using namespace Windows::Storage;
using namespace Windows::Storage::Pickers;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;

namespace winrt::DocRedactorApp::implementation
{
    WelcomePage::WelcomePage()
    {
        InitializeComponent();
    }

    void WelcomePage::DropZone_DragOver(
        IInspectable const& /*sender*/,
        DragEventArgs const& e)
    {
        if (e.DataView().Contains(StandardDataFormats::StorageItems()))
        {
            e.AcceptedOperation(DataPackageOperation::Copy);
            DropZone().BorderThickness(ThicknessHelper::FromUniformLength(3));
        }
        else
        {
            e.AcceptedOperation(DataPackageOperation::None);
        }
    }

    void WelcomePage::DropZone_DragLeave(
        IInspectable const& /*sender*/,
        DragEventArgs const& /*e*/)
    {
        DropZone().BorderThickness(ThicknessHelper::FromUniformLength(2));
    }

    void WelcomePage::DropZone_Drop(
        IInspectable const& /*sender*/,
        DragEventArgs const& /*e*/)
    {
        DropZone().BorderThickness(ThicknessHelper::FromUniformLength(2));
        OutputDebugStringW(L"[DocRedactor] File dropped -- handler stub.\n");
        // TODO Step 13+: extract StorageFile from e.DataView(),
        // validate .xps/.oxps extension, navigate to ReviewPage with file.
    }

    IAsyncAction WelcomePage::OpenFileButton_Click(
        IInspectable const& /*sender*/,
        RoutedEventArgs const& /*e*/)
    {
        OutputDebugStringW(L"[DocRedactor] Open File clicked.\n");

        // Build the picker
        FileOpenPicker picker;
        picker.SuggestedStartLocation(PickerLocationId::DocumentsLibrary);
        picker.FileTypeFilter().Append(L".xps");
        picker.FileTypeFilter().Append(L".oxps");

        // Bind the picker to our main window's HWND.
        // WinUI 3 packaged apps require explicit window association -- without
        // this, PickSingleFileAsync throws E_FAIL "no window handle."
        // Get the cached HWND directly from App (no projection/get_self needed).
        HWND hwnd = implementation::App::MainWindowHandle();

        auto initWithWindow = picker.as<::IInitializeWithWindow>();
        check_hresult(initWithWindow->Initialize(hwnd));

        // Show the picker
        StorageFile file = co_await picker.PickSingleFileAsync();
        if (file == nullptr)
        {
            OutputDebugStringW(L"[DocRedactor] File picker cancelled.\n");
            co_return;
        }

        std::wstring msg = L"[DocRedactor] Selected: ";
        msg += file.Path();
        msg += L"\n";
        OutputDebugStringW(msg.c_str());

        // TODO Step 13+: navigate to ReviewPage with the file as parameter.
    }
}