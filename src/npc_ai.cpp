
/* $Id$
 * EOSERV is released under the zlib license.
 * See LICENSE.txt for more info.
 */

#include "npc_ai.hpp"

#include "character.hpp"
#include "config.hpp"
#include "map.hpp"
#include "npc.hpp"
#include "timer.hpp"
#include "world.hpp"

#include "console.hpp"
#include "util.hpp"

NPC_AI::NPC_AI(NPC* npc)
	: npc(npc)
{ }

NPC_AI::~NPC_AI()
{ }

void NPC_AI::HitBy(Character* character, bool spell, int damage)
{ }

void NPC_AI::Spawn()
{ }

void NPC_AI::Act()
{ }

bool NPC_AI::Dying()
{
	return false;
}

void NPC_AI::Unregister(Character* character)
{ }

NPC_AI_Legacy::NPC_AI_Legacy(NPC* npc)
	: NPC_AI(npc)
	, walk_idle_for(0)
{ }

NPC_AI_Legacy::~NPC_AI_Legacy()
{ }

void NPC_AI_Legacy::RandomWalk()
{
	int act;

	if (this->walk_idle_for == 0)
	{
		act = util::rand(1,10);
	}
	else
	{
		--this->walk_idle_for;
		act = 11;
	}

	if (act >= 1 && act <= 6) // 60% chance walk foward
	{
		this->npc->Walk(this->npc->direction);
	}
	else if (act >= 7 && act <= 9) // 30% change direction
	{
		this->npc->Walk(static_cast<Direction>(util::rand(0,3)));
	}
	else if (act == 10) // 10% take a break
	{
		this->walk_idle_for = util::rand(1,4);
	}
}

void NPC_AI_Legacy::Spawn()
{
	this->walk_idle_for = 0;
}

Character* NPC_AI_Legacy::PickTarget(bool legacy) const
{
	Character *attacker = nullptr;
	unsigned char attacker_distance = static_cast<int>(this->npc->map->world->config["NPCChaseDistance"]);
	unsigned short attacker_damage = 0;

	if (this->npc->ENF().type == ENF::Passive || this->npc->ENF().type == ENF::Aggressive)
	{
		UTIL_FOREACH_CREF(this->npc->damagelist, opponent)
		{
			if (opponent->attacker->map != this->npc->map || opponent->attacker->nowhere || opponent->last_hit < Timer::GetTime() - static_cast<double>(this->npc->map->world->config["NPCBoredTimer"]))
			{
				continue;
			}

			int distance = util::path_length(opponent->attacker->x, opponent->attacker->y, this->npc->x, this->npc->y);

			if (distance == 0)
				distance = 1;

			if ((distance < attacker_distance) || (distance == attacker_distance && opponent->damage > attacker_damage))
			{
				attacker = opponent->attacker;
				attacker_damage = opponent->damage;
				attacker_distance = distance;
			}
		}

		if (this->npc->parent)
		{
			UTIL_FOREACH_CREF(this->npc->parent->damagelist, opponent)
			{
				if (opponent->attacker->map != this->npc->map || opponent->attacker->nowhere || opponent->last_hit < Timer::GetTime() - static_cast<double>(this->npc->map->world->config["NPCBoredTimer"]))
				{
					continue;
				}

				int distance = util::path_length(opponent->attacker->x, opponent->attacker->y, this->npc->x, this->npc->y);

				if (distance == 0)
					distance = 1;

				if ((distance < attacker_distance) || (distance == attacker_distance && opponent->damage > attacker_damage))
				{
					attacker = opponent->attacker;
					attacker_damage = opponent->damage;
					attacker_distance = distance;
				}
			}
		}
	}

	if (((legacy || !attacker) && this->npc->ENF().type == ENF::Aggressive) || (this->npc->parent && attacker))
	{
		Character *closest = nullptr;
		unsigned char closest_distance = static_cast<int>(this->npc->map->world->config["NPCChaseDistance"]);

		if (attacker)
		{
			closest = attacker;
			closest_distance = std::min(closest_distance, attacker_distance);
		}

		UTIL_FOREACH(this->npc->map->characters, character)
		{
			if (character->IsHideNpc() || !character->CanInteractCombat())
				continue;

			int distance = util::path_length(character->x, character->y, this->npc->x, this->npc->y);

			if (distance == 0)
				distance = 1;

			if (distance < closest_distance)
			{
				closest = character;
				closest_distance = distance;
			}
		}

		if (closest)
		{
			attacker = closest;
		}
	}
	
	return attacker;
}

bool NPC_AI_Legacy::IsNextTo(int x, int y) const
{
	return util::path_length(this->npc->x, this->npc->y, x, y) <= 1;
}

