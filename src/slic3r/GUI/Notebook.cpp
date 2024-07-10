#include "Notebook.hpp"

//#ifdef _WIN32

#include "GUI_App.hpp"
#include "wxExtensions.hpp"
#include "Widgets/Button.hpp"

//BBS set font size
#include "Widgets/Label.hpp"

#include <wx/button.h>
#include <wx/sizer.h>

wxDEFINE_EVENT(wxCUSTOMEVT_NOTEBOOK_SEL_CHANGED, wxCommandEvent);

ButtonsListCtrl::ButtonsListCtrl(wxWindow *parent, wxBoxSizer* side_tools) :
    wxControl(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE | wxTAB_TRAVERSAL)
{
#ifdef __WINDOWS__
    SetDoubleBuffered(true);
#endif //__WINDOWS__

    wxColour default_btn_bg;
#ifdef __APPLE__
    default_btn_bg = wxColour("#475467"); // Gradient #414B4E
#else
    default_btn_bg = wxColour("#475467"); // Gradient #414B4E
#endif

    SetBackgroundColour(default_btn_bg);

    int em = em_unit(this);// Slic3r::GUI::wxGetApp().em_unit();
    // BBS: no gap
    m_btn_margin = 0; // std::lround(0.3 * em);
    m_line_margin = std::lround(0.1 * em);

    m_sizer = new wxBoxSizer(wxVERTICAL);
    this->SetSizer(m_sizer);

    auto create_buttons_panel = [&]() {
        auto panel = new wxPanel(this);
        panel->SetBackgroundColour("#667085");
        m_buttons_sizer = new wxFlexGridSizer(1, 1, m_btn_margin, m_btn_margin);
        panel->SetSizer(m_buttons_sizer);
        panel->Layout();
        return panel;
    };

    auto buttons_panel = create_buttons_panel();
    m_sizer->Add(buttons_panel, 0, wxALIGN_CENTER_HORIZONTAL | wxBOTTOM, m_btn_margin);

    if (side_tools != NULL) {
        m_sizer->AddStretchSpacer(1);
        for (size_t idx = 0; idx < side_tools->GetItemCount(); idx++) {
            wxSizerItem* item = side_tools->GetItem(idx);
            wxWindow* item_win = item->GetWindow();
            if (item_win) {
                item_win->Reparent(this);
            }
        }
        m_sizer->Add(side_tools, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT | wxBOTTOM, m_btn_margin);
    }

    // BBS: disable custom paint
    //this->Bind(wxEVT_PAINT, &ButtonsListCtrl::OnPaint, this);
    Bind(wxEVT_SYS_COLOUR_CHANGED, [this](auto& e){
    });
}

void ButtonsListCtrl::OnPaint(wxPaintEvent&)
{
    //Slic3r::GUI::wxGetApp().UpdateDarkUI(this);
    const wxSize sz = GetSize();
    wxPaintDC dc(this);

    if (m_selection < 0 || m_selection >= (int)m_pageButtons.size())
        return;

    wxColour selected_btn_bg("#1F8EEA");
    wxColour default_btn_bg("#3B4446"); // Gradient #414B4E
    const wxColour& btn_marker_color = Slic3r::GUI::wxGetApp().get_color_hovered_btn_label();

    // highlight selected notebook button

    for (int idx = 0; idx < int(m_pageButtons.size()); idx++) {
        Button* btn = m_pageButtons[idx];

        btn->SetBackgroundColor(idx == m_selection ? selected_btn_bg : default_btn_bg);

        wxPoint pos = btn->GetPosition();
        wxSize size = btn->GetSize();
        const wxColour& clr = idx == m_selection ? btn_marker_color : default_btn_bg;
        dc.SetPen(clr);
        dc.SetBrush(clr);
        dc.DrawRectangle(pos.x, pos.y + size.y, size.x, sz.y - size.y);
    }

#if 0
    // highlight selected mode button
    if (m_mode_sizer) {
        const std::vector<ModeButton*>& mode_btns = m_mode_sizer->get_btns();
        for (int idx = 0; idx < int(mode_btns.size()); idx++) {
            ModeButton* btn = mode_btns[idx];
            btn->SetBackgroundColor(btn->is_selected() ? selected_btn_bg : default_btn_bg);

            //wxPoint pos = btn->GetPosition();
            //wxSize size = btn->GetSize();
            //const wxColour& clr = btn->is_selected() ? btn_marker_color : default_btn_bg;
            //dc.SetPen(clr);
            //dc.SetBrush(clr);
            //dc.DrawRectangle(pos.x, pos.y + size.y, size.x, sz.y - size.y);
        }
    }
#endif

    // Draw orange bottom line

    dc.SetPen(btn_marker_color);
    dc.SetBrush(btn_marker_color);
    dc.DrawRectangle(1, sz.y - m_line_margin, sz.x, m_line_margin);
}

void ButtonsListCtrl::UpdateMode()
{
    //m_mode_sizer->SetMode(Slic3r::GUI::wxGetApp().get_mode());
}

void ButtonsListCtrl::Rescale()
{
    //m_mode_sizer->msw_rescale();
    int em = em_unit(this);
    for (Button* btn : m_pageButtons) {
        //BBS
        btn->SetMinSize({FromDIP(70), FromDIP(70)});
        btn->Rescale();
    }

    // BBS: no gap
    //m_btn_margin = std::lround(0.3 * em);
    //m_line_margin = std::lround(0.1 * em);
    //m_buttons_sizer->SetVGap(m_btn_margin);
    //m_buttons_sizer->SetHGap(m_btn_margin);

    m_sizer->Layout();
}

