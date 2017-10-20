





























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


#ifndef HEADER_ENGINE_HPP
#define HEADER_ENGINE_HPP

/**
 * \defgroup guiengine
 * Contains the generic GUI engine (contains the widgets and the backing logic
 * for event handling, the skin, screens and dialogs). See module @ref states_screens
 * for the actual STK GUI screens. Note that all input comes through this module
 * too.
 */

namespace irr
{
    class IrrlichtDevice;
    namespace gui   { class IGUIEnvironment; class ScalableFont; }
    namespace video { class IVideoDriver; class ITexture;        }
}

#include <string>

#include "utils/constants.hpp"
#include "utils/ptr_vector.hpp"

#include "guiengine/ft_environment.hpp"
#include "guiengine/glyph_page_creator.hpp"

/**
 * \ingroup guiengine
 * \brief Contains all GUI engine related classes and functions
 *
 * See \ref gui_overview for more information.
 */
namespace GUIEngine
{
    class Screen;
    class Widget;
    class Skin;
    class AbstractStateManager;

    /** \brief Returns the widget currently focused by given player, or NULL if none.
      * \note Do NOT use irrLicht's GUI focus facilities; it's too limited for our
      *       needs, so we use ours. (i.e. always call these functions are never those
      *       in IGUIEnvironment)
      */
    Widget* getFocusForPlayer(const unsigned int playerID);

    /** \brief Focuses nothing for given player (removes any selection for this player).
      * \note Do NOT use irrLicht's GUI focus facilities; it's too limited for our
      *       needs, so we use ours. (i.e. always call these functions are never those
      *       in IGUIEnvironment)
      */
    void focusNothingForPlayer(const unsigned int playerID);

    /** \brief Returns whether given the widget is currently focused by given player.
      * \note  Do NOT use irrLicht's GUI focus facilities; it's too limited for our
      *        needs, so we use ours. (i.e. always call these functions are never those
      *        in IGUIEnvironment)
      */
    bool isFocusedForPlayer(const Widget*w, const unsigned int playerID);

    /**
      * In an attempt to make getters as fast as possible, by possibly still allowing inlining
      * These fields should never be accessed outside of the GUI engine.
      */
    namespace Private
    {
        extern irr::gui::IGUIEnvironment* g_env;
        extern Skin* g_skin;
        extern FTEnvironment*  g_ft_env;
        extern GlyphPageCreator*  g_gp_creator;
        extern irr::gui::ScalableFont* g_small_font;
        extern irr::gui::ScalableFont* g_font;
        extern irr::gui::ScalableFont* g_outline_font;
        extern irr::gui::ScalableFont* g_large_font;
        extern irr::gui::ScalableFont* g_title_font;
        extern irr::gui::ScalableFont* g_digit_font;

        extern irr::IrrlichtDevice* g_device;
        extern irr::video::IVideoDriver* g_driver;
        extern Screen* g_current_screen;
        extern AbstractStateManager* g_state_manager;
        extern Widget* g_focus_for_player[MAX_PLAYER_COUNT];
    }

    /** Widgets that need to be notified at every frame can add themselves there (FIXME: unclean) */
    extern PtrVector<Widget, REF> needsUpdate;

    /**
      * \brief               Call this method to init the GUI engine.
      * \pre        A irrlicht device and its corresponding video drivers must have been created
      * \param device        An initialized irrlicht device object
      * \param driver        An initialized irrlicht driver object
      * \param state_manager An instance of a class derived from abstract base AbstractStateManager
      */
    void init(irr::IrrlichtDevice* device, irr::video::IVideoDriver* driver, AbstractStateManager* state_manager);

    void cleanUp();

    void deallocate();


    /**
      * \return the irrlicht device object
      */
    inline irr::IrrlichtDevice*       getDevice()        { return Private::g_device;         }

    /**
      * \return the irrlicht GUI environment object
      */
    inline irr::gui::IGUIEnvironment* getGUIEnv()        { return Private::g_env;            }

    /**
      * \return the irrlicht video driver object
     */
    inline irr::video::IVideoDriver*  getDriver()        { return Private::g_driver;         }

    /**
      * \return the smaller font (useful for less important messages)
      */
    inline irr::gui::ScalableFont*    getSmallFont()     { return Private::g_small_font;     }

