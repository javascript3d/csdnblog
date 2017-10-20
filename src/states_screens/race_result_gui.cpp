//
//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2010-2015 Joerg Henrichs
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

#include "states_screens/race_result_gui.hpp"

#include "audio/music_manager.hpp"
#include "audio/sfx_manager.hpp"
#include "audio/sfx_base.hpp"
#include "challenges/unlock_manager.hpp"
#include "config/player_manager.hpp"
#include "config/user_config.hpp"
#include "graphics/2dutils.hpp"
#include "graphics/material.hpp"
#include "guiengine/engine.hpp"
#include "guiengine/modaldialog.hpp"
#include "guiengine/scalable_font.hpp"
#include "guiengine/widget.hpp"
#include "guiengine/widgets/icon_button_widget.hpp"
#include "guiengine/widgets/label_widget.hpp"
#include "io/file_manager.hpp"
#include "karts/abstract_kart.hpp"
#include "karts/controller/controller.hpp"
#include "karts/controller/end_controller.hpp"
#include "karts/controller/local_player_controller.hpp"
#include "karts/kart_properties.hpp"
#include "karts/kart_properties_manager.hpp"
#include "modes/cutscene_world.hpp"
#include "modes/demo_world.hpp"
#include "modes/overworld.hpp"
#include "modes/soccer_world.hpp"
#include "modes/world_with_rank.hpp"
#include "network/protocol_manager.hpp"
#include "network/protocols/client_lobby_room_protocol.hpp"
#include "race/highscores.hpp"
#include "scriptengine/property_animator.hpp"
#include "states_screens/feature_unlocked.hpp"
#include "states_screens/main_menu_screen.hpp"
#include "states_screens/networking_lobby.hpp"
#include "states_screens/network_kart_selection.hpp"
#include "states_screens/online_profile_servers.hpp"
#include "states_screens/race_setup_screen.hpp"
#include "states_screens/server_selection.hpp"
#include "tracks/track.hpp"
#include "tracks/track_manager.hpp"
#include "utils/string_utils.hpp"
#include <algorithm>

DEFINE_SCREEN_SINGLETON(RaceResultGUI);

/** Constructor, initialises internal data structures.
 */
RaceResultGUI::RaceResultGUI() : Screen("race_result.stkgui",
    /*pause race*/ false)
{
}   // RaceResultGUI

//-----------------------------------------------------------------------------
/** Besides calling init in the base class this makes all buttons of this
 *  screen invisible. The buttons will only displayed once the animation is
 *  over.
 */
void RaceResultGUI::init()
{
    Screen::init();
    determineTableLayout();
    m_animation_state = RR_INIT;

    m_timer = 0;

    getWidget("top")->setVisible(false);
    getWidget("middle")->setVisible(false);
    getWidget("bottom")->setVisible(false);

    music_manager->stopMusic();

    bool human_win = true;
    unsigned int num_karts = race_manager->getNumberOfKarts();
    for (unsigned int kart_id = 0; kart_id < num_karts; kart_id++)
    {
        const AbstractKart *kart = World::getWorld()->getKart(kart_id);
        if (kart->getController()->isPlayerController())
            human_win = human_win && kart->getRaceResult();
    }

    m_finish_sound = SFXManager::get()->quickSound(
        human_win ? "race_finish_victory" : "race_finish");

    //std::string path = (human_win ? Different result music too later
    //    file_manager->getAsset(FileManager::MUSIC, "race_summary.music") :
    //    file_manager->getAsset(FileManager::MUSIC, "race_summary.music"));
    std::string path = file_manager->getAsset(FileManager::MUSIC, "race_summary.music");
    m_race_over_music = music_manager->getMusicInformation(path);

    if (!m_finish_sound)
    {
        // If there is no finish sound (because sfx are disabled), start
        // the race over music here (since the race over music is only started
        // when the finish sound has been played).
        music_manager->startMusic(m_race_over_music);
    }

    // Calculate how many track screenshots can fit into the "result-table" widget
    GUIEngine::Widget* result_table = getWidget("result-table");
    assert(result_table != NULL);
    m_sshot_height = (int)(UserConfigParams::m_height*0.1275);
    m_max_tracks = std::max(1, ((result_table->m_h - getFontHeight() * 5) /
        (m_sshot_height + SSHOT_SEPARATION))); //Show at least one

    // Calculate screenshot scrolling parameters
    const std::vector<std::string> tracks =
        race_manager->getGrandPrix().getTrackNames();
    int n_tracks = (int)tracks.size();
    int currentTrack = race_manager->getTrackNumber();
    m_start_track = currentTrack;
    if (n_tracks > m_max_tracks)
    {
        m_start_track = std::min(currentTrack, n_tracks - m_max_tracks);
        m_end_track = std::min(currentTrack + m_max_tracks, n_tracks);
    }
    else
    {
        m_start_track = 0;
        m_end_track = tracks.size();
    }
}   // init

//-----------------------------------------------------------------------------
void RaceResultGUI::tearDown()
{
    Screen::tearDown();
    m_font->setMonospaceDigits(m_was_monospace);

    if (m_finish_sound != NULL &&
        m_finish_sound->getStatus() == SFXBase::SFX_PLAYING)
    {
        m_finish_sound->stop();
    }
}   // tearDown

//-----------------------------------------------------------------------------
/** Makes the correct buttons visible again, and gives them the right label.
 *  1) If something was unlocked, only a 'next' button is displayed.
 */
void RaceResultGUI::enableAllButtons()
{
    GUIEngine::Widget *top = getWidget("top");
    GUIEngine::Widget *middle = getWidget("middle");
    GUIEngine::Widget *bottom = getWidget("bottom");

    if (race_manager->getMajorMode() == RaceManager::MAJOR_MODE_GRAND_PRIX)
    {
        enableGPProgress();
    }

    // If we're in a network world, change the buttons text
    if (World::getWorld()->isNetworkWorld())
    {
        Log::info("This work was networked", "This is a network world.");
        top->setVisible(false);
        middle->setText(_("Continue."));
        middle->setVisible(true);
        middle->setFocusForPlayer(PLAYER_ID_GAME_MASTER);
        bottom->setText(_("Quit the server."));
        bottom->setVisible(true);
        if (race_manager->getMajorMode() == RaceManager::MAJOR_MODE_GRAND_PRIX)
        {
            middle->setVisible(false); // you have to wait the server to start again
            bottom->setFocusForPlayer(PLAYER_ID_GAME_MASTER);
        }
        return;
    }
    Log::info("This work was NOT networked", "This is NOT a network world.");

    // If something was unlocked
    // -------------------------
    int n = (int)PlayerManager::getCurrentPlayer()->getRecentlyCompletedChallenges().size();
    if (n > 0)
    {
        top->setText(n == 1 ? _("You completed a challenge!")
            : _("You completed challenges!"));
        top->setVisible(true);
        top->setFocusForPlayer(PLAYER_ID_GAME_MASTER);
    }
    else if (race_manager->getMajorMode() == RaceManager::MAJOR_MODE_GRAND_PRIX)
    {
        // In case of a GP:
        // ----------------
        top->setVisible(false);

        middle->setText(_("Continue"));
        middle->setVisible(true);

        bottom->setText(_("Abort Grand Prix"));
        bottom->setVisible(true);

        middle->setFocusForPlayer(PLAYER_ID_GAME_MASTER);
    }
    else
    {
        // Normal race
        // -----------

        middle->setText(_("Restart"));
        middle->setVisible(true);

        if (race_manager->raceWasStartedFromOverworld())
        {
            top->setVisible(false);
            bottom->setText(_("Back to challenge selection"));
        }
        else
        {
            top->setText(_("Setup New Race"));
            top->setVisible(true);
            bottom->setText(_("Back to the menu"));
        }
        bottom->setVisible(true);

        bottom->setFocusForPlayer(PLAYER_ID_GAME_MASTER);
    }
}   // enableAllButtons

