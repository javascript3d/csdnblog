//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2014-2015 SuperTuxKart-Team
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

#ifndef STKANIMATEDMESH_HPP
#define STKANIMATEDMESH_HPP

#include "graphics/stk_mesh.hpp"
#include "utils/ptr_vector.hpp"

#include "../lib/irrlicht/source/Irrlicht/CAnimatedMeshSceneNode.h"
#include <IAnimatedMesh.h>
#include <irrTypes.h>

class STKAnimatedMesh : public irr::scene::CAnimatedMeshSceneNode, public STKMeshCommon
{
protected:
    bool isMaterialInitialized;
    bool isGLInitialized;
    std::vector<GLMesh> GLmeshes;
    core::matrix4 ModelViewProjectionMatrix;
    void cleanGLMeshes();
public:
    virtual void updateNoGL();
    virtual void updateGL();
  STKAnimatedMesh(irr::scene::IAnimatedMesh* mesh, irr::scene::ISceneNode* parent,
     irr::scene::ISceneManager* mgr, irr::s32 id, const std::string& debug_name,
     const irr::core::vector3df& position = irr::core::vector3df(0,0,0),
     const irr::core::vector3df& rotation = irr::core::vector3df(0,0,0),
     const irr::core::vector3df& scale = irr::core::vector3df(1.0f, 1.0f, 1.0f));
  ~STKAnimatedMesh();

  virtual void render();
  virtual void setMesh(irr::scene::IAnimatedMesh* mesh);
  virtual bool glow() const { return false; }
};

#endif // STKANIMATEDMESH_HPP
