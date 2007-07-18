// Copyright (C) 2002-2007 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "CGUIListBox.h"
#include "IGUISkin.h"
#include "IGUIEnvironment.h"
#include "IVideoDriver.h"
#include "IGUIFont.h"
#include "IGUISpriteBank.h"
#include "CGUIScrollBar.h"
#include "os.h"

namespace irr
{
namespace gui
{

//! constructor
CGUIListBox::CGUIListBox(IGUIEnvironment* environment, IGUIElement* parent,
						s32 id, core::rect<s32> rectangle, bool clip,
						bool drawBack, bool moveOverSelect)
: IGUIListBox(environment, parent, id, rectangle), Selected(-1), ItemHeight(0),
	TotalItemHeight(0), ItemsIconWidth(0), Font(0), IconBank(0),
	ScrollBar(0), Selecting(false), DrawBack(drawBack),
	MoveOverSelect(moveOverSelect), selectTime(0), AutoScroll(true), 
	KeyBuffer(), LastKeyTime(0), HighlightWhenNotFocused(true)
{
	#ifdef _DEBUG
	setDebugName("CGUIListBox");
	#endif

	IGUISkin* skin = Environment->getSkin();
	s32 s = skin->getSize(EGDS_SCROLLBAR_SIZE);

	ScrollBar = new CGUIScrollBar(false, Environment, this, 0,
		core::rect<s32>(RelativeRect.getWidth() - s, 0, RelativeRect.getWidth(), RelativeRect.getHeight()),
		!clip);
	ScrollBar->setSubElement(true);
	ScrollBar->setTabStop(false);
	ScrollBar->setAlignment(EGUIA_LOWERRIGHT, EGUIA_LOWERRIGHT, EGUIA_UPPERLEFT, EGUIA_LOWERRIGHT);
	ScrollBar->setVisible(false);
	ScrollBar->drop();

	ScrollBar->setPos(0);
	ScrollBar->grab();

	setNotClipped(!clip);

	// this element can be tabbed to
	setTabStop(true);
	setTabOrder(-1);

	updateAbsolutePosition();
}


//! destructor
CGUIListBox::~CGUIListBox()
{
	if (ScrollBar)
		ScrollBar->drop();

	if (Font)
		Font->drop();

	if (IconBank)
		IconBank->drop();
}



//! returns amount of list items
s32 CGUIListBox::getItemCount()
{
	return Items.size();
}



//! returns string of a list item. the may be a value from 0 to itemCount-1
const wchar_t* CGUIListBox::getListItem(s32 id)
{
	if (id<0 || id>((s32)Items.size())-1)
		return 0;

	return Items[id].text.c_str();
}

//! Returns the icon of an item
s32 CGUIListBox::getIcon(s32 id) const
{
	if (id<0 || id>((s32)Items.size())-1)
		return -1;

	return Items[id].icon;
}


//! adds an list item, returns id of item
s32 CGUIListBox::addItem(const wchar_t* text)
{
	ListItem i;
	i.text = text;

	Items.push_back(i);
	recalculateItemHeight();
	recalculateScrollPos();

	return Items.size() - 1;
}

//! adds an list item, returns id of item
void CGUIListBox::removeItem(s32 id)
{
	if (id < 0 || id >= (s32)Items.size())
		return;

	if (Selected==id)
	{
		Selected = -1;
	}
	else if (Selected > id)
	{
		Selected -= 1;
		selectTime = os::Timer::getTime();
	}

	Items.erase(id);

	recalculateItemHeight();
}




//! clears the list
void CGUIListBox::clear()
{
	Items.clear();
	ItemsIconWidth = 0;
	Selected = -1;

	if (ScrollBar)
		ScrollBar->setPos(0);

	recalculateItemHeight();
}



void CGUIListBox::recalculateItemHeight()
{
	IGUISkin* skin = Environment->getSkin();

	if (Font != skin->getFont())
	{
		if (Font)
			Font->drop();

		Font = skin->getFont();
		ItemHeight = 0;

		if (Font)
		{
			ItemHeight = Font->getDimension(L"A").Height + 4;
			Font->grab();
		}
	}

	TotalItemHeight = ItemHeight * Items.size();
	ScrollBar->setMax(TotalItemHeight - AbsoluteRect.getHeight());

	if( TotalItemHeight <= AbsoluteRect.getHeight() )
		ScrollBar->setVisible(false);
	else
		ScrollBar->setVisible(true);

}



//! returns id of selected item. returns -1 if no item is selected.
s32 CGUIListBox::getSelected()
{
	return Selected;
}



//! sets the selected item. Set this to -1 if no item should be selected
void CGUIListBox::setSelected(s32 id)
{
	if (id<0 || id>((s32)Items.size())-1)
		Selected = -1;
	else
		Selected = id;

	selectTime = os::Timer::getTime();

	recalculateScrollPos();
}



//! called if an event happened.
bool CGUIListBox::OnEvent(SEvent event)
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
		else if (event.KeyInput.PressedDown && event.KeyInput.Char)
		{
			// change selection based on text as it is typed.
			u32 now = os::Timer::getTime();

			if (now - LastKeyTime < 500)
			{
				// add to key buffer if it isn't a key repeat
				if (!(KeyBuffer.size() == 1 && KeyBuffer[0] == event.KeyInput.Char))
				{
					KeyBuffer += L" ";
					KeyBuffer[KeyBuffer.size()-1] = event.KeyInput.Char;
				}
			}
			else 
			{
				KeyBuffer = L" ";
				KeyBuffer[0] = event.KeyInput.Char;
			}
			LastKeyTime = now;

			// find the selected item, starting at the current selection
			s32 start = Selected;
			s32 current = start+1;
			// dont change selection if the key buffer matches the current item
			if (Selected > -1 && KeyBuffer.size() > 1)
			{
				if (Items[Selected].text.size() >= KeyBuffer.size() && 
					KeyBuffer.equals_ignore_case(Items[Selected].text.subString(0,KeyBuffer.size())))
					return true;
			}

			while (current < (s32)Items.size())
			{
				if (Items[current].text.size() >= KeyBuffer.size())
				{
					if (KeyBuffer.equals_ignore_case(Items[current].text.subString(0,KeyBuffer.size())))
					{
						if (Parent && Selected != current && !Selecting && !MoveOverSelect)
						{
							SEvent e;
							e.EventType = EET_GUI_EVENT;
							e.GUIEvent.Caller = this;
							e.GUIEvent.Element = 0;
							e.GUIEvent.EventType = EGET_LISTBOX_CHANGED;
							Parent->OnEvent(e);
						}
						setSelected(current);
						return true;
					}
				}
				current++;
			}
			current = 0;
			while (current <= start)
			{
				if (Items[current].text.size() >= KeyBuffer.size())
				{
					if (KeyBuffer.equals_ignore_case(Items[current].text.subString(0,KeyBuffer.size())))
					{
						if (Parent && Selected != current && !Selecting && !MoveOverSelect)
						{
							Selected = current;
							SEvent e;
							e.EventType = EET_GUI_EVENT;
							e.GUIEvent.Caller = this;
							e.GUIEvent.Element = 0;
							e.GUIEvent.EventType = EGET_LISTBOX_CHANGED;
							Parent->OnEvent(e);
						}
						setSelected(current);
						return true;
					}					
				}
				current++;
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
				ScrollBar->setPos(ScrollBar->getPos() + (s32)event.MouseInput.Wheel*-10);
				return true;

			case EMIE_LMOUSE_PRESSED_DOWN:
			{
				Selecting = true;
				return true;
			}

			case EMIE_LMOUSE_LEFT_UP:

				if (!isPointInside(p))
				{
					Selecting = false;
					return true;
				}

				Selecting = false;
				selectNew(event.MouseInput.Y);
				return true;

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
	}


	return Parent ? Parent->OnEvent(event) : false;
}


void CGUIListBox::selectNew(s32 ypos, bool onlyHover)
{
	s32 oldSelected = Selected;

	// find new selected item.
	if (ItemHeight!=0)
		Selected = ((ypos - AbsoluteRect.UpperLeftCorner.Y - 1) + ScrollBar->getPos()) / ItemHeight;

	if (Selected >= (s32)Items.size())
		Selected = Items.size() - 1;
	else
	if (Selected<0)
		Selected = 0;

	recalculateScrollPos();

	// post the news
	if (Parent && !onlyHover)
	{
		SEvent event;
		event.EventType = EET_GUI_EVENT;
		event.GUIEvent.Caller = this;
		event.GUIEvent.Element = 0;
		event.GUIEvent.EventType = (Selected != oldSelected) ? EGET_LISTBOX_CHANGED : EGET_LISTBOX_SELECTED_AGAIN;
		Parent->OnEvent(event);
	}
}

//! Update the position and size of the listbox, and update the scrollbar
void CGUIListBox::updateAbsolutePosition()
{
	IGUIElement::updateAbsolutePosition();

	recalculateItemHeight();
}

//! draws the element and its children
void CGUIListBox::draw()
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
			textRect.UpperLeftCorner.X += 3;

