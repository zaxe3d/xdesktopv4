#include "ZaxeRegisterPrinterDialog.hpp"

#include "GUI_App.hpp"
#include "Widgets/Label.hpp"
#include "slic3r/GUI/I18N.hpp"
#include "slic3r/GUI/format.hpp"

namespace Slic3r::GUI {
RegisterPrinterDialog::RegisterPrinterDialog(
    wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style)
    : DPIDialog(parent, id, _L("Register Printer To Me"), pos, size, style)
{
    SetBackgroundColour(*wxWHITE);
    this->SetSizeHints(wxDefaultSize, wxDefaultSize);

    auto hint = GUI::format_wxstr(_L("If you register this printer (%1%) to your account, you will be able to access it online."), title);
    hint_text = new wxStaticText(this, wxID_ANY, hint, wxDefaultPosition, wxDefaultSize, 0);
    hint_text->SetFont(Label::Body_15);
    hint_text->SetForegroundColour(wxColour(50, 58, 61));
    hint_text->Wrap(-1);

    wxBoxSizer* sizer_top;
    sizer_top = new wxBoxSizer(wxVERTICAL);
    sizer_top->Add(hint_text, 0, wxRIGHT | wxLEFT, FromDIP(10));

    confirm_btn = new Button(this, _L("Confirm"));
    confirm_btn->SetFont(Label::Body_14);
    confirm_btn->SetMinSize(wxSize(-1, FromDIP(48)));
    confirm_btn->SetCornerRadius(FromDIP(16));
    confirm_btn->SetTextColor(wxColour("#FFFFFE"));

    StateColor btn_bg(std::pair<wxColour, int>(wxColour("#36BFFA"), StateColor::Hovered),
                      std::pair<wxColour, int>(wxColour("#009ADE"), StateColor::Normal));
    StateColor btn_bd(std::pair<wxColour, int>(wxColour("#009ADE"), StateColor::Normal));
    StateColor btn_text(std::pair<wxColour, int>(wxColour(255, 255, 255), StateColor::Normal));

    confirm_btn->SetBackgroundColor(btn_bg);
    confirm_btn->SetBorderColor(btn_bd);
    confirm_btn->SetTextColor(btn_text);

    wxBoxSizer* sizer_bottom;
    sizer_bottom = new wxBoxSizer(wxHORIZONTAL);
    sizer_bottom->AddStretchSpacer(7);
    sizer_bottom->Add(confirm_btn, 2, wxALL | wxALIGN_RIGHT, 0);
    sizer_bottom->AddStretchSpacer(1);

    wxBoxSizer* main_sizer;
    main_sizer = new wxBoxSizer(wxVERTICAL);
    main_sizer->Add(0, FromDIP(30));
    main_sizer->Add(sizer_top, 0, wxEXPAND);
    main_sizer->Add(0, FromDIP(10));
    main_sizer->Add(sizer_bottom, 0, wxEXPAND);
    main_sizer->Add(0, FromDIP(10));

    this->SetSizer(main_sizer);
    this->Layout();
    this->Fit();
    CentreOnParent();

    confirm_btn->Bind(wxEVT_BUTTON, &RegisterPrinterDialog::on_button_confirm, this);
    wxGetApp().UpdateDlgDarkUI(this);
}

RegisterPrinterDialog::~RegisterPrinterDialog() {}

void RegisterPrinterDialog::end_modal(wxStandardID id) { EndModal(id); }

void RegisterPrinterDialog::on_button_confirm(wxCommandEvent& event) { EndModal(wxID_OK); }

void RegisterPrinterDialog::on_dpi_changed(const wxRect& suggested_rect)
{
    confirm_btn->SetCornerRadius(FromDIP(12));
    confirm_btn->Rescale();

    Layout();
    this->Refresh();
}
} // namespace Slic3r::GUI