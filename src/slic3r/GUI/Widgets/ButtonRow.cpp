#include "ButtonRow.hpp"

ButtonRow::ButtonRow(wxWindow* parent, wxWindowID id, wxPoint position, wxSize size) : wxPanel(parent, id, position, size)
{
    sizer = new wxBoxSizer(wxHORIZONTAL);
    SetSizerAndFit(sizer);
    Refresh();
}

Button* ButtonRow::insertNewButton(const wxString& label, std::function<void(wxCommandEvent&)> cb)
{
    auto b        = new Button(this, label);
    auto bg_color = StateColor(std::pair<wxColour, int>(wxColour(0, 137, 123), StateColor::Pressed),
                               std::pair<wxColour, int>(wxColour(38, 166, 154), StateColor::Hovered),
                               std::pair<wxColour, int>(wxColour(0, 150, 136), StateColor::Normal));
    b->SetBackgroundColor(bg_color);
    b->SetBorderColor(wxColour(0, 150, 136));
    b->SetTextColor(wxColour("#FFFFFE"));
    b->SetSize(wxSize(FromDIP(58), FromDIP(24)));
    b->SetMinSize(wxSize(FromDIP(58), FromDIP(24)));

    b->Bind(wxEVT_BUTTON, cb);

    sizer->Add(b, 0, wxEXPAND, 0);

    Layout();
    Refresh();
    return b;
}
