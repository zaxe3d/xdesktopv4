#include "CustomIPDialog.hpp"
#include "I18N.hpp"

#include "GUI_App.hpp"
#include "Plater.hpp"
#include "MainFrame.hpp"

#include <boost/regex.hpp>

namespace Slic3r { namespace GUI {

CustomIPDialog::CustomIPDialog()
    : DPIDialog(static_cast<wxWindow*>(wxGetApp().mainframe),
                wxID_ANY,
                _L("Custom IP Configuration"),
                wxDefaultPosition,
                wxSize(45 * wxGetApp().em_unit(), 40 * wxGetApp().em_unit()),
                wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxMAXIMIZE_BOX)
{
    this->SetFont(wxGetApp().normal_font());
#ifdef _WIN32
    wxGetApp().UpdateDarkUI(this);
#else
    this->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
#endif

    std::vector<std::string> custom_ips = wxGetApp().app_config->get_custom_ips();

    lc_ip_list = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxSize(this->GetSize().GetWidth(), -1), wxLC_REPORT | wxLC_SINGLE_SEL);
    lc_ip_list->InsertColumn(0, _L("IP"));
    lc_ip_list->SetColumnWidth(0, this->GetSize().GetWidth());
    wxGetApp().UpdateDarkUI(lc_ip_list);

    ip_list = wxGetApp().app_config->get_custom_ips();
    for (int i = 0; i < ip_list.size(); i++) {
        int insertion_point = lc_ip_list->GetItemCount();
        lc_ip_list->InsertItem(insertion_point, ip_list[i]);
    }

    auto create_btn = [&](const wxString& label, auto callback) {
        const wxString gray300{"#D0D5DD"};
        const wxString blue500{"#009ADE"};
        auto           btn = new Button(this, label, "");
        btn->SetPaddingSize(wxSize(8, 5));
        btn->SetBackgroundColor(*wxWHITE);
        auto color = StateColor(std::pair<wxColour, int>(gray300, StateColor::Disabled),
                                std::pair<wxColour, int>(blue500, StateColor::Normal));
        btn->SetBorderColor(color);
        btn->SetTextColor(color);
        btn->Enable(false);
        wxGetApp().UpdateDarkUI(btn);
        btn->Bind(wxEVT_BUTTON, callback);
        return btn;
    };

    btn_add = create_btn(_L("Add New"), [this](wxCommandEvent& evt) {
        auto ip = this->txt_ip->GetTextCtrl()->GetValue().ToStdString();
        if (ip == "")
            return;
        if (std::find(ip_list.begin(), ip_list.end(), ip) == ip_list.end()) {
            int insertionPoint = this->lc_ip_list->GetItemCount();
            this->lc_ip_list->InsertItem(insertionPoint, this->txt_ip->GetTextCtrl()->GetValue());
            ip_list.push_back(ip);
            wxGetApp().app_config->set_custom_ips(ip_list);
            wxGetApp().sidebar().machine_manager()->addMachine(ip, 9294, "Zaxe (m.)");
        }
        this->btn_add->Enable(false);
        this->txt_ip->GetTextCtrl()->SetValue("");
    });

    btn_remove = create_btn(_L("Remove Selection"), [this](const wxCommandEvent& evt) {
        this->btn_remove->Enable(false);
        CallAfter(&CustomIPDialog::delete_item, this->selected_idx);
    });

    txt_ip = new TextInput(this, "", "", "zaxe_search", wxDefaultPosition, wxDefaultSize, wxBORDER_NONE, FromDIP(24));
    txt_ip->SetCornerRadius(FromDIP(4));
    auto gray100 = wxColor("#F2F4F7");
    txt_ip->SetBackgroundColor(std::make_pair(gray100, (int) StateColor::Normal));
    txt_ip->GetTextCtrl()->SetBackgroundColour(gray100);
    txt_ip->GetTextCtrl()->SetMaxLength(25);
    txt_ip->GetTextCtrl()->SetHint(_L("Enter Custom IP"));
    wxGetApp().UpdateDarkUI(txt_ip);
    wxGetApp().UpdateDarkUI(txt_ip->GetTextCtrl());

    lc_ip_list->Bind(wxEVT_COMMAND_LIST_ITEM_SELECTED, [this](const wxListEvent& evt) {
        this->btn_remove->Enable(true);
        this->selected_idx = evt.GetItem().GetId();
    });

    lc_ip_list->Bind(wxEVT_COMMAND_LIST_ITEM_DESELECTED, [this](const wxListEvent& evt) {
        this->btn_remove->Enable(false);
        this->selected_idx = -1;
    });

    txt_ip->GetTextCtrl()->Bind(wxEVT_KEY_UP, [this](wxKeyEvent& evt) {
        boost::regex regxIPAddr("^((25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9][0-9]?|0)\\.){3}(25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9][0-9]?|0)$");
        this->btn_add->Enable(boost::regex_search(this->txt_ip->GetTextCtrl()->GetValue().ToStdString(), regxIPAddr));
        evt.Skip(); // continue to propagate.
    });

    auto text_input_sizer = new wxBoxSizer(wxHORIZONTAL);
    text_input_sizer->Add(txt_ip, 1, wxEXPAND | wxRIGHT | wxLEFT, 20);

    auto action_sizer = new wxBoxSizer(wxHORIZONTAL);
    action_sizer->Add(btn_add, 1, wxEXPAND | wxLEFT, 15);
    action_sizer->AddSpacer(FromDIP(5));
    action_sizer->Add(btn_remove, 1, wxEXPAND | wxRIGHT, 15);

    wxStdDialogButtonSizer* buttons = this->CreateStdDialogButtonSizer(wxCLOSE);
    wxGetApp().UpdateDarkUI(static_cast<Button*>(this->FindWindowById(wxID_CLOSE, this)));
    this->SetEscapeId(wxID_CLOSE);
    this->Bind(wxEVT_BUTTON, &CustomIPDialog::on_close_dialog, this, wxID_CLOSE);

    wxBoxSizer* vsizer = new wxBoxSizer(wxVERTICAL);
    vsizer->Add(lc_ip_list, 1, wxEXPAND, 0);
    vsizer->AddSpacer(FromDIP(5));
    vsizer->Add(text_input_sizer, 0, wxEXPAND);
    vsizer->AddSpacer(FromDIP(5));
    vsizer->Add(action_sizer, 0, wxEXPAND);
    vsizer->AddSpacer(FromDIP(5));
    vsizer->Add(buttons, 0, wxEXPAND | wxRIGHT | wxBOTTOM, 3);

    this->SetSizer(vsizer);
}

void CustomIPDialog::on_dpi_changed(const wxRect& suggested_rect)
{
    const int& em = em_unit();

    msw_buttons_rescale(this, em, {wxID_CLOSE});

    const wxSize& size = wxSize(45 * em, 40 * em);
    SetMinSize(size);
    Fit();

    Refresh();
}

void CustomIPDialog::delete_item(size_t item)
{
    // workaround for wx bug https://groups.google.com/g/wx-dev/c/YfCRYNh6g7I
    for (int i = 0; i < this->lc_ip_list->GetItemCount(); i++)
        this->lc_ip_list->SetItemState(i, 0, wxLIST_STATE_SELECTED);
    this->lc_ip_list->DeleteItem(item);
    ip_list.erase(ip_list.begin() + item);
    wxGetApp().app_config->set_custom_ips(ip_list);
}

void CustomIPDialog::on_close_dialog(wxEvent&) { this->EndModal(wxID_CLOSE); }

}} // namespace Slic3r::GUI
