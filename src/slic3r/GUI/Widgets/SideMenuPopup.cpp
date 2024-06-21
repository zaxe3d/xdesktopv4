#include "SideMenuPopup.hpp"
#include "Label.hpp"

#include <wx/display.h>
#include <wx/dcgraph.h>
#include "../GUI_App.hpp"



wxBEGIN_EVENT_TABLE(SidePopup,PopupWindow)
EVT_PAINT(SidePopup::paintEvent)
wxEND_EVENT_TABLE()

SidePopup::SidePopup(wxWindow* parent, bool style1)
    :PopupWindow(parent,
    wxBORDER_NONE |
    wxPU_CONTAINS_CONTROLS),
    style1(style1)
{
#ifdef __WINDOWS__
    SetDoubleBuffered(true);
#endif //__WINDOWS__
}

SidePopup::~SidePopup()
{
    ;
}

void SidePopup::OnDismiss()
{
    Slic3r::GUI::wxGetApp().set_side_menu_popup_status(false);
    PopupWindow::OnDismiss();
}

bool SidePopup::ProcessLeftDown(wxMouseEvent& event)
{
    return PopupWindow::ProcessLeftDown(event);
}
bool SidePopup::Show( bool show )
{
    return PopupWindow::Show(show);
}

void SidePopup::Popup(wxWindow* focus)
{
    if (focus) {
        Create(focus);
        wxPoint pos = focus->ClientToScreen(wxPoint(0, 0));
        int shift = focus->GetSize().y * btn_list.size() + FromDIP(10);
        Position(pos, {0, -1 * shift});
    }
    Slic3r::GUI::wxGetApp().set_side_menu_popup_status(true);
    PopupWindow::Popup();
}

void SidePopup::Create(wxWindow* focus)
{
    int width  = focus->GetSize().x;
    int height = btn_list.size() * focus->GetSize().y;
    SetSize(wxSize(width, height));

    wxString   blue400{"#36BFFA"};
    wxString   blue500{"#009ADE"};
    StateColor btn_bg(std::pair<wxColour, int>(style1 ? blue400 : *wxWHITE, StateColor::Hovered),
                      std::pair<wxColour, int>(style1 ? blue500 : *wxWHITE, StateColor::Normal));
    btn_bg.setTakeFocusedAsHovered(false);

    StateColor text_fg(std::pair<wxColour, int>(style1 ? *wxWHITE : blue400, StateColor::Hovered),
                       std::pair<wxColour, int>(style1 ? *wxWHITE : blue500, StateColor::Normal));
    text_fg.setTakeFocusedAsHovered(false);

    wxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    for (auto btn : btn_list) {
        btn->SetCornerRadius(FromDIP(12));
        btn->SetBackgroundColor(btn_bg);
        // btn->SetBackgroundColour(blue500);
        btn->SetTextColor(text_fg);
        if (!style1) {
            btn->SetBorderWidth(2);
            btn->SetBorderColor(blue500);
        }

        sizer->Add(btn, 1, wxEXPAND, 0);
    }

    SetSizer(sizer);

    Layout();
    Refresh();
}

void SidePopup::paintEvent(wxPaintEvent& evt)
{
    wxPaintDC dc(this);
    wxSize size = GetSize();
    dc.SetBrush(wxColor("#229A1E"));
    dc.DrawRectangle(0, 0, size.x, size.y);
}

void SidePopup::append_button(Button* btn)
{
    btn_list.push_back(btn);
}