void ButtonsListCtrl::SetSelection(int sel)
{
    if (m_selection == sel)
        return;
    // BBS: change button color
    // wxColour selected_btn_bg("#009688");    // Gradient #009688
    if (m_selection >= 0) {
        StateColor bg_color = StateColor(
        std::pair{wxColour(107, 107, 107), (int) StateColor::Hovered},
        std::pair<wxColour, int>{"#475467", StateColor::Normal});
        m_pageButtons[m_selection]->SetBackgroundColor(bg_color);
        StateColor text_color = StateColor(
        std::pair{wxColour(254,254, 254), (int) StateColor::Normal}
        );
        m_pageButtons[m_selection]->SetSelected(false);
        m_pageButtons[m_selection]->SetTextColor(text_color);
    }
    m_selection = sel;

    StateColor bg_color = StateColor(
        std::pair<wxColour, int>{"#009ADE", StateColor::Hovered},
        std::pair<wxColour, int>{"#009ADE", StateColor::Normal});
    m_pageButtons[m_selection]->SetBackgroundColor(bg_color);

    StateColor text_color = StateColor(
        std::pair{wxColour(254, 254, 254), (int) StateColor::Normal}
        );
    m_pageButtons[m_selection]->SetSelected(true);
    m_pageButtons[m_selection]->SetTextColor(text_color);
    
    Refresh();
}

bool ButtonsListCtrl::InsertPage(size_t n, const wxString &text, bool bSelect /* = false*/, const std::string &bmp_name /* = ""*/, const std::string &inactive_bmp_name)
{
    Button* btn = new Button(m_buttons_sizer->GetContainingWindow(), text.empty() ? text : " " + text, bmp_name, wxNO_BORDER | wxVERTICAL, FromDIP(36));
    btn->SetCornerRadius(0);

    // int em = em_unit(this);
    //BBS set size for button
    btn->SetMinSize({FromDIP(70), FromDIP(70)});

    StateColor bg_color = StateColor(
        std::pair{wxColour(107, 107, 107), (int) StateColor::Hovered},
        std::pair<wxColour, int>{"#475467", StateColor::Normal});

    btn->SetBackgroundColor(bg_color);
    StateColor text_color = StateColor(
        std::pair{wxColour(254,254, 254), (int) StateColor::Normal});
    btn->SetTextColor(text_color);
    btn->SetInactiveIcon(inactive_bmp_name);
    btn->SetSelected(false);
    btn->Bind(wxEVT_BUTTON, [this, btn](wxCommandEvent& event) {
        if (auto it = std::find(m_pageButtons.begin(), m_pageButtons.end(), btn); it != m_pageButtons.end()) {
            auto sel = it - m_pageButtons.begin();
            //do it later
            //SetSelection(sel);
            
            wxCommandEvent evt = wxCommandEvent(wxCUSTOMEVT_NOTEBOOK_SEL_CHANGED);
            evt.SetId(sel);
            wxPostEvent(this->GetParent(), evt);
        }
    });
    Slic3r::GUI::wxGetApp().UpdateDarkUI(btn);
    m_pageButtons.insert(m_pageButtons.begin() + n, btn);
    m_buttons_sizer->Insert(n, btn, wxSizerFlags().Expand().Border(wxBOTTOM, 1));
    m_buttons_sizer->SetRows(m_buttons_sizer->GetRows() + 1);
    m_sizer->Layout();
    return true;
}

void ButtonsListCtrl::RemovePage(size_t n)
{
    Button* btn = m_pageButtons[n];
    m_pageButtons.erase(m_pageButtons.begin() + n);
    m_buttons_sizer->Remove(n);
#if __WXOSX__
    RemoveChild(btn);
#else
    btn->Reparent(nullptr);
#endif
    btn->Destroy();
    m_sizer->Layout();
}

bool ButtonsListCtrl::SetPageImage(size_t n, const std::string& bmp_name) const
{
    if (n >= m_pageButtons.size())
        return false;
     
    // BBS
    //return m_pageButtons[n]->SetBitmap_(bmp_name);
    ScalableBitmap bitmap(NULL, bmp_name);
    //m_pageButtons[n]->SetBitmap_(bitmap);
    return true;
}

void ButtonsListCtrl::SetPageText(size_t n, const wxString& strText)
{
    Button* btn = m_pageButtons[n];
    btn->SetLabel(strText);
}

wxString ButtonsListCtrl::GetPageText(size_t n) const
{
    Button* btn = m_pageButtons[n];
    return btn->GetLabel();
}

//#endif // _WIN32

void Notebook::Init()
{
    // We don't need any border as we don't have anything to separate the
    // page contents from.
    SetInternalBorder(0);

    // No effects by default.
    m_showEffect = m_hideEffect = wxSHOW_EFFECT_NONE;

    m_showTimeout = m_hideTimeout = 0;

    /* On Linux, Gstreamer wxMediaCtrl does not seem to get along well with
     * 32-bit X11 visuals (the overlay does not work).  Is this a wxWindows
     * bug?  Is this a Gstreamer bug?  No idea, but it is our problem ... 
     * and anyway, this transparency thing just isn't all that interesting,
     * so we just don't do it on Linux. 
     */
#ifndef __WXGTK__
    SetBackgroundStyle(wxBG_STYLE_TRANSPARENT);
#endif
}
