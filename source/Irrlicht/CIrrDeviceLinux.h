// Copyright (C) 2002-2006 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#ifndef __C_IRR_DEVICE_LINUX_H_INCLUDED__
#define __C_IRR_DEVICE_LINUX_H_INCLUDED__

#include "IrrCompileConfig.h"

#ifdef LINUX

#include "CIrrDeviceStub.h"
#include "IrrlichtDevice.h"
#include "IImagePresenter.h"
#include "IGUIEnvironment.h"
#include "ICursorControl.h"

#ifdef _IRR_COMPILE_WITH_OPENGL_
#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glxext.h>
#endif
#include <X11/Xlib.h>
#ifdef _IRR_LINUX_X11_VIDMODE_
#include <X11/extensions/xf86vmode.h>
#endif
#include <X11/keysym.h>

namespace irr
{

	class CIrrDeviceLinux : public CIrrDeviceStub, public video::IImagePresenter
	{
	public:

		//! constructor
		CIrrDeviceLinux(video::E_DRIVER_TYPE deviceType, 
			const core::dimension2d<s32>& windowSize, u32 bits,
			bool fullscreen, bool stencilbuffer, bool vsync, bool antiAlias, IEventReceiver* receiver,
			const char* version);

		//! destructor
		virtual ~CIrrDeviceLinux();

		//! runs the device. Returns false if device wants to be deleted
		virtual bool run();

		//! sets the caption of the window
		virtual void setWindowCaption(const wchar_t* text);

		//! returns if window is active. if not, nothing need to be drawn
		virtual bool isWindowActive();

		//! presents a surface in the client area
		virtual void present(video::IImage* surface, s32 windowId = 0, core::rect<s32>* src=0 );

		//! notifies the device that it should close itself
		virtual void closeDevice();

	private:

		//! create the driver
		void createDriver(video::E_DRIVER_TYPE driverType,
			const core::dimension2d<s32>& windowSize, u32 bits,
			bool vsync, bool antiAlias);

		bool createWindow(const core::dimension2d<s32>& windowSize, u32 bits);

		void createKeyMap();

		//! Implementation of the linux cursor control
		class CCursorControl : public gui::ICursorControl
		{
		public:

			CCursorControl(CIrrDeviceLinux* dev, bool null)
				: Device(dev), IsVisible(true), Null(null)
			{
				if (!null)
				{
					GC gc;
					XGCValues values;
					unsigned long valuemask = 0;

					XColor fg, bg;
					int depth = 1;

					// this code, for making the cursor invisible was sent in by
					// Sirshane, thank your very much!

					Colormap screen_colormap;

					invisBitmap = XCreatePixmap( Device->display, Device->window, 32, 32, depth );
					maskBitmap = XCreatePixmap( Device->display, Device->window, 32, 32, depth );

					screen_colormap = DefaultColormap( Device->display, DefaultScreen( Device->display ) );
					XAllocNamedColor( Device->display, screen_colormap, "black", &fg, &fg );
					XAllocNamedColor( Device->display, screen_colormap, "white", &bg, &bg );

					gc = XCreateGC( Device->display, invisBitmap, valuemask, &values );

					XSetForeground( Device->display, gc, BlackPixel( Device->display, DefaultScreen( Device->display ) ) );
					XFillRectangle( Device->display, invisBitmap, gc, 0, 0, 32, 32 );
					XFillRectangle( Device->display, maskBitmap, gc, 0, 0, 32, 32 );

					invisCursor = XCreatePixmapCursor( Device->display, invisBitmap, maskBitmap, &fg, &bg, 1, 1 );
				}
			}

			//! Changes the visible state of the mouse cursor.
			virtual void setVisible(bool visible)
			{
				IsVisible = visible;
				if (!Null)
				{
					if ( !IsVisible )
						XDefineCursor( Device->display, Device->window, invisCursor );
					else
						XUndefineCursor( Device->display, Device->window );
				}
			}

			//! Returns if the cursor is currently visible.
			virtual bool isVisible()
			{
				return IsVisible;
			}

			//! Sets the new position of the cursor.
			virtual void setPosition(const core::position2d<f32> &pos)
			{
				setPosition(pos.X, pos.Y);
			}

			//! Sets the new position of the cursor.
			virtual void setPosition(f32 x, f32 y)
			{
				setPosition((s32)(x*Device->Width), (s32)(y*Device->Height));
			}

			//! Sets the new position of the cursor.
			virtual void setPosition(const core::position2d<s32> &pos)
			{
				setPosition(pos.X, pos.Y);
			}

			//! Sets the new position of the cursor.
			virtual void setPosition(s32 x, s32 y)
			{
				if (!Null)
				{
					XWarpPointer(Device->display,
						None,
				 		Device->window, 0, 0,
				 		Device->Width,
				 		Device->Height, x, y);
					XFlush(Device->display);
				}
			}

			//! Returns the current position of the mouse cursor.
			virtual core::position2d<s32> getPosition()
			{
				updateCursorPos();
				return CursorPos;
			}

			//! Returns the current position of the mouse cursor.
			virtual core::position2d<f32> getRelativePosition()
			{
				updateCursorPos();
				return core::position2d<f32>(CursorPos.X / (f32)Device->Width,
					CursorPos.Y / (f32)Device->Height);
			}

		private:

			void updateCursorPos()
			{
				if (Null)
					return;
					
				Window tmp;
				int itmp1, itmp2;
				unsigned  int maskreturn;
				XQueryPointer(Device->display, Device->window,
					&tmp, &tmp,
					&itmp1, &itmp2,
					&CursorPos.X, &CursorPos.Y, &maskreturn);

				if (CursorPos.X < 0)
					CursorPos.X = 0;
				if (CursorPos.X > Device->Width)
					CursorPos.X = Device->Width;
				if (CursorPos.Y < 0)
					CursorPos.Y = 0;
				if (CursorPos.Y > Device->Height)
					CursorPos.Y = Device->Height;
			}

			core::position2d<s32> CursorPos;
			bool IsVisible;
			CIrrDeviceLinux* Device;
			Cursor invisCursor;
			Pixmap invisBitmap;
			Pixmap maskBitmap;
			bool Null;
		};

		friend class CCursorControl;

		Display *display;
		int screennr;
		Window window;
		#ifdef _IRR_COMPILE_WITH_OPENGL_
		GLXWindow glxWin;
		GLXContext Context;
		#endif
		XSetWindowAttributes attributes;
		bool Fullscreen;
		bool StencilBuffer;
		#ifdef _IRR_LINUX_X11_VIDMODE_
		XF86VidModeModeInfo oldVideoMode;
		#endif
		XEvent event;
		XImage* SoftwareImage;
		video::E_DRIVER_TYPE DriverType;

		int x,y;
		unsigned int Width, Height, Depth;
		bool close;

		struct SKeyMap
		{
			SKeyMap() {}
			SKeyMap(s32 x11, s32 win32)
				: X11Key(x11), Win32Key(win32)
			{
			}

			KeySym X11Key;
			s32 Win32Key;

			bool operator<(const SKeyMap& o) const
			{
				return X11Key<o.X11Key;
			}
		};

		core::array<SKeyMap> KeyMap;
	};


} // end namespace irr

#endif // LINUX
#endif // __C_IRR_DEVICE_LINUX_H_INCLUDED__

