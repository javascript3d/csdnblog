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

#include "graphics/irr_driver.hpp"

#include "config/user_config.hpp"
#include "graphics/callbacks.hpp"
#include "graphics/central_settings.hpp"
#include "graphics/glwrap.hpp"
#include "graphics/post_processing.hpp"
#include "graphics/rtts.hpp"
#include "graphics/shaders.hpp"
#include "graphics/shadow_matrices.hpp"
#include "graphics/stk_scene_manager.hpp"
#include "modes/world.hpp"
#include "utils/log.hpp"
#include "utils/profiler.hpp"
#include "utils/tuple.hpp"

#include <S3DVertex.h>

/**
\page render_geometry Geometry Rendering Overview

\section adding_material Adding a solid material

You need to consider twice before adding a new material : in the worst case a material requires 8 shaders :
one for each solid pass, one for shadow pass, one for RSM pass, and you need to double that for instanced version.

You need to declare a new enum in MeshMaterial and to write the corresponding dispatch code in getMeshMaterialFromType
and to create 2 new List* structures (one for standard and one for instanced version).

Then you need to write the code in stkscenemanager.cpp that will add any mesh with the new material to their corresponding
lists : in handleSTKCommon for the standard version and in the body of PrepareDrawCalls for instanced version.

\section vertex_layout Available Vertex Layout

There are 3 different layout that comes from Irrlicht loading routines :
EVT_STANDARD, EVT_2TCOORDS, EVT_TANGENT.

Below are the attributes for each vertex layout and their predefined location.

\subsection EVT_STANDARD
layout(location = 0) in vec3 Position;
layout(location = 1) in vec3 Normal;
layout(location = 2) in vec4 Color;
layout(location = 3) in vec2 Texcoord;

\subsection EVT_2TCOORDS
layout(location = 0) in vec3 Position;
layout(location = 1) in vec3 Normal;
layout(location = 2) in vec4 Color;
layout(location = 3) in vec2 Texcoord;
layout(location = 4) in vec2 SecondTexcoord;

\subsection EVT_TANGENT
layout(location = 0) in vec3 Position;
layout(location = 1) in vec3 Normal;
layout(location = 2) in vec4 Color;
layout(location = 3) in vec2 Texcoord;
layout(location = 5) in vec3 Tangent;
layout(location = 6) in vec3 Bitangent;

*/

// ============================================================================
class InstancedObjectPass1Shader : public TextureShader<InstancedObjectPass1Shader, 1>
{
public:
    InstancedObjectPass1Shader()
    {
        loadProgram(OBJECT, GL_VERTEX_SHADER, "utils/getworldmatrix.vert",
                            GL_VERTEX_SHADER, "instanced_object_pass.vert",
                            GL_FRAGMENT_SHADER, "utils/encode_normal.frag",
                            GL_FRAGMENT_SHADER, "instanced_object_pass1.frag");

        assignUniforms();
        assignSamplerNames(0, "glosstex", ST_TRILINEAR_ANISOTROPIC_FILTERED);
    }   // InstancedObjectPass1Shader
};   // class InstancedObjectPass1Shader

// ============================================================================
class InstancedObjectRefPass1Shader : public TextureShader<InstancedObjectRefPass1Shader, 2>
{
public:
    InstancedObjectRefPass1Shader()
    {
        loadProgram(OBJECT, GL_VERTEX_SHADER, "utils/getworldmatrix.vert",
                            GL_VERTEX_SHADER, "instanced_object_pass.vert",
                            GL_FRAGMENT_SHADER, "utils/encode_normal.frag",
                            GL_FRAGMENT_SHADER, "instanced_objectref_pass1.frag");

        assignUniforms();
        assignSamplerNames(0, "tex", ST_TRILINEAR_ANISOTROPIC_FILTERED,
                           1, "glosstex", ST_TRILINEAR_ANISOTROPIC_FILTERED);
    }

};   // InstancedObjectRefPass1Shader

// ============================================================================
class ObjectRefPass2Shader : public TextureShader<ObjectRefPass2Shader, 5,
                                                core::matrix4, core::matrix4>
{
public:
    ObjectRefPass2Shader()
    {
        loadProgram(OBJECT, GL_VERTEX_SHADER, "object_pass.vert",
                            GL_FRAGMENT_SHADER, "utils/getLightFactor.frag",
                            GL_FRAGMENT_SHADER, "objectref_pass2.frag");
        assignUniforms("ModelMatrix", "TextureMatrix");
        assignSamplerNames(0, "DiffuseMap", ST_NEAREST_FILTERED,
                           1, "SpecularMap", ST_NEAREST_FILTERED,
                           2, "SSAO", ST_BILINEAR_FILTERED,
                           3, "Albedo", ST_TRILINEAR_ANISOTROPIC_FILTERED,
                           4, "SpecMap", ST_TRILINEAR_ANISOTROPIC_FILTERED);
    }   // ObjectRefPass2Shader
};   // ObjectRefPass2Shader

// ============================================================================
class InstancedObjectPass2Shader : public TextureShader<InstancedObjectPass2Shader, 5>
{
public:
    InstancedObjectPass2Shader()
    {
        loadProgram(OBJECT, GL_VERTEX_SHADER, "utils/getworldmatrix.vert",
                            GL_VERTEX_SHADER, "instanced_object_pass.vert",
                            GL_FRAGMENT_SHADER, "utils/getLightFactor.frag",
                            GL_FRAGMENT_SHADER, "instanced_object_pass2.frag");
        assignUniforms();
        assignSamplerNames(0, "DiffuseMap", ST_NEAREST_FILTERED,
                           1, "SpecularMap", ST_NEAREST_FILTERED,
                           2, "SSAO", ST_BILINEAR_FILTERED,
                           3, "Albedo", ST_TRILINEAR_ANISOTROPIC_FILTERED,
                           4, "SpecMap", ST_TRILINEAR_ANISOTROPIC_FILTERED);
    }   // InstancedObjectPass2Shader
};   // InstancedObjectPass2Shader

// ============================================================================
class InstancedObjectRefPass2Shader : public TextureShader<InstancedObjectRefPass2Shader, 5>
{
public:
    InstancedObjectRefPass2Shader()
    {
        loadProgram(OBJECT, GL_VERTEX_SHADER, "utils/getworldmatrix.vert",
                            GL_VERTEX_SHADER, "instanced_object_pass.vert",
                            GL_FRAGMENT_SHADER, "utils/getLightFactor.frag",
                            GL_FRAGMENT_SHADER, "instanced_objectref_pass2.frag");
        assignUniforms();
        assignSamplerNames(0, "DiffuseMap", ST_NEAREST_FILTERED,
                           1, "SpecularMap", ST_NEAREST_FILTERED,
                           2, "SSAO", ST_BILINEAR_FILTERED,
                           3, "Albedo", ST_TRILINEAR_ANISOTROPIC_FILTERED,
                           4, "SpecMap", ST_TRILINEAR_ANISOTROPIC_FILTERED);
    }    // InstancedObjectRefPass2Shader
};   // InstancedObjectRefPass2Shader

// ============================================================================
class ShadowShader : public TextureShader<ShadowShader, 0, int, core::matrix4>
{
public:
    ShadowShader()
    {
        // Geometry shader needed
        if (CVS->getGLSLVersion() < 150)
            return;
        if (CVS->isAMDVertexShaderLayerUsable())
        {
            loadProgram(OBJECT, GL_VERTEX_SHADER, "shadow.vert",
                                GL_FRAGMENT_SHADER, "shadow.frag");
        }
        else
        {
            loadProgram(OBJECT, GL_VERTEX_SHADER, "shadow.vert",
                                GL_GEOMETRY_SHADER, "shadow.geom",
                                GL_FRAGMENT_SHADER, "shadow.frag");
        }
        assignUniforms("layer", "ModelMatrix");
    }   // ShadowShader
};   // ShadowShader

// ============================================================================
class InstancedShadowShader : public TextureShader<InstancedShadowShader, 0, int>
{
public:
    InstancedShadowShader()
    {
        // Geometry shader needed
        if (CVS->getGLSLVersion() < 150)
            return;
        if (CVS->isAMDVertexShaderLayerUsable())
        {
            loadProgram(OBJECT, GL_VERTEX_SHADER, "utils/getworldmatrix.vert",
                                GL_VERTEX_SHADER, "instanciedshadow.vert",
                                GL_FRAGMENT_SHADER, "shadow.frag");
        }
        else
        {
            loadProgram(OBJECT, GL_VERTEX_SHADER, "utils/getworldmatrix.vert",
                                GL_VERTEX_SHADER, "instanciedshadow.vert",
                                GL_GEOMETRY_SHADER, "instanced_shadow.geom",
                                GL_FRAGMENT_SHADER, "shadow.frag");
        }
        assignUniforms("layer");
    }   // InstancedShadowShader

};   // InstancedShadowShader

// ============================================================================
class CRSMShader : public TextureShader<CRSMShader, 1, core::matrix4, core::matrix4, 
                                 core::matrix4>
{
public:
    CRSMShader()
    {
        loadProgram(OBJECT, GL_VERTEX_SHADER, "rsm.vert",
                            GL_FRAGMENT_SHADER, "rsm.frag");

        assignUniforms("RSMMatrix", "ModelMatrix", "TextureMatrix");
        assignSamplerNames(0, "tex", ST_TRILINEAR_ANISOTROPIC_FILTERED);
    }   // CRSMShader
};   // CRSMShader


// ============================================================================
class SplattingRSMShader : public TextureShader<SplattingRSMShader, 5, core::matrix4,
                                  core::matrix4>
{
public:
    SplattingRSMShader()
    {
        loadProgram(OBJECT, GL_VERTEX_SHADER, "rsm.vert",
                            GL_FRAGMENT_SHADER, "splatting_rsm.frag");

        assignUniforms("RSMMatrix", "ModelMatrix");
        assignSamplerNames(0, "tex_layout", ST_TRILINEAR_ANISOTROPIC_FILTERED,
                           1, "tex_detail0", ST_TRILINEAR_ANISOTROPIC_FILTERED,
                           2, "tex_detail1", ST_TRILINEAR_ANISOTROPIC_FILTERED,
                           3, "tex_detail2", ST_TRILINEAR_ANISOTROPIC_FILTERED,
                           4, "tex_detail3", ST_TRILINEAR_ANISOTROPIC_FILTERED);
    }   // SplattingRSMShader

};  // SplattingRSMShader

// ============================================================================
class CInstancedRSMShader : public TextureShader<CInstancedRSMShader, 1, core::matrix4>
{
public:
    CInstancedRSMShader()
    {
        loadProgram(OBJECT, GL_VERTEX_SHADER, "utils/getworldmatrix.vert",
                            GL_VERTEX_SHADER, "instanced_rsm.vert",
                            GL_FRAGMENT_SHADER, "instanced_rsm.frag");

        assignUniforms("RSMMatrix");
        assignSamplerNames(0, "tex", ST_TRILINEAR_ANISOTROPIC_FILTERED);
    }   // CInstancedRSMShader
};   // CInstancedRSMShader

