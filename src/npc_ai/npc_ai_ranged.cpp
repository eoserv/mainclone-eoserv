
/* $Id$
 * EOSERV is released under the zlib license.
 * See LICENSE.txt for more info.
 */

#include "npc_ai_ranged.hpp"

#include "../character.hpp"
#include "../config.hpp"
#include "../map.hpp"
#include "../npc.hpp"
#include "../timer.hpp"
#include "../world.hpp"

#include "../util.hpp"

NPC_AI_Ranged::NPC_AI_Ranged(NPC* npc)
	: NPC_AI_Standard(npc)
{ }

NPC_AI_Ranged::~NPC_AI_Ranged()
{ }

Character* NPC_AI_Ranged::PickTargetRanged(int range, bool legacy) const
{
	Character *attacker = nullptr;
	unsigned char attacker_distance = static_cast<int>(this->npc->map->world->config["NPCChaseDistance"]);
	unsigned short attacker_damage = 0;
	
	int ax = this->npc->x;
	int ay = this->npc->y;
	
	auto eval_opponent = [&](const std::unique_ptr<NPC_Opponent>& opponent)
	{
		int tx = opponent->attacker->x;
		int ty = opponent->attacker->y;

		int x[2] = {
			util::clamp(ax, tx - range, tx + range),
			tx
		};

		int y[2] = {
			ty,
			util::clamp(ay, ty - range, ty + range)
		};

		for (int i = 0; i < 2; ++i)
		{
			int distance = util::path_length(x[i], y[i], ax, ay);

			if (distance == 0)
				distance = 1;

			if ((distance < attacker_distance) || (distance == attacker_distance && opponent->damage > attacker_damage))
			{
				attacker = opponent->attacker;
				attacker_damage = opponent->damage;
				attacker_distance = distance;
			}
		}
	};

	if (this->npc->ENF().type == ENF::Passive || this->npc->ENF().type == ENF::Aggressive)
	{
		UTIL_FOREACH_CREF(this->npc->damagelist, opponent)
		{
			if (opponent->attacker->map != this->npc->map || opponent->attacker->nowhere || opponent->last_hit < Timer::GetTime() - static_cast<double>(this->npc->map->world->config["NPCBoredTimer"]))
				continue;

			eval_opponent(opponent);
		}

		if (this->npc->parent)
		{
			UTIL_FOREACH_CREF(this->npc->parent->damagelist, opponent)
			{
				if (opponent->attacker->map != this->npc->map || opponent->attacker->nowhere || opponent->last_hit < Timer::GetTime() - static_cast<double>(this->npc->map->world->config["NPCBoredTimer"]))
					continue;

				eval_opponent(opponent);
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

bool NPC_AI_Ranged::IsInStraightRange(int x, int y, int range) const
{
	if (util::path_length(this->npc->x, this->npc->y, x, y) <= range)
	{
		if (this->npc->x == x)
		{
			int from = std::min<int>(y, this->npc->y);
			int to = std::max<int>(y, this->npc->y);
		
			for (int i = from + 1; i < to; ++i)
			{
				if (!this->npc->map->Walkable(x, i))
					return false;
			}
		}
		else if (this->npc->y == y)
		{
			int from = std::min<int>(x, this->npc->x);
			int to = std::max<int>(x, this->npc->x);
		
			for (int i = from + 1; i < to; ++i)
			{
				if (!this->npc->map->Walkable(i, y))
					return false;
			}
		}
		else
		{
			return false;
		}
		
		return true;
	}
	
	return false;
}

void NPC_AI_Ranged::Act()
{
	Character* attacker = this->PickTargetRanged(6);
	
	if (this->target != attacker)
	{
		this->target = attacker;
		this->path.clear();
	}

	if (this->target)
	{		
		if (this->IsInStraightRange(this->target->x, this->target->y, 6))
		{
			this->npc->Attack(this->target);
		}
		else
		{
			// Range target seeking isn't good without wall checks!
			//this->SmartChase(this->target, 7);
			this->SmartChase(this->target);
		}
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