			if (Font)
			{
				if (IconBank && (Items[i].icon > -1))
				{
					core::position2di iconPos = textRect.UpperLeftCorner;
					iconPos.Y += textRect.getHeight() / 2;
					iconPos.X += ItemsIconWidth/2;
					IconBank->draw2DSprite( (u32)Items[i].icon, iconPos, &clientClip,
						skin->getColor((i==Selected && hl) ? EGDC_ICON_HIGH_LIGHT : EGDC_ICON),
						(i==Selected && hl) ? selectTime : 0 , (i==Selected) ? os::Timer::getTime() : 0, false, true);
				}

				textRect.UpperLeftCorner.X += ItemsIconWidth+3;

				Font->draw(Items[i].text.c_str(), textRect, skin->getColor((i==Selected && hl) ? EGDC_HIGH_LIGHT_TEXT : EGDC_BUTTON_TEXT), false, true, &clientClip);

				textRect.UpperLeftCorner.X -= ItemsIconWidth+3;
			}
		}

		frameRect.UpperLeftCorner.Y += ItemHeight;
		frameRect.LowerRightCorner.Y += ItemHeight;
	}

	IGUIElement::draw();
}



//! adds an list item with an icon
s32 CGUIListBox::addItem(const wchar_t* text, s32 icon)
{
	ListItem i;
	i.text = text;
	i.icon = icon;

	Items.push_back(i);
	recalculateItemHeight();

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

	return Items.size() - 1;
}