// ============================================================================
class SphereMapShader : public TextureShader<SphereMapShader, 4, core::matrix4,
                                      core::matrix4>
{
public:
    SphereMapShader()
    {
        loadProgram(OBJECT, GL_VERTEX_SHADER, "object_pass.vert",
                            GL_FRAGMENT_SHADER, "utils/getLightFactor.frag",
                            GL_FRAGMENT_SHADER, "utils/getPosFromUVDepth.frag",
                            GL_FRAGMENT_SHADER, "objectpass_spheremap.frag");
        assignUniforms("ModelMatrix", "InverseModelMatrix");
        assignSamplerNames(0, "DiffuseMap", ST_NEAREST_FILTERED,
                           1, "SpecularMap", ST_NEAREST_FILTERED,
                           2, "SSAO", ST_BILINEAR_FILTERED,
                           3, "tex", ST_TRILINEAR_ANISOTROPIC_FILTERED);
    }   // SphereMapShader
};   // SphereMapShader

// ============================================================================
class InstancedSphereMapShader : public TextureShader<InstancedSphereMapShader, 4>
{
public:
    InstancedSphereMapShader()
    {
        loadProgram(OBJECT, GL_VERTEX_SHADER, "utils/getworldmatrix.vert",
                    GL_VERTEX_SHADER, "instanced_object_pass.vert",
                    GL_FRAGMENT_SHADER, "utils/getLightFactor.frag",
                    GL_FRAGMENT_SHADER, "utils/getPosFromUVDepth.frag",
                    GL_FRAGMENT_SHADER, "instanced_objectpass_spheremap.frag");
        assignUniforms();
        assignSamplerNames(0, "DiffuseMap", ST_NEAREST_FILTERED,
                           1, "SpecularMap", ST_NEAREST_FILTERED,
                           2, "SSAO", ST_BILINEAR_FILTERED,
                           3, "tex", ST_TRILINEAR_ANISOTROPIC_FILTERED);
    }   // InstancedSphereMapShader
};   // InstancedSphereMapShader

// ============================================================================
class SplattingShader : public TextureShader<SplattingShader, 8, core::matrix4>
{
public:
    SplattingShader()
    {
        loadProgram(OBJECT, GL_VERTEX_SHADER, "object_pass.vert",
                            GL_FRAGMENT_SHADER, "utils/getLightFactor.frag",
                            GL_FRAGMENT_SHADER, "splatting.frag");
        assignUniforms("ModelMatrix");

        assignSamplerNames(0, "DiffuseMap", ST_NEAREST_FILTERED,
                           1, "SpecularMap", ST_NEAREST_FILTERED,
                           2, "SSAO", ST_BILINEAR_FILTERED,
                           3, "tex_layout", ST_TRILINEAR_ANISOTROPIC_FILTERED,
                           4, "tex_detail0", ST_TRILINEAR_ANISOTROPIC_FILTERED,
                           5, "tex_detail1", ST_TRILINEAR_ANISOTROPIC_FILTERED,
                           6, "tex_detail2", ST_TRILINEAR_ANISOTROPIC_FILTERED,
                           7, "tex_detail3", ST_TRILINEAR_ANISOTROPIC_FILTERED);
    }   // SplattingShader
};   // SplattingShader

// ============================================================================
class ObjectRefPass1Shader : public TextureShader<ObjectRefPass1Shader, 2, core::matrix4,
                                           core::matrix4, core::matrix4>
{
public:
    ObjectRefPass1Shader()
    {
        loadProgram(OBJECT, GL_VERTEX_SHADER, "object_pass.vert",
                            GL_FRAGMENT_SHADER, "utils/encode_normal.frag",
                            GL_FRAGMENT_SHADER, "objectref_pass1.frag");
        assignUniforms("ModelMatrix", "InverseModelMatrix", "TextureMatrix");
        assignSamplerNames(0, "tex", ST_TRILINEAR_ANISOTROPIC_FILTERED,
                           1, "glosstex", ST_TRILINEAR_ANISOTROPIC_FILTERED);
    }   // ObjectRefPass1Shader
};  // ObjectRefPass1Shader


// ============================================================================
class NormalMapShader : public TextureShader<NormalMapShader, 2, core::matrix4, 
                                      core::matrix4>
{
public:
    NormalMapShader()
    {
        loadProgram(OBJECT, GL_VERTEX_SHADER, "object_pass.vert",
                            GL_FRAGMENT_SHADER, "utils/encode_normal.frag",
                            GL_FRAGMENT_SHADER, "normalmap.frag");
        assignUniforms("ModelMatrix", "InverseModelMatrix");
        assignSamplerNames(1, "normalMap", ST_TRILINEAR_ANISOTROPIC_FILTERED,
                           0, "DiffuseForAlpha", ST_TRILINEAR_ANISOTROPIC_FILTERED);
    }   // NormalMapShader

};   // NormalMapShader

// ============================================================================
class InstancedNormalMapShader : public TextureShader<InstancedNormalMapShader, 2>
{
public:
    InstancedNormalMapShader()
    {
        loadProgram(OBJECT, GL_VERTEX_SHADER, "utils/getworldmatrix.vert",
                            GL_VERTEX_SHADER, "instanced_object_pass.vert",
                            GL_FRAGMENT_SHADER, "utils/encode_normal.frag",
                            GL_FRAGMENT_SHADER, "instanced_normalmap.frag");
        assignUniforms();
        assignSamplerNames(0, "normalMap", ST_TRILINEAR_ANISOTROPIC_FILTERED,
                           1, "glossMap", ST_TRILINEAR_ANISOTROPIC_FILTERED);
    }   // InstancedNormalMapShader
};   // InstancedNormalMapShader

// ============================================================================
class ObjectUnlitShader : public TextureShader<ObjectUnlitShader, 4, core::matrix4, 
                                        core::matrix4>
{
public:
    ObjectUnlitShader()
    {
        loadProgram(OBJECT, GL_VERTEX_SHADER, "object_pass.vert",
                            GL_FRAGMENT_SHADER, "object_unlit.frag");
        assignUniforms("ModelMatrix", "TextureMatrix");
        assignSamplerNames(0, "DiffuseMap", ST_NEAREST_FILTERED,
                           1, "SpecularMap", ST_NEAREST_FILTERED,
                           2, "SSAO", ST_BILINEAR_FILTERED,
                           3, "tex", ST_TRILINEAR_ANISOTROPIC_FILTERED);
    }   // ObjectUnlitShader
};   // ObjectUnlitShader

// ============================================================================
class InstancedObjectUnlitShader : public TextureShader<InstancedObjectUnlitShader, 4>
{
public:
    InstancedObjectUnlitShader()
    {
        loadProgram(OBJECT, GL_VERTEX_SHADER, "utils/getworldmatrix.vert",
                            GL_VERTEX_SHADER, "instanced_object_pass.vert",
                            GL_FRAGMENT_SHADER, "instanced_object_unlit.frag");
        assignUniforms();
        assignSamplerNames(0, "DiffuseMap", ST_NEAREST_FILTERED,
                           1, "SpecularMap", ST_NEAREST_FILTERED,
                           2, "SSAO", ST_BILINEAR_FILTERED,
                           3, "tex", ST_TRILINEAR_ANISOTROPIC_FILTERED);
    }   // InstancedObjectUnlitShader
};   // InstancedObjectUnlitShader

// ============================================================================
class RefShadowShader : public TextureShader<RefShadowShader, 1, 
                                             int, core::matrix4>
{
public:
    RefShadowShader()
    {
        // Geometry shader needed
        if (CVS->getGLSLVersion() < 150)
            return;
        if (CVS->isAMDVertexShaderLayerUsable())
        {
            loadProgram(OBJECT, GL_VERTEX_SHADER, "shadow.vert",
                                GL_FRAGMENT_SHADER, "shadowref.frag");
        }
        else
        {
            loadProgram(OBJECT, GL_VERTEX_SHADER, "shadow.vert",
                                GL_GEOMETRY_SHADER, "shadow.geom",
                                GL_FRAGMENT_SHADER, "shadowref.frag");
        }
        assignUniforms("layer", "ModelMatrix");
        assignSamplerNames(0, "tex", ST_TRILINEAR_ANISOTROPIC_FILTERED);
    }   // RefShadowShader
};   // RefShadowShader

// ============================================================================
class InstancedRefShadowShader : public TextureShader<InstancedRefShadowShader,
                                                      1, int>
{
public:
    InstancedRefShadowShader()
    {
        // Geometry shader needed
        if (CVS->getGLSLVersion() < 150)
            return;
        if (CVS->isAMDVertexShaderLayerUsable())
        {
            loadProgram(OBJECT,GL_VERTEX_SHADER, "utils/getworldmatrix.vert",
                               GL_VERTEX_SHADER, "instanciedshadow.vert",
                               GL_FRAGMENT_SHADER, "instanced_shadowref.frag");
        }
        else
        {
            loadProgram(OBJECT,GL_VERTEX_SHADER, "utils/getworldmatrix.vert",
                               GL_VERTEX_SHADER, "instanciedshadow.vert",
                               GL_GEOMETRY_SHADER, "instanced_shadow.geom",
                               GL_FRAGMENT_SHADER, "instanced_shadowref.frag");
        }
        assignUniforms("layer");
        assignSamplerNames(0, "tex", ST_TRILINEAR_ANISOTROPIC_FILTERED);
    }   // InstancedRefShadowShader
};   // InstancedRefShadowShader

// ============================================================================
class DisplaceMaskShader : public Shader<DisplaceMaskShader, core::matrix4>
{
public:
    DisplaceMaskShader()
    {
        loadProgram(OBJECT, GL_VERTEX_SHADER, "displace.vert",
                            GL_FRAGMENT_SHADER, "white.frag");
        assignUniforms("ModelMatrix");
    }   // DisplaceMaskShader
};   // DisplaceMaskShader

// ============================================================================
class DisplaceShader : public TextureShader<DisplaceShader, 4, core::matrix4,
                                          core::vector2df, core::vector2df>
{
public:
    DisplaceShader()
    {
        loadProgram(OBJECT, GL_VERTEX_SHADER, "displace.vert",
                            GL_FRAGMENT_SHADER, "displace.frag");
        assignUniforms("ModelMatrix", "dir", "dir2");

        assignSamplerNames(0, "displacement_tex", ST_BILINEAR_FILTERED, 
                           1, "color_tex", ST_BILINEAR_FILTERED,
                           2, "mask_tex", ST_BILINEAR_FILTERED,
                           3, "tex", ST_TRILINEAR_ANISOTROPIC_FILTERED);
    }   // DisplaceShader
};   // DisplaceShader

// ============================================================================
class NormalVisualizer : public Shader<NormalVisualizer, video::SColor>
{
public:
    NormalVisualizer()
    {
        loadProgram(OBJECT, GL_VERTEX_SHADER, "utils/getworldmatrix.vert",
                            GL_VERTEX_SHADER, "instanced_object_pass.vert",
                            GL_GEOMETRY_SHADER, "normal_visualizer.geom",
                            GL_FRAGMENT_SHADER, "coloredquad.frag");
        assignUniforms("color");
    }   // NormalVisualizer
};   // NormalVisualizer

