#include "pch.h"
#include "WelcomePage.xaml.h"
#if __has_include("WelcomePage.g.cpp")
#include "WelcomePage.g.cpp"
#endif

#include "App.xaml.h"
#include "ReviewPage.xaml.h"

#include <winrt/Windows.ApplicationModel.DataTransfer.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Pickers.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Interop.h>

#include <Shobjidl.h>

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::ApplicationModel::DataTransfer;
using namespace Windows::Storage;
using namespace Windows::Storage::Pickers;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Interop;

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

    IAsyncAction WelcomePage::DropZone_Drop(
        IInspectable const& /*sender*/,
        DragEventArgs const& e)
    {
        DropZone().BorderThickness(ThicknessHelper::FromUniformLength(2));

        if (!e.DataView().Contains(StandardDataFormats::StorageItems()))
        {
            OutputDebugStringW(L"[DocRedactor] Drop ignored -- no storage items.\n");
            co_return;
        }

        auto items = co_await e.DataView().GetStorageItemsAsync();
        if (items.Size() == 0)
        {
            OutputDebugStringW(L"[DocRedactor] Drop ignored -- empty item list.\n");
            co_return;
        }

        // Take only the first dropped item; ignore extras for v1.
        auto file = items.GetAt(0).try_as<StorageFile>();
        if (file == nullptr)
        {
            OutputDebugStringW(L"[DocRedactor] Drop ignored -- not a file.\n");
            co_return;
        }

        auto ext = file.FileType();  // includes the leading dot, e.g. ".xps"
        if (ext != L".xps" && ext != L".oxps")
        {
            std::wstring msg = L"[DocRedactor] Drop ignored -- wrong extension: ";
            msg += ext;
            msg += L"\n";
            OutputDebugStringW(msg.c_str());
            co_return;
        }

        std::wstring msg = L"[DocRedactor] File dropped: ";
        msg += file.Path();
        msg += L"\n";
        OutputDebugStringW(msg.c_str());

        NavigateToReview(file.Path());
    }

    IAsyncAction WelcomePage::OpenFileButton_Click(
        IInspectable const& /*sender*/,
        RoutedEventArgs const& /*e*/)
    {
        OutputDebugStringW(L"[DocRedactor] Open File clicked.\n");

        FileOpenPicker picker;
        picker.SuggestedStartLocation(PickerLocationId::DocumentsLibrary);
        picker.FileTypeFilter().Append(L".xps");
        picker.FileTypeFilter().Append(L".oxps");

        // WinUI 3 packaged apps require explicit window association.
        HWND hwnd = implementation::App::MainWindowHandle();
        auto initWithWindow = picker.as<::IInitializeWithWindow>();
        check_hresult(initWithWindow->Initialize(hwnd));

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

        NavigateToReview(file.Path());
    }

    void WelcomePage::NavigateToReview(winrt::hstring const& path)
    {
        if (auto frame = Frame())
        {
            frame.Navigate(
                xaml_typename<DocRedactorApp::ReviewPage>(),
                box_value(path));
        }
        else
        {
            OutputDebugStringW(L"[DocRedactor] No parent Frame -- cannot navigate.\n");
        }
    }
}