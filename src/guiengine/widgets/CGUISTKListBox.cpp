// Copyright (C) 2002-2015 Nikolaus Gebhardt
//               2013 Glenn De Jonghe
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "guiengine/widgets/CGUISTKListBox.hpp"

#include "IGUISkin.h"
#include "IGUIEnvironment.h"
#include "IVideoDriver.h"
#include "IGUIFont.h"
#include "IGUISpriteBank.h"
#include "IGUIScrollBar.h"
#include "utils/time.hpp"


namespace irr
{
namespace gui
{

//! constructor
CGUISTKListBox::CGUISTKListBox(IGUIEnvironment* environment, IGUIElement* parent,
            s32 id, core::rect<s32> rectangle, bool clip,
            bool drawBack, bool moveOverSelect)
: IGUIElement(EGUIET_LIST_BOX, environment, parent, id, rectangle), Selected(-1),
    ItemHeight(0),ItemHeightOverride(0),
    TotalItemHeight(0), ItemsIconWidth(0), Font(0), IconBank(0),
    ScrollBar(0), selectTime(0), Selecting(false), DrawBack(drawBack),
    MoveOverSelect(moveOverSelect), AutoScroll(true), HighlightWhenNotFocused(true)
{
    #ifdef _DEBUG
    setDebugName("CGUISTKListBox");
    #endif

    IGUISkin* skin = Environment->getSkin();
    const s32 s = skin->getSize(EGDS_SCROLLBAR_SIZE);

    ScrollBar = Environment->addScrollBar(false,
                            core::rect<s32>(RelativeRect.getWidth() - s, 0,
                                            RelativeRect.getWidth(), RelativeRect.getHeight()), this, -1);
    ScrollBar->grab();
    ScrollBar->setSubElement(true);
    ScrollBar->setTabStop(false);
    ScrollBar->setAlignment(EGUIA_LOWERRIGHT, EGUIA_LOWERRIGHT, EGUIA_UPPERLEFT, EGUIA_LOWERRIGHT);
    ScrollBar->setVisible(false);
    ScrollBar->setPos(0);

    setNotClipped(!clip);

    // this element can be tabbed to
    setTabStop(true);
    setTabOrder(-1);

    updateAbsolutePosition();
}


//! destructor
CGUISTKListBox::~CGUISTKListBox()
{
    if (ScrollBar)
        ScrollBar->drop();

    if (Font)
        Font->drop();

    if (IconBank)
        IconBank->drop();
}


//! returns amount of list items
u32 CGUISTKListBox::getItemCount() const
{
    return Items.size();
}



const wchar_t* CGUISTKListBox::getCellText(u32 row_num, u32 col_num) const
{
        if ( row_num >= Items.size() )
                return 0;
        if ( col_num >= Items[row_num].m_contents.size() )
                return 0;
    return Items[row_num].m_contents[col_num].m_text.c_str();
}

CGUISTKListBox::ListItem CGUISTKListBox::getItem(u32 id) const
{
    return Items[id];
}


//! Returns the icon of an item
s32 CGUISTKListBox::getIcon(u32 row_num, u32 col_num) const
{
    if ( row_num >= Items.size() )
            return -1;
    if ( col_num >= Items[row_num].m_contents.size() )
            return -1;
    return Items[row_num].m_contents[col_num].m_icon;
}

void CGUISTKListBox::removeItem(u32 id)
{
    if (id >= Items.size())
        return;

    if ((u32)Selected==id)
    {
        Selected = -1;
    }
    else if ((u32)Selected > id)
    {
        Selected -= 1;
        selectTime = (u32)StkTime::getTimeSinceEpoch();
    }

    Items.erase(id);

    recalculateItemHeight();
}


s32 CGUISTKListBox::getItemAt(s32 xpos, s32 ypos) const
{
    if (     xpos < AbsoluteRect.UpperLeftCorner.X || xpos >= AbsoluteRect.LowerRightCorner.X
        ||    ypos < AbsoluteRect.UpperLeftCorner.Y || ypos >= AbsoluteRect.LowerRightCorner.Y
        )
        return -1;

    if ( ItemHeight == 0 )
        return -1;

    s32 item = ((ypos - AbsoluteRect.UpperLeftCorner.Y - 1) + ScrollBar->getPos()) / ItemHeight;
    if ( item < 0 || item >= (s32)Items.size())
        return -1;

    return item;
}

//! clears the list
void CGUISTKListBox::clear()
{
    Items.clear();
    ItemsIconWidth = 0;
    Selected = -1;

    if (ScrollBar)
        ScrollBar->setPos(0);

    recalculateItemHeight();
}


void CGUISTKListBox::recalculateItemHeight()
{
    IGUISkin* skin = Environment->getSkin();

    if (Font != skin->getFont())
    {
        if (Font)
            Font->drop();

        Font = skin->getFont();
        if ( 0 == ItemHeightOverride )
            ItemHeight = 0;

        if (Font)
        {
            if ( 0 == ItemHeightOverride )
                ItemHeight = Font->getDimension(L"A").Height + 4;

            Font->grab();
        }
    }

    TotalItemHeight = ItemHeight * Items.size();
    ScrollBar->setMax( core::max_(0, TotalItemHeight - AbsoluteRect.getHeight()) );
    s32 minItemHeight = ItemHeight > 0 ? ItemHeight : 1;
    ScrollBar->setSmallStep ( minItemHeight );
    ScrollBar->setLargeStep ( 2*minItemHeight );

    if ( TotalItemHeight <= AbsoluteRect.getHeight() )
        ScrollBar->setVisible(false);
    else
        ScrollBar->setVisible(true);
}


//! returns id of selected item. returns -1 if no item is selected.
s32 CGUISTKListBox::getSelected() const
{
    return Selected;
}


//! sets the selected item. Set this to -1 if no item should be selected
void CGUISTKListBox::setSelected(s32 id)
{
    if ((u32)id>=Items.size())
        Selected = -1;
    else
        Selected = id;

    selectTime = (u32)StkTime::getTimeSinceEpoch();

    recalculateScrollPos();
}

s32 CGUISTKListBox::getRowByCellText(const wchar_t * text)
{
    s32 row_index = -1;
    s32 col_index = -1;
    if (text)
    {
        for ( row_index = 0; row_index < (s32) Items.size(); ++row_index )
        {
            for ( col_index = 0; col_index < (s32) Items[row_index].m_contents.size(); ++col_index )
            {
                if ( Items[row_index].m_contents[col_index].m_text == text ) return row_index;
            }
        }
    }
    return -1;
}

//! sets the selected item. Set this to -1 if no item should be selected
void CGUISTKListBox::setSelectedByCellText(const wchar_t * text)
{
    setSelected(getRowByCellText(text));
}

s32 CGUISTKListBox::getRowByInternalName(const std::string & text) const
{
    s32 row_index = -1;
    if (text != "")
    {
        for ( row_index = 0; row_index < (s32) Items.size(); ++row_index )
        {
            if (Items[row_index].m_internal_name == text) return row_index;
        }
    }
    return -1;
}

//! called if an event happened.
bool CGUISTKListBox::OnEvent(const SEvent& event)
{
    if (isEnabled())
    {
        switch(event.EventType)
        {
        case EET_KEY_INPUT_EVENT:
            if (event.KeyInput.PressedDown &&
                (event.KeyInput.Key == KEY_DOWN ||
                event.KeyInput.Key == KEY_UP   ||
                event.KeyInput.Key == KEY_HOME ||
                event.KeyInput.Key == KEY_END  ||
                event.KeyInput.Key == KEY_NEXT ||
                event.KeyInput.Key == KEY_PRIOR ) )
            {
                s32 oldSelected = Selected;
                switch (event.KeyInput.Key)
                {
                    case KEY_DOWN:
                        Selected += 1;
                        break;
                    case KEY_UP:
                        Selected -= 1;
                        break;
                    case KEY_HOME:
                        Selected = 0;
                        break;
                    case KEY_END:
                        Selected = (s32)Items.size()-1;
                        break;
                    case KEY_NEXT:
                        Selected += AbsoluteRect.getHeight() / ItemHeight;
                        break;
                    case KEY_PRIOR:
                        Selected -= AbsoluteRect.getHeight() / ItemHeight;
                        break;
                    default:
                        break;
                }
                if (Selected >= (s32)Items.size())
                    Selected = Items.size() - 1;
                else
                if (Selected<0)
                    Selected = 0;

                recalculateScrollPos();

                // post the news

                if (oldSelected != Selected && Parent && !Selecting && !MoveOverSelect)
                {
                    SEvent e;
                    e.EventType = EET_GUI_EVENT;
                    e.GUIEvent.Caller = this;
                    e.GUIEvent.Element = 0;
                    e.GUIEvent.EventType = EGET_LISTBOX_CHANGED;
                    Parent->OnEvent(e);
                }

                return true;
            }
            else
            if (!event.KeyInput.PressedDown && ( event.KeyInput.Key == KEY_RETURN || event.KeyInput.Key == KEY_SPACE ) )
            {
                if (Parent)
                {
                    SEvent e;
                    e.EventType = EET_GUI_EVENT;
                    e.GUIEvent.Caller = this;
                    e.GUIEvent.Element = 0;
                    e.GUIEvent.EventType = EGET_LISTBOX_SELECTED_AGAIN;
                    Parent->OnEvent(e);
                }
                return true;
            }
            break;

        case EET_GUI_EVENT:
            switch(event.GUIEvent.EventType)
            {
            case gui::EGET_SCROLL_BAR_CHANGED:
                if (event.GUIEvent.Caller == ScrollBar)
                    return true;
                break;
            case gui::EGET_ELEMENT_FOCUS_LOST:
                {
                    if (event.GUIEvent.Caller == this)
                        Selecting = false;
                    break;
                }

            default:
            break;
            }
            break;

        case EET_MOUSE_INPUT_EVENT:
            {
                core::position2d<s32> p(event.MouseInput.X, event.MouseInput.Y);

                switch(event.MouseInput.Event)
                {
                case EMIE_MOUSE_WHEEL:
                    ScrollBar->setPos(ScrollBar->getPos() + (event.MouseInput.Wheel < 0 ? -1 : 1)*-ItemHeight/2);
                    return true;

                case EMIE_LMOUSE_PRESSED_DOWN:
                {
                    Selecting = true;
                    return true;
                }

                case EMIE_LMOUSE_LEFT_UP:
                {
                    Selecting = false;

                    if (isPointInside(p))
                        selectNew(event.MouseInput.Y);

                    return true;
                }

                case EMIE_MOUSE_MOVED:
                    if (Selecting || MoveOverSelect)
                    {
                        if (isPointInside(p))
                        {
                            selectNew(event.MouseInput.Y, true);
                            return true;
                        }
                    }
                default:
                break;
                }
            }
            break;
        case EET_LOG_TEXT_EVENT:
        case EET_USER_EVENT:
        case EET_JOYSTICK_INPUT_EVENT:
        case EGUIET_FORCE_32_BIT:
            break;
        }
    }

    return IGUIElement::OnEvent(event);
}


void CGUISTKListBox::selectNew(s32 ypos, bool onlyHover)
{
    u32 now = (u32)StkTime::getTimeSinceEpoch();
    s32 oldSelected = Selected;

    Selected = getItemAt(AbsoluteRect.UpperLeftCorner.X, ypos);
    if (Selected<0 && !Items.empty())
        Selected = 0;

    recalculateScrollPos();

    gui::EGUI_EVENT_TYPE eventType = (Selected == oldSelected && now < selectTime + 500) ? EGET_LISTBOX_SELECTED_AGAIN : EGET_LISTBOX_CHANGED;
    selectTime = now;
    // post the news
    if (Parent && !onlyHover)
    {
        SEvent event;
        event.EventType = EET_GUI_EVENT;
        event.GUIEvent.Caller = this;
        event.GUIEvent.Element = 0;
        event.GUIEvent.EventType = eventType;
        Parent->OnEvent(event);
    }
}


//! Update the position and size of the listbox, and update the scrollbar
void CGUISTKListBox::updateAbsolutePosition()
{
    IGUIElement::updateAbsolutePosition();

    recalculateItemHeight();
}


//! draws the element and its children
void CGUISTKListBox::draw()
{
    if (!IsVisible)
        return;

    recalculateItemHeight(); // if the font changed

    IGUISkin* skin = Environment->getSkin();

    core::rect<s32>* clipRect = 0;

    // draw background
    core::rect<s32> frameRect(AbsoluteRect);

    // draw items

    core::rect<s32> clientClip(AbsoluteRect);
    clientClip.UpperLeftCorner.Y += 1;
    clientClip.UpperLeftCorner.X += 1;
    if (ScrollBar->isVisible())
        clientClip.LowerRightCorner.X = AbsoluteRect.LowerRightCorner.X - skin->getSize(EGDS_SCROLLBAR_SIZE);
    clientClip.LowerRightCorner.Y -= 1;
    clientClip.clipAgainst(AbsoluteClippingRect);

    skin->draw3DSunkenPane(this, skin->getColor(EGDC_3D_HIGH_LIGHT), true,
        DrawBack, frameRect, &clientClip);

    if (clipRect)
        clientClip.clipAgainst(*clipRect);

    frameRect = AbsoluteRect;
    frameRect.UpperLeftCorner.X += 1;
    if (ScrollBar->isVisible())
        frameRect.LowerRightCorner.X = AbsoluteRect.LowerRightCorner.X - skin->getSize(EGDS_SCROLLBAR_SIZE);

    frameRect.LowerRightCorner.Y = AbsoluteRect.UpperLeftCorner.Y + ItemHeight;

    frameRect.UpperLeftCorner.Y -= ScrollBar->getPos();
    frameRect.LowerRightCorner.Y -= ScrollBar->getPos();

    bool hl = (HighlightWhenNotFocused || Environment->hasFocus(this) || Environment->hasFocus(ScrollBar));

    for (s32 i=0; i<(s32)Items.size(); ++i)
    {
        if (frameRect.LowerRightCorner.Y >= AbsoluteRect.UpperLeftCorner.Y &&
            frameRect.UpperLeftCorner.Y <= AbsoluteRect.LowerRightCorner.Y)
        {
            if (i == Selected && hl)
                skin->draw2DRectangle(this, skin->getColor(EGDC_HIGH_LIGHT), frameRect, &clientClip);

            core::rect<s32> textRect = frameRect;

            if (Font)
            {
                int total_proportion = 0;
                for(unsigned int x = 0; x < Items[i].m_contents.size(); ++x)
                {
                    total_proportion += Items[i].m_contents[x].m_proportion;
                }
                int part_size = (int)(textRect.getWidth() / float(total_proportion));

                for(unsigned int x = 0; x < Items[i].m_contents.size(); ++x)
                {
                    textRect.LowerRightCorner.X = textRect.UpperLeftCorner.X +
                                                  (Items[i].m_contents[x].m_proportion * part_size);
                    textRect.UpperLeftCorner.X += 3;

                    if (IconBank && (Items[i].m_contents[x].m_icon > -1))
                    {
                        core::position2di iconPos = textRect.UpperLeftCorner;
                        iconPos.Y += textRect.getHeight() / 2;
                        iconPos.X += ItemsIconWidth/2;

                        if ( i==Selected && hl )
                        {
                            IconBank->draw2DSprite(
                                (u32)Items[i].m_contents[x].m_icon,
                                iconPos, &clientClip,
                                hasItemOverrideColor(i, EGUI_LBC_ICON_HIGHLIGHT) ?
                                getItemOverrideColor(i, EGUI_LBC_ICON_HIGHLIGHT) : getItemDefaultColor(EGUI_LBC_ICON_HIGHLIGHT),
                                selectTime, (u32)StkTime::getTimeSinceEpoch(), false, true);
                        }
                        else
                        {
                            IconBank->draw2DSprite(
                                (u32)Items[i].m_contents[x].m_icon,
                                iconPos,
                                &clientClip,
                                hasItemOverrideColor(i, EGUI_LBC_ICON) ? getItemOverrideColor(i, EGUI_LBC_ICON) : getItemDefaultColor(EGUI_LBC_ICON),
                                0 , (i==Selected) ? (u32)StkTime::getTimeSinceEpoch() : 0, false, true);
                        }
                        textRect.UpperLeftCorner.X += ItemsIconWidth;
                    }

                    textRect.UpperLeftCorner.X += 3;

                    if ( i==Selected && hl )
                    {
                        Font->draw(
                            Items[i].m_contents[x].m_text.c_str(),
                            textRect,
                            hasItemOverrideColor(i, EGUI_LBC_TEXT_HIGHLIGHT) ?
                            getItemOverrideColor(i, EGUI_LBC_TEXT_HIGHLIGHT) : getItemDefaultColor(EGUI_LBC_TEXT_HIGHLIGHT),
                            Items[i].m_contents[x].m_center, true, &clientClip);
                    }
                    else
                    {
                        Font->draw(
                            Items[i].m_contents[x].m_text.c_str(),
                            textRect,
                            hasItemOverrideColor(i, EGUI_LBC_TEXT) ? getItemOverrideColor(i, EGUI_LBC_TEXT) : getItemDefaultColor(EGUI_LBC_TEXT),
                            Items[i].m_contents[x].m_center, true, &clientClip);
                    }
                    //Position back to inital pos
                    textRect.UpperLeftCorner.X -= ItemsIconWidth+6;
                    //Calculate new beginning
                    textRect.UpperLeftCorner.X += Items[i].m_contents[x].m_proportion * part_size;
                }
            }
        }

        frameRect.UpperLeftCorner.Y += ItemHeight;
        frameRect.LowerRightCorner.Y += ItemHeight;
    }

    IGUIElement::draw();
}


//! adds an list item with an icon
u32 CGUISTKListBox::addItem(const ListItem & item)
{
    Items.push_back(item);
    recalculateItemHeight();
    recalculateIconWidth();
    return Items.size() - 1;
}


void CGUISTKListBox::setSpriteBank(IGUISpriteBank* bank)
{
    if ( bank == IconBank )
        return;
    if (IconBank)
        IconBank->drop();

    IconBank = bank;
    if (IconBank)
        IconBank->grab();
}


void CGUISTKListBox::recalculateScrollPos()
{
    if (!AutoScroll)
        return;

    const s32 selPos = (Selected == -1 ? TotalItemHeight : Selected * ItemHeight) - ScrollBar->getPos();

    if (selPos < 0)
    {
        ScrollBar->setPos(ScrollBar->getPos() + selPos);
    }
    else
    if (selPos > AbsoluteRect.getHeight() - ItemHeight)
    {
        ScrollBar->setPos(ScrollBar->getPos() + selPos - AbsoluteRect.getHeight() + ItemHeight);
    }
}


void CGUISTKListBox::setAutoScrollEnabled(bool scroll)
{
    AutoScroll = scroll;
}


bool CGUISTKListBox::isAutoScrollEnabled() const
{
    _IRR_IMPLEMENT_MANAGED_MARSHALLING_BUGFIX;
    return AutoScroll;
}

void CGUISTKListBox::recalculateIconWidth()
{
    for(int x = 0; x < (int)Items.getLast().m_contents.size(); ++x)
    {
        s32 icon = Items.getLast().m_contents[x].m_icon;
    if (IconBank && icon > -1 &&
        IconBank->getSprites().size() > (u32)icon &&
        IconBank->getSprites()[(u32)icon].Frames.size())
    {
        u32 rno = IconBank->getSprites()[(u32)icon].Frames[0].rectNumber;
        if (IconBank->getPositions().size() > rno)
        {
            const s32 w = IconBank->getPositions()[rno].getWidth();
            if (w > ItemsIconWidth)
                ItemsIconWidth = w;
        }
    }
    }
}


void CGUISTKListBox::setCell(u32 row_num, u32 col_num, const wchar_t* text, s32 icon)
{
    if ( row_num >= Items.size() )
        return;
        if ( col_num >= Items[row_num].m_contents.size() )
                return;
        Items[row_num].m_contents[col_num].m_text = text;
        Items[row_num].m_contents[col_num].m_icon = icon;

    recalculateItemHeight();
    recalculateIconWidth();
}

void CGUISTKListBox::swapItems(u32 index1, u32 index2)
{
    if ( index1 >= Items.size() || index2 >= Items.size() )
        return;

    ListItem dummmy = Items[index1];
    Items[index1] = Items[index2];
    Items[index2] = dummmy;
}


void CGUISTKListBox::setItemOverrideColor(u32 index, video::SColor color)
{
    for ( u32 c=0; c < EGUI_LBC_COUNT; ++c )
    {
        Items[index].OverrideColors[c].Use = true;
        Items[index].OverrideColors[c].Color = color;
    }
}


void CGUISTKListBox::setItemOverrideColor(u32 index, EGUI_LISTBOX_COLOR colorType, video::SColor color)
{
    if ( index >= Items.size() || colorType < 0 || colorType >= EGUI_LBC_COUNT )
        return;

    Items[index].OverrideColors[colorType].Use = true;
    Items[index].OverrideColors[colorType].Color = color;
}


void CGUISTKListBox::clearItemOverrideColor(u32 index)
{
    for (u32 c=0; c < (u32)EGUI_LBC_COUNT; ++c )
    {
        Items[index].OverrideColors[c].Use = false;
    }
}


void CGUISTKListBox::clearItemOverrideColor(u32 index, EGUI_LISTBOX_COLOR colorType)
{
    if ( index >= Items.size() || colorType < 0 || colorType >= EGUI_LBC_COUNT )
        return;

    Items[index].OverrideColors[colorType].Use = false;
}


bool CGUISTKListBox::hasItemOverrideColor(u32 index, EGUI_LISTBOX_COLOR colorType) const
{
    if ( index >= Items.size() || colorType < 0 || colorType >= EGUI_LBC_COUNT )
        return false;

    return Items[index].OverrideColors[colorType].Use;
}


video::SColor CGUISTKListBox::getItemOverrideColor(u32 index, EGUI_LISTBOX_COLOR colorType) const
{
    if ( (u32)index >= Items.size() || colorType < 0 || colorType >= EGUI_LBC_COUNT )
        return video::SColor();

    return Items[index].OverrideColors[colorType].Color;
}


video::SColor CGUISTKListBox::getItemDefaultColor(EGUI_LISTBOX_COLOR colorType) const
{
    IGUISkin* skin = Environment->getSkin();
    if ( !skin )
        return video::SColor();

    switch ( colorType )
    {
        case EGUI_LBC_TEXT:
            return skin->getColor(EGDC_BUTTON_TEXT);
        case EGUI_LBC_TEXT_HIGHLIGHT:
            return skin->getColor(EGDC_HIGH_LIGHT_TEXT);
        case EGUI_LBC_ICON:
            return skin->getColor(EGDC_ICON);
        case EGUI_LBC_ICON_HIGHLIGHT:
            return skin->getColor(EGDC_ICON_HIGH_LIGHT);
        default:
            return video::SColor();
    }
}

//! set global itemHeight
void CGUISTKListBox::setItemHeight( s32 height )
{
    ItemHeight = height;
    ItemHeightOverride = 1;
}


//! Sets whether to draw the background
void CGUISTKListBox::setDrawBackground(bool draw)
{
    DrawBack = draw;
}


} // end namespace gui
} // end namespace irr


