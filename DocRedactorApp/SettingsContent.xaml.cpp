#include "pch.h"
#include "SettingsContent.xaml.h"
#if __has_include("SettingsContent.g.cpp")
#include "SettingsContent.g.cpp"
#endif

#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Pickers.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>

#include <Shobjidl.h>

#include "App.xaml.h"
#include "AppSettings.h"

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Storage;
using namespace Windows::Storage::Pickers;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;

namespace winrt::DocRedactorApp::implementation
{
    SettingsContent::SettingsContent()
    {
        InitializeComponent();
        LoadSettingsIntoControls();
    }

    void SettingsContent::LoadSettingsIntoControls()
    {
        switch (AppSettings::GetSaveMode())
        {
        case AppSettings::SaveMode::NextTo: SaveModeNextToRadio().IsChecked(true); break;
        case AppSettings::SaveMode::Fixed:  SaveModeFixedRadio().IsChecked(true);  break;
        case AppSettings::SaveMode::Ask:
        default:                            SaveModeAskRadio().IsChecked(true);    break;
        }

        m_pickedFolder = AppSettings::GetFixedSaveFolder();
        FixedFolderText().Text(m_pickedFolder);

        uint32_t flags = AppSettings::GetDetectionFlags();
        EmailCheck().IsChecked((flags & AppSettings::Pii_Email) != 0);
        PhoneCheck().IsChecked((flags & AppSettings::Pii_Phone) != 0);
        SsnCheck().IsChecked((flags & AppSettings::Pii_Ssn) != 0);
        CreditCardCheck().IsChecked((flags & AppSettings::Pii_CreditCard) != 0);
        IpCheck().IsChecked((flags & AppSettings::Pii_IpAddress) != 0);
        DobCheck().IsChecked((flags & AppSettings::Pii_DateOfBirth) != 0);
    }

    winrt::Windows::Foundation::IAsyncAction SettingsContent::BrowseFolderButton_Click(
        IInspectable const& /*sender*/,
        RoutedEventArgs const& /*e*/)
    {
        FolderPicker picker;
        picker.SuggestedStartLocation(PickerLocationId::DocumentsLibrary);
        picker.FileTypeFilter().Append(L"*");

        HWND hwnd = implementation::App::MainWindowHandle();
        auto initWithWindow = picker.as<::IInitializeWithWindow>();
        initWithWindow->Initialize(hwnd);

        auto folder = co_await picker.PickSingleFolderAsync();

        if (folder == nullptr)
        {
            // User cancelled the folder picker.
            co_return;
        }

        // Persist immediately. The flyout may have been light-dismissed while the
        // picker dialog was open (focus loss closes WinUI flyouts), so we can't rely
        // on Save being clicked afterwards. Folder selection is an explicit gesture
        // and is treated as commit-on-pick separately from the Save/Cancel pattern
        // used for the rest of the settings.
        m_pickedFolder = folder.Path();
        AppSettings::SetFixedSaveFolder(m_pickedFolder);

        // Best-effort UI update. If the flyout was dismissed during picker open,
        // FixedFolderText may not be in the visual tree any more — try anyway.
        try
        {
            FixedFolderText().Text(m_pickedFolder);
        }
        catch (...)
        {
            // Swallow - the persistence above already succeeded.
        }
    }

    void SettingsContent::SaveButton_Click(
        IInspectable const& /*sender*/,
        RoutedEventArgs const& /*e*/)
    {
        AppSettings::SaveMode mode = AppSettings::SaveMode::Ask;
        if (SaveModeNextToRadio().IsChecked().GetBoolean())      mode = AppSettings::SaveMode::NextTo;
        else if (SaveModeFixedRadio().IsChecked().GetBoolean())  mode = AppSettings::SaveMode::Fixed;
        AppSettings::SetSaveMode(mode);

        AppSettings::SetFixedSaveFolder(m_pickedFolder);

        uint32_t flags = 0;
        if (EmailCheck().IsChecked().GetBoolean())      flags |= AppSettings::Pii_Email;
        if (PhoneCheck().IsChecked().GetBoolean())      flags |= AppSettings::Pii_Phone;
        if (SsnCheck().IsChecked().GetBoolean())        flags |= AppSettings::Pii_Ssn;
        if (CreditCardCheck().IsChecked().GetBoolean()) flags |= AppSettings::Pii_CreditCard;
        if (IpCheck().IsChecked().GetBoolean())         flags |= AppSettings::Pii_IpAddress;
        if (DobCheck().IsChecked().GetBoolean())        flags |= AppSettings::Pii_DateOfBirth;
        AppSettings::SetDetectionFlags(flags);

        m_requestClose(*this, true);
    }

    void SettingsContent::CancelButton_Click(
        IInspectable const& /*sender*/,
        RoutedEventArgs const& /*e*/)
    {
        LoadSettingsIntoControls();
        m_requestClose(*this, false);
    }
}