//
//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2004-2005 Steve Baker <sjbaker1@airmail.net>
//  Copyright (C) 2006-2007 Eduardo Hernandez Munoz
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

#ifndef HEADER_BATTLE_AI_HPP
#define HEADER_BATTLE_AI_HPP

#include "karts/controller/arena_ai.hpp"

class ThreeStrikesBattle;
class Vec3;
class Item;

/** The actual battle AI.
 * \ingroup controller
 */
class BattleAI : public ArenaAI
{
private:
    /** Keep a pointer to world. */
    ThreeStrikesBattle *m_world;

    bool m_mini_skid;

    virtual void findClosestKart(bool use_difficulty);
    virtual void findTarget();
    virtual int  getCurrentNode() const;
    virtual bool isWaiting() const;
    virtual bool canSkid(float steer_fraction) { return m_mini_skid; }
public:
                 BattleAI(AbstractKart *kart);
                ~BattleAI();
    virtual void update      (float delta);
    virtual void reset       ();
};

#endif