// ============================================================================
struct DefaultMaterial
{
    typedef InstancedObjectPass1Shader InstancedFirstPassShader;
    typedef InstancedObjectPass2Shader InstancedSecondPassShader;
    typedef InstancedShadowShader InstancedShadowPassShader;
    typedef CInstancedRSMShader InstancedRSMShader;
    typedef ListInstancedMatDefault InstancedList;
    typedef Shaders::ObjectPass1Shader FirstPassShader;
    typedef Shaders::ObjectPass2Shader SecondPassShader;
    typedef ShadowShader ShadowPassShader;
    typedef CRSMShader RSMShader;
    typedef ListMatDefault List;
    static const enum video::E_VERTEX_TYPE VertexType = video::EVT_STANDARD;
    static const enum Material::ShaderType MaterialType
                                               = Material::SHADERTYPE_SOLID;
    static const enum InstanceType Instance = InstanceTypeDualTex;
    static const STK::Tuple<size_t> FirstPassTextures;
    static const STK::Tuple<size_t, size_t> SecondPassTextures;
    static const STK::Tuple<> ShadowTextures;
    static const STK::Tuple<size_t> RSMTextures;
};   // struct DefaultMaterial

const STK::Tuple<size_t> DefaultMaterial::FirstPassTextures
    = STK::Tuple<size_t>(1);
const STK::Tuple<size_t, size_t> DefaultMaterial::SecondPassTextures
    = STK::Tuple<size_t, size_t>(0, 1);
const STK::Tuple<> DefaultMaterial::ShadowTextures;
const STK::Tuple<size_t> DefaultMaterial::RSMTextures = STK::Tuple<size_t>(0);

// ----------------------------------------------------------------------------
struct AlphaRef
{
    typedef InstancedObjectRefPass1Shader InstancedFirstPassShader;
    typedef InstancedObjectRefPass2Shader InstancedSecondPassShader;
    typedef InstancedRefShadowShader InstancedShadowPassShader;
    typedef CInstancedRSMShader InstancedRSMShader;
    typedef ListInstancedMatAlphaRef InstancedList;
    typedef ObjectRefPass1Shader FirstPassShader;
    typedef ObjectRefPass2Shader SecondPassShader;
    typedef RefShadowShader ShadowPassShader;
    typedef CRSMShader RSMShader;
    typedef ListMatAlphaRef List;
    static const enum video::E_VERTEX_TYPE VertexType = video::EVT_STANDARD;
    static const enum Material::ShaderType MaterialType = Material::SHADERTYPE_ALPHA_TEST;
    static const enum InstanceType Instance = InstanceTypeDualTex;
    static const STK::Tuple<size_t, size_t> FirstPassTextures;
    static const STK::Tuple<size_t, size_t> SecondPassTextures;
    static const STK::Tuple<size_t> ShadowTextures;
    static const STK::Tuple<size_t> RSMTextures;
};   // struct AlphaRef

// ----------------------------------------------------------------------------
const STK::Tuple<size_t, size_t> AlphaRef::FirstPassTextures
    = STK::Tuple<size_t, size_t>(0, 1);
const STK::Tuple<size_t, size_t> AlphaRef::SecondPassTextures 
    = STK::Tuple<size_t, size_t>(0, 1);
const STK::Tuple<size_t> AlphaRef::ShadowTextures = STK::Tuple<size_t>(0);
const STK::Tuple<size_t> AlphaRef::RSMTextures = STK::Tuple<size_t>(0);

// ----------------------------------------------------------------------------
struct SphereMap
{
    typedef InstancedObjectPass1Shader InstancedFirstPassShader;
    typedef InstancedSphereMapShader InstancedSecondPassShader;
    typedef InstancedShadowShader InstancedShadowPassShader;
    typedef CInstancedRSMShader InstancedRSMShader;
    typedef ListInstancedMatSphereMap InstancedList;
    typedef Shaders::ObjectPass1Shader FirstPassShader;
    typedef SphereMapShader SecondPassShader;
    typedef ShadowShader ShadowPassShader;
    typedef CRSMShader RSMShader;
    typedef ListMatSphereMap List;
    static const enum video::E_VERTEX_TYPE VertexType = video::EVT_STANDARD;
    static const enum Material::ShaderType MaterialType 
                                          = Material::SHADERTYPE_SPHERE_MAP;
    static const enum InstanceType Instance = InstanceTypeDualTex;
    static const STK::Tuple<size_t> FirstPassTextures;
    static const STK::Tuple<size_t> SecondPassTextures;
    static const STK::Tuple<> ShadowTextures;
    static const STK::Tuple<size_t> RSMTextures;
};   // struct SphereMap

// ----------------------------------------------------------------------------
const STK::Tuple<size_t> SphereMap::FirstPassTextures = STK::Tuple<size_t>(1);
const STK::Tuple<size_t> SphereMap::SecondPassTextures = STK::Tuple<size_t>(0);
const STK::Tuple<> SphereMap::ShadowTextures;
const STK::Tuple<size_t> SphereMap::RSMTextures = STK::Tuple<size_t>(0);

// ----------------------------------------------------------------------------
struct UnlitMat
{
    typedef InstancedObjectRefPass1Shader InstancedFirstPassShader;
    typedef InstancedObjectUnlitShader InstancedSecondPassShader;
    typedef InstancedRefShadowShader InstancedShadowPassShader;
    typedef CInstancedRSMShader InstancedRSMShader;
    typedef ListInstancedMatUnlit InstancedList;
    typedef ObjectRefPass1Shader FirstPassShader;
    typedef ObjectUnlitShader SecondPassShader;
    typedef RefShadowShader ShadowPassShader;
    typedef CRSMShader RSMShader;
    typedef ListMatUnlit List;
    static const enum video::E_VERTEX_TYPE VertexType = video::EVT_STANDARD;
    static const enum Material::ShaderType MaterialType =
                                           Material::SHADERTYPE_SOLID_UNLIT;
    static const enum InstanceType Instance = InstanceTypeDualTex;
    static const STK::Tuple<size_t, size_t> FirstPassTextures;
    static const STK::Tuple<size_t> SecondPassTextures;
    static const STK::Tuple<size_t> ShadowTextures;
    static const STK::Tuple<size_t> RSMTextures;
};   // struct UnlitMat

// ----------------------------------------------------------------------------
const STK::Tuple<size_t, size_t> UnlitMat::FirstPassTextures
    = STK::Tuple<size_t, size_t>(0, 1);
const STK::Tuple<size_t> UnlitMat::SecondPassTextures = STK::Tuple<size_t>(0);
const STK::Tuple<size_t> UnlitMat::ShadowTextures = STK::Tuple<size_t>(0);
const STK::Tuple<size_t> UnlitMat::RSMTextures = STK::Tuple<size_t>(0);

// ============================================================================
class GrassPass1Shader : public TextureShader<GrassPass1Shader, 2, core::matrix4,
                                       core::matrix4, core::vector3df>
{
public:
    GrassPass1Shader()
    {
        loadProgram(OBJECT, GL_VERTEX_SHADER, "grass_pass.vert",
                            GL_FRAGMENT_SHADER, "utils/encode_normal.frag",
                            GL_FRAGMENT_SHADER, "objectref_pass1.frag");
        assignUniforms("ModelMatrix", "InverseModelMatrix", "windDir");
        assignSamplerNames(0, "tex", ST_TRILINEAR_ANISOTROPIC_FILTERED,
                           1, "glosstex", ST_TRILINEAR_ANISOTROPIC_FILTERED);
    }   // GrassPass1Shader

};   // class GrassPass1Shader

// ============================================================================
class InstancedGrassPass1Shader : public TextureShader<InstancedGrassPass1Shader, 2,
                                                core::vector3df>
{
public:
    InstancedGrassPass1Shader()
    {
        loadProgram(OBJECT, GL_VERTEX_SHADER, "utils/getworldmatrix.vert",
                            GL_VERTEX_SHADER, "instanced_grass.vert",
                            GL_FRAGMENT_SHADER, "utils/encode_normal.frag",
                            GL_FRAGMENT_SHADER, "instanced_objectref_pass1.frag");
        assignUniforms("windDir");
        assignSamplerNames(0, "tex", ST_TRILINEAR_ANISOTROPIC_FILTERED,
                           1, "glosstex", ST_TRILINEAR_ANISOTROPIC_FILTERED);
    }   // InstancedGrassPass1Shader
};   // InstancedGrassPass1Shader

// ============================================================================
class GrassShadowShader : public TextureShader<GrassShadowShader, 1, int, core::matrix4,
                                        core::vector3df>
{
public:
    GrassShadowShader()
    {
        // Geometry shader needed
        if (CVS->getGLSLVersion() < 150)
            return;
        if (CVS->isAMDVertexShaderLayerUsable())
        {
            loadProgram(OBJECT, GL_VERTEX_SHADER, "shadow_grass.vert",
                                GL_FRAGMENT_SHADER, "instanced_shadowref.frag");
        }
        else
        {
            loadProgram(OBJECT, GL_VERTEX_SHADER, "shadow_grass.vert",
                                GL_GEOMETRY_SHADER, "shadow.geom",
                                GL_FRAGMENT_SHADER, "instanced_shadowref.frag");
        }
        assignUniforms("layer", "ModelMatrix", "windDir");
        assignSamplerNames(0, "tex", ST_TRILINEAR_ANISOTROPIC_FILTERED);
    }   // GrassShadowShader
};   // GrassShadowShader

// ============================================================================
class InstancedGrassShadowShader : public TextureShader<InstancedGrassShadowShader, 1, 
                                                 int, core::vector3df>
{
public:
    InstancedGrassShadowShader()
    {
        // Geometry shader needed
        if (CVS->getGLSLVersion() < 150)
            return;
        if (CVS->isAMDVertexShaderLayerUsable())
        {
            loadProgram(OBJECT, GL_VERTEX_SHADER, "utils/getworldmatrix.vert",
                                GL_VERTEX_SHADER, "instanciedgrassshadow.vert",
                                GL_FRAGMENT_SHADER, "instanced_shadowref.frag");
        }
        else
        {
            loadProgram(OBJECT, GL_VERTEX_SHADER, "utils/getworldmatrix.vert",
                                GL_VERTEX_SHADER, "instanciedgrassshadow.vert",
                                GL_GEOMETRY_SHADER, "instanced_shadow.geom",
                                GL_FRAGMENT_SHADER, "instanced_shadowref.frag");
        }

        assignSamplerNames(0, "tex", ST_TRILINEAR_ANISOTROPIC_FILTERED);
        assignUniforms("layer", "windDir");
    }   // InstancedGrassShadowShader
};   // InstancedGrassShadowShader


// ============================================================================
class GrassPass2Shader : public TextureShader<GrassPass2Shader, 5, core::matrix4,
                                       core::vector3df>
{
public:
    GrassPass2Shader()
    {
        loadProgram(OBJECT, GL_VERTEX_SHADER, "grass_pass.vert",
                            GL_FRAGMENT_SHADER, "utils/getLightFactor.frag",
                            GL_FRAGMENT_SHADER, "grass_pass2.frag");
        assignUniforms("ModelMatrix", "windDir");
        assignSamplerNames(0, "DiffuseMap", ST_NEAREST_FILTERED,
                           1, "SpecularMap", ST_NEAREST_FILTERED,
                           2, "SSAO", ST_BILINEAR_FILTERED,
                           3, "Albedo", ST_TRILINEAR_ANISOTROPIC_FILTERED,
                           4, "SpecMap", ST_TRILINEAR_ANISOTROPIC_FILTERED);
    }   // GrassPass2Shader
};   // GrassPass2Shader