void CGUIListBox::setSpriteBank(IGUISpriteBank* bank)
{
	if (IconBank)
		IconBank->drop();

	IconBank = bank;
	if (IconBank)
		IconBank->grab();
}
void CGUIListBox::recalculateScrollPos()
{
	if (!AutoScroll)
		return;

	s32 selPos = (Selected == -1 ? TotalItemHeight : Selected * ItemHeight) - ScrollBar->getPos();

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

void CGUIListBox::setAutoScrollEnabled(bool scroll)
{
	AutoScroll = scroll;
}

bool CGUIListBox::isAutoScrollEnabled()
{
	_IRR_IMPLEMENT_MANAGED_MARSHALLING_BUGFIX;
	return AutoScroll;
}

//! Writes attributes of the element.
void CGUIListBox::serializeAttributes(io::IAttributes* out, io::SAttributeReadWriteOptions* options=0)
{
	IGUIListBox::serializeAttributes(out,options);

	// todo: out->addString	("IconBank",		IconBank->getName?);
	out->addBool    ("DrawBack",        DrawBack);
	out->addBool    ("MoveOverSelect",  MoveOverSelect);
	out->addBool    ("AutoScroll",      AutoScroll);

	// todo: save list of items and icons.
	/*core::array<core::stringw> tmpText;
	core::array<core::stringw> tmpIcons;
	u32 i;
	for (i=0;i<Items.size(); ++i)
	{
		tmpText.push_back(Items[i].text);
		tmpIcons.push_back(Items[i].icon);
	}

	out->addArray	("ItemText",		tmpText);
	out->addArray	("ItemIcons",		tmpIcons);

	out->addInt		("Selected",		Selected);
	*/

}

//! Reads attributes of the element
void CGUIListBox::deserializeAttributes(io::IAttributes* in, io::SAttributeReadWriteOptions* options=0)
{
	DrawBack        = in->getAttributeAsBool("DrawBack");
	MoveOverSelect  = in->getAttributeAsBool("MoveOverSelect");
	AutoScroll      = in->getAttributeAsBool("AutoScroll");

	IGUIListBox::deserializeAttributes(in,options);

	// read arrays
	/*
	core::array<core::stringw> tmpText;
	core::array<core::stringw> tmpIcons;

	tmpText			= in->getAttributeAsArray("ItemText");
	tmpIcons		= in->getAttributeAsArray("ItemIcons");
	u32 i;
	for (i=0; i<Items.size(); ++i)
		addItem(tmpText[i].c_str(), tmpIcons[i].c_str());

	this->setSelected(in->getAttributeAsInt("Selected"));
	*/

}



} // end namespace gui
} // end namespace irr
