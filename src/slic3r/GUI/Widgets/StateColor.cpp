#include "StateColor.hpp"

static bool gDarkMode = false;

static bool operator<(wxColour const &l, wxColour const &r) { return l.GetRGBA() < r.GetRGBA(); }

static std::map<wxColour, wxColour> gDarkColors{
    {"#009688", "#00675b"},
    {"#1F8EEA", "#2778D2"},
    {"#FF6F00", "#D15B00"},
    {"#D01B1B", "#BB2A3A"},
    {"#262E30", "#EFEFF0"},
    {"#2C2C2E", "#B3B3B4"},
    {"#6B6B6B", "#818183"},
    {"#ACACAC", "#54545A"},
    {"#EEEEEE", "#4C4C55"},
    {"#E8E8E8", "#3E3E45"},
    {"#323A3D", "#E5E5E4"},
    {"#FFFFFF", "#2D2D31"},
    {"#F8F8F8", "#36363C"},
    {"#F1F1F1", "#36363B"},
    {"#3B4446", "#2D2D30"},
    {"#CECECE", "#54545B"},
    {"#DBFDD5", "#3B3B40"},
    {"#000000", "#FFFFFE"},
    {"#F4F4F4", "#36363D"},
    {"#DBDBDB", "#4A4A51"},
    {"#EDFAF2", "#283232"},
    {"#323A3C", "#E5E5E6"},
    {"#6B6B6A", "#B3B3B5"},
    {"#303A3C", "#E5E5E5"},
    {"#FEFFFF", "#242428"},
    {"#A6A9AA", "#2D2D29"},
    {"#363636", "#B2B3B5"},
    {"#F0F0F1", "#404040"},
    {"#9E9E9E", "#53545A"},
    {"#D7E8DE", "#1F2B27"},
    {"#2B3436", "#808080"},
    {"#ABABAB", "#ABABAB"},
    {"#D9D9D9", "#2D2D32"},
    //{"#F0F0F0", "#4C4C54"},
    {"#FCFCFD", "#231F21"}, // gray25, gray900D
    {"#231F20", "#FCFCFC"}, // gray900, gray25D
    {"#F9FAFB", "#1D2938"}, // gray50, gray800D
    {"#1D2939", "#F9FAFA"}, // gray800, gray50D
    {"#F2F4F7", "#344053"}, // gray100, gray700D
    {"#344054", "#F2F4F8"}, // gray700, gray100D
    {"#EAECF0", "#475468"}, // gray200, gray600D
    //{"#475467", "#EAECF1"}, // gray600, gray200D
    {"#D0D5DD", "#667086"}, // gray300, gray500D
    {"#667085", "#D0D5D8"}, // gray500, gray300D
    {"#98A2B3", "#98A2B4"}, // gray400, gray400D
    {"#F5FBFF", "#0B4A6A"}, // blue25, blue900D
    {"#0B4A6F", "#F5FBFE"}, // blue900, blue25D
    {"#F0F9FF", "#065985"}, // blue50, blue800D
    {"#065986", "#F0F9FE"}, // blue800, blue50D
    {"#E0F2FE", "#026AA3"}, // blue100, blue700D
    {"#026AA2", "#E0F2FC"}, // blue700, blue100D
    {"#B9E6FE", "#0086C8"}, // blue200, blue600D
    {"#0086C9", "#B9E6FC"}, // blue600, blue200D
    {"#7CD4FD", "#009ADC"}, // blue300, blue500D
    {"#009ADE", "#7CD4FC"}, // blue500, blue300D
    {"#36BFFA", "#36BFFB"}  // blue400, blue400D

};

std::map<wxColour, wxColour> const & StateColor::GetDarkMap() 
{
    return gDarkColors;
}

void StateColor::SetDarkMode(bool dark) { gDarkMode = dark; }