// ============================================================================
class InstancedGrassPass2Shader : public TextureShader<InstancedGrassPass2Shader, 6,
                                             core::vector3df, core::vector3df>
{
public:
    InstancedGrassPass2Shader()
    {
        loadProgram(OBJECT, GL_VERTEX_SHADER, "utils/getworldmatrix.vert",
                            GL_VERTEX_SHADER, "instanced_grass.vert",
                            GL_FRAGMENT_SHADER, "utils/getLightFactor.frag",
                            GL_FRAGMENT_SHADER, "instanced_grass_pass2.frag");
        assignUniforms("windDir", "SunDir");
        assignSamplerNames(0, "DiffuseMap", ST_NEAREST_FILTERED,
                           1, "SpecularMap", ST_NEAREST_FILTERED,
                           2, "SSAO", ST_BILINEAR_FILTERED,
                           3, "dtex", ST_NEAREST_FILTERED,
                           4, "Albedo", ST_TRILINEAR_ANISOTROPIC_FILTERED,
                           5, "SpecMap", ST_TRILINEAR_ANISOTROPIC_FILTERED);
    }   // InstancedGrassPass2Shader
};   // InstancedGrassPass2Shader

// ============================================================================
class DetailedObjectPass2Shader : public TextureShader<DetailedObjectPass2Shader, 6,
                                                 core::matrix4>
{
public:
    DetailedObjectPass2Shader()
    {
        loadProgram(OBJECT, GL_VERTEX_SHADER, "object_pass.vert",
                            GL_FRAGMENT_SHADER, "utils/getLightFactor.frag",
                            GL_FRAGMENT_SHADER, "detailed_object_pass2.frag");
        assignUniforms("ModelMatrix");
        assignSamplerNames(0, "DiffuseMap", ST_NEAREST_FILTERED,
                           1, "SpecularMap", ST_NEAREST_FILTERED,
                           2, "SSAO", ST_BILINEAR_FILTERED,
                           3, "Albedo", ST_TRILINEAR_ANISOTROPIC_FILTERED,
                           4, "Detail", ST_TRILINEAR_ANISOTROPIC_FILTERED,
                           5, "SpecMap", ST_TRILINEAR_ANISOTROPIC_FILTERED);
    }  // DetailedObjectPass2Shader
};   // DetailedObjectPass2Shader

// ============================================================================
class InstancedDetailedObjectPass2Shader : public TextureShader<InstancedDetailedObjectPass2Shader, 6>
{
public:
    InstancedDetailedObjectPass2Shader()
    {
        loadProgram(OBJECT, GL_VERTEX_SHADER, "utils/getworldmatrix.vert",
                   GL_VERTEX_SHADER, "instanced_object_pass.vert",
                   GL_FRAGMENT_SHADER, "utils/getLightFactor.frag",
                   GL_FRAGMENT_SHADER, "instanced_detailed_object_pass2.frag");
        assignUniforms();
        assignSamplerNames(0, "DiffuseMap", ST_NEAREST_FILTERED,
                           1, "SpecularMap", ST_NEAREST_FILTERED,
                           2, "SSAO", ST_BILINEAR_FILTERED,
                           3, "Albedo", ST_TRILINEAR_ANISOTROPIC_FILTERED,
                           4, "Detail", ST_TRILINEAR_ANISOTROPIC_FILTERED,
                           5, "SpecMap", ST_TRILINEAR_ANISOTROPIC_FILTERED);
    }   // InstancedDetailedObjectPass2Shader
};   // InstancedDetailedObjectPass2Shader

// ============================================================================



// ----------------------------------------------------------------------------
struct GrassMat
{
    typedef InstancedGrassPass1Shader InstancedFirstPassShader;
    typedef InstancedGrassPass2Shader InstancedSecondPassShader;
    typedef InstancedGrassShadowShader InstancedShadowPassShader;
    typedef CInstancedRSMShader InstancedRSMShader;
    typedef ListInstancedMatGrass InstancedList;
    typedef GrassPass1Shader FirstPassShader;
    typedef GrassPass2Shader SecondPassShader;
    typedef GrassShadowShader ShadowPassShader;
    typedef CRSMShader RSMShader;
    typedef ListMatGrass List;
    static const enum video::E_VERTEX_TYPE VertexType = video::EVT_STANDARD;
    static const enum Material::ShaderType MaterialType 
        = Material::SHADERTYPE_VEGETATION;
    static const enum InstanceType Instance = InstanceTypeDualTex;
    static const STK::Tuple<size_t, size_t> FirstPassTextures;
    static const STK::Tuple<size_t, size_t> SecondPassTextures;
    static const STK::Tuple<size_t> ShadowTextures;
    static const STK::Tuple<size_t> RSMTextures;
};   // GrassMat

// ----------------------------------------------------------------------------
const STK::Tuple<size_t, size_t> GrassMat::FirstPassTextures 
    = STK::Tuple<size_t, size_t>(0, 1);
const STK::Tuple<size_t, size_t> GrassMat::SecondPassTextures 
    = STK::Tuple<size_t, size_t>(0, 1);
const STK::Tuple<size_t> GrassMat::ShadowTextures = STK::Tuple<size_t>(0);
const STK::Tuple<size_t> GrassMat::RSMTextures = STK::Tuple<size_t>(0);

// ----------------------------------------------------------------------------
struct NormalMat
{
    typedef InstancedNormalMapShader InstancedFirstPassShader;
    typedef InstancedObjectPass2Shader InstancedSecondPassShader;
    typedef InstancedShadowShader InstancedShadowPassShader;
    typedef CInstancedRSMShader InstancedRSMShader;
    typedef ListInstancedMatNormalMap InstancedList;
    typedef NormalMapShader FirstPassShader;
    typedef Shaders::ObjectPass2Shader SecondPassShader;
    typedef ShadowShader ShadowPassShader;
    typedef CRSMShader RSMShader;
    typedef ListMatNormalMap List;
    static const enum video::E_VERTEX_TYPE VertexType = video::EVT_TANGENTS;
    static const enum Material::ShaderType MaterialType
                                          = Material::SHADERTYPE_NORMAL_MAP;
    static const enum InstanceType Instance = InstanceTypeThreeTex;
    static const STK::Tuple<size_t, size_t> FirstPassTextures;
    static const STK::Tuple<size_t, size_t> SecondPassTextures;
    static const STK::Tuple<> ShadowTextures;
    static const STK::Tuple<size_t> RSMTextures;
};   // NormalMat

// ----------------------------------------------------------------------------
const STK::Tuple<size_t, size_t> NormalMat::FirstPassTextures
    = STK::Tuple<size_t, size_t>(2, 1);
const STK::Tuple<size_t, size_t> NormalMat::SecondPassTextures 
    = STK::Tuple<size_t, size_t>(0, 1);
const STK::Tuple<> NormalMat::ShadowTextures;
const STK::Tuple<size_t> NormalMat::RSMTextures = STK::Tuple<size_t>(0);

// ----------------------------------------------------------------------------
struct DetailMat
{
    typedef InstancedObjectPass1Shader InstancedFirstPassShader;
    typedef InstancedDetailedObjectPass2Shader InstancedSecondPassShader;
    typedef InstancedShadowShader InstancedShadowPassShader;
    typedef CInstancedRSMShader InstancedRSMShader;
    typedef ListInstancedMatDetails InstancedList;
    typedef Shaders::ObjectPass1Shader FirstPassShader;
    typedef DetailedObjectPass2Shader SecondPassShader;
    typedef ShadowShader ShadowPassShader;
    typedef CRSMShader RSMShader;
    typedef ListMatDetails List;
    static const enum video::E_VERTEX_TYPE VertexType = video::EVT_2TCOORDS;
    static const enum Material::ShaderType MaterialType
                                          = Material::SHADERTYPE_DETAIL_MAP;
    static const enum InstanceType Instance = InstanceTypeThreeTex;
    static const STK::Tuple<size_t> FirstPassTextures;
    static const STK::Tuple<size_t, size_t, size_t> SecondPassTextures;
    static const STK::Tuple<> ShadowTextures;
    static const STK::Tuple<size_t> RSMTextures;
};   // DetailMat

// ----------------------------------------------------------------------------
const STK::Tuple<size_t> DetailMat::FirstPassTextures = STK::Tuple<size_t>(1);
const STK::Tuple<size_t, size_t, size_t> DetailMat::SecondPassTextures
    = STK::Tuple<size_t, size_t, size_t>(0, 2, 1);
const STK::Tuple<> DetailMat::ShadowTextures;
const STK::Tuple<size_t> DetailMat::RSMTextures = STK::Tuple<size_t>(0);

// ----------------------------------------------------------------------------
struct SplattingMat
{
    typedef Shaders::ObjectPass1Shader FirstPassShader;
    typedef SplattingShader SecondPassShader;
    typedef ShadowShader ShadowPassShader;
    typedef SplattingRSMShader RSMShader;
    typedef ListMatSplatting List;
    static const enum video::E_VERTEX_TYPE VertexType = video::EVT_2TCOORDS;
    static const STK::Tuple<size_t> FirstPassTextures;
    static const STK::Tuple<size_t, size_t, size_t, size_t, size_t> SecondPassTextures;
    static const STK::Tuple<> ShadowTextures;
    static const STK::Tuple<size_t, size_t, size_t, size_t, size_t> RSMTextures;
};   // SplattingMat

// ----------------------------------------------------------------------------

const STK::Tuple<size_t> SplattingMat::FirstPassTextures = STK::Tuple<size_t>(6);
const STK::Tuple<size_t, size_t, size_t, size_t, size_t> 
          SplattingMat::SecondPassTextures 
              = STK::Tuple<size_t, size_t, size_t, size_t, size_t>(1, 2, 3, 4, 5);
const STK::Tuple<> SplattingMat::ShadowTextures;
const STK::Tuple<size_t, size_t, size_t, size_t, size_t> SplattingMat::RSMTextures
    = STK::Tuple<size_t, size_t, size_t, size_t, size_t>(1, 2, 3, 4, 5);

// ============================================================================


namespace RenderGeometry
{
    struct TexUnit
    {
        GLuint m_id;
        bool m_premul_alpha;

        TexUnit(GLuint id, bool premul_alpha)
        {
            m_id = id;
            m_premul_alpha = premul_alpha;
        }
    };   // struct TexUnit

    // ------------------------------------------------------------------------
    template <typename T>
    std::vector<TexUnit> TexUnits(T curr) // required on older clang versions
    {
        std::vector<TexUnit> v;
        v.push_back(curr);
        return v;
    }   // TexUnits

    // ------------------------------------------------------------------------
    // required on older clang versions
    template <typename T, typename... R>
    std::vector<TexUnit> TexUnits(T curr, R... rest)
    {
        std::vector<TexUnit> v;
        v.push_back(curr);
        VTexUnits(v, rest...);
        return v;
    }   // TexUnits

