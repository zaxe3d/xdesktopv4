#include "CustomIPDialog.hpp"
#include "I18N.hpp"

#include "GUI_App.hpp"
#include "Plater.hpp"
#include "MainFrame.hpp"

namespace Slic3r { 
namespace GUI {

CustomIPDialog::CustomIPDialog()
    : DPIDialog(static_cast<wxWindow*>(wxGetApp().mainframe), wxID_ANY, _L("Custom IP Configuration"), wxDefaultPosition,
               wxSize(45 * wxGetApp().em_unit(), 40 * wxGetApp().em_unit()), 
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxMAXIMIZE_BOX)
{
    this->SetFont(wxGetApp().normal_font());
#ifdef _WIN32
    wxGetApp().UpdateDarkUI(this);
#else
    this->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
#endif

    wxBoxSizer* vsizer = new wxBoxSizer(wxVERTICAL);
    this->SetSizer(vsizer);

    std::vector<std::string> custom_ips = wxGetApp().app_config->get_custom_ips();

    listCtrl = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxSize(this->GetSize().GetWidth(), -1), wxLC_REPORT | wxLC_SINGLE_SEL);
    listCtrl->InsertColumn(0, _L("IP"));
    listCtrl->SetColumnWidth(0, this->GetSize().GetWidth());

    this->ips = wxGetApp().app_config->get_custom_ips();
    for (int i = 0; i < this->ips.size(); i++) {
        int insertionPoint = listCtrl->GetItemCount();
        listCtrl->InsertItem(insertionPoint, this->ips[i]);
    }

    wxBoxSizer* actionSizer(new wxBoxSizer(wxHORIZONTAL));
    btnAdd = new wxButton(this, wxID_ANY, _L("Add New"), wxDefaultPosition, wxDefaultSize);
    btnAdd->Enable(false);
    btnRemove = new wxButton(this, wxID_ANY, _L("Remove Selection"), wxDefaultPosition, wxDefaultSize);
    btnRemove->Enable(false); // re-enable when something is selected.
    txtIP = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize);
    listCtrl->Bind(wxEVT_COMMAND_LIST_ITEM_SELECTED, [this](const wxListEvent &evt) {
        this->btnRemove->Enable(true);
        this->selectedItemId = evt.GetItem().GetId();
    });
    listCtrl->Bind(wxEVT_COMMAND_LIST_ITEM_DESELECTED, [this](const wxListEvent &evt) {
        this->btnRemove->Enable(false);
        this->selectedItemId = -1;
    });
    btnRemove->Bind(wxEVT_BUTTON, [this](const wxCommandEvent &evt) {
        this->btnRemove->Enable(false);
        CallAfter(&CustomIPDialog::DeleteListCtrlItem, this->selectedItemId);
    });
    txtIP->Bind(wxEVT_KEY_UP, [this](wxKeyEvent &evt) {
        boost::regex regxIPAddr("^((25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9][0-9]?|0)\\.){3}(25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9][0-9]?|0)$");
        this->btnAdd->Enable(boost::regex_search(this->txtIP->GetValue().ToStdString(), regxIPAddr));
        evt.Skip(); // continue to propagate.
    });
    btnAdd->Bind(wxEVT_BUTTON, [this](wxCommandEvent &evt) {
        auto ip = this->txtIP->GetValue().ToStdString();
        if (ip == "") return;
        if (std::find(this->ips.begin(), this->ips.end(), ip) == this->ips.end()) {
            int insertionPoint = this->listCtrl->GetItemCount();
            this->listCtrl->InsertItem(insertionPoint, this->txtIP->GetValue());
            this->ips.push_back(ip);
            wxGetApp().app_config->set_custom_ips(this->ips);
            // TODO zaxe wxGetApp().sidebar().machine_manager()->addMachine(ip, 9294, "Zaxe (m.)");
        }
        this->btnAdd->Enable(false);
        this->txtIP->SetValue("");
    });
    actionSizer->Add(txtIP, 1, wxEXPAND, 0);
    actionSizer->Add(btnAdd, 1, wxEXPAND, 0);
    actionSizer->Add(btnRemove, 1, wxEXPAND, 0);
    vsizer->Add(listCtrl, 1, wxEXPAND, 0);
    vsizer->Add(actionSizer, 0, wxEXPAND);

    wxStdDialogButtonSizer* buttons = this->CreateStdDialogButtonSizer(wxCLOSE);
    wxGetApp().UpdateDarkUI(static_cast<wxButton*>(this->FindWindowById(wxID_CLOSE, this)));
    this->SetEscapeId(wxID_CLOSE);
    this->Bind(wxEVT_BUTTON, &CustomIPDialog::onCloseDialog, this, wxID_CLOSE);
    vsizer->Add(buttons, 0, wxEXPAND | wxRIGHT | wxBOTTOM, 3);
}

void CustomIPDialog::on_dpi_changed(const wxRect &suggested_rect)
{
    const int& em = em_unit();

    msw_buttons_rescale(this, em, { wxID_CLOSE});

    const wxSize& size = wxSize(45 * em, 40 * em);
    SetMinSize(size);
    Fit();

    Refresh();
}

void CustomIPDialog::DeleteListCtrlItem(long item)
{
    // workaround for wx bug https://groups.google.com/g/wx-dev/c/YfCRYNh6g7I
    for (int i = 0; i < this->listCtrl->GetItemCount(); i++)
        this->listCtrl->SetItemState(i, 0, wxLIST_STATE_SELECTED);
    this->listCtrl->DeleteItem(item);
    this->ips.erase(this->ips.begin() + item);
    wxGetApp().app_config->set_custom_ips(this->ips);
}

void CustomIPDialog::onCloseDialog(wxEvent &)
{
    this->EndModal(wxID_CLOSE);
}

} // namespace GUI
} // namespace Slic3r
