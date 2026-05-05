#include "pch.h"
#include "WelcomePage.xaml.h"
#if __has_include("WelcomePage.g.cpp")
#include "WelcomePage.g.cpp"
#endif

#include <winrt/Windows.ApplicationModel.DataTransfer.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::ApplicationModel::DataTransfer;
using namespace Windows::Storage;
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
        OutputDebugStringW(L"[DocRedactor] File dropped — handler stub.\n");
        // TODO Step 13+: extract StorageFile from e.DataView(),
        // validate .xps/.oxps extension, navigate to ReviewPage with file.
    }

    IAsyncAction WelcomePage::OpenFileButton_Click(
        IInspectable const& /*sender*/,
        RoutedEventArgs const& /*e*/)
    {
        OutputDebugStringW(L"[DocRedactor] Open File clicked — picker wiring deferred to Step 12B.\n");
        co_return;
    }
}