// Copyright (C) 2006 Michael Zeilfelder
// This file uses the licence of the Irrlicht Engine.

#ifndef __C_GUI_SPIN_BOX_H_INCLUDED__
#define __C_GUI_SPIN_BOX_H_INCLUDED__

#include "IGUISpinBox.h"

namespace irr
{
namespace gui
{
	class IGUIEditBox;
	class IGUIButton;

	class CGUISpinBox : public IGUISpinBox
	{
	public:

		//! constructor
		CGUISpinBox(const wchar_t* text, IGUIEnvironment* environment,
			IGUIElement* parent, s32 id, const core::rect<s32>& rectangle);

		//! destructor
		~CGUISpinBox();

		//! Access the edit box used in the spin control
		/** \param enable: If set to true, the override color, which can be set
		with IGUIEditBox::setOverrideColor is used, otherwise the
		EGDC_BUTTON_TEXT color of the skin. */
		virtual IGUIEditBox* getEditBox();

		//! set the current value of the spinbox
		/** \param val: value to be set in the spinbox */
		virtual void setValue(f32 val);

		//! Get the current value of the spinbox
		virtual f32 getValue();

		//! set the range of values which can be used in the spinbox
		/** \param min: minimum value
		\param max: maximum value */
		virtual void setRange(f32 min, f32 max);

		//! get the minimum value which can be used in the spinbox
		virtual f32 getMin();

		//! get the maximum value which can be used in the spinbox
		virtual f32 getMax();

		//! step size by which values are changed when pressing the spin buttons
		/** \param step: stepsize used for value changes when pressing spin buttons */
		virtual void setStepSize(f32 step=1.f);

		//! returns the step size
		virtual f32 getStepSize();

		//! called if an event happened.
		virtual bool OnEvent(SEvent event);

		//! Sets the new caption of the element
		virtual void setText(const wchar_t* text);

		//! Returns caption of this element.
		virtual const wchar_t* getText();

		//! Sets the number of decimal places to display.
		/** \param places: The number of decimal places to display, use -1 to reset */
		virtual void setDecimalPlaces(s32 places);

		//! Writes attributes of the element.
		virtual void serializeAttributes(io::IAttributes* out, io::SAttributeReadWriteOptions* options);

		//! Reads attributes of the element
		virtual void deserializeAttributes(io::IAttributes* in, io::SAttributeReadWriteOptions* options);

	protected:
		virtual void verifyValueRange();

		IGUIEditBox * EditBox;
		IGUIButton * ButtonSpinUp;
		IGUIButton * ButtonSpinDown;
		f32 StepSize;
		f32 RangeMin;
		f32 RangeMax;

		core::stringw FormatString;
		s32 DecimalPlaces;
	};
} // end namespace gui
} // end namespace irr

#endif // __C_GUI_SPIN_BOX_H_INCLUDED__

