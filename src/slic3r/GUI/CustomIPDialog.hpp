#ifndef slic3r_GUI_CustomIPDialog_hpp_
#define slic3r_GUI_CustomIPDialog_hpp_

#include "GUI.hpp"
#include "GUI_Utils.hpp"

#include <wx/wx.h>
#include <wx/intl.h>
#include <wx/listctrl.h>   // listctrl

#include <boost/regex.hpp> // regex
// TODO zaxe
namespace Slic3r {
namespace GUI {

class CustomIPDialog : public DPIDialog
{
public:
    CustomIPDialog();

protected:
    void on_dpi_changed(const wxRect &suggested_rect) override;
private:
    wxListCtrl *listCtrl;
    wxButton *btnRemove;
    wxButton *btnAdd;
    wxTextCtrl *txtIP;
    long selectedItemId = -1;
    std::vector<std::string> ips;
    void DeleteListCtrlItem(long itemId);
    void onCloseDialog(wxEvent &);
};

} // namespace GUI
} // namespace Slic3r

#endif /* slic3r_GUI_CustomIPDialog_hpp_ */