inline wxColour darkModeColorFor2(wxColour const &color)
{
    if (!gDarkMode)
        return color;
    auto iter = gDarkColors.find(color);
    wxASSERT(iter != gDarkColors.end());
    if (iter != gDarkColors.end()) return iter->second;
    return color;
}

std::map<wxColour, wxColour> revert(std::map<wxColour, wxColour> const & map)
{
    std::map<wxColour, wxColour> map2;
    for (auto &p : map) map2.emplace(p.second, p.first);
    return map2;
}

wxColour StateColor::lightModeColorFor(wxColour const &color)
{
    static std::map<wxColour, wxColour> gLightColors = revert(gDarkColors);
    auto iter = gLightColors.find(color);
    wxASSERT(iter != gLightColors.end());
    if (iter != gLightColors.end()) return iter->second;
    return color;
}

wxColour StateColor::darkModeColorFor(wxColour const &color) { return darkModeColorFor2(color); }

StateColor::StateColor(wxColour const &color) { append(color, 0); }

StateColor::StateColor(wxString const &color) { append(color, 0); }

StateColor::StateColor(unsigned long color) { append(color, 0); }

void StateColor::append(wxColour const & color, int states)
{
    statesList_.push_back(states);
    colors_.push_back(color);
}

void StateColor::append(wxString const & color, int states)
{
    wxColour c1(color);
    append(c1, states);
}

void StateColor::append(unsigned long color, int states)
{
    if ((color & 0xff000000) == 0)
        color |= 0xff000000;
    wxColour cl; cl.SetRGBA(color & 0xff00ff00 | ((color & 0xff) << 16) | ((color >> 16) & 0xff));
    append(cl, states);
}

void StateColor::clear()
{
    statesList_.clear();
    colors_.clear();
}

int StateColor::states() const
{
    int states = 0;
    for (auto s : statesList_) states |= s;
    states = (states & 0xffff) | (states >> 16);
    if (takeFocusedAsHovered_ && (states & Hovered))
        states |= Focused;
    return states;
}

wxColour StateColor::defaultColor() {
    return colorForStates(0);
}

wxColour StateColor::colorForStates(int states)
{
    bool focused = takeFocusedAsHovered_ && (states & Focused);
    for (int i = 0; i < statesList_.size(); ++i) {
        int s = statesList_[i];
        int on = s & 0xffff;
        int off = s >> 16;
        if ((on & states) == on && (off & ~states) == off) {
            return darkModeColorFor2(colors_[i]);
        }
        if (focused && (on & Hovered)) {
            on |= Focused;
            on &= ~Hovered;
            if ((on & states) == on && (off & ~states) == off) {
                return darkModeColorFor2(colors_[i]);
            }
        }
    }
    return wxColour(0, 0, 0, 0);
}

wxColour StateColor::colorForStatesNoDark(int states)
{
    bool focused = takeFocusedAsHovered_ && (states & Focused);
    for (int i = 0; i < statesList_.size(); ++i) {
        int s = statesList_[i];
        int on = s & 0xffff;
        int off = s >> 16;
        if ((on & states) == on && (off & ~states) == off) {
            return colors_[i];
        }
        if (focused && (on & Hovered)) {
            on |= Focused;
            on &= ~Hovered;
            if ((on & states) == on && (off & ~states) == off) {
                return colors_[i];
            }
        }
    }
    return wxColour(0, 0, 0, 0);
}

int StateColor::colorIndexForStates(int states)
{
    for (int i = 0; i < statesList_.size(); ++i) {
        int s   = statesList_[i];
        int on  = s & 0xffff;
        int off = s >> 16;
        if ((on & states) == on && (off & ~states) == off) { return i; }
    }
    return -1;
}

bool StateColor::setColorForStates(wxColour const &color, int states)
{
    for (int i = 0; i < statesList_.size(); ++i) {
        if (statesList_[i] == states) {
            colors_[i] = color;
            return true;
        }
    }
    return false;
}

void StateColor::setTakeFocusedAsHovered(bool set) { takeFocusedAsHovered_ = set; }
