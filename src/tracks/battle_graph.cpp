//
//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2009-2015 Joerg Henrichs
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
//  Foundation, Inc., 59 Temple Place - Suite 330, B

#include "tracks/battle_graph.hpp"

#include <IMesh.h>
#include <ICameraSceneNode.h>
#include <IMeshSceneNode.h>

#include "config/user_config.hpp"
#include "io/xml_node.hpp"
#include "items/item_manager.hpp"
#include "race/race_manager.hpp"
#include "tracks/navmesh.hpp"
#include "utils/log.hpp"

const int BattleGraph::UNKNOWN_POLY  = -1;
BattleGraph * BattleGraph::m_battle_graph = NULL;

/** Constructor, Creates a navmesh, builds a graph from the navmesh and
*    then runs shortest path algorithm to find and store paths to be used
*    by the AI. */
BattleGraph::BattleGraph(const std::string &navmesh_file_name,
                         const XMLNode& node)
{
    m_items_on_graph.clear();

    NavMesh::create(navmesh_file_name);
    m_navmesh_file = navmesh_file_name;
    buildGraph(NavMesh::get());
    computeFloydWarshall();
    if (race_manager->getMinorMode() == RaceManager::MINOR_MODE_SOCCER)
        loadGoalNodes(node);

} // BattleGraph

// -----------------------------------------------------------------------------
/** Destructor, destroys NavMesh and the debug mesh if it exists */
BattleGraph::~BattleGraph(void)
{
    NavMesh::destroy();

    if(UserConfigParams::m_track_debug)
        cleanupDebugMesh();
    GraphStructure::destroyRTT();
} // ~BattleGraph

// -----------------------------------------------------------------------------
/** Builds a graph from an existing NavMesh. The graph is stored as an adjacency
*    matrix. */
void BattleGraph::buildGraph(NavMesh* navmesh)
{
    unsigned int n_polys = navmesh->getNumberOfPolys();

    m_distance_matrix = std::vector< std::vector<float> > (n_polys, std::vector<float>(n_polys, 9999.9f));
    for(unsigned int i=0; i<n_polys; i++)
    {
        NavPoly currentPoly = navmesh->getNavPoly(i);
        std::vector<int> adjacents = navmesh->getAdjacentPolys(i);
        for(unsigned int j=0; j<adjacents.size(); j++)
        {
            Vec3 diff = navmesh->getCenterOfPoly(adjacents[j]) - currentPoly.getCenter();
            float distance = diff.length();
            m_distance_matrix[i][adjacents[j]] = distance;
            //m_distance_matrix[adjacents[j]][i] = distance;
        }
        m_distance_matrix[i][i] = 0.0f;
    }

}    // buildGraph

// -----------------------------------------------------------------------------
/** computeFloydWarshall() computes the shortest distance between any two nodes.
 *  At the end of the computation, m_distance_matrix[i][j] stores the shortest path
 *  distance from i to j and m_parent_poly[i][j] stores the last vertex visited on the
 *  shortest path from i to j before visiting j. Suppose the shortest path from i to j is
 *  i->......->k->j  then m_parent_poly[i][j] = k
 */
void BattleGraph::computeFloydWarshall()
{
    unsigned int n = getNumNodes();

    // initialize m_parent_poly with unknown_poly so that if no path is found b/w i and j
    // then m_parent_poly[i][j] = -1 (UNKNOWN_POLY)
    // AI must check this
    m_parent_poly = std::vector< std::vector<int> > (n, std::vector<int>(n,BattleGraph::UNKNOWN_POLY));
    for(unsigned int i=0; i<n; i++)
    {
        for(unsigned int j=0; j<n; j++)
        {
            if(i == j || m_distance_matrix[i][j]>=9899.9f) m_parent_poly[i][j]=-1;
            else    m_parent_poly[i][j] = i;
        }
    }

    for(unsigned int k=0; k<n; k++)
    {
        for(unsigned int i=0; i<n; i++)
        {
            for(unsigned int j=0; j<n; j++)
            {
                if( (m_distance_matrix[i][k] + m_distance_matrix[k][j]) < m_distance_matrix[i][j])
                {
                    m_distance_matrix[i][j] = m_distance_matrix[i][k] + m_distance_matrix[k][j];
                    m_parent_poly[i][j] = m_parent_poly[k][j];
                }
            }
        }
    }

}    // computeFloydWarshall

