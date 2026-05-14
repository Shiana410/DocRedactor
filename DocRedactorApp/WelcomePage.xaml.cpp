#include "pch.h"
#include "WelcomePage.xaml.h"
#if __has_include("WelcomePage.g.cpp")
#include "WelcomePage.g.cpp"
#endif

#include "App.xaml.h"
#include "ReviewPage.xaml.h"
#include "RecentFilesStore.h"

#include <winrt/Windows.ApplicationModel.DataTransfer.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Pickers.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Interop.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.Navigation.h>
#include <winrt/Microsoft.UI.Dispatching.h>

#include <filesystem>
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
        RefreshRecentsList();
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

    void WelcomePage::CtrlO_Invoked(
        Microsoft::UI::Xaml::Input::KeyboardAccelerator const& /*sender*/,
        Microsoft::UI::Xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args)
    {
        args.Handled(true);

        // Reuse the existing click handler. Fire-and-forget — the picker is
        // async and there's nothing for us to do with the result here.
        OpenFileButton_Click(OpenFileButton(), nullptr);
    }

    void WelcomePage::NavigateToReview(winrt::hstring const& path)
    {
        // Record this open in recents before navigating — even if the
        // navigation itself fails, the file was successfully selected and
        // belongs in recents.
        winrt::DocRedactorApp::RecentFilesStore::Bump(path);

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

    void WelcomePage::OnNavigatedTo(
        Microsoft::UI::Xaml::Navigation::NavigationEventArgs const& /*e*/)
    {
        // Refresh recents when coming back from ReviewPage — the file we
        // just opened needs to bubble to the top of the list.
        RefreshRecentsList();
    }

    void WelcomePage::RefreshRecentsList()
    {
        auto entries = winrt::DocRedactorApp::RecentFilesStore::Load();

        // ListView gets cleared and rebuilt. With max 10 items, the cost
        // is trivial and avoids fiddly partial-update logic.
        RecentsList().Items().Clear();

        for (auto const& entry : entries)
        {
            auto row = BuildRecentRow(entry);
            RecentsList().Items().Append(row);
        }

        bool hasEntries = !entries.empty();
        RecentsList().Visibility(hasEntries ? Visibility::Visible : Visibility::Collapsed);
        RecentsEmptyText().Visibility(hasEntries ? Visibility::Collapsed : Visibility::Visible);
        ClearRecentsButton().IsEnabled(hasEntries);
    }

    Microsoft::UI::Xaml::FrameworkElement WelcomePage::BuildRecentRow(
        winrt::DocRedactorApp::RecentFile const& entry)
    {
        // Split path into filename + parent folder using std::filesystem.
        // entry.Path is absolute Windows path, e.g. "C:\Foo\bar.xps".
        std::wstring fullPath{ entry.Path };
        std::filesystem::path p{ fullPath };
        std::wstring filename = p.filename().wstring();
        std::wstring parent = p.parent_path().wstring();

        if (filename.empty())
        {
            // Defensive: malformed path. Fall back to the whole string.
            filename = fullPath;
            parent = L"";
        }

        StackPanel stack;
        stack.Orientation(Orientation::Vertical);
        stack.Spacing(2);

        TextBlock nameBlock;
        nameBlock.Text(winrt::hstring{ filename });
        nameBlock.Style(
            Application::Current().Resources()
            .Lookup(box_value(L"BodyStrongTextBlockStyle"))
            .as<Microsoft::UI::Xaml::Style>());
        nameBlock.TextTrimming(TextTrimming::CharacterEllipsis);
        nameBlock.TextWrapping(TextWrapping::NoWrap);

        TextBlock folderBlock;
        folderBlock.Text(winrt::hstring{ parent });
        folderBlock.Style(
            Application::Current().Resources()
            .Lookup(box_value(L"CaptionTextBlockStyle"))
            .as<Microsoft::UI::Xaml::Style>());
        folderBlock.Foreground(
            Application::Current().Resources()
            .Lookup(box_value(L"TextFillColorSecondaryBrush"))
            .as<Microsoft::UI::Xaml::Media::Brush>());
        folderBlock.TextTrimming(TextTrimming::CharacterEllipsis);
        folderBlock.TextWrapping(TextWrapping::NoWrap);

        stack.Children().Append(nameBlock);
        stack.Children().Append(folderBlock);

        // Stash the full path in Tag so the click handler can find it
        // without round-tripping through the displayed text.
        stack.Tag(box_value(entry.Path));

        return stack;
    }

    winrt::Windows::Foundation::IAsyncAction WelcomePage::RecentsList_ItemClick(
        IInspectable const& /*sender*/,
        Microsoft::UI::Xaml::Controls::ItemClickEventArgs const& e)
    {
        auto clicked = e.ClickedItem().try_as<FrameworkElement>();
        if (clicked == nullptr) co_return;

        auto pathBoxed = clicked.Tag();
        auto path = winrt::unbox_value_or<winrt::hstring>(pathBoxed, L"");
        if (path.empty()) co_return;

        // Lazy stale handling: try to open. If it fails, set a flag and
        // handle outside the try block — C++ coroutines forbid co_await
        // inside a catch handler, so we can't auto-dismiss the toast there.
        bool fileExists = true;
        try
        {
            auto file = co_await Windows::Storage::StorageFile::GetFileFromPathAsync(path);
            (void)file;  // existence check only; NavigateToReview re-opens.
        }
        catch (winrt::hresult_error const&)
        {
            fileExists = false;
        }

        if (fileExists)
        {
            NavigateToReview(path);
            co_return;
        }

        // Stale file path: prune from recents, show toast, auto-dismiss.
        winrt::DocRedactorApp::RecentFilesStore::Remove(path);
        RefreshRecentsList();

        UnsupportedFileBar().Severity(Microsoft::UI::Xaml::Controls::InfoBarSeverity::Warning);
        UnsupportedFileBar().Title(L"File not found");
        std::wstring msg = L"Removed from recents: ";
        msg += std::wstring{ path };
        UnsupportedFileBar().Message(winrt::hstring{ msg });
        UnsupportedFileBar().IsOpen(true);

        auto uiDispatcher = DispatcherQueue();
        co_await std::chrono::seconds{ 4 };
        uiDispatcher.TryEnqueue([weakThis = get_weak()]()
            {
                if (auto self = weakThis.get())
                {
                    self->UnsupportedFileBar().IsOpen(false);
                }
            });
    }

    void WelcomePage::ClearRecentsButton_Click(
        IInspectable const& /*sender*/,
        RoutedEventArgs const& /*e*/)
    {
        winrt::DocRedactorApp::RecentFilesStore::Clear();
        RefreshRecentsList();
    }
}