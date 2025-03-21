#pragma once

#include <wx/listctrl.h>

#include "GUI_Utils.hpp"
#include "Widgets/Button.hpp"
#include "Widgets/Label.hpp"
#include "Widgets/TextInput.hpp"

namespace Slic3r { namespace GUI {

class CustomIPDialog : public DPIDialog
{
public:
    CustomIPDialog();

protected:
    void on_dpi_changed(const wxRect& suggested_rect) override;

private:
    wxListCtrl* lc_ip_list;
    TextInput*  txt_ip;
    Button*     btn_add;
    Button*     btn_remove;

    size_t                   selected_idx = -1;
    std::vector<std::string> ip_list;

    void delete_item(size_t itemId);
    void on_close_dialog(wxEvent&);
};

}} // namespace Slic3r::GUI