    // ------------------------------------------------------------------------
    // required on older clang versions
    template <typename T, typename... R>
    void VTexUnits(std::vector<TexUnit>& v, T curr, R... rest)
    {
        v.push_back(curr);
        VTexUnits(v, rest...);
    }   // VTexUnits
    // ------------------------------------------------------------------------
    template <typename T>
    void VTexUnits(std::vector<TexUnit>& v, T curr)
    {
        v.push_back(curr);
    }   // VTexUnits
}   // namespace RenderGeometry

using namespace RenderGeometry;


// ----------------------------------------------------------------------------
template<typename T, typename...uniforms>
void draw(const T *Shader, const GLMesh *mesh, uniforms... Args)
{
    irr_driver->IncreaseObjectCount();
    GLenum ptype = mesh->PrimitiveType;
    GLenum itype = mesh->IndexType;
    size_t count = mesh->IndexCount;

    Shader->setUniforms(Args...);
    glDrawElementsBaseVertex(ptype, (int)count, itype,
                             (GLvoid *)mesh->vaoOffset,
                             (int)mesh->vaoBaseVertex);
}   // draw

// ----------------------------------------------------------------------------

template<int...List>
struct custom_unroll_args;

template<>
struct custom_unroll_args<>
{
    template<typename T, typename ...TupleTypes, typename ...Args>
    static void exec(const T *Shader, const STK::Tuple<TupleTypes...> &t, Args... args)
    {
        draw<T>(Shader, STK::tuple_get<0>(t), args...);
    }   // exec
};   // custom_unroll_args

// ----------------------------------------------------------------------------
template<int N, int...List>
struct custom_unroll_args<N, List...>
{
    template<typename T, typename ...TupleTypes, typename ...Args>
    static void exec(const T *Shader, const STK::Tuple<TupleTypes...> &t, Args... args)
    {
        custom_unroll_args<List...>::template exec<T>(Shader, t, STK::tuple_get<N>(t), args...);
    }   // exec
};   // custom_unroll_args

// ----------------------------------------------------------------------------
template<typename T, int N>
struct TexExpander_impl
{
    template<typename...TupleArgs, typename... Args>
    static void ExpandTex(GLMesh &mesh, const STK::Tuple<TupleArgs...> &TexSwizzle,
                          Args... args)
    {
        size_t idx = STK::tuple_get<sizeof...(TupleArgs) - N>(TexSwizzle);
        TexExpander_impl<T, N - 1>::template
            ExpandTex(mesh, TexSwizzle, 
                      args..., getTextureGLuint(mesh.textures[idx]));
    }   // ExpandTex
};   // TexExpander_impl

// ----------------------------------------------------------------------------
template<typename T>
struct TexExpander_impl<T, 0>
{
    template<typename...TupleArgs, typename... Args>
    static void ExpandTex(GLMesh &mesh, const STK::Tuple<TupleArgs...> &TexSwizzle,
                          Args... args)
    {
        T::getInstance()->setTextureUnits(args...);
    }   // ExpandTex
};   // TexExpander_impl

// ----------------------------------------------------------------------------
template<typename T>
struct TexExpander
{
    template<typename...TupleArgs, typename... Args>
    static void ExpandTex(GLMesh &mesh, const STK::Tuple<TupleArgs...> &TexSwizzle,
                          Args... args)
    {
        TexExpander_impl<T, sizeof...(TupleArgs)>::ExpandTex(mesh, TexSwizzle,
                                                             args...);
    }   // ExpandTex
};   // TexExpander

// ----------------------------------------------------------------------------
template<typename T, int N>
struct HandleExpander_impl
{
    template<typename...TupleArgs, typename... Args>
    static void Expand(uint64_t *TextureHandles, 
                       const STK::Tuple<TupleArgs...> &TexSwizzle, Args... args)
    {
        size_t idx = STK::tuple_get<sizeof...(TupleArgs)-N>(TexSwizzle);
        HandleExpander_impl<T, N - 1>::template 
            Expand(TextureHandles, TexSwizzle, args..., TextureHandles[idx]);
    }   // Expand
};   // HandleExpander_impl

// ----------------------------------------------------------------------------
template<typename T>
struct HandleExpander_impl<T, 0>
{
    template<typename...TupleArgs, typename... Args>
    static void Expand(uint64_t *TextureHandles,
                       const STK::Tuple<TupleArgs...> &TexSwizzle, Args... args)
    {
        T::getInstance()->setTextureHandles(args...);
    }   // Expand
};   // HandleExpander_impl

// ----------------------------------------------------------------------------
template<typename T>
struct HandleExpander
{
    template<typename...TupleArgs, typename... Args>
    static void Expand(uint64_t *TextureHandles, const STK::Tuple<TupleArgs...> &TexSwizzle, Args... args)
    {
        HandleExpander_impl<T, sizeof...(TupleArgs)>::Expand(TextureHandles, TexSwizzle, args...);
    }   // Expand
};   // HandleExpander

// ----------------------------------------------------------------------------
template<typename T, int ...List>
void renderMeshes1stPass()
{
    auto &meshes = T::List::getInstance()->SolidPass;
    T::FirstPassShader::getInstance()->use();
    if (CVS->isARBBaseInstanceUsable())
        glBindVertexArray(VAOManager::getInstance()->getVAO(T::VertexType));
    for (unsigned i = 0; i < meshes.size(); i++)
    {
        std::vector<GLuint> Textures;
        std::vector<uint64_t> Handles;
        GLMesh &mesh = *(STK::tuple_get<0>(meshes.at(i)));
        if (!CVS->isARBBaseInstanceUsable())
            glBindVertexArray(mesh.vao);
        if (mesh.VAOType != T::VertexType)
        {
#ifdef DEBUG
            Log::error("Materials", "Wrong vertex Type associed to pass 1 (hint texture : %s)", mesh.textures[0]->getName().getPath().c_str());
#endif
            continue;
        }

        if (CVS->isAZDOEnabled())
            HandleExpander<typename T::FirstPassShader>::template Expand(mesh.TextureHandles, T::FirstPassTextures);
        else
            TexExpander<typename T::FirstPassShader>::template ExpandTex(mesh, T::FirstPassTextures);
        custom_unroll_args<List...>::template exec(T::FirstPassShader::getInstance(), meshes.at(i));
    }
}   // renderMeshes1stPass

// ----------------------------------------------------------------------------
template<typename T, typename...Args>
void renderInstancedMeshes1stPass(Args...args)
{
    std::vector<GLMesh *> &meshes = T::InstancedList::getInstance()->SolidPass;
    T::InstancedFirstPassShader::getInstance()->use();
    glBindVertexArray(VAOManager::getInstance()->getInstanceVAO(T::VertexType, T::Instance));
    for (unsigned i = 0; i < meshes.size(); i++)
    {
        std::vector<GLuint> Textures;
        GLMesh *mesh = meshes[i];
#ifdef DEBUG
        if (mesh->VAOType != T::VertexType)
        {
            Log::error("RenderGeometry", "Wrong instanced vertex format (hint : %s)", 
                mesh->textures[0]->getName().getPath().c_str());
            continue;
        }
#endif
        TexExpander<typename T::InstancedFirstPassShader>::template ExpandTex(*mesh, T::FirstPassTextures);

        T::InstancedFirstPassShader::getInstance()->setUniforms(args...);
        glDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_SHORT, (const void*)((SolidPassCmd::getInstance()->Offset[T::MaterialType] + i) * sizeof(DrawElementsIndirectCommand)));
    }
}   // renderInstancedMeshes1stPass

// ----------------------------------------------------------------------------
template<typename T, typename...Args>
void multidraw1stPass(Args...args)
{
    T::InstancedFirstPassShader::getInstance()->use();
    glBindVertexArray(VAOManager::getInstance()->getInstanceVAO(T::VertexType, T::Instance));
    if (SolidPassCmd::getInstance()->Size[T::MaterialType])
    {
        T::InstancedFirstPassShader::getInstance()->setUniforms(args...);
        glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_SHORT,
            (const void*)(SolidPassCmd::getInstance()->Offset[T::MaterialType] * sizeof(DrawElementsIndirectCommand)),
            (int)SolidPassCmd::getInstance()->Size[T::MaterialType],
            sizeof(DrawElementsIndirectCommand));
    }
}   // multidraw1stPass

static core::vector3df windDir;

// ----------------------------------------------------------------------------
void IrrDriver::renderSolidFirstPass()
{
    windDir = getWindDir();

    if (CVS->supportsIndirectInstancingRendering())
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, SolidPassCmd::getInstance()->drawindirectcmd);

    {
        ScopedGPUTimer Timer(getGPUTimer(Q_SOLID_PASS1));
        irr_driver->setPhase(SOLID_NORMAL_AND_DEPTH_PASS);

        for (unsigned i = 0; i < ImmediateDrawList::getInstance()->size(); i++)
            ImmediateDrawList::getInstance()->at(i)->render();

        renderMeshes1stPass<DefaultMaterial, 2, 1>();
        renderMeshes1stPass<SplattingMat, 2, 1>();
        renderMeshes1stPass<UnlitMat, 3, 2, 1>();
        renderMeshes1stPass<AlphaRef, 3, 2, 1>();
        renderMeshes1stPass<GrassMat, 3, 2, 1>();
        renderMeshes1stPass<NormalMat, 2, 1>();
        renderMeshes1stPass<SphereMap, 2, 1>();
        renderMeshes1stPass<DetailMat, 2, 1>();

        if (CVS->isAZDOEnabled())
        {
            multidraw1stPass<DefaultMaterial>();
            multidraw1stPass<AlphaRef>();
            multidraw1stPass<SphereMap>();
            multidraw1stPass<UnlitMat>();
            multidraw1stPass<GrassMat>(windDir);

            multidraw1stPass<NormalMat>();
            multidraw1stPass<DetailMat>();
        }
        else if (CVS->supportsIndirectInstancingRendering())
        {
            renderInstancedMeshes1stPass<DefaultMaterial>();
            renderInstancedMeshes1stPass<AlphaRef>();
            renderInstancedMeshes1stPass<UnlitMat>();
            renderInstancedMeshes1stPass<SphereMap>();
            renderInstancedMeshes1stPass<GrassMat>(windDir);
            renderInstancedMeshes1stPass<DetailMat>();
            renderInstancedMeshes1stPass<NormalMat>();
        }
    }
}   // renderSolidFirstPass

// ----------------------------------------------------------------------------
template<typename T, int...List>
void renderMeshes2ndPass( const std::vector<uint64_t> &Prefilled_Handle,
    const std::vector<GLuint> &Prefilled_Tex)
{
    auto &meshes = T::List::getInstance()->SolidPass;
    T::SecondPassShader::getInstance()->use();
    if (CVS->isARBBaseInstanceUsable())
        glBindVertexArray(VAOManager::getInstance()->getVAO(T::VertexType));
    for (unsigned i = 0; i < meshes.size(); i++)
    {
        GLMesh &mesh = *(STK::tuple_get<0>(meshes.at(i)));
        if (!CVS->isARBBaseInstanceUsable())
            glBindVertexArray(mesh.vao);

        if (mesh.VAOType != T::VertexType)
        {
#ifdef DEBUG
            Log::error("Materials", "Wrong vertex Type associed to pass 2 "
                                    "(hint texture : %s)", 
                       mesh.textures[0]->getName().getPath().c_str());
#endif
            continue;
        }

        if (CVS->isAZDOEnabled())
            HandleExpander<typename T::SecondPassShader>::template 
                Expand(mesh.TextureHandles, T::SecondPassTextures, 
                       Prefilled_Handle[0], Prefilled_Handle[1],
                       Prefilled_Handle[2]);
        else
            TexExpander<typename T::SecondPassShader>::template 
                ExpandTex(mesh, T::SecondPassTextures, Prefilled_Tex[0], 
                          Prefilled_Tex[1], Prefilled_Tex[2]);
        custom_unroll_args<List...>::template 
            exec(T::SecondPassShader::getInstance(), meshes.at(i));
    }
}   // renderMeshes2ndPass

