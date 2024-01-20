/*
The zlib/libpng License

Copyright (c) 2018 Arthur Brainville
Copyright (c) 2015 Andrew Fenn
Copyright (c) 2005-2010 Phillip Castaneda (pjcast -- www.wreckedgames.com)

This software is provided 'as-is', without any express or implied warranty. In no
event will the authors be held liable for any damages arising from the use of this
software.

Permission is granted to anyone to use this software for any purpose, including
commercial applications, and to alter it and redistribute it freely, subject to the
following restrictions:

    1. The origin of this software must not be misrepresented; you must not claim that
        you wrote the original software. If you use this software in a product,
        an acknowledgment in the product documentation would be appreciated
        but is not required.

    2. Altered source versions must be plainly marked as such, and must not be
        misrepresented as being the original software.

    3. This notice may not be removed or altered from any source distribution.
*/
//////////////////////////////// OS Nuetral Headers ////////////////
#include "OISInputManager.h"
#include "OISException.h"
#include "OISKeyboard.h"
#include "OISMouse.h"
#include "OISJoyStick.h"
#include "OISEvents.h"

//Advanced Usage
#include "OISForceFeedback.h"

#include <iostream>
#include <vector>
#include <sstream>
#include <algorithm>

////////////////////////////////////Needed Windows Headers////////////
#if defined OIS_WIN32_PLATFORM
#define WIN32_LEAN_AND_MEAN
#include "windows.h"
#ifdef min
#undef min
#endif
#include "resource.h"
LRESULT DlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
//////////////////////////////////////////////////////////////////////
////////////////////////////////////Needed Linux Headers//////////////
#elif defined OIS_LINUX_PLATFORM
#include <X11/Xlib.h>
#include <X11/Xatom.h>
void checkX11Events();
//////////////////////////////////////////////////////////////////////
////////////////////////////////////Needed Mac Headers//////////////
#elif defined OIS_APPLE_PLATFORM
#import <Cocoa/Cocoa.h>
void checkMacEvents();
#endif
//////////////////////////////////////////////////////////////////////
using namespace OIS;

//-- Some local prototypes --//
void doStartup();
void handleNonBufferedKeys();
void handleNonBufferedMouse();
void handleNonBufferedJoy(JoyStick* js);

//-- Easy access globals --//
bool appRunning = true; //Global Exit Flag

const char* g_DeviceType[6] = { "OISUnknown", "OISKeyboard", "OISMouse", "OISJoyStick", "OISTablet", "OISOther" };

InputManager* g_InputManager = nullptr;								   //Our Input System
Keyboard* g_kb				 = nullptr;								   //Keyboard Device
Mouse* g_m					 = nullptr;								   //Mouse Device
JoyStick* g_joys[4]			 = { nullptr, nullptr, nullptr, nullptr }; //This demo supports up to 4 controllers

//-- OS Specific Globals --//
#if defined OIS_WIN32_PLATFORM
HWND hWnd = nullptr;
#elif defined OIS_LINUX_PLATFORM
Display* xDisp = 0;
Window xWin	   = 0;
#elif defined OIS_APPLE_PLATFORM
WindowRef mWin = 0;
#endif

