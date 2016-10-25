
/* $Id$
 * EOSERV is released under the zlib license.
 * See LICENSE.txt for more info.
 */

#include "astar.hpp"

#include <algorithm>
#include <climits>

#include "util.hpp"

bool operator <(const AStarCoordPri& a, const AStarCoordPri& b)
{
	return a.pri > b.pri;
}

void AStar::SetMaxDistance(int max_distance)
{
	int table_width = 2 * max_distance + 1;
	int max_path_size = 2 * max_distance + 2;
	int table_size = table_width * table_width;

	frontier_pri_storage.resize(max_path_size);
	frontier_info_table.resize(table_size);
}

AStar::AStar()
{ }

AStar::~AStar()
{ }

std::deque<std::pair<int,int>> AStar::FindPath(std::pair<int, int> start, std::pair<int, int> goal, int max_distance, std::function<bool(int, int)> is_walkable)
{
	auto in_range = [&](int x, int y)
	{
		return std::abs(start.first - x) < max_distance
		    && std::abs(start.second - y) < max_distance
		    && util::path_length(start.first, start.second, x, y) <= max_distance;
	};

	int table_width = 2 * max_distance + 1;
	int table_size = table_width * table_width;

	std::fill(frontier_pri_storage.begin(), frontier_pri_storage.end(), AStarCoordPri{{0,0}, 0});
	std::size_t frontier_pri_storage_size = 0;

	auto pri_push = [&](const AStarCoordPri& a)
	{
		frontier_pri_storage[frontier_pri_storage_size] = a;
		std::push_heap(frontier_pri_storage.begin(), frontier_pri_storage.begin() + frontier_pri_storage_size);
		++frontier_pri_storage_size;
	};

	auto pri_pop = [&]()
	{
		std::pop_heap(frontier_pri_storage.begin(), frontier_pri_storage.begin() + frontier_pri_storage_size);
		--frontier_pri_storage_size;
	};

	std::fill(frontier_info_table.begin(), frontier_info_table.begin() + table_size, AStarFrontier{INT_MAX, {0,0}});

	auto frontier_info_get = [&](std::pair<int, int> a) -> AStarFrontier&
	{
		int tablex = max_distance + a.first - start.first;
		int tabley = max_distance + a.second - start.second;
		return frontier_info_table.at(tabley * table_width + tablex);
	};

	pri_push(AStarCoordPri{start, 0});

	frontier_info_get(start) = {0, start};

	while (frontier_pri_storage_size > 0 && frontier_pri_storage_size < frontier_pri_storage.size())
	{
		const AStarCoordPri current = frontier_pri_storage[0];
		auto& current_info = frontier_info_get(current.coord);
		pri_pop();

		if (current.coord == goal)
		{
			std::pair<int, int> backtrack = current.coord;
			AStarFrontier& backtrack_info = frontier_info_get(backtrack);

			std::vector<std::pair<int, int>> result;

			result.reserve(frontier_pri_storage_size);
			result.push_back(backtrack);

			while (!(backtrack_info.from == backtrack))
			{
				backtrack = backtrack_info.from;
				backtrack_info = frontier_info_get(backtrack);

				result.push_back(backtrack);
			}

			std::reverse(result.begin(), result.end());

			return std::deque<std::pair<int, int>>(result.begin(), result.end());
		}

		for (int i = 0; i < 4; ++i)
		{
			int a = ((i & 2) >> 1);      // 0 if i=0..1, or i if i=2..3
			int b = ((i & 1) << 1) - 1; // -1 if i is odd, or 1 if i is even
			int dx =  a * b;
			int dy = !a * b;

			std::pair<int, int> nextcoord{current.coord.first + dx, current.coord.second + dy};
			AStarFrontier& next_info = frontier_info_get(nextcoord);
			int nextcost = current_info.cost + 1;
			

			if (!in_range(nextcoord.first, nextcoord.second))
				continue;

			if (!is_walkable(nextcoord.first, nextcoord.second))
				continue;

			if (nextcost < next_info.cost)
			{
				int nextpri = nextcost + util::path_length(nextcoord.first, nextcoord.second, goal.first, goal.second);
				next_info.cost = nextcost;
				pri_push(AStarCoordPri{nextcoord, nextpri});

				next_info.from = current.coord;
			}
		}
	}

	return {};
}