// ----------------------------------------------------------------------------
template<typename T, typename...Args>
void renderInstancedMeshes2ndPass(const std::vector<GLuint> &Prefilled_tex, Args...args)
{
    std::vector<GLMesh *> &meshes = T::InstancedList::getInstance()->SolidPass;
    T::InstancedSecondPassShader::getInstance()->use();
    glBindVertexArray(VAOManager::getInstance()->getInstanceVAO(T::VertexType,
                                                                T::Instance));
    for (unsigned i = 0; i < meshes.size(); i++)
    {
        GLMesh *mesh = meshes[i];
        TexExpander<typename T::InstancedSecondPassShader>::template
            ExpandTex(*mesh, T::SecondPassTextures, Prefilled_tex[0],
                      Prefilled_tex[1], Prefilled_tex[2]);
        T::InstancedSecondPassShader::getInstance()->setUniforms(args...);
        glDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_SHORT, 
           (const void*)((SolidPassCmd::getInstance()->Offset[T::MaterialType] + i)
           * sizeof(DrawElementsIndirectCommand)));
    }
}   // renderInstancedMeshes2ndPass

// ----------------------------------------------------------------------------
template<typename T, typename...Args>
void multidraw2ndPass(const std::vector<uint64_t> &Handles, Args... args)
{
    T::InstancedSecondPassShader::getInstance()->use();
    glBindVertexArray(VAOManager::getInstance()->getInstanceVAO(T::VertexType,
                                                                T::Instance));
    uint64_t nulltex[10] = {};
    if (SolidPassCmd::getInstance()->Size[T::MaterialType])
    {
        HandleExpander<typename T::InstancedSecondPassShader>::template
            Expand(nulltex, T::SecondPassTextures, Handles[0], Handles[1],
                   Handles[2]);
        T::InstancedSecondPassShader::getInstance()->setUniforms(args...);
        glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_SHORT,
            (const void*)(SolidPassCmd::getInstance()->Offset[T::MaterialType] 
             * sizeof(DrawElementsIndirectCommand)),
            (int)SolidPassCmd::getInstance()->Size[T::MaterialType],
            (int)sizeof(DrawElementsIndirectCommand));
    }
}   // multidraw2ndPass

// ----------------------------------------------------------------------------
void IrrDriver::renderSolidSecondPass()
{
    irr_driver->setPhase(SOLID_LIT_PASS);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    uint64_t DiffuseHandle = 0, SpecularHandle = 0, SSAOHandle = 0, DepthHandle = 0;

    if (CVS->isAZDOEnabled())
    {
        DiffuseHandle = glGetTextureSamplerHandleARB(m_rtts->getRenderTarget(RTT_DIFFUSE),
                          Shaders::ObjectPass2Shader::getInstance()->m_sampler_ids[0]);
        if (!glIsTextureHandleResidentARB(DiffuseHandle))
            glMakeTextureHandleResidentARB(DiffuseHandle);

        SpecularHandle = glGetTextureSamplerHandleARB(m_rtts->getRenderTarget(RTT_SPECULAR),
                           Shaders::ObjectPass2Shader::getInstance()->m_sampler_ids[1]);
        if (!glIsTextureHandleResidentARB(SpecularHandle))
            glMakeTextureHandleResidentARB(SpecularHandle);

        SSAOHandle = glGetTextureSamplerHandleARB(m_rtts->getRenderTarget(RTT_HALF1_R),
                        Shaders::ObjectPass2Shader::getInstance()->m_sampler_ids[2]);
        if (!glIsTextureHandleResidentARB(SSAOHandle))
            glMakeTextureHandleResidentARB(SSAOHandle);

        DepthHandle = glGetTextureSamplerHandleARB(getDepthStencilTexture(),
                     Shaders::ObjectPass2Shader::getInstance()->m_sampler_ids[3]);
        if (!glIsTextureHandleResidentARB(DepthHandle))
            glMakeTextureHandleResidentARB(DepthHandle);
    }

    if (CVS->supportsIndirectInstancingRendering())
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER,
                    SolidPassCmd::getInstance()->drawindirectcmd);

    {
        ScopedGPUTimer Timer(getGPUTimer(Q_SOLID_PASS2));

        irr_driver->setPhase(SOLID_LIT_PASS);

        for (unsigned i = 0; i < ImmediateDrawList::getInstance()->size(); i++)
            ImmediateDrawList::getInstance()->at(i)->render();

        std::vector<GLuint> DiffSpecSSAOTex = 
            createVector<GLuint>(m_rtts->getRenderTarget(RTT_DIFFUSE), 
                                 m_rtts->getRenderTarget(RTT_SPECULAR),
                                 m_rtts->getRenderTarget(RTT_HALF1_R));

        renderMeshes2ndPass<DefaultMaterial, 3, 1>(createVector<uint64_t>(DiffuseHandle, SpecularHandle, SSAOHandle), DiffSpecSSAOTex);
        renderMeshes2ndPass<AlphaRef, 3, 1 >(createVector<uint64_t>(DiffuseHandle, SpecularHandle, SSAOHandle), DiffSpecSSAOTex);
        renderMeshes2ndPass<UnlitMat, 3, 1>(createVector<uint64_t>(DiffuseHandle, SpecularHandle, SSAOHandle), DiffSpecSSAOTex);
        renderMeshes2ndPass<SplattingMat, 1>(createVector<uint64_t>(DiffuseHandle, SpecularHandle, SSAOHandle), DiffSpecSSAOTex);
        renderMeshes2ndPass<SphereMap, 2, 1>(createVector<uint64_t>(DiffuseHandle, SpecularHandle, SSAOHandle), DiffSpecSSAOTex);
        renderMeshes2ndPass<DetailMat, 1>(createVector<uint64_t>(DiffuseHandle, SpecularHandle, SSAOHandle), DiffSpecSSAOTex);
        renderMeshes2ndPass<GrassMat, 3, 1>(createVector<uint64_t>(DiffuseHandle, SpecularHandle, SSAOHandle), DiffSpecSSAOTex);
        renderMeshes2ndPass<NormalMat, 3, 1>(createVector<uint64_t>(DiffuseHandle, SpecularHandle, SSAOHandle), DiffSpecSSAOTex);

        if (CVS->isAZDOEnabled())
        {
            multidraw2ndPass<DefaultMaterial>(createVector<uint64_t>(DiffuseHandle, SpecularHandle, SSAOHandle, 0, 0));
            multidraw2ndPass<AlphaRef>(createVector<uint64_t>(DiffuseHandle, SpecularHandle, SSAOHandle, 0, 0));
            multidraw2ndPass<SphereMap>(createVector<uint64_t>(DiffuseHandle, SpecularHandle, SSAOHandle, 0));
            multidraw2ndPass<UnlitMat>(createVector<uint64_t>(DiffuseHandle, SpecularHandle, SSAOHandle, 0));
            multidraw2ndPass<NormalMat>(createVector<uint64_t>(DiffuseHandle, SpecularHandle, SSAOHandle, 0, 0));
            multidraw2ndPass<DetailMat>(createVector<uint64_t>(DiffuseHandle, SpecularHandle, SSAOHandle, 0, 0, 0));

            // template does not work with template due to extra depth texture
            {
                GrassMat::InstancedSecondPassShader::getInstance()->use();
                glBindVertexArray(VAOManager::getInstance()->getInstanceVAO(GrassMat::VertexType,
                                                                           GrassMat::Instance));
                uint64_t nulltex[10] = {};
                if (SolidPassCmd::getInstance()->Size[GrassMat::MaterialType])
                {
                    HandleExpander<GrassMat::InstancedSecondPassShader>
                         ::Expand(nulltex, GrassMat::SecondPassTextures, DiffuseHandle,
                                  SpecularHandle, SSAOHandle, DepthHandle);
                    GrassMat::InstancedSecondPassShader::getInstance()->setUniforms(windDir,
                                                             irr_driver->getSunDirection());
                    glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_SHORT,
                        (const void*)(SolidPassCmd::getInstance()->Offset[GrassMat::MaterialType]
                        * sizeof(DrawElementsIndirectCommand)),
                        (int)SolidPassCmd::getInstance()->Size[GrassMat::MaterialType],
                        (int)sizeof(DrawElementsIndirectCommand));
                }
            }
        }
        else if (CVS->supportsIndirectInstancingRendering())
        {
            renderInstancedMeshes2ndPass<DefaultMaterial>(DiffSpecSSAOTex);
            renderInstancedMeshes2ndPass<AlphaRef>(DiffSpecSSAOTex);
            renderInstancedMeshes2ndPass<UnlitMat>(DiffSpecSSAOTex);
            renderInstancedMeshes2ndPass<SphereMap>(DiffSpecSSAOTex);
            renderInstancedMeshes2ndPass<DetailMat>(DiffSpecSSAOTex);
            renderInstancedMeshes2ndPass<NormalMat>(DiffSpecSSAOTex);

            // template does not work with template due to extra depth texture
            {
                std::vector<GLMesh *> &meshes = GrassMat::InstancedList::getInstance()->SolidPass;
                GrassMat::InstancedSecondPassShader::getInstance()->use();
                glBindVertexArray(VAOManager::getInstance()
                                  ->getInstanceVAO(GrassMat::VertexType, GrassMat::Instance));
                for (unsigned i = 0; i < meshes.size(); i++)
                {
                    GLMesh *mesh = meshes[i];
                    TexExpander<GrassMat::InstancedSecondPassShader>
                        ::ExpandTex(*mesh, GrassMat::SecondPassTextures, DiffSpecSSAOTex[0],
                                    DiffSpecSSAOTex[1], DiffSpecSSAOTex[2],
                                    irr_driver->getDepthStencilTexture());
                    GrassMat::InstancedSecondPassShader::getInstance()
                            ->setUniforms(windDir, irr_driver->getSunDirection());
                    glDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_SHORT,
                          (const void*)((SolidPassCmd::getInstance()->Offset[GrassMat::MaterialType] + i)
                               * sizeof(DrawElementsIndirectCommand)));
                }
            }
        }
    }
}   // renderSolidSecondPass

// ----------------------------------------------------------------------------
template<typename T>
static void renderInstancedMeshNormals()
{
    std::vector<GLMesh *> &meshes = T::InstancedList::getInstance()->SolidPass;
    NormalVisualizer::getInstance()->use();
    glBindVertexArray(VAOManager::getInstance()->getInstanceVAO(T::VertexType, T::Instance));
    for (unsigned i = 0; i < meshes.size(); i++)
    {
        NormalVisualizer::getInstance()->setUniforms(video::SColor(255, 0, 255, 0));
        glDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_SHORT, 
             (const void*)((SolidPassCmd::getInstance()->Offset[T::MaterialType] + i) 
             * sizeof(DrawElementsIndirectCommand)));
    }
}   // renderInstancedMeshNormals

