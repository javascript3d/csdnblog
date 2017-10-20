//
//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2016 SuperTuxKart-Team
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

#ifndef HEADER_ARENA_AI_HPP
#define HEADER_ARENA_AI_HPP

#include "karts/controller/ai_base_controller.hpp"
#include "race/race_manager.hpp"
#include "utils/random_generator.hpp"

#undef AI_DEBUG
#ifdef AI_DEBUG
#include "graphics/irr_driver.hpp"
#endif

class Vec3;

namespace irr
{
    namespace scene { class ISceneNode; }
    namespace video { class ITexture; }
}

/** A base class for AI that use navmesh to work.
 * \ingroup controller
 */
class ArenaAI : public AIBaseController
{
protected:
    /** Pointer to the closest kart around this kart. */
    AbstractKart *m_closest_kart;

    int m_closest_kart_node;
    Vec3 m_closest_kart_point;

    posData m_closest_kart_pos_data;

    /** Holds the current difficulty. */
    RaceManager::Difficulty m_cur_difficulty;

    /** For debugging purpose: a sphere indicating where the AI
     *  is targeting at. */
    irr::scene::ISceneNode *m_debug_sphere;
    irr::scene::ISceneNode *m_debug_sphere_next;

    /** The node(poly) at which the target point lies in. */
    int m_target_node;

    /** The target point. */
    Vec3 m_target_point;

    bool m_avoiding_banana;

    void  collectItemInArena(Vec3*, int*) const;
    float findAngleFrom3Edges(float a, float b, float c);
private:
    /** Used by handleArenaUTurn, it tells whether to do left or right
     *  turning when steering is overridden. */
    bool m_adjusting_side;

    posData m_cur_kart_pos_data;

   /** Indicates that the kart is currently stuck, and m_time_since_reversing is
     * counting down. */
    bool m_is_stuck;

    /** Indicates that the kart need a uturn to reach a node behind, and
     *  m_time_since_uturn is counting down. */
    bool m_is_uturn;

    /** Holds the unique node ai has driven through, useful to tell if AI is
     *  stuck by determine the size of this set. */
    std::set<int> m_on_node;

    /** Time an item has been collected and not used. */
    float m_time_since_last_shot;

    /** This is a timer that counts down when the kart is reversing to get unstuck. */
    float m_time_since_reversing;

    /** This is a timer that counts down when the kart is starting to drive. */
    float m_time_since_driving;

    /** This is a timer that counts down when the kart is doing u-turn. */
    float m_time_since_uturn;

    float m_turn_radius;
    float m_turn_angle;

    Vec3 m_current_forward_point;
    int m_current_forward_node;

    std::set<int> m_aiming_nodes;
    std::vector<Vec3> m_aiming_points;

    void         checkIfStuck(const float dt);
    void         handleArenaAcceleration(const float dt);
    void         handleArenaBraking();
    void         handleArenaItems(const float dt);
    void         handleArenaSteering(const float dt);
    void         handleArenaUTurn(const float dt);
    bool         handleArenaUnstuck(const float dt);
    bool         updateAimingPosition();
    void         updateBananaLocation();
    void         updateTurnRadius(const Vec3& p1, const Vec3& p2,
                                  const Vec3& p3);
    virtual int  getCurrentNode() const = 0;
    virtual bool isWaiting() const = 0;
    virtual void resetAfterStop() {};
    virtual void findClosestKart(bool use_difficulty) = 0;
    virtual void findTarget() = 0;
    virtual bool forceBraking() { return m_avoiding_banana; }
    virtual bool ignorePathFinding() { return false; }
public:
    static int   m_test_node_for_banana;
                 ArenaAI(AbstractKart *kart);
    virtual     ~ArenaAI() {};
    virtual void update      (float delta);
    virtual void reset       ();
    virtual void newLap(int lap) {};
};

#endif