    /**
      * \return the "normal" font (useful for text)
      */
    inline irr::gui::ScalableFont*    getFont()          { return Private::g_font;           }

    inline irr::gui::ScalableFont*    getOutlineFont()   { return Private::g_outline_font;   }

    /**
      * \return the "large" font (useful for text)
      */
    inline irr::gui::ScalableFont*    getLargeFont()     { return Private::g_large_font;     }

    /**
     * \return the "high-res digits" font (useful for big numbers)
     */
    inline irr::gui::ScalableFont*    getHighresDigitFont() { return Private::g_digit_font;  }

    /**
      * \return the "title" font (it's bigger and orange, useful for headers/captions)
      */
    inline irr::gui::ScalableFont*    getTitleFont()     { return Private::g_title_font;     }

    /**
      * \return the currently shown screen, or NULL if none
      */
    inline Screen*                    getCurrentScreen() { return Private::g_current_screen; }

    /**
      * \return the state manager being used, as passed to GUIEngine::init
      */
    inline AbstractStateManager*      getStateManager()  { return Private::g_state_manager;  }

    void clearScreenCache();

    /**
      * \pre GUIEngine::init must have been called first
      * \return       the skin object used to render widgets
      */
    inline Skin*                      getSkin()          { return Private::g_skin;           }

    /**
      * \pre GUIEngine::init must have been called first
      * \return       the freetype and library with face
      */
    inline FTEnvironment*             getFreetype()      { return Private::g_ft_env;         }

    /**
      * \pre GUIEngine::init must have been called first
      * \return       the glyph page creator, useful to create a glyph page from individual char
      */
    inline GlyphPageCreator*          getGlyphPageCreator() { return Private::g_gp_creator;  }

    Screen*                           getScreenNamed(const char* name);

    /** \return the height of the title font in pixels */
    int   getTitleFontHeight();

    /** \return the height of the font in pixels */
    int   getFontHeight();

    /** \return the height of the small font in pixels */
    int   getSmallFontHeight();

    /**
      * \pre the value returned by this function is only valid when invoked from GUIEngine::render
      * \return the time delta between the last two frames
      */
    float getLatestDt();

    /**
      * \brief shows a message at the bottom of the screen for a while
      * \param message  the message to display
      * \param time     the time to display the message, in seconds
      */
    void showMessage(const wchar_t* message, const float time=5.0f);

    /** \brief Add a screen to the list of screens known by the gui engine */
    void  addScreenToList(Screen* screen);
    /** \brief Remove a screen from the list of screens known by the gui engine */
    void  removeScreen(const char* name);

    /** \brief Low-level mean to change current screen.
      * \note Do not use directly. Use a state manager instead to get higher-level functionnality.
      */
    void switchToScreen(const char* );

    /** \brief erases the currently displayed screen, removing all added irrLicht widgets
      * \note Do not use directly. Use a state manager instead to get higher-level functionnality.
      */
    void clear();

    void update(float dt);

    /** \brief like GUIEngine::clear, but to be called before going into game */
    void cleanForGame();

    /** \brief to be called after e.g. a resolution switch */
    void reshowCurrentScreen();

    /**
      * \brief called on every frame to trigger the rendering of the GUI
      */
    void render(float dt);

    /** \brief renders a "loading" screen */ it is what i want! yeah
    void renderLoading(bool clearIcons = true);

    /** \brief to spice up a bit the loading icon : add icons to the loading screen */
    void addLoadingIcon(irr::video::ITexture* icon);

    /** \brief      Finds a widget from its name (PROP_ID) in the current screen/dialog
      * \param name the name (PROP_ID) of the widget to search for
      * \return     the widget that bears that name, or NULL if it was not found
      */
    Widget* getWidget(const char* name);

    /** \brief      Finds a widget from its irrlicht widget ID in the current screen/dialog
      * \param name the irrlicht widget ID (not to be confused with PROP_ID, which is a string)
      *             of the widget to search for
      * \return     the widget that bears that irrlicht ID, or NULL if it was not found
      */
    Widget* getWidget(const int id);

    /**
      * \brief call when skin in user config was updated
      */
    void reloadSkin();

    /**
      * \brief call when translation in user config was updated for freetype rendering STK
      */
    void cleanHollowCopyFont();
    void reloadHollowCopyFont(irr::gui::ScalableFont*);
}

#endif