//-----------------------------------------------------------------------------
void RaceResultGUI::eventCallback(GUIEngine::Widget* widget,
    const std::string& name, const int playerID)
{
    int n_tracks = race_manager->getGrandPrix().getNumberOfTracks();
    if (name == "up_button" && n_tracks > m_max_tracks && m_start_track > 0)
    {
        m_start_track--;
        m_end_track--;
        displayScreenShots();
    }
    else if (name == "down_button" && n_tracks > m_max_tracks &&
        m_start_track < (n_tracks - m_max_tracks))
    {
        m_start_track++;
        m_end_track++;
        displayScreenShots();
    }

    // If something was unlocked, the 'continue' button was
    // actually used to display "Show unlocked feature(s)" text.
    // ---------------------------------------------------------
    int n = (int)PlayerManager::getCurrentPlayer()
        ->getRecentlyCompletedChallenges().size();
    if (n>0)
    {
        if (name == "top")
        {
            if (race_manager->getMajorMode() == RaceManager::MAJOR_MODE_GRAND_PRIX)
            {
                cleanupGPProgress();
            }

            std::vector<const ChallengeData*> unlocked =
                PlayerManager::getCurrentPlayer()->getRecentlyCompletedChallenges();

            bool gameCompleted = false;
            for (unsigned int n = 0; n < unlocked.size(); n++)
            {
                if (unlocked[n]->getId() == "fortmagma")
                {
                    gameCompleted = true;
                    break;
                }
            }

            PlayerManager::getCurrentPlayer()->clearUnlocked();

            if (gameCompleted)
            {
                // clear the race

                // kart will no longer be available during cutscene, drop reference
                StateManager::get()->getActivePlayer(playerID)->setKart(NULL);
                PropertyAnimator::get()->clear();
                World::deleteWorld();

                CutsceneWorld::setUseDuration(true);
                StateManager::get()->enterGameState();
                race_manager->setMinorMode(RaceManager::MINOR_MODE_CUTSCENE);
                race_manager->setNumKarts(0);
                race_manager->setNumPlayers(0);
                race_manager->startSingleRace("endcutscene", 999, false);

                std::vector<std::string> parts;
                parts.push_back("endcutscene");
                ((CutsceneWorld*)World::getWorld())->setParts(parts);
            }
            else
            {
                StateManager::get()->popMenu();
                PropertyAnimator::get()->clear();
                World::deleteWorld();

                CutsceneWorld::setUseDuration(false);
                StateManager::get()->enterGameState();
                race_manager->setMinorMode(RaceManager::MINOR_MODE_CUTSCENE);
                race_manager->setNumKarts(0);
                race_manager->setNumPlayers(0);
                race_manager->startSingleRace("featunlocked", 999, race_manager->raceWasStartedFromOverworld());

                FeatureUnlockedCutScene* scene =
                    FeatureUnlockedCutScene::getInstance();

                scene->addTrophy(race_manager->getDifficulty());
                scene->findWhatWasUnlocked(race_manager->getDifficulty());
                scene->push();
                race_manager->setAIKartOverride("");

                std::vector<std::string> parts;
                parts.push_back("featunlocked");
                ((CutsceneWorld*)World::getWorld())->setParts(parts);
            }
            return;
        }
        Log::fatal("RaceResultGUI", "Incorrect event '%s' when things are unlocked.",
            name.c_str());
    }

    // If we're playing online :
    if (World::getWorld()->isNetworkWorld())
    {
        StateManager::get()->popMenu();
        if (name == "middle") // Continue button (return to server lobby)
        {
            // Signal to the server that this client is back in the lobby now.
            Protocol* protocol =
                ProtocolManager::getInstance()->getProtocol(PROTOCOL_LOBBY_ROOM);
            ClientLobbyRoomProtocol* clrp =
                static_cast<ClientLobbyRoomProtocol*>(protocol);
            if(clrp)
                clrp->doneWithResults();
            backToLobby();
        }
        if (name == "bottom") // Quit server (return to main menu)
        {
            race_manager->exitRace();
            race_manager->setAIKartOverride("");
            StateManager::get()->resetAndGoToScreen(MainMenuScreen::getInstance());
        }
        return;
    }

    // Next check for GP
    // -----------------
    if (race_manager->getMajorMode() == RaceManager::MAJOR_MODE_GRAND_PRIX)
    {
        if (name == "middle")        // Next GP
        {
            cleanupGPProgress();
            StateManager::get()->popMenu();
            race_manager->next();
        }
        else if (name == "bottom")        // Abort
        {
            new MessageDialog(_("Do you really want to abort the Grand Prix?"),
                MessageDialog::MESSAGE_DIALOG_CONFIRM, this, false);
        }
        else if (!getWidget(name.c_str())->isVisible())
            Log::fatal("RaceResultGUI", "Incorrect event '%s' when things are unlocked.",
                name.c_str());
        return;
    }

    // This is a normal race, nothing was unlocked
    // -------------------------------------------
    StateManager::get()->popMenu();
    if (name == "top")                 // Setup new race
    {
        race_manager->exitRace();
        race_manager->setAIKartOverride("");
        // FIXME: why is this call necessary here? tearDown should be
        // automatically called when the screen is left. Note that the
        // NetworkKartSelectionScreen::getInstance()->tearDown(); caused #1347
        KartSelectionScreen::getRunningInstance()->tearDown();
        Screen* newStack[] = { MainMenuScreen::getInstance(),
                              RaceSetupScreen::getInstance(),
                              NULL };
        StateManager::get()->resetAndSetStack(newStack);
    }
    else if (name == "middle")        // Restart
    {
        race_manager->rerunRace();
    }
    else if (name == "bottom")        // Back to main
    {
        race_manager->exitRace();
        race_manager->setAIKartOverride("");
        // FIXME: why is this call necessary here? tearDown should be
        // automatically called when the screen is left. Note that the
        // NetworkKartSelectionScreen::getInstance()->tearDown(); caused #1347
        //if (KartSelectionScreen::getRunningInstance() != NULL)
        //    KartSelectionScreen::getRunningInstance()->tearDown();
        StateManager::get()->resetAndGoToScreen(MainMenuScreen::getInstance());

        if (race_manager->raceWasStartedFromOverworld())
        {
            OverWorld::enterOverWorld();
        }
    }
    else
        Log::fatal("RaceResultGUI", "Incorrect event '%s' for normal race.",
            name.c_str());
    return;
}   // eventCallback

