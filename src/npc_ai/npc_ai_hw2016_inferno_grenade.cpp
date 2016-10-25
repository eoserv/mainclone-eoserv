
/* $Id$
 * EOSERV is released under the zlib license.
 * See LICENSE.txt for more info.
 */

#include "npc_ai_hw2016_inferno_grenade.hpp"

#include "../character.hpp"
#include "../config.hpp"
#include "../map.hpp"
#include "../npc.hpp"
#include "../timer.hpp"
#include "../world.hpp"

#include "../util.hpp"

#include <list>

NPC_AI_HW2016_Inferno_Grenade::NPC_AI_HW2016_Inferno_Grenade(NPC* npc)
	: NPC_AI_Standard(npc)
	, charging(-40)
{ }

NPC_AI_HW2016_Inferno_Grenade::~NPC_AI_HW2016_Inferno_Grenade()
{ }

bool NPC_AI_HW2016_Inferno_Grenade::IsInRange(int x, int y, int range) const
{
	return util::path_length(this->npc->x, this->npc->y, x, y) <= range;
}

bool NPC_AI_HW2016_Inferno_Grenade::Dying()
{
	if (this->charging < 0)
		this->charging = 0;

	this->npc->hp = this->npc->ENF().hp;

	return true;
}

void NPC_AI_HW2016_Inferno_Grenade::Act()
{
	++charging;

	if (charging > 6)
	{
		std::list<Character*> burned;

		for (int x = -2; x <= 2; ++x)
		{
			for (int y = -2; y <= 2; ++y)
			{
				if ((x != 0 || y != 0) && (std::abs(x) + std::abs(y)) < 4)
				{
					for (Character* c : this->npc->map->CharactersInRange(this->npc->x + x, this->npc->y + y, 0))
						burned.push_back(c);

					this->npc->map->SpellEffect(this->npc->x + x, this->npc->y + y, 0);
				}
			}
		}
		
		for (Character* c : burned)
		{
			c->SpikeDamage(c->maxhp * 0.4);
			
			if (c->hp < 0)
				c->DeathRespawn();
		}
		
		this->npc->Die();
	}
	else if (charging >= 0 && charging % 2 == 0)
	{
		this->npc->Effect(17);
		this->npc->hp = this->npc->ENF().hp;
	}
	else if (charging < 0)
	{
		Character* attacker = this->PickTarget();
		
		if (this->target != attacker)
		{
			this->target = attacker;
			this->path.clear();
		}
		
		if (this->target)
		{
			this->SmartChase(this->target);
			
			if (this->IsNextTo(this->target->x, this->target->y))
				charging = 0;
		}
	}
}