//////////// Common Event handler class ////////
class EventHandler : public KeyListener, public MouseListener, public JoyStickListener
{
public:
	EventHandler() { }
	~EventHandler() { }
	bool keyPressed(const KeyEvent& arg)
	{
		std::cout << " KeyPressed {" << std::hex << arg.key << std::dec
				  << "} || Character (" << (char)arg.text << ")" << std::endl;

		if(arg.key == OIS::KeyCode::KC_UP)
		{
			std::cout << "up key!\n";
		}

		return true;
	}
	bool keyReleased(const KeyEvent& arg)
	{
		if(arg.key == KC_ESCAPE || arg.key == KC_Q)
			appRunning = false;
		std::cout << "KeyReleased {" << std::hex << arg.key << std::dec
				  << "}\n";
		return true;
	}
	bool mouseMoved(const MouseEvent& arg)
	{
		const OIS::MouseState& s = arg.state;
		std::cout << "\nMouseMoved: Abs("
				  << s.X.abs << ", " << s.Y.abs << ", " << s.Z.abs << ") Rel("
				  << s.X.rel << ", " << s.Y.rel << ", " << s.Z.rel << ")";
		return true;
	}
	bool mousePressed(const MouseEvent& arg, MouseButtonID id)
	{
		const OIS::MouseState& s = arg.state;
		std::cout << "\nMouse button #" << id << " pressed. Abs("
				  << s.X.abs << ", " << s.Y.abs << ", " << s.Z.abs << ") Rel("
				  << s.X.rel << ", " << s.Y.rel << ", " << s.Z.rel << ")";
		return true;
	}
	bool mouseReleased(const MouseEvent& arg, MouseButtonID id)
	{
		const OIS::MouseState& s = arg.state;
		std::cout << "\nMouse button #" << id << " released. Abs("
				  << s.X.abs << ", " << s.Y.abs << ", " << s.Z.abs << ") Rel("
				  << s.X.rel << ", " << s.Y.rel << ", " << s.Z.rel << ")";
		return true;
	}
	bool buttonPressed(const JoyStickEvent& arg, int button)
	{
		std::cout << std::endl
				  << arg.device->vendor() << ". Button Pressed # " << button;
		return true;
	}
	bool buttonReleased(const JoyStickEvent& arg, int button)
	{
		std::cout << std::endl
				  << arg.device->vendor() << ". Button Released # " << button;
		return true;
	}
	bool axisMoved(const JoyStickEvent& arg, int axis)
	{
		//Provide a little dead zone
		if(arg.state.mAxes[axis].abs > 2500 || arg.state.mAxes[axis].abs < -2500)
			std::cout << std::endl
					  << arg.device->vendor() << ". Axis # " << axis << " Value: " << arg.state.mAxes[axis].abs;
		return true;
	}
	bool sliderMoved(const JoyStickEvent& arg, int index)
	{
		std::cout << std::endl
				  << arg.device->vendor() << ". Slider # " << index
				  << " X Value: " << arg.state.mSliders[index].abX
				  << " Y Value: " << arg.state.mSliders[index].abY;
		return true;
	}
	bool povMoved(const JoyStickEvent& arg, int pov)
	{
		std::cout << std::endl
				  << arg.device->vendor() << ". POV" << pov << " ";

		if(arg.state.mPOV[pov].direction & Pov::North) //Going up
			std::cout << "North";
		else if(arg.state.mPOV[pov].direction & Pov::South) //Going down
			std::cout << "South";

		if(arg.state.mPOV[pov].direction & Pov::East) //Going right
			std::cout << "East";
		else if(arg.state.mPOV[pov].direction & Pov::West) //Going left
			std::cout << "West";

		if(arg.state.mPOV[pov].direction == Pov::Centered) //stopped/centered out
			std::cout << "Centered";
		return true;
	}

	bool vector3Moved(const JoyStickEvent& arg, int index)
	{
		std::cout.precision(2);
		std::cout.flags(std::ios::fixed | std::ios::right);
		std::cout << std::endl
				  << arg.device->vendor() << ". Orientation # " << index
				  << " X Value: " << arg.state.mVectors[index].x
				  << " Y Value: " << arg.state.mVectors[index].y
				  << " Z Value: " << arg.state.mVectors[index].z;
		std::cout.precision();
		std::cout.flags();
		return true;
	}
};

//Create a global instance
EventHandler handler;