// ----------------------------------------------------------------------------
template<typename T>
static void renderMultiMeshNormals()
{
    NormalVisualizer::getInstance()->use();
    glBindVertexArray(VAOManager::getInstance()->getInstanceVAO(T::VertexType, T::Instance));
    if (SolidPassCmd::getInstance()->Size[T::MaterialType])
    {
        NormalVisualizer::getInstance()->setUniforms(video::SColor(255, 0, 255, 0));
        glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_SHORT,
            (const void*)(SolidPassCmd::getInstance()->Offset[T::MaterialType] * sizeof(DrawElementsIndirectCommand)),
            (int)SolidPassCmd::getInstance()->Size[T::MaterialType],
            (int)sizeof(DrawElementsIndirectCommand));
    }
}   // renderMultiMeshNormals

// ----------------------------------------------------------------------------
void IrrDriver::renderNormalsVisualisation()
{
    if (CVS->isAZDOEnabled()) {
        renderMultiMeshNormals<DefaultMaterial>();
        renderMultiMeshNormals<AlphaRef>();
        renderMultiMeshNormals<UnlitMat>();
        renderMultiMeshNormals<SphereMap>();
        renderMultiMeshNormals<DetailMat>();
        renderMultiMeshNormals<NormalMat>();
    }
    else if (CVS->supportsIndirectInstancingRendering())
    {
        renderInstancedMeshNormals<DefaultMaterial>();
        renderInstancedMeshNormals<AlphaRef>();
        renderInstancedMeshNormals<UnlitMat>();
        renderInstancedMeshNormals<SphereMap>();
        renderInstancedMeshNormals<DetailMat>();
        renderInstancedMeshNormals<NormalMat>();
    }
}   // renderNormalsVisualisation

// ----------------------------------------------------------------------------
template<typename Shader, enum video::E_VERTEX_TYPE VertexType, int...List, 
         typename... TupleType>
void renderTransparenPass(const std::vector<RenderGeometry::TexUnit> &TexUnits, 
                          std::vector<STK::Tuple<TupleType...> > *meshes)
{
    Shader::getInstance()->use();
    if (CVS->isARBBaseInstanceUsable())
        glBindVertexArray(VAOManager::getInstance()->getVAO(VertexType));
    for (unsigned i = 0; i < meshes->size(); i++)
    {
        GLMesh &mesh = *(STK::tuple_get<0>(meshes->at(i)));
        if (!CVS->isARBBaseInstanceUsable())
            glBindVertexArray(mesh.vao);
        if (mesh.VAOType != VertexType)
        {
#ifdef DEBUG
            Log::error("Materials", "Wrong vertex Type associed to pass 2 "
                                    "(hint texture : %s)",
                       mesh.textures[0]->getName().getPath().c_str());
#endif
            continue;
        }

        if (CVS->isAZDOEnabled())
            Shader::getInstance()->setTextureHandles(mesh.TextureHandles[0]);
        else
            Shader::getInstance()->setTextureUnits(getTextureGLuint(mesh.textures[0]));
        custom_unroll_args<List...>::template exec(Shader::getInstance(), meshes->at(i));
    }
}   // renderTransparenPass

static video::ITexture *displaceTex = 0;

// ----------------------------------------------------------------------------
void IrrDriver::renderTransparent()
{

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glDisable(GL_CULL_FACE);

    irr_driver->setPhase(TRANSPARENT_PASS);

    for (unsigned i = 0; i < ImmediateDrawList::getInstance()->size(); i++)
        ImmediateDrawList::getInstance()->at(i)->render();

    if (CVS->isARBBaseInstanceUsable())
        glBindVertexArray(VAOManager::getInstance()->getVAO(video::EVT_STANDARD));

    if (World::getWorld() && World::getWorld()->isFogEnabled())
    {
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        renderTransparenPass<Shaders::TransparentFogShader, video::EVT_STANDARD,
                             8, 7, 6, 5, 4, 3, 2, 1>(
                             TexUnits(RenderGeometry::TexUnit(0, true)),
                              ListBlendTransparentFog::getInstance());
        glBlendFunc(GL_ONE, GL_ONE);
        renderTransparenPass<Shaders::TransparentFogShader, 
                             video::EVT_STANDARD, 8, 7, 6, 5, 4, 3, 2, 1>(
                             TexUnits(RenderGeometry::TexUnit(0, true)), 
                                       ListAdditiveTransparentFog::getInstance());
    }
    else
    {
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        renderTransparenPass<Shaders::TransparentShader,
                             video::EVT_STANDARD, 2, 1>(
                                  TexUnits(RenderGeometry::TexUnit(0, true)),
                                           ListBlendTransparent::getInstance());
        glBlendFunc(GL_ONE, GL_ONE);
        renderTransparenPass<Shaders::TransparentShader, video::EVT_STANDARD, 2, 1>(
                             TexUnits(RenderGeometry::TexUnit(0, true)),
                                      ListAdditiveTransparent::getInstance());
    }

    for (unsigned i = 0; i < BillBoardList::getInstance()->size(); i++)
        BillBoardList::getInstance()->at(i)->render();

    if (!CVS->isDefferedEnabled())
        return;

    // Render displacement nodes
    irr_driver->getFBO(FBO_TMP1_WITH_DS).bind();
    glClear(GL_COLOR_BUFFER_BIT);
    irr_driver->getFBO(FBO_DISPLACE).bind();
    glClear(GL_COLOR_BUFFER_BIT);

    DisplaceProvider * const cb =
        (DisplaceProvider *)Shaders::getCallback(ES_DISPLACE);
    cb->update();

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);
    glClear(GL_STENCIL_BUFFER_BIT);
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_ALWAYS, 1, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

    if (CVS->isARBBaseInstanceUsable())
        glBindVertexArray(VAOManager::getInstance()->getVAO(video::EVT_2TCOORDS));
    // Generate displace mask
    // Use RTT_TMP4 as displace mask
    irr_driver->getFBO(FBO_TMP1_WITH_DS).bind();
    for (unsigned i = 0; i < ListDisplacement::getInstance()->size(); i++)
    {
        const GLMesh &mesh =
            *(STK::tuple_get<0>(ListDisplacement::getInstance()->at(i)));
        if (!CVS->isARBBaseInstanceUsable())
            glBindVertexArray(mesh.vao);
        const core::matrix4 &AbsoluteTransformation
            = STK::tuple_get<1>(ListDisplacement::getInstance()->at(i));
        if (mesh.VAOType != video::EVT_2TCOORDS)
        {
#ifdef DEBUG
            Log::error("Materials", "Displacement has wrong vertex type");
#endif
            continue;
        }

        GLenum ptype = mesh.PrimitiveType;
        GLenum itype = mesh.IndexType;
        size_t count = mesh.IndexCount;

        DisplaceMaskShader::getInstance()->use();
        DisplaceMaskShader::getInstance()->setUniforms(AbsoluteTransformation);
        glDrawElementsBaseVertex(ptype, (int)count, itype,
                                 (GLvoid *)mesh.vaoOffset, (int)mesh.vaoBaseVertex);
    }

    irr_driver->getFBO(FBO_DISPLACE).bind();
    if (!displaceTex)
        displaceTex = irr_driver->getTexture(FileManager::TEXTURE, "displace.png");
    for (unsigned i = 0; i < ListDisplacement::getInstance()->size(); i++)
    {
        const GLMesh &mesh = 
            *(STK::tuple_get<0>(ListDisplacement::getInstance()->at(i)));
        if (!CVS->isARBBaseInstanceUsable())
            glBindVertexArray(mesh.vao);
        const core::matrix4 &AbsoluteTransformation =
            STK::tuple_get<1>(ListDisplacement::getInstance()->at(i));
        if (mesh.VAOType != video::EVT_2TCOORDS)
            continue;

        GLenum ptype = mesh.PrimitiveType;
        GLenum itype = mesh.IndexType;
        size_t count = mesh.IndexCount;
        // Render the effect
        DisplaceShader::getInstance()->setTextureUnits(
            getTextureGLuint(displaceTex),
            irr_driver->getRenderTargetTexture(RTT_COLOR),
            irr_driver->getRenderTargetTexture(RTT_TMP1),
            getTextureGLuint(mesh.textures[0]));
        DisplaceShader::getInstance()->use();
        DisplaceShader::getInstance()->setUniforms(AbsoluteTransformation,
            core::vector2df(cb->getDirX(), cb->getDirY()),
            core::vector2df(cb->getDir2X(), cb->getDir2Y()));

        glDrawElementsBaseVertex(ptype, (int)count, itype, (GLvoid *)mesh.vaoOffset,
                                 (int)mesh.vaoBaseVertex);
    }

    irr_driver->getFBO(FBO_COLORS).bind();
    glStencilFunc(GL_EQUAL, 1, 0xFF);
    m_post_processing->renderPassThrough(m_rtts->getRenderTarget(RTT_DISPLACE),
                                         irr_driver->getFBO(FBO_COLORS).getWidth(), 
                                         irr_driver->getFBO(FBO_COLORS).getHeight());
    glDisable(GL_STENCIL_TEST);

}   // renderTransparent

// ----------------------------------------------------------------------------
template<typename T, typename...uniforms>
void drawShadow(const T *Shader, unsigned cascade, const GLMesh *mesh, uniforms... Args)
{
    irr_driver->IncreaseObjectCount();
    GLenum ptype = mesh->PrimitiveType;
    GLenum itype = mesh->IndexType;
    size_t count = mesh->IndexCount;

    Shader->setUniforms(cascade, Args...);
    glDrawElementsBaseVertex(ptype, (int)count, itype,
                             (GLvoid *)mesh->vaoOffset, (int)mesh->vaoBaseVertex);
}   // drawShadow

// ----------------------------------------------------------------------------
template<int...List>
struct shadow_custom_unroll_args;

template<>
struct shadow_custom_unroll_args<>
{
    template<typename T, typename ...TupleTypes, typename ...Args>
    static void exec(const T *Shader, unsigned cascade, const STK::Tuple<TupleTypes...> &t, Args... args)
    {
        drawShadow<T>(Shader, cascade, STK::tuple_get<0>(t), args...);
    }   // exec
};   // shadow_custom_unroll_args

// ----------------------------------------------------------------------------
template<int N, int...List>
struct shadow_custom_unroll_args<N, List...>
{
    template<typename T, typename ...TupleTypes, typename ...Args>
    static void exec(const T *Shader, unsigned cascade, const STK::Tuple<TupleTypes...> &t, Args... args)
    {
        shadow_custom_unroll_args<List...>::template exec<T>(Shader, cascade, t, STK::tuple_get<N>(t), args...);
    }   // exec
};   // shadow_custom_unroll_args

