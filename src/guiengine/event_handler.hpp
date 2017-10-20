//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2010-2015 Marianne Gagnon
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

#ifndef HEADER_EVENT_HANDLER_HPP
#define HEADER_EVENT_HANDLER_HPP

#include <vector2d.h>
#include <IEventReceiver.h>
#include "input/input.hpp"
#include "utils/leak_check.hpp"

/**
 * \ingroup guiengine
 */
namespace GUIEngine
{

    /**
      * \ingroup guiengine
      */
    enum EventPropagation
    {
        EVENT_BLOCK,
        EVENT_LET
    };

    class Widget;

    /**
     * \brief Class to handle irrLicht events (GUI and input as well)
     *
     * input events will be redirected to the input module in game mode.
     * In menu mode, input is mapped to game actions with the help of the input
     * module, then calls are made to move focus / trigger an event / etc.
     *
     * This is really only the irrLicht events bit, not to be confused with my own simple events dispatched
     * mainly through AbstractStateManager, and also to widgets (this class is some kind of bridge between
     * the base irrLicht GUI engine and the STK layer on top of it)
     *
     * \ingroup guiengine
     */
    /*
    //! Enumeration for all event types in IEventReceiver there are.
    enum EEVENT_TYPE
    {
        //! An event of the graphical user interface.
        /** GUI events are created by the GUI environment or the GUI elements in response
        to mouse or keyboard events. When a GUI element receives an event it will either
        process it and return true, or pass the event to its parent. If an event is not absorbed
        before it reaches the root element then it will then be passed to the user receiver. */
        //  EET_GUI_EVENT = 0,

        //! A mouse input event.
        /** Mouse events are created by the device and passed to IrrlichtDevice::postEventFromUser
        in response to mouse input received from the operating system.
        Mouse events are first passed to the user receiver, then to the GUI environment and its elements,
        then finally the input receiving scene manager where it is passed to the active camera.
        */
        //  EET_MOUSE_INPUT_EVENT,

        //! A key input event.
        /** Like mouse events, keyboard events are created by the device and passed to
        IrrlichtDevice::postEventFromUser. They take the same path as mouse events. */
        //  EET_KEY_INPUT_EVENT,

        //! A joystick (joypad, gamepad) input event.
        /** Joystick events are created by polling all connected joysticks once per
        device run() and then passing the events to IrrlichtDevice::postEventFromUser.
        They take the same path as mouse events.
        Windows, SDL: Implemented.
        Linux: Implemented, with POV hat issues.
        MacOS / Other: Not yet implemented.
        */
        //  EET_JOYSTICK_INPUT_EVENT,

        //! A log event
        /** Log events are only passed to the user receiver if there is one. If they are absorbed by the
        user receiver then no text will be sent to the console. */
        //  EET_LOG_TEXT_EVENT,

//#if defined(_IRR_COMPILE_WITH_WINDOWS_DEVICE_)
        //! A input method event
        /** Input method events are created by the input method message and passed to IrrlichtDevice::postEventFromUser.
        Windows: Implemented.
        Linux / Other: Not yet implemented. */
        //  EET_IMPUT_METHOD_EVENT,
//#endif

        //! A user event with user data.
        /** This is not used by Irrlicht and can be used to send user
        specific data though the system. The Irrlicht 'window handle'
        can be obtained from IrrlichtDevice::getExposedVideoData()
        The usage and behavior depends on the operating system:
        Windows: send a WM_USER message to the Irrlicht Window; the
            wParam and lParam will be used to populate the
            UserData1 and UserData2 members of the SUserEvent.
        Linux: send a ClientMessage via XSendEvent to the Irrlicht
            Window; the data.l[0] and data.l[1] members will be
            casted to s32 and used as UserData1 and UserData2.
        MacOS: Not yet implemented
        */
        //  EET_USER_EVENT,

        //! This enum is never used, it only forces the compiler to
        //! compile these enumeration values to 32 bit.
        //  EGUIET_FORCE_32_BIT = 0x7fffffff

    //};
    class EventHandler : public irr::IEventReceiver
    {
        /** This variable is used to ignore events during the initial load screen, so that
            a player cannot trigger an action by clicking on the window during loading screen
            for example */
        bool m_accept_events;//it is excellent
        
        EventPropagation onGUIEvent(const irr::SEvent& event);//SEvents hold information about an event
        EventPropagation onWidgetActivated(Widget* w, const int playerID);
        void navigate(const int playerID, Input::InputType type, const bool pressedDown, const bool reverse);

        /** \brief          send an event to the GUI module user's event callback
          * \param widget   the widget that triggerred this event
          * \param name     the name/ID (PROP_ID) of the widget that triggerred this event
          * \param playerID ID of the player that triggerred this event
          */
        void sendEventToUser(Widget* widget, std::string& name, const int playerID);

        /** Last position of the mouse cursor */
        irr::core::vector2di     m_mouse_pos;//in web i have the counterpart

    public:

        LEAK_CHECK()

        EventHandler();
        ~EventHandler();

        /**
         * All irrLicht events will go through this (input as well GUI; input events are
         * immediately delegated to the input module, GUI events are processed here)
         */
        bool OnEvent (const irr::SEvent &event);

        /**
         * When the input module is done processing an input and mapped it to an action,
         * and this action needs to be applied to the GUI (e.g. fire pressed, left
         * pressed, etc.) this method is called back by the input module.
         */
        void processGUIAction(const PlayerAction action, int deviceID, const unsigned int value,
                              Input::InputType type, const int playerID);

        /** Get the mouse position */
        const irr::core::vector2di& getMousePos() const { return m_mouse_pos; }

        /** singleton access */
        static EventHandler* get();
        static void deallocate();
        
        void startAcceptingEvents() { m_accept_events = true; }
    };

}

#endif
