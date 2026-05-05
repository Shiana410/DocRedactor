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
#include <winrt/Microsoft.UI.Dispatching.h>

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

        auto file = items.GetAt(0).try_as<StorageFile>();
        if (file == nullptr)
        {
            OutputDebugStringW(L"[DocRedactor] Drop ignored -- not a file.\n");
            co_return;
        }

        auto ext = file.FileType();
        if (ext != L".xps" && ext != L".oxps")
        {
            std::wstring msg = L"[DocRedactor] Drop ignored -- wrong extension: ";
            msg += ext;
            msg += L"\n";
            OutputDebugStringW(msg.c_str());

            ShowUnsupportedFileMessage(ext);
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

    IAsyncAction WelcomePage::ShowUnsupportedFileMessage(
        winrt::hstring const& fileExtension)
    {
        // Capture a strong ref to ourselves so the page isn't destroyed
        // mid-coroutine if navigation happens.
        auto strongThis = get_strong();

        // Capture the UI dispatcher while we're still on the UI thread.
        auto uiDispatcher = DispatcherQueue();

        std::wstring msg = L"DocRedactor only accepts .xps and .oxps files. You dropped: ";
        msg += fileExtension;

        UnsupportedFileBar().Message(msg);
        UnsupportedFileBar().IsOpen(true);

        // Wait 4 seconds on a thread-pool timer.
        co_await std::chrono::seconds{ 4 };

        // Marshal back to the UI thread via TryEnqueue, since std::chrono
        // awaits resume on the thread pool. We can't touch UnsupportedFileBar
        // directly here -- it'd throw hresult_wrong_thread.
        uiDispatcher.TryEnqueue([weakThis = get_weak()]()
            {
                if (auto self = weakThis.get())
                {
                    self->UnsupportedFileBar().IsOpen(false);
                }
            });
    }
}