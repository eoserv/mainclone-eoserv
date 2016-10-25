
/* $Id$
 * EOSERV is released under the zlib license.
 * See LICENSE.txt for more info.
 */

#ifndef ASTAR_HPP_INCLUDED
#define ASTAR_HPP_INCLUDED

#include <deque>
#include <utility>
#include <vector>

struct AStarCoordPri
{
	std::pair<int, int> coord;
	int pri;
};

bool operator <(const AStarCoordPri& a, const AStarCoordPri& b);

struct AStarFrontier
{
	int cost;
	std::pair<int, int> from;
};

// Not thread safe - use one instance per thread
class AStar
{
	private:
		std::vector<AStarCoordPri> frontier_pri_storage;
		std::vector<AStarFrontier> frontier_info_table;

	public:
		AStar();
		~AStar();

		void SetMaxDistance(int max_distance);

		std::deque<std::pair<int,int>> FindPath(std::pair<int, int> start, std::pair<int, int> goal, int max_distance, std::function<bool(int, int)> is_walkable);
};

#endif // ASTAR_HPP_INCLUDED