// ----------------------------------------------------------------------------
template<typename T, int...List>
void renderShadow(unsigned cascade)
{
    auto &t = T::List::getInstance()->Shadows[cascade];
    T::ShadowPassShader::getInstance()->use();
    if (CVS->isARBBaseInstanceUsable())
        glBindVertexArray(VAOManager::getInstance()->getVAO(T::VertexType));
    for (unsigned i = 0; i < t.size(); i++)
    {
        GLMesh *mesh = STK::tuple_get<0>(t.at(i));
        if (!CVS->isARBBaseInstanceUsable())
            glBindVertexArray(mesh->vao);
        if (CVS->isAZDOEnabled())
            HandleExpander<typename T::ShadowPassShader>::template Expand(mesh->TextureHandles, T::ShadowTextures);
        else
            TexExpander<typename T::ShadowPassShader>::template ExpandTex(*mesh, T::ShadowTextures);
        shadow_custom_unroll_args<List...>::template exec<typename T::ShadowPassShader>(T::ShadowPassShader::getInstance(), cascade, t.at(i));
    }   // for i
}   // renderShadow

// ----------------------------------------------------------------------------
template<typename T, typename...Args>
void renderInstancedShadow(unsigned cascade, Args ...args)
{
    T::InstancedShadowPassShader::getInstance()->use();
    glBindVertexArray(VAOManager::getInstance()->getInstanceVAO(T::VertexType,
                                                          InstanceTypeShadow));
    std::vector<GLMesh *> &t = T::InstancedList::getInstance()->Shadows[cascade];
    for (unsigned i = 0; i < t.size(); i++)
    {
        GLMesh *mesh = t[i];

        TexExpander<typename T::InstancedShadowPassShader>::template 
                                       ExpandTex(*mesh, T::ShadowTextures);
        T::InstancedShadowPassShader::getInstance()->setUniforms(cascade, args...);
        size_t tmp = ShadowPassCmd::getInstance()->Offset[cascade][T::MaterialType] + i;
        glDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_SHORT,
                               (const void*)((tmp) 
                               * sizeof(DrawElementsIndirectCommand)));
    }   // for i

}   // renderInstancedShadow

// ----------------------------------------------------------------------------
template<typename T, typename...Args>
static void multidrawShadow(unsigned i, Args ...args)
{
    T::InstancedShadowPassShader::getInstance()->use();
    glBindVertexArray(VAOManager::getInstance()->getInstanceVAO(T::VertexType,
                                                           InstanceTypeShadow));
    if (ShadowPassCmd::getInstance()->Size[i][T::MaterialType])
    {
        T::InstancedShadowPassShader::getInstance()->setUniforms(i, args...);
        glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_SHORT, 
            (const void*)(ShadowPassCmd::getInstance()->Offset[i][T::MaterialType] 
            * sizeof(DrawElementsIndirectCommand)),
            (int)ShadowPassCmd::getInstance()->Size[i][T::MaterialType], 
            sizeof(DrawElementsIndirectCommand));
    }
}   // multidrawShadow

// ----------------------------------------------------------------------------
void IrrDriver::renderShadows()
{
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    m_rtts->getShadowFBO().bind();
    if (!CVS->isESMEnabled())
    {
        glDrawBuffer(GL_NONE);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1.5, 50.);
    }
    glCullFace(GL_BACK);
    glEnable(GL_CULL_FACE);

    glClearColor(1., 1., 1., 1.);
    glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    glClearColor(0., 0., 0., 0.);

    for (unsigned cascade = 0; cascade < 4; cascade++)
    {
        ScopedGPUTimer Timer(getGPUTimer(Q_SHADOWS_CASCADE0 + cascade));

        renderShadow<DefaultMaterial, 1>(cascade);
        renderShadow<SphereMap, 1>(cascade);
        renderShadow<DetailMat, 1>(cascade);
        renderShadow<SplattingMat, 1>(cascade);
        renderShadow<NormalMat, 1>(cascade);
        renderShadow<AlphaRef, 1>(cascade);
        renderShadow<UnlitMat, 1>(cascade);
        renderShadow<GrassMat, 3, 1>(cascade);

        if (CVS->supportsIndirectInstancingRendering())
            glBindBuffer(GL_DRAW_INDIRECT_BUFFER, ShadowPassCmd::getInstance()->drawindirectcmd);

        if (CVS->isAZDOEnabled())
        {
            multidrawShadow<DefaultMaterial>(cascade);
            multidrawShadow<DetailMat>(cascade);
            multidrawShadow<NormalMat>(cascade);
            multidrawShadow<AlphaRef>(cascade);
            multidrawShadow<UnlitMat>(cascade);
            multidrawShadow<GrassMat>(cascade, windDir);
        }
        else if (CVS->supportsIndirectInstancingRendering())
        {
            renderInstancedShadow<DefaultMaterial>(cascade);
            renderInstancedShadow<DetailMat>(cascade);
            renderInstancedShadow<AlphaRef>(cascade);
            renderInstancedShadow<UnlitMat>(cascade);
            renderInstancedShadow<GrassMat>(cascade, windDir);
            renderInstancedShadow<NormalMat>(cascade);
        }
    }

    glDisable(GL_POLYGON_OFFSET_FILL);

    if (CVS->isESMEnabled())
    {
        ScopedGPUTimer Timer(getGPUTimer(Q_SHADOW_POSTPROCESS));

        if (CVS->isARBTextureViewUsable())
        {
            const std::pair<float, float>* shadow_scales 
                = getShadowMatrices()->getShadowScales();

            for (unsigned i = 0; i < 2; i++)
            {
                m_post_processing->renderGaussian6BlurLayer(m_rtts->getShadowFBO(), i,
                    2.f * shadow_scales[0].first / shadow_scales[i].first,
                    2.f * shadow_scales[0].second / shadow_scales[i].second);
            }
        }
        glBindTexture(GL_TEXTURE_2D_ARRAY, m_rtts->getShadowFBO().getRTT()[0]);
        glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
    }
}   // renderShadows



// ----------------------------------------------------------------------------
template<int...List>
struct rsm_custom_unroll_args;

template<>
struct rsm_custom_unroll_args<>
{
    template<typename T, typename ...TupleTypes, typename ...Args>
    static void exec(const core::matrix4 &rsm_matrix, 
                      const STK::Tuple<TupleTypes...> &t, Args... args)
    {
        draw<T>(T::getInstance(), STK::tuple_get<0>(t), rsm_matrix, args...);
    }
};   // rsm_custom_unroll_args

// ----------------------------------------------------------------------------
template<int N, int...List>
struct rsm_custom_unroll_args<N, List...>
{
    template<typename T, typename ...TupleTypes, typename ...Args>
    static void exec(const core::matrix4 &rsm_matrix,
                     const STK::Tuple<TupleTypes...> &t, Args... args)
    {
        rsm_custom_unroll_args<List...>::template 
            exec<T>(rsm_matrix, t, STK::tuple_get<N>(t), args...);
    }
};   // rsm_custom_unroll_args

// ----------------------------------------------------------------------------
template<typename T, int... Selector>
void drawRSM(const core::matrix4 & rsm_matrix)
{
    T::RSMShader::getInstance()->use();
    if (CVS->isARBBaseInstanceUsable())
        glBindVertexArray(VAOManager::getInstance()->getVAO(T::VertexType));
    auto t = T::List::getInstance()->RSM;
    for (unsigned i = 0; i < t.size(); i++)
    {
        std::vector<GLuint> Textures;
        GLMesh *mesh = STK::tuple_get<0>(t.at(i));
        if (!CVS->isARBBaseInstanceUsable())
            glBindVertexArray(mesh->vao);
        if (CVS->isAZDOEnabled())
            HandleExpander<typename T::RSMShader>::template Expand(mesh->TextureHandles, T::RSMTextures);
        else
            TexExpander<typename T::RSMShader>::template ExpandTex(*mesh, T::RSMTextures);
        rsm_custom_unroll_args<Selector...>::template exec<typename T::RSMShader>(rsm_matrix, t.at(i));
    }
}   // drawRSM

// ----------------------------------------------------------------------------
template<typename T, typename...Args>
void renderRSMShadow(Args ...args)
{
    T::InstancedRSMShader::getInstance()->use();
    glBindVertexArray(VAOManager::getInstance()->getInstanceVAO(T::VertexType, InstanceTypeRSM));
    auto t = T::InstancedList::getInstance()->RSM;
    for (unsigned i = 0; i < t.size(); i++)
    {
        std::vector<GLuint> Textures;
        GLMesh *mesh = t[i];

        TexExpander<typename T::InstancedRSMShader>::template ExpandTex(*mesh, T::RSMTextures);
        T::InstancedRSMShader::getInstance()->setUniforms(args...);
        glDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_SHORT,
           (const void*)((RSMPassCmd::getInstance()->Offset[T::MaterialType] + i)
           * sizeof(DrawElementsIndirectCommand)));
    }
}   // renderRSMShadow

// ----------------------------------------------------------------------------
template<typename T, typename... Args>
void multidrawRSM(Args...args)
{
    T::InstancedRSMShader::getInstance()->use();
    glBindVertexArray(VAOManager::getInstance()->getInstanceVAO(T::VertexType,
                                                               InstanceTypeRSM));
    if (RSMPassCmd::getInstance()->Size[T::MaterialType])
    {
        T::InstancedRSMShader::getInstance()->setUniforms(args...);
        glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_SHORT,
            (const void*)(RSMPassCmd::getInstance()->Offset[T::MaterialType] 
               * sizeof(DrawElementsIndirectCommand)),
            (int)RSMPassCmd::getInstance()->Size[T::MaterialType],
            sizeof(DrawElementsIndirectCommand));
    }
}   // multidrawRSM

// ----------------------------------------------------------------------------
void IrrDriver::renderRSM()
{
    if (getShadowMatrices()->isRSMMapAvail())
        return;
    ScopedGPUTimer Timer(getGPUTimer(Q_RSM));
    m_rtts->getRSM().bind();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const core::matrix4 &rsm_matrix = getShadowMatrices()->getRSMMatrix();
    drawRSM<DefaultMaterial, 3, 1>(rsm_matrix);
    drawRSM<AlphaRef, 3, 1>(rsm_matrix);
    drawRSM<NormalMat, 3, 1>(rsm_matrix);
    drawRSM<UnlitMat, 3, 1>(rsm_matrix);
    drawRSM<DetailMat, 3, 1>(rsm_matrix);
    drawRSM<SplattingMat, 1>(rsm_matrix);

    if (CVS->supportsIndirectInstancingRendering())
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER,
                     RSMPassCmd::getInstance()->drawindirectcmd);

    if (CVS->isAZDOEnabled())
    {
        multidrawRSM<DefaultMaterial>(rsm_matrix);
        multidrawRSM<NormalMat>(rsm_matrix);
        multidrawRSM<AlphaRef>(rsm_matrix);
        multidrawRSM<UnlitMat>(rsm_matrix);
        multidrawRSM<DetailMat>(rsm_matrix);
    }
    else if (CVS->supportsIndirectInstancingRendering())
    {
        renderRSMShadow<DefaultMaterial>(rsm_matrix);
        renderRSMShadow<AlphaRef>(rsm_matrix);
        renderRSMShadow<UnlitMat>(rsm_matrix);
        renderRSMShadow<NormalMat>(rsm_matrix);
        renderRSMShadow<DetailMat>(rsm_matrix);
    }
    getShadowMatrices()->setRSMMapAvail(true);
}   // renderRSM