void NPC_AI_Legacy::DumbChase(int x, int y)
{
	int xdiff = this->npc->x - x;
	int ydiff = this->npc->y - y;
	int absxdiff = std::abs(xdiff);
	int absydiff = std::abs(ydiff);

	if (absxdiff > absydiff)
	{
		if (xdiff < 0)
		{
			this->npc->direction = DIRECTION_RIGHT;
		}
		else
		{
			this->npc->direction = DIRECTION_LEFT;
		}
	}
	else
	{
		if (ydiff < 0)
		{
			this->npc->direction = DIRECTION_DOWN;
		}
		else
		{
			this->npc->direction = DIRECTION_UP;
		}
	}

	if (this->npc->Walk(this->npc->direction) == Map::WalkFail)
	{
		if (this->npc->direction == DIRECTION_UP || this->npc->direction == DIRECTION_DOWN)
		{
			if (xdiff < 0)
			{
				this->npc->direction = DIRECTION_RIGHT;
			}
			else
			{
				this->npc->direction = DIRECTION_LEFT;
			}
		}

		if (this->npc->Walk(static_cast<Direction>(this->npc->direction)) == Map::WalkFail)
		{
			this->npc->Walk(static_cast<Direction>(util::rand(0,3)));
		}
	}
}

void NPC_AI_Legacy::Act()
{
	Character *attacker = this->PickTarget();

	if (attacker)
	{
		if (IsNextTo(attacker->x, attacker->y))
			this->npc->Attack(attacker);
		else
			this->DumbChase(attacker->x, attacker->y);
	}
	else
	{
		this->RandomWalk();
	}
}

NPC_AI_Standard::NPC_AI_Standard(NPC* npc)
	: NPC_AI_Legacy(npc)
	, target(nullptr)
{ }

NPC_AI_Standard::~NPC_AI_Standard()
{ }

void NPC_AI_Standard::HitBy(Character* character, bool spell, int damage)
{
	this->target = nullptr;
	this->path.clear();
	NPC_AI_Legacy::HitBy(character, spell, damage);
}

void NPC_AI_Standard::Act()
{
	Character* attacker = this->PickTarget();
	
	if (this->target != attacker)
	{
		this->target = attacker;
		this->path.clear();
	}

	if (this->target)
	{
		if (this->IsNextTo(this->target->x, this->target->y))
			this->npc->Attack(this->target);
		else
			this->SmartChase(this->target);
	}
	else
	{
		if (this->npc->ENF().type == ENF::Aggressive)
		{
			// return to spawn point
			if (util::path_length(this->npc->x, this->npc->y, this->npc->spawn_x, this->npc->spawn_y) > 3)
				this->DumbChase(this->npc->spawn_x, this->npc->spawn_y);
		}
		else
		{
			this->RandomWalk();
		}
	}
}

void NPC_AI_Standard::Unregister(Character* character)
{
	if (this->target == character)
	{
		this->target = nullptr;
		this->path.clear();
	}

	NPC_AI_Legacy::Unregister(character);
}

void NPC_AI_Standard::SmartChase(Character* attacker, int range)
{
	int ax = this->npc->x;
	int ay = this->npc->y;
	int tx = attacker->x;
	int ty = attacker->y;
	int x = tx;
	int y = ty;
	
	if (range > 0)
	{
		int rx[2] = {
			util::clamp(ax, tx - range, tx + range),
			tx
		};

		int ry[2] = {
			ty,
			util::clamp(ay, ty - range, ty + range)
		};
		
		if (util::path_length(ax, ay, rx[0], ry[0]) <= util::path_length(ax, ay, rx[1], ry[1]))
		{
			x = rx[0];
			y = ry[0];
		}
		else
		{
			x = rx[1];
			y = ry[1];
		}
	}

	auto remove_from_damagelist = [this, attacker]()
	{
		// Will call our Unregister function too
		this->npc->UnregisterCharacter(attacker);
	};
	
	auto recalculate_path = [this, &remove_from_damagelist, x, y]()
	{
		this->path = this->npc->map->world->astar.FindPath({this->npc->x, this->npc->y}, {x, y}, 15,
			[this, x, y](int cx, int cy)
			{
				return (cx == x && cy == y)
				    || (this->npc->map->Walkable(cx, cy, true) && !this->npc->map->Occupied(cx, cy, Map::PlayerAndNPC));
			}
		);
		
		// OLD: Can't find a path to target, remove them from the damage list and do nothing this action
		// NEW: Can't find a path to target, use dumbwalk instead!
		if (this->path.size() < 2)
		{
			this->DumbChase(x, y);
			//remove_from_damagelist();
			return false;
		}
		
		this->path.pop_front();
		
		return true;
	};
	
	if (this->path.size() == 0)
	{
		if (!recalculate_path())
			return;
	}
	
	std::pair<int, int> target = this->path.back();
	
	// Target has moved away from the end of the pathfinding path
	if (util::path_length(target.first, target.second, x, y) > 1)
	{
		if (!recalculate_path())
			return;
	}
	
	std::pair<int, int> next_step = this->path.front();
	
	int direction_no = (next_step.first - this->npc->x) + 2 * (next_step.second - this->npc->y);
	
	Direction direction;
	
	switch (direction_no)
	{
		case -2: direction = DIRECTION_UP; break;
		case -1: direction = DIRECTION_LEFT; break;
		case 1:  direction = DIRECTION_RIGHT; break;
		case 2:  direction = DIRECTION_DOWN; break;
			
		default:
			Console::Wrn("NPC path-finding state is corrupt!");
			recalculate_path();
			return;
	}
	
	if (!this->npc->Walk(direction, true))
	{
		recalculate_path();
	}
	else
	{
		this->path.pop_front();
	}
}