int main()
{
	std::cout << "\n\n*** OIS Console Demo App is starting up... *** \n";
	try
	{
		doStartup();
		std::cout << "\nStartup done... Hit 'q' or ESC to exit.\n\n";

		while(appRunning)
		{
//Throttle down CPU usage
#if defined OIS_WIN32_PLATFORM
			Sleep(90);
			MSG msg;
			while(PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
#elif defined OIS_LINUX_PLATFORM
			   checkX11Events();
			   usleep(500);
#elif defined OIS_APPLE_PLATFORM
			checkMacEvents();
			usleep(500);
#endif

			if(g_kb)
			{
				g_kb->capture();
				if(!g_kb->buffered())
					handleNonBufferedKeys();
			}

			if(g_m)
			{
				g_m->capture();
				if(!g_m->buffered())
					handleNonBufferedMouse();
			}

			for(int i = 0; i < 4; ++i)
			{
				if(g_joys[i])
				{
					g_joys[i]->capture();
					if(!g_joys[i]->buffered())
						handleNonBufferedJoy(g_joys[i]);
				}
			}
		}
	}
	catch(const Exception& ex)
	{
#if defined OIS_WIN32_PLATFORM
		MessageBox(nullptr, ex.eText, "An exception has occurred!", MB_OK | MB_ICONERROR | MB_TASKMODAL);
#else
		std::cout << "\nOIS Exception Caught!\n"
				  << "\t" << ex.eText << "[Line "
				  << ex.eLine << " in " << ex.eFile << "]\nExiting App";
#endif
	}
	catch(std::exception& ex)
	{
		std::cout << "Caught std::exception: what = " << ex.what() << std::endl;
	}

	//Destroying the manager will cleanup unfreed devices
	std::cout << "Cleaning up...\n";
	if(g_InputManager)
		InputManager::destroyInputSystem(g_InputManager);

#if defined OIS_LINUX_PLATFORM
	// Be nice to X and clean up the x window
	XDestroyWindow(xDisp, xWin);
	XCloseDisplay(xDisp);
#endif

	std::cout << "\nGoodbye!\n";
	return 0;
}

void doStartup()
{
	ParamList pl;

#if defined OIS_WIN32_PLATFORM
	//Create a capture window for Input Grabbing
	hWnd = CreateDialog(nullptr, MAKEINTRESOURCE(IDD_DIALOG1), nullptr, (DLGPROC)DlgProc);
	if(hWnd == nullptr)
		OIS_EXCEPT(E_General, "Failed to create Win32 Window Dialog!");

	ShowWindow(hWnd, SW_SHOW);

	std::ostringstream wnd;
	wnd << (size_t)hWnd;

	pl.insert(std::make_pair(std::string("WINDOW"), wnd.str()));

	//Default mode is foreground exclusive..but, we want to show mouse - so nonexclusive
//	pl.insert(std::make_pair(std::string("w32_mouse"), std::string("DISCL_FOREGROUND" )));
//	pl.insert(std::make_pair(std::string("w32_mouse"), std::string("DISCL_NONEXCLUSIVE")));
#elif defined OIS_LINUX_PLATFORM
	//Connects to default X window
	if(!(xDisp = XOpenDisplay(0)))
		OIS_EXCEPT(E_General, "Error opening X!");
	//Create a window
	xWin = XCreateSimpleWindow(xDisp, DefaultRootWindow(xDisp), 0, 0, 100, 100, 0, 0, 0);
	//bind our connection to that window
	XMapWindow(xDisp, xWin);
	// XInternAtom
	//Select what events we want to listen to locally
	XSelectInput(xDisp, xWin, StructureNotifyMask | SubstructureNotifyMask);
	Atom wmProto  = XInternAtom(xDisp, "WM_PROTOCOLS", False);
	Atom wmDelete = XInternAtom(xDisp, "WM_DELETE_WINDOW", False);
	XChangeProperty(xDisp, xWin, wmProto, XA_ATOM, 32, 0, (const unsigned char*)&wmDelete, 1);
	XEvent evtent;
	do
	{
		XNextEvent(xDisp, &evtent);
	} while(evtent.type != MapNotify);

	std::ostringstream wnd;
	wnd << xWin;

	pl.insert(std::make_pair(std::string("WINDOW"), wnd.str()));

	//For this demo, show mouse and do not grab (confine to window)
//	pl.insert(std::make_pair(std::string("x11_mouse_grab"), std::string("false")));
//	pl.insert(std::make_pair(std::string("x11_mouse_hide"), std::string("false")));
#elif defined OIS_APPLE_PLATFORM

	//Plese note, Carbon based code used to be there, this is progressively being replaced with Cocoa code.

	//This create an NS application and a cocoa window
	[NSAutoreleasePool new];
	[NSApplication sharedApplication];
	[NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
	id menubar	   = [[NSMenu new] autorelease];
	id appMenuItem = [[NSMenuItem new] autorelease];
	[menubar addItem:appMenuItem];
	[NSApp setMainMenu:menubar];
	id appMenu		= [[NSMenu new] autorelease];
	id appName		= [[NSProcessInfo processInfo] processName];
	id quitTitle	= [@"Quit " stringByAppendingString:appName];
	id quitMenuItem = [[[NSMenuItem alloc] initWithTitle:quitTitle
												  action:@selector(terminate:)
										   keyEquivalent:@"q"] autorelease];
	[appMenu addItem:quitMenuItem];
	[appMenuItem setSubmenu:appMenu];

	//This is the interesing bit
	id window = [[[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 200, 200)
											 styleMask:NSWindowStyleMaskTitled
											   backing:NSBackingStoreBuffered
												 defer:NO]
		autorelease];
	[window cascadeTopLeftFromPoint:NSMakePoint(20, 20)];
	[window setTitle:@"OIS Input"];
	[window makeKeyAndOrderFront:nil];
	[NSApp activateIgnoringOtherApps:YES];
	//	[NSApp run];

	//Apparently the "id" type the window variable was declared into boils down to a simple pointer.
	//To give it to OIS, put the numerical value of the pointer into a string, as you do with Windows and Linux
	pl.insert(std::make_pair(std::string("WINDOW"), std::to_string((size_t)window)));

#endif

	//This never returns null.. it will raise an exception on errors
	g_InputManager = InputManager::createInputSystem(pl);

	//Lets enable all addons that were compiled in:
	g_InputManager->enableAddOnFactory(InputManager::AddOn_All);

	//Print debugging information
	unsigned int v = g_InputManager->getVersionNumber();
	std::cout << "OIS Version: " << (v >> 16) << "." << ((v >> 8) & 0x000000FF) << "." << (v & 0x000000FF)
			  << "\nRelease Name: " << g_InputManager->getVersionName()
			  << "\nManager: " << g_InputManager->inputSystemName()
			  << "\nTotal Keyboards: " << g_InputManager->getNumberOfDevices(OISKeyboard)
			  << "\nTotal Mice: " << g_InputManager->getNumberOfDevices(OISMouse)
			  << "\nTotal JoySticks: " << g_InputManager->getNumberOfDevices(OISJoyStick);

	//List all devices
	DeviceList list = g_InputManager->listFreeDevices();
	for(DeviceList::iterator i = list.begin(); i != list.end(); ++i)
		std::cout << "\n\tDevice: " << g_DeviceType[i->first] << " Vendor: " << i->second;

	g_kb = (Keyboard*)g_InputManager->createInputObject(OISKeyboard, true);
	g_kb->setEventCallback(&handler);

	g_m = (Mouse*)g_InputManager->createInputObject(OISMouse, true);
	g_m->setEventCallback(&handler);
	const MouseState& ms = g_m->getMouseState();
	ms.width			 = 100;
	ms.height			 = 100;

	try
	{
		//This demo uses at most 4 joysticks - use old way to create (i.e. disregard vendor)
		int numSticks = std::min(g_InputManager->getNumberOfDevices(OISJoyStick), 4);
		for(int i = 0; i < numSticks; ++i)
		{
			g_joys[i] = (JoyStick*)g_InputManager->createInputObject(OISJoyStick, true);
			g_joys[i]->setEventCallback(&handler);
			std::cout << "\n\nCreating Joystick " << (i + 1)
					  << "\n\tAxes: " << g_joys[i]->getNumberOfComponents(OIS_Axis)
					  << "\n\tSliders: " << g_joys[i]->getNumberOfComponents(OIS_Slider)
					  << "\n\tPOV/HATs: " << g_joys[i]->getNumberOfComponents(OIS_POV)
					  << "\n\tButtons: " << g_joys[i]->getNumberOfComponents(OIS_Button)
					  << "\n\tVector3: " << g_joys[i]->getNumberOfComponents(OIS_Vector3);
		}
	}
	catch(OIS::Exception& ex)
	{
		std::cout << "\nException raised on joystick creation: " << ex.eText << std::endl;
	}
}

void handleNonBufferedKeys()
{
	if(g_kb->isKeyDown(KC_ESCAPE) || g_kb->isKeyDown(KC_Q))
		appRunning = false;

	if(g_kb->isModifierDown(Keyboard::Shift))
		std::cout << "Shift is down..\n";
	if(g_kb->isModifierDown(Keyboard::Alt))
		std::cout << "Alt is down..\n";
	if(g_kb->isModifierDown(Keyboard::Ctrl))
		std::cout << "Ctrl is down..\n";
}

void handleNonBufferedMouse()
{
	//Just dump the current mouse state
	const MouseState& ms = g_m->getMouseState();
	std::cout << "\nMouse: Abs(" << ms.X.abs << " " << ms.Y.abs << " " << ms.Z.abs
			  << ") B: " << ms.buttons << " Rel(" << ms.X.rel << " " << ms.Y.rel << " " << ms.Z.rel << ")";
}

void handleNonBufferedJoy(JoyStick* js)
{
	//Just dump the current joy state
	const JoyStickState& joy = js->getJoyStickState();
	for(unsigned int i = 0; i < joy.mAxes.size(); ++i)
		std::cout << "\nAxis " << i << " X: " << joy.mAxes[i].abs;
}

#if defined OIS_WIN32_PLATFORM
LRESULT DlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	return FALSE;
}
#endif

#if defined OIS_LINUX_PLATFORM
//This is just here to show that you still recieve x11 events, as the lib only needs mouse/key events
void checkX11Events()
{
	if(!appRunning)
		return;

	XEvent event;

	while(XPending(xDisp) > 0)
	{
		XNextEvent(xDisp, &event);
		//Handle Resize events
		if(event.type == ConfigureNotify)
		{
			if(g_m)
			{
				const MouseState& ms = g_m->getMouseState();
				ms.width			 = event.xconfigure.width;
				ms.height			 = event.xconfigure.height;
			}
		}
		else if(event.type == ClientMessage || event.type == DestroyNotify)
		{ // We only get DestroyNotify for child windows. However, we regeistered earlier to receive WM_DELETE_MESSAGEs
			std::cout << "Exiting...\n";
			appRunning = false;
			return;
		}
		else
		{
			std::cout << "\nUnknown X Event: " << event.type << std::endl;
		}
	}
}
#endif

#if defined OIS_APPLE_PLATFORM
void checkMacEvents()
{
	//TODO - Check for window resize events, and then adjust the members of mousestate
	NSEvent *event;

	while(event = [NSApp nextEventMatchingMask:NSAnyEventMask untilDate:nil inMode:NSDefaultRunLoopMode dequeue:YES])
	{
		[NSApp sendEvent:event];
	}

	[NSApp updateWindows];
}
#endif
