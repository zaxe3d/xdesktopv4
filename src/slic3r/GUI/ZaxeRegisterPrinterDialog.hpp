#pragma once

#include "GUI.hpp"
#include "GUI_Utils.hpp"
#include <wx/string.h>
#include <wx/stattext.h>
#include <wx/font.h>
#include <wx/colour.h>
#include <wx/settings.h>
#include <wx/sizer.h>
#include "Widgets/Button.hpp"

namespace Slic3r::GUI {
class RegisterPrinterDialog : public DPIDialog
{
public:
    RegisterPrinterDialog(wxWindow*       parent,
                          wxWindowID      id    = wxID_ANY,
                          const wxString& title = wxEmptyString,
                          const wxPoint&  pos   = wxDefaultPosition,
                          const wxSize&   size  = wxDefaultSize,
                          long            style = wxCLOSE_BOX | wxCAPTION);

    ~RegisterPrinterDialog();

    void end_modal(wxStandardID id);
    void on_button_confirm(wxCommandEvent& event);
    void on_dpi_changed(const wxRect& suggested_rect) override;

private:
    wxStaticText* hint_text;
    Button*       confirm_btn;
};
} // namespace Slic3r::GUI