//-----------------------------------------------------------------------------
/** Sets up the giu to go back to the lobby. Can only be called in case of a
 *  networked game.
 */
void RaceResultGUI::backToLobby()
{
    race_manager->exitRace();
    race_manager->setAIKartOverride("");
    Screen* newStack[] = { MainMenuScreen::getInstance(),
                           OnlineProfileServers::getInstance(),
                           ServerSelection::getInstance(),
                           NetworkingLobby::getInstance(),
                           NULL                                  };
    StateManager::get()->resetAndSetStack(newStack);
}   // backToLobby

//-----------------------------------------------------------------------------
    void RaceResultGUI::onConfirm()
    {
        //race_manager->saveGP(); // Save the aborted GP
        GUIEngine::ModalDialog::dismiss();
        cleanupGPProgress();
        StateManager::get()->popMenu();
        race_manager->exitRace();
        race_manager->setAIKartOverride("");
        StateManager::get()->resetAndGoToScreen(
            MainMenuScreen::getInstance());

        if (race_manager->raceWasStartedFromOverworld())
        {
            OverWorld::enterOverWorld();
        }
    }

    //-----------------------------------------------------------------------------
    /** This determines the layout, i.e. the size of all columns, font size etc.
     */
    void RaceResultGUI::determineTableLayout()
    {
        GUIEngine::Widget *table_area = getWidget("result-table");

        m_font = GUIEngine::getFont();
        assert(m_font);
        m_was_monospace = m_font->getMonospaceDigits();
        m_font->setMonospaceDigits(true);
        WorldWithRank *rank_world = (WorldWithRank*)World::getWorld();

        unsigned int first_position = 1;
        if (race_manager->getMinorMode() == RaceManager::MINOR_MODE_FOLLOW_LEADER)
            first_position = 2;

        // Use only the karts that are supposed to be displayed (and
        // ignore e.g. the leader in a FTL race).
        unsigned int num_karts = race_manager->getNumberOfKarts() - first_position + 1;

        // In FTL races the leader kart is not displayed
        m_all_row_infos.resize(num_karts);


        // Determine the kart to display in the right order,
        // and the maximum width for the kart name column
        // -------------------------------------------------
        m_width_kart_name = 0;
        float max_finish_time = 0;

        for (unsigned int position = first_position;
        position <= race_manager->getNumberOfKarts(); position++)
        {
            const AbstractKart *kart = rank_world->getKartAtPosition(position);

            // Save a pointer to the current row_info entry
            RowInfo *ri = &(m_all_row_infos[position - first_position]);
            ri->m_is_player_kart = kart->getController()->isLocalPlayerController();
            ri->m_kart_name = getKartDisplayName(kart);

            video::ITexture *icon =
                kart->getKartProperties()->getIconMaterial()->getTexture();
            ri->m_kart_icon = icon;

            // FTL karts will get a time assigned, they are not shown as eliminated
            if (kart->isEliminated() &&
                race_manager->getMinorMode() != RaceManager::MINOR_MODE_FOLLOW_LEADER)
            {
                ri->m_finish_time_string = core::stringw(_("Eliminated"));
            }
            else
            {
                const float time = kart->getFinishTime();
                if (time > max_finish_time) max_finish_time = time;
                std::string time_string = StringUtils::timeToString(time);
                ri->m_finish_time_string = time_string.c_str();
            }

            core::dimension2du rect =
                m_font->getDimension(ri->m_kart_name.c_str());
            if (rect.Width > m_width_kart_name)
                m_width_kart_name = rect.Width;
        }   // for position

        std::string max_time = StringUtils::timeToString(max_finish_time);
        core::stringw string_max_time(max_time.c_str());
        core::dimension2du r = m_font->getDimension(string_max_time.c_str());
        m_width_finish_time = r.Width;

        // Top pixel where to display text
        m_top = table_area->m_y;

        // Height of the result display
        unsigned int height = table_area->m_h;

        // Setup different timing information for the different phases
        // -----------------------------------------------------------
        // How much time between consecutive rows
        m_time_between_rows = 0.1f;

        // How long it takes for one line to scroll from right to left
        m_time_single_scroll = 0.2f;

        // Time to rotate the entries to the proper GP position.
        m_time_rotation = 1.0f;

        // The time the first phase is being displayed: add the start time
        // of the last kart to the duration of the scroll plus some time
        // of rest before the next phase starts
        m_time_overall_scroll = (num_karts - 1)*m_time_between_rows
            + m_time_single_scroll + 2.0f;

        // The time to increase the number of points.
        m_time_for_points = 1.0f;

        // Determine text height
        r = m_font->getDimension(L"Y");
        m_distance_between_rows = (int)(1.5f*r.Height);

        // If there are too many karts, reduce size between rows
        if (m_distance_between_rows * num_karts > height)
            m_distance_between_rows = height / num_karts;

        m_width_icon = table_area->m_h / 18;

        m_width_column_space = 10;

        // Determine width of new points column

        m_font->setMonospaceDigits(true);
        core::dimension2du r_new_p = m_font->getDimension(L"+99");

        m_width_new_points = r_new_p.Width;

        // Determine width of overall points column
        core::dimension2du r_all_p = m_font->getDimension(L"999");
        m_font->setMonospaceDigits(false);

        m_width_all_points = r_all_p.Width;

        m_table_width = m_width_icon + m_width_column_space
            + m_width_kart_name;

        if (race_manager->getMinorMode() != RaceManager::MINOR_MODE_FOLLOW_LEADER)
            m_table_width += m_width_finish_time + m_width_column_space;

        // Only in GP mode are the points displayed.
        if (race_manager->getMajorMode() == RaceManager::MAJOR_MODE_GRAND_PRIX)
            m_table_width += m_width_new_points + m_width_all_points
            + 2 * m_width_column_space;

        m_leftmost_column = table_area->m_x;
    }   // determineTableLayout

    //-----------------------------------------------------------------------------
    /** This function is called when one of the player presses 'fire'. The next
     *  phase of the animation will be displayed. E.g.
     *  in a GP: pressing fire while/after showing the latest race result will
     *           start the animation for the current GP result
     *  in a normal race: when pressing fire while an animation is played,
     *           start the menu showing 'rerun, new race, back to main' etc.
     */
    void RaceResultGUI::nextPhase()
    {
        // This will trigger the next phase in the next render call.
        m_timer = 9999;
    }   // nextPhase

    //-----------------------------------------------------------------------------
    /** If escape is pressed, don't do the default option (close the screen), but
     *  advance to the next animation phase.
     */
    bool RaceResultGUI::onEscapePressed()
    {
        nextPhase();
        return false;   // indicates 'do not close'
    }   // onEscapePressed

    //-----------------------------------------------------------------------------
    /** This is called before an event is sent to a widget. Since in this case
     *  no widget is active, the event would be lost, so we act on fire events
     *  here and trigger the next phase.
     */
    GUIEngine::EventPropagation RaceResultGUI::filterActions(PlayerAction action,
        int deviceID,
        const unsigned int value,
        Input::InputType type,
        int playerId)
    {
        if (action != PA_FIRE) return GUIEngine::EVENT_LET;

        // If the buttons are already visible, let the event go through since
        // it will be triggering eventCallback where this is handles.

        if (m_animation_state == RR_WAIT_TILL_END) return GUIEngine::EVENT_LET;

        nextPhase();
        return GUIEngine::EVENT_BLOCK;
    }   // filterActions

    //-----------------------------------------------------------------------------
    /** Called once a frame, this now triggers the rendering of the actual
     *  race result gui.
     */
    void RaceResultGUI::onUpdate(float dt)
    {
        renderGlobal(dt);

        // When the finish sound has been played, start the race over music.
        if (m_finish_sound && m_finish_sound->getStatus() != SFXBase::SFX_PLAYING)
        {
            try
            {
                // This call is done once each frame, but startMusic() is cheap
                // if the music is already playing.
                music_manager->startMusic(m_race_over_music);
            }
            catch (std::exception& e)
            {
                Log::error("RaceResultGUI", "Exception caught when "
                    "trying to load music: %s", e.what());
            }
        }
    }   // onUpdate

    //-----------------------------------------------------------------------------
    /** Render all global parts of the race gui, i.e. things that are only
     *  displayed once even in splitscreen.
     *  \param dt Timestep sized.
     */
    void RaceResultGUI::renderGlobal(float dt)
    {
        bool isSoccerWorld = race_manager->getMinorMode() == RaceManager::MINOR_MODE_SOCCER;

        m_timer += dt;
        assert(World::getWorld()->getPhase() == WorldStatus::RESULT_DISPLAY_PHASE);
        unsigned int num_karts = (unsigned int)m_all_row_infos.size();

        // First: Update the finite state machine
        // ======================================
        switch (m_animation_state)
        {
        case RR_INIT:
            for (unsigned int i = 0; i < num_karts; i++)
            {
                RowInfo *ri = &(m_all_row_infos[i]);
                ri->m_start_at = m_time_between_rows * i;
                ri->m_x_pos = (float)UserConfigParams::m_width;
                ri->m_y_pos = (float)(m_top + i*m_distance_between_rows);
            }
            m_animation_state = RR_RACE_RESULT;
            break;
        case RR_RACE_RESULT:
            if (m_timer > m_time_overall_scroll)
            {
                // Make sure that all lines are aligned to the left
                // (in case that the animation was skipped).
                for (unsigned int i = 0; i < num_karts; i++)
                {
                    RowInfo *ri = &(m_all_row_infos[i]);
                    ri->m_x_pos = (float)m_leftmost_column;
                }
                if (race_manager->getMajorMode() !=
                    RaceManager::MAJOR_MODE_GRAND_PRIX)
                {
                    m_animation_state = RR_WAIT_TILL_END;
                    enableAllButtons();
                    break;
                }

                determineGPLayout();
                m_animation_state = RR_OLD_GP_RESULTS;
                m_timer = 0;
            }
            break;
        case RR_OLD_GP_RESULTS:
            if (m_timer > m_time_overall_scroll)
            {
                m_animation_state = RR_INCREASE_POINTS;
                m_timer = 0;
                for (unsigned int i = 0; i < num_karts; i++)
                {
                    RowInfo *ri = &(m_all_row_infos[i]);
                    ri->m_x_pos = (float)m_leftmost_column;
                }
            }
            break;
        case RR_INCREASE_POINTS:
            // Have one second delay before the resorting starts.
            if (m_timer > 1 + m_time_for_points)
            {
                m_animation_state = RR_RESORT_TABLE;
                if (m_gp_position_was_changed)
                    m_timer = 0;
                else
                    // This causes the phase to go to RESORT_TABLE once, and then
                    // immediately wait till end. This has the advantage that any
                    // phase change settings will be processed properly.
                    m_timer = m_time_rotation + 1;
                // Make the new row permanent; necessary in case
                // that the animation is skipped.
                for (unsigned int i = 0; i < num_karts; i++)
                {
                    RowInfo *ri = &(m_all_row_infos[i]);
                    ri->m_new_points = 0;
                    ri->m_current_displayed_points =
                        (float)ri->m_new_overall_points;
                }

            }
            break;
        case RR_RESORT_TABLE:
            if (m_timer > m_time_rotation)
            {
                m_animation_state = RR_WAIT_TILL_END;
                // Make the new row permanent.
                for (unsigned int i = 0; i < num_karts; i++)
                {
                    RowInfo *ri = &(m_all_row_infos[i]);
                    ri->m_y_pos = ri->m_centre_point - ri->m_radius;
                }
                enableAllButtons();
            }
            break;
        case RR_WAIT_TILL_END:
            if (race_manager->getMajorMode() == RaceManager::MAJOR_MODE_GRAND_PRIX)
                displayGPProgress();
            if (m_timer - m_time_rotation > 1.0f &&
                dynamic_cast<DemoWorld*>(World::getWorld()))
            {
                race_manager->exitRace();
                StateManager::get()->resetAndGoToScreen(MainMenuScreen::getInstance());
            }
            break;
        }   // switch

        // Second phase: update X and Y positions for the various animations
        // =================================================================
        float v = 0.9f*UserConfigParams::m_width / m_time_single_scroll;
        if (!isSoccerWorld)
        {
            for (unsigned int i = 0; i < m_all_row_infos.size(); i++)
            {
                RowInfo *ri = &(m_all_row_infos[i]);
                float x = ri->m_x_pos;
                float y = ri->m_y_pos;
                switch (m_animation_state)
                {
                    // Both states use the same scrolling:
                case RR_INIT: break;   // Remove compiler warning
                case RR_RACE_RESULT:
                case RR_OLD_GP_RESULTS:
                    if (m_timer > ri->m_start_at)
                    {   // if active
                        ri->m_x_pos -= dt*v;
                        if (ri->m_x_pos < m_leftmost_column)
                            ri->m_x_pos = (float)m_leftmost_column;
                        x = ri->m_x_pos;
                    }
                    break;
                case RR_INCREASE_POINTS:
                {
                    WorldWithRank *wwr = dynamic_cast<WorldWithRank*>(World::getWorld());
                    assert(wwr);
                    int most_points;
                    if (race_manager->getMinorMode() == RaceManager::MINOR_MODE_FOLLOW_LEADER)
                        most_points = wwr->getScoreForPosition(2);
                    else
                        most_points = wwr->getScoreForPosition(1);
                    ri->m_current_displayed_points +=
                        dt*most_points / m_time_for_points;
                    if (ri->m_current_displayed_points > ri->m_new_overall_points)
                    {
                        ri->m_current_displayed_points =
                            (float)ri->m_new_overall_points;
                    }
                    ri->m_new_points -=
                        dt*most_points / m_time_for_points;
                    if (ri->m_new_points < 0)
                        ri->m_new_points = 0;
                    break;
                }
                case RR_RESORT_TABLE:
                    x = ri->m_x_pos
                        - ri->m_radius*sin(m_timer / m_time_rotation*M_PI);
                    y = ri->m_centre_point
                        + ri->m_radius*cos(m_timer / m_time_rotation*M_PI);
                    break;
                case RR_WAIT_TILL_END:
                    break;
                }   // switch
                displayOneEntry((unsigned int)x, (unsigned int)y, i, true);
            }   // for i
        }
        else
            displaySoccerResults();

        // Display highscores
        if (race_manager->getMajorMode() != RaceManager::MAJOR_MODE_GRAND_PRIX ||
            m_animation_state == RR_RACE_RESULT)
        {
            displayHighScores();
        }
    }   // renderGlobal

    //-----------------------------------------------------------------------------
    /** Determine the layout and fields for the GP table based on the previous
     *  GP results.
     */
    void RaceResultGUI::determineGPLayout()
    {
        unsigned int num_karts = race_manager->getNumberOfKarts();
        std::vector<int> old_rank(num_karts, 0);
        for (unsigned int kart_id = 0; kart_id < num_karts; kart_id++)
        {
            int rank = race_manager->getKartGPRank(kart_id);
            // In case of FTL mode: ignore the leader
            if (rank < 0) continue;
            old_rank[kart_id] = rank;
            const AbstractKart *kart = World::getWorld()->getKart(kart_id);
            RowInfo *ri = &(m_all_row_infos[rank]);
            ri->m_kart_icon =
                kart->getKartProperties()->getIconMaterial()->getTexture();
            ri->m_is_player_kart = kart->getController()->isLocalPlayerController();
            ri->m_kart_name = getKartDisplayName(kart);

            // In FTL karts do have a time, which is shown even when the kart
            // is eliminated
            if (kart->isEliminated() &&
                race_manager->getMinorMode() != RaceManager::MINOR_MODE_FOLLOW_LEADER)
            {
                ri->m_finish_time_string = core::stringw(_("Eliminated"));
            }
            else
            {
                float time = race_manager->getOverallTime(kart_id);
                ri->m_finish_time_string
                    = StringUtils::timeToString(time).c_str();
            }
            ri->m_start_at = m_time_between_rows * rank;
            ri->m_x_pos = (float)UserConfigParams::m_width;
            ri->m_y_pos = (float)(m_top + rank*m_distance_between_rows);
            int p = race_manager->getKartPrevScore(kart_id);
            ri->m_current_displayed_points = (float)p;
            if (kart->isEliminated() &&
                race_manager->getMinorMode() != RaceManager::MINOR_MODE_FOLLOW_LEADER)
            {
                ri->m_new_points = 0;
            }
            else
            {
                WorldWithRank *wwr = dynamic_cast<WorldWithRank*>(World::getWorld());
                assert(wwr);
                ri->m_new_points =
                    (float)wwr->getScoreForPosition(kart->getPosition());
            }
        }

        // Now update the GP ranks, and determine the new position
        // -------------------------------------------------------
        race_manager->computeGPRanks();
        m_gp_position_was_changed = false;
        for (unsigned int i = 0; i < num_karts; i++)
        {
            int j = old_rank[i];
            int gp_position = race_manager->getKartGPRank(i);
            m_gp_position_was_changed |= j != gp_position;
            RowInfo *ri = &(m_all_row_infos[j]);
            ri->m_radius = (j - gp_position)*(int)m_distance_between_rows*0.5f;
            ri->m_centre_point = m_top + (gp_position + j)*m_distance_between_rows*0.5f;
            int p = race_manager->getKartScore(i);
            ri->m_new_overall_points = p;
        }   // i < num_karts
    }   // determineGPLayout

    //-----------------------------------------------------------------------------
    /** Returns a string to display next to a kart. For a player that's the name
     *  of the player, for an AI kart it's the name of the driver. 
     */
    core::stringw RaceResultGUI::getKartDisplayName(const AbstractKart *kart) const
    {
        const EndController *ec = 
            dynamic_cast<const EndController*>(kart->getController());
        // If the race was given up, there is no end controller for the
        // players, so this case needs to be handled separately
        if(ec && ec->isLocalPlayerController())
            return ec->getName();
        else
        {
            // No end controller, check explicitely for a player controller
            const PlayerController *pc = 
                dynamic_cast<const PlayerController*>(kart->getController());
            // Check if the kart is a player controller to get the real name
            if(pc) return pc->getName();
        }
        return translations->fribidize(kart->getName());
    }   // getKartDisplayName

    //-----------------------------------------------------------------------------
    /** Displays the race results for a single kart.
     *  \param n Index of the kart to be displayed.
     *  \param display_points True if GP points should be displayed, too
     */
    void RaceResultGUI::displayOneEntry(unsigned int x, unsigned int y,
        unsigned int n, bool display_points)
    {
        RowInfo *ri = &(m_all_row_infos[n]);
        video::SColor color = ri->m_is_player_kart
            ? video::SColor(255, 255, 0, 0)
            : video::SColor(255, 255, 255, 255);

        unsigned int current_x = x;

        // First draw the icon
        // -------------------
        if (ri->m_kart_icon)
        {
            core::recti source_rect(core::vector2di(0, 0),
                ri->m_kart_icon->getSize());
            core::recti dest_rect(current_x, y,
                current_x + m_width_icon, y + m_width_icon);
            draw2DImage(ri->m_kart_icon, dest_rect,
                source_rect, NULL, NULL,
                true);
        }

        current_x += m_width_icon + m_width_column_space;

        // Draw the name
        // -------------

        core::recti pos_name(current_x, y,
            UserConfigParams::m_width, y + m_distance_between_rows);
        m_font->draw(ri->m_kart_name, pos_name, color, false, false, NULL,
            true /* ignoreRTL */);
        current_x += m_width_kart_name + m_width_column_space;


        core::recti dest_rect = core::recti(current_x, y, current_x + 100, y + 10);
        m_font->draw(ri->m_finish_time_string, dest_rect, color, false, false,
            NULL, true /* ignoreRTL */);
        current_x += m_width_finish_time + m_width_column_space;

        // Only display points in GP mode and when the GP results are displayed.
        // =====================================================================
        if (race_manager->getMajorMode() == RaceManager::MAJOR_MODE_GRAND_PRIX &&
            m_animation_state != RR_RACE_RESULT)
        {
            // Draw the new points
            // -------------------
            if (ri->m_new_points > 0)
            {
                core::recti dest_rect = core::recti(current_x, y,
                    current_x + 100, y + 10);
                core::stringw point_string = core::stringw("+")
                    + core::stringw((int)ri->m_new_points);
                // With mono-space digits space has the same width as each digit,
                // so we can simply fill up the string with spaces to get the
                // right aligned.
                while (point_string.size() < 3)
                    point_string = core::stringw(" ") + point_string;
                m_font->draw(point_string, dest_rect, color, false, false, NULL,
                    true /* ignoreRTL */);
            }
            current_x += m_width_new_points + m_width_column_space;

            // Draw the old_points plus increase value
            // ---------------------------------------
            core::recti dest_rect = core::recti(current_x, y, current_x + 100, y + 10);
            core::stringw point_inc_string =
                core::stringw((int)(ri->m_current_displayed_points));
            while (point_inc_string.size() < 3)
                point_inc_string = core::stringw(" ") + point_inc_string;
            m_font->draw(point_inc_string, dest_rect, color, false, false, NULL,
                true /* ignoreRTL */);
        }
    }   // displayOneEntry

    //-----------------------------------------------------------------------------
    void RaceResultGUI::displaySoccerResults()
    {

        //Draw win text
        core::stringw result_text;
        static video::SColor color = video::SColor(255, 255, 255, 255);
        gui::IGUIFont* font = GUIEngine::getTitleFont();
        int current_x = UserConfigParams::m_width / 2;
        RowInfo *ri = &(m_all_row_infos[0]);
        int current_y = (int)ri->m_y_pos;
        SoccerWorld* sw = (SoccerWorld*)World::getWorld();
        const int red_score = sw->getScore(SOCCER_TEAM_RED);
        const int blue_score = sw->getScore(SOCCER_TEAM_BLUE);

        GUIEngine::Widget *table_area = getWidget("result-table");
        int height = table_area->m_h + table_area->m_y;

        if (red_score > blue_score)
        {
            result_text = _("Red Team Wins");
        }
        else if (blue_score > red_score)
        {
            result_text = _("Blue Team Wins");
        }
        else
        {
            //Cannot really happen now. Only in time limited matches.
            result_text = _("It's a draw");
        }
        core::rect<s32> pos(current_x, current_y, current_x, current_y);
        font->draw(result_text.c_str(), pos, color, true, true);

        core::dimension2du rect = font->getDimension(result_text.c_str());

        //Draw team scores:
        current_y += rect.Height;
        current_x /= 2;
        irr::video::ITexture* red_icon = irr_driver->getTexture(FileManager::GUI,
            "soccer_ball_red.png");
        irr::video::ITexture* blue_icon = irr_driver->getTexture(FileManager::GUI,
            "soccer_ball_blue.png");

        core::recti source_rect(core::vector2di(0, 0), red_icon->getSize());
        core::recti dest_rect(current_x, current_y, current_x + red_icon->getSize().Width / 2,
            current_y + red_icon->getSize().Height / 2);
        draw2DImage(red_icon, dest_rect, source_rect,
            NULL, NULL, true);
        current_x += UserConfigParams::m_width / 2 - red_icon->getSize().Width / 2;
        dest_rect = core::recti(current_x, current_y, current_x + red_icon->getSize().Width / 2,
            current_y + red_icon->getSize().Height / 2);
        draw2DImage(blue_icon, dest_rect, source_rect,
            NULL, NULL, true);

        result_text = StringUtils::toWString(blue_score);
        rect = font->getDimension(result_text.c_str());
        current_x += red_icon->getSize().Width / 4;
        current_y += red_icon->getSize().Height / 2 + rect.Height / 4;
        pos = core::rect<s32>(current_x, current_y, current_x, current_y);
        font->draw(result_text.c_str(), pos, color, true, false);

        current_x -= UserConfigParams::m_width / 2 - red_icon->getSize().Width / 2;
        result_text = StringUtils::toWString(red_score);
        pos = core::rect<s32>(current_x, current_y, current_x, current_y);
        font->draw(result_text.c_str(), pos, color, true, false);

        int center_x = UserConfigParams::m_width / 2;
        pos = core::rect<s32>(center_x, current_y, center_x, current_y);
        font->draw("-", pos, color, true, false);

        //Draw goal scorers:
        //The red scorers:
        current_y += rect.Height / 2 + rect.Height / 4;
        font = GUIEngine::getSmallFont();
        std::vector<SoccerWorld::ScorerData> scorers = sw->getScorers(SOCCER_TEAM_RED);
        std::vector<float> score_times = sw->getScoreTimes(SOCCER_TEAM_RED);
        irr::video::ITexture* scorer_icon;

        int prev_y = current_y;
        for (unsigned int i = 0; i < scorers.size(); i++)
        {
            const bool own_goal = !(scorers.at(i).m_correct_goal);

            const int kart_id = scorers.at(i).m_id;
            const int rm_id = kart_id -
                (race_manager->getNumberOfKarts() - race_manager->getNumPlayers());

            if (rm_id >= 0)
                result_text = race_manager->getKartInfo(rm_id).getPlayerName();
            else
                result_text = sw->getKart(kart_id)->
                getKartProperties()->getName();

            if (own_goal)
            {
                result_text.append(" ");
                result_text.append(_("(Own Goal)"));
            }

            result_text.append("  ");
            result_text.append(StringUtils::timeToString(score_times.at(i)).c_str());
            rect = font->getDimension(result_text.c_str());

            if (height - prev_y < ((short)scorers.size() + 1)*(short)rect.Height)
                current_y += (height - prev_y) / ((short)scorers.size() + 1);
            else
                current_y += rect.Height;

            if (current_y > height) break;

            pos = core::rect<s32>(current_x, current_y, current_x, current_y);
            font->draw(result_text, pos, (own_goal ?
                video::SColor(255, 255, 0, 0) : color), true, false);
            scorer_icon = sw->getKart(scorers.at(i).m_id)
                ->getKartProperties()->getIconMaterial()->getTexture();
            source_rect = core::recti(core::vector2di(0, 0), scorer_icon->getSize());
            irr::u32 offset_x = (irr::u32)(font->getDimension(result_text.c_str()).Width / 1.5f);
            dest_rect = core::recti(current_x - offset_x - 30, current_y, current_x - offset_x, current_y + 30);
            draw2DImage(scorer_icon, dest_rect, source_rect,
                NULL, NULL, true);
        }

        //The blue scorers:
        current_y = prev_y;
        current_x += UserConfigParams::m_width / 2 - red_icon->getSize().Width / 2;
        scorers = sw->getScorers(SOCCER_TEAM_BLUE);
        score_times = sw->getScoreTimes(SOCCER_TEAM_BLUE);
        for (unsigned int i = 0; i < scorers.size(); i++)
        {
            const bool own_goal = !(scorers.at(i).m_correct_goal);

            const int kart_id = scorers.at(i).m_id;
            const int rm_id = kart_id -
                (race_manager->getNumberOfKarts() - race_manager->getNumPlayers());

            if (rm_id >= 0)
                result_text = race_manager->getKartInfo(rm_id).getPlayerName();
            else
                result_text = sw->getKart(kart_id)->
                getKartProperties()->getName();

            if (own_goal)
            {
                result_text.append(" ");
                result_text.append(_("(Own Goal)"));
            }

            result_text.append("  ");
            result_text.append(StringUtils::timeToString(score_times.at(i)).c_str());
            rect = font->getDimension(result_text.c_str());

            if (height - prev_y < ((short)scorers.size() + 1)*(short)rect.Height)
                current_y += (height - prev_y) / ((short)scorers.size() + 1);
            else
                current_y += rect.Height;

            if (current_y > height) break;

            pos = core::rect<s32>(current_x, current_y, current_x, current_y);
            font->draw(result_text, pos, (own_goal ?
                video::SColor(255, 255, 0, 0) : color), true, false);
            scorer_icon = sw->getKart(scorers.at(i).m_id)->
                getKartProperties()->getIconMaterial()->getTexture();
            source_rect = core::recti(core::vector2di(0, 0), scorer_icon->getSize());
            irr::u32 offset_x = (irr::u32)(font->getDimension(result_text.c_str()).Width / 1.5f);

            dest_rect = core::recti(current_x - offset_x - 30, current_y, current_x - offset_x, current_y + 30);
            draw2DImage(scorer_icon, dest_rect, source_rect,
                NULL, NULL, true);
        }
    }

    //-----------------------------------------------------------------------------

    void RaceResultGUI::clearHighscores()
    {
        m_highscore_rank = 0;
    }   // clearHighscores

    //-----------------------------------------------------------------------------

    void RaceResultGUI::setHighscore(int rank)
    {
        m_highscore_rank = rank;
    }   // setHighscore

    // ----------------------------------------------------------------------------
    void RaceResultGUI::enableGPProgress()
    {
        if (race_manager->getMajorMode() == RaceManager::MAJOR_MODE_GRAND_PRIX)
        {
            GUIEngine::Widget* result_table = getWidget("result-table");
            assert(result_table != NULL);

            int currentTrack = race_manager->getTrackNumber();
            int font_height = getFontHeight();
            int w = (int)(UserConfigParams::m_width*0.17);
            int x = (int)(result_table->m_x + result_table->m_w - w - 15);
            int y = (m_top + font_height + 5);

            //Current progress
            GUIEngine::LabelWidget* status_label = new GUIEngine::LabelWidget();
            status_label->m_properties[GUIEngine::PROP_ID] = "status_label";
            status_label->m_properties[GUIEngine::PROP_TEXT_ALIGN] = "center";
            status_label->m_x = x;
            status_label->m_y = y;
            status_label->m_w = w;
            status_label->m_h = font_height;
            status_label->add();
            status_label->setText(_("Track %i/%i", currentTrack + 1,
                race_manager->getGrandPrix().getNumberOfTracks()), true);
            addGPProgressWidget(status_label);
            y = (status_label->m_y + status_label->m_h + 5);

            //Scroll up button
            GUIEngine::IconButtonWidget* up_button = new GUIEngine::IconButtonWidget(
                GUIEngine::IconButtonWidget::SCALE_MODE_KEEP_CUSTOM_ASPECT_RATIO,
                false, false, GUIEngine::IconButtonWidget::ICON_PATH_TYPE_ABSOLUTE);
            up_button->m_properties[GUIEngine::PROP_ID] = "up_button";
            up_button->m_x = x;
            up_button->m_y = y;
            up_button->m_w = w;
            up_button->m_h = font_height;
            up_button->add();
            up_button->setImage(file_manager->getAsset(FileManager::GUI, "scroll_up.png"));
            addGPProgressWidget(up_button);
            y = (up_button->m_y + up_button->m_h + SSHOT_SEPARATION);

            //Track screenshots and labels
            int n_sshot = 1;
            for (int i = m_start_track; i < m_end_track; i++)
            {
                //Screenshot
                GUIEngine::IconButtonWidget* screenshot_widget =
                    new GUIEngine::IconButtonWidget(
                        GUIEngine::IconButtonWidget::
                        SCALE_MODE_KEEP_CUSTOM_ASPECT_RATIO,
                        false, false,
                        GUIEngine::IconButtonWidget::ICON_PATH_TYPE_ABSOLUTE);
                screenshot_widget->setCustomAspectRatio(4.0f / 3.0f);
                screenshot_widget->m_x = x;
                screenshot_widget->m_y = y;
                screenshot_widget->m_w = w;
                screenshot_widget->m_h = m_sshot_height;
                screenshot_widget->m_properties[GUIEngine::PROP_ID] =
                    ("sshot_" + StringUtils::toString(n_sshot));
                screenshot_widget->add();
                addGPProgressWidget(screenshot_widget);

                //Label
                GUIEngine::LabelWidget* sshot_label = new GUIEngine::LabelWidget();
                sshot_label->m_properties[GUIEngine::PROP_ID] =
                    ("sshot_label_" + StringUtils::toString(n_sshot));
                sshot_label->m_properties[GUIEngine::PROP_TEXT_ALIGN] = "left";
                sshot_label->m_x = (x + w + 5);
                sshot_label->m_y = (y + (m_sshot_height / 2) - (font_height / 2));
                sshot_label->m_w = (w / 2);
                sshot_label->m_h = font_height;
                sshot_label->add();
                addGPProgressWidget(sshot_label);

                y += (m_sshot_height + SSHOT_SEPARATION);
                n_sshot++;
            }   // for
            displayScreenShots();

            //Scroll down button
            GUIEngine::IconButtonWidget* down_button = new GUIEngine::IconButtonWidget(
                GUIEngine::IconButtonWidget::SCALE_MODE_KEEP_CUSTOM_ASPECT_RATIO,
                false, false, GUIEngine::IconButtonWidget::ICON_PATH_TYPE_ABSOLUTE);
            down_button->m_properties[GUIEngine::PROP_ID] = "down_button";
            down_button->m_x = x;
            down_button->m_y = y;
            down_button->m_w = w;
            down_button->m_h = font_height;
            down_button->add();
            down_button->setImage(file_manager->getAsset(FileManager::GUI, "scroll_down.png"));
            addGPProgressWidget(down_button);

        }   // if MAJOR_MODE_GRAND_PRIX)

    }   // enableGPProgress

    // ----------------------------------------------------------------------------
    void RaceResultGUI::addGPProgressWidget(GUIEngine::Widget* widget)
    {
        m_widgets.push_back(widget);
        m_gp_progress_widgets.push_back(widget);
    }

    // ----------------------------------------------------------------------------
    void RaceResultGUI::displayGPProgress()
    {
        core::stringw msg = _("Grand Prix progress:");

        GUIEngine::Widget* result_table = getWidget("result-table");
        assert(result_table != NULL);

        video::SColor color = video::SColor(255, 255, 0, 0);
        core::recti dest_rect(
            result_table->m_x + result_table->m_w - m_font->getDimension(msg.c_str()).Width - 5,
            m_top, 0, 0);

        m_font->draw(msg, dest_rect, color, false, false, NULL, true);
    }   // displayGPProgress

    // ----------------------------------------------------------------------------
    void RaceResultGUI::cleanupGPProgress()
    {
        for (size_t i = 0; i < m_gp_progress_widgets.size(); i++)
            m_widgets.remove(m_gp_progress_widgets.get(i));
        m_gp_progress_widgets.clearAndDeleteAll();
    }   // cleanupGPProgress

    // ----------------------------------------------------------------------------
    void RaceResultGUI::displayHighScores()
    {
        // This happens in demo world
        if (!World::getWorld())
            return;

        Highscores* scores = World::getWorld()->getHighscores();
        // In some case for exemple FTL they will be no highscores
        if (scores != NULL)
        {
            video::SColor white_color = video::SColor(255, 255, 255, 255);

            int x = (int)(UserConfigParams::m_width*0.65f);
            int y = m_top;

            // First draw title
            GUIEngine::getFont()->draw(_("Highscores"),
                core::recti(x, y, 0, 0),
                white_color,
                false, false, NULL, true /* ignoreRTL */);

            std::string kart_name;
            irr::core::stringw player_name;

            // prevent excessive long name
            unsigned int max_characters = 15;
            unsigned int max_width = (UserConfigParams::m_width / 2 - 200) / 10;
            if (max_width < 15)
                max_characters = max_width;

            float time;
            for (int i = 0; i < scores->getNumberEntries(); i++)
            {
                scores->getEntry(i, kart_name, player_name, &time);
                if (player_name.size() > max_characters)
                {
                    int begin = (int(m_timer / 0.4f)) % (player_name.size() - max_characters);
                    player_name = player_name.subString(begin, max_characters, false);
                }

                video::SColor text_color = white_color;
                if (m_highscore_rank - 1 == i)
                {
                    text_color = video::SColor(255, 255, 0, 0);
                }

                int current_x = x;
                int current_y = y + (int)((i + 1) * m_distance_between_rows * 1.5f);

                const KartProperties* prop = kart_properties_manager->getKart(kart_name);
                if (prop != NULL)
                {
                    const std::string &icon_path = prop->getAbsoluteIconFile();
                    video::ITexture* kart_icon_texture = irr_driver->getTexture(icon_path);

                    if (kart_icon_texture != NULL)
                    {
                        core::recti source_rect(core::vector2di(0, 0),
                            kart_icon_texture->getSize());

                        core::recti dest_rect(current_x, current_y,
                            current_x + m_width_icon, current_y + m_width_icon);

                        draw2DImage(
                            kart_icon_texture, dest_rect,
                            source_rect, NULL, NULL,
                            true);

                        current_x += m_width_icon + m_width_column_space;
                    }
                }

                // draw the player name
                GUIEngine::getSmallFont()->draw(player_name.c_str(),
                    core::recti(current_x, current_y, current_x + 150, current_y + 10),
                    text_color,
                    false, false, NULL, true /* ignoreRTL */);

                current_x = (int)(UserConfigParams::m_width * 0.85f);

                // Finally draw the time
                std::string time_string = StringUtils::timeToString(time);
                GUIEngine::getSmallFont()->draw(time_string.c_str(),
                    core::recti(current_x, current_y, current_x + 100, current_y + 10),
                    text_color,
                    false, false, NULL, true /* ignoreRTL */);
            }
        }
    }

    // ----------------------------------------------------------------------------
    void RaceResultGUI::displayScreenShots()
    {
        const std::vector<std::string> tracks =
            race_manager->getGrandPrix().getTrackNames();
        int currentTrack = race_manager->getTrackNumber();

        int n_sshot = 1;
        for (int i = m_start_track; i < m_end_track; i++)
        {
            Track* track = track_manager->getTrack(tracks[i]);
            GUIEngine::IconButtonWidget* sshot = getWidget<GUIEngine::IconButtonWidget>(
                ("sshot_" + StringUtils::toString(n_sshot)).c_str());
            GUIEngine::LabelWidget* label = getWidget<GUIEngine::LabelWidget>(
                ("sshot_label_" + StringUtils::toString(n_sshot)).c_str());
            assert(track != NULL && sshot != NULL && label != NULL);

            sshot->setImage(track->getScreenshotFile());
            if (i <= currentTrack)
                sshot->setBadge(GUIEngine::OK_BADGE);
            else
                sshot->resetAllBadges();

            label->setText(StringUtils::toWString(i + 1), true);

            n_sshot++;
        }
    }

    // ----------------------------------------------------------------------------
    int RaceResultGUI::getFontHeight() const
    {
        assert(m_font != NULL);
        return m_font->getDimension(L"A").Height; //Could be any capital letter
    }