// -----------------------------------------------------------------------------
/** Maps items on battle graph */
void BattleGraph::findItemsOnGraphNodes()
{
    const ItemManager* item_manager = ItemManager::get();
    unsigned int item_count = item_manager->getNumberOfItems();

    for (unsigned int i = 0; i < item_count; ++i)
    {
        const Item* item = item_manager->getItem(i);
        Vec3 xyz = item->getXYZ();
        int polygon = BattleGraph::UNKNOWN_POLY;

        for (unsigned int j = 0; j < this->getNumNodes(); ++j)
        {
            if (NavMesh::get()->getNavPoly(j).pointInPoly(xyz, false))
                polygon = j;
        }

        if (polygon != BattleGraph::UNKNOWN_POLY)
        {
            m_items_on_graph.push_back(std::make_pair(item, polygon));
            Log::debug("BattleGraph","item number %d is on polygon %d", i, polygon);
        }
        else
            Log::debug("BattleGraph","Can't map item number %d with a suitable polygon", i);
    }
}    // findItemsOnGraphNodes

// -----------------------------------------------------------------------------
int BattleGraph::pointToNode(const int cur_node,
                             const Vec3& cur_point,
                             bool ignore_vertical) const
{
    int final_node = BattleGraph::UNKNOWN_POLY;

    if (cur_node == BattleGraph::UNKNOWN_POLY)
    {
        // Try all nodes in the battle graph
        bool found = false;
        unsigned int node = 0;
        while (!found && node < this->getNumNodes())
        {
            const NavPoly& p_all = this->getPolyOfNode(node);
            if (p_all.pointInPoly(cur_point, ignore_vertical))
            {
                final_node = node;
                found = true;
            }
            node++;
        }
    }
    else
    {
        // Check if the point is still on the same node
        const NavPoly& p_cur = this->getPolyOfNode(cur_node);
        if (p_cur.pointInPoly(cur_point, ignore_vertical)) return cur_node;

        // If not then check all adjacent polys
        const std::vector<int>& adjacents = NavMesh::get()
            ->getAdjacentPolys(cur_node);

        bool found = false;
        unsigned int num = 0;
        while (!found && num < adjacents.size())
        {
            const NavPoly& p_temp = this->getPolyOfNode(adjacents[num]);
            if (p_temp.pointInPoly(cur_point, ignore_vertical))
            {
                final_node = adjacents[num];
                found = true;
            }
            num++;
        }

        // Current node is still unkown
        if (final_node == BattleGraph::UNKNOWN_POLY)
        {
            // Calculated distance from saved node to current position,
            // if it's close enough than use the saved node anyway, it
            // may happen when the kart stays on the edge of obstacles
            const NavPoly& p = this->getPolyOfNode(cur_node);
            const float dist = (p.getCenter() - cur_point).length_2d();

            if (dist < 3.0f)
                final_node = cur_node;
        }
    }
    return final_node;
}    // pointToNode

// -----------------------------------------------------------------------------
const int BattleGraph::getNextShortestPathPoly(int i, int j) const
{
    if (i == BattleGraph::UNKNOWN_POLY || j == BattleGraph::UNKNOWN_POLY)
        return BattleGraph::UNKNOWN_POLY;
    return m_parent_poly[j][i];
}    // getNextShortestPathPoly

// -----------------------------------------------------------------------------
const bool BattleGraph::differentNodeColor(int n, NodeColor* c) const
{
    std::set<int>::iterator it;
    it = m_red_node.find(n);
    if (it != m_red_node.end())
    {
        *c = COLOR_RED;
        return true;
    }

    it = m_blue_node.find(n);
    if (it != m_blue_node.end())
    {
        *c = COLOR_BLUE;
        return true;
    }
    return false;
}    // differentNodeColor

// -----------------------------------------------------------------------------
void BattleGraph::loadGoalNodes(const XMLNode& node)
{
    m_red_node.clear();
    m_blue_node.clear();

    const XMLNode *check_node = node.getNode("checks");
    for (unsigned int i = 0; i < check_node->getNumNodes(); i++)
    {
        const XMLNode *goal = check_node->getNode(i);
        if (goal->getName() =="goal")
        {
            Vec3 p1, p2;
            bool first_goal = false;
            goal->get("first_goal", &first_goal);
            goal->get("p1", &p1);
            goal->get("p2", &p2);

            int first = pointToNode(/*cur_node*/-1, p1, true);
            int last = pointToNode(/*cur_node*/-1, p2, true);

            first_goal ? m_blue_node.insert(first) : m_red_node.insert(first);
            first_goal ? m_blue_node.insert(last) : m_red_node.insert(last);
            while (first != last)
            {
                // Find all the nodes which connect the two points of
                // goal, notice: only work if it's a straight line
                first = getNextShortestPathPoly(first, last);
                first_goal ? m_blue_node.insert(first) :
                    m_red_node.insert(first);
            }
        }
    }
}    // loadGoalNodes
