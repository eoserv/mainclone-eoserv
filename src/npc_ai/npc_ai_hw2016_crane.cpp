
/* $Id$
 * EOSERV is released under the zlib license.
 * See LICENSE.txt for more info.
 */

#include "npc_ai_hw2016_crane.hpp"

#include "../character.hpp"
#include "../config.hpp"
#include "../map.hpp"
#include "../npc.hpp"
#include "../timer.hpp"
#include "../world.hpp"

#include "../util.hpp"

NPC_AI_HW2016_Crane::NPC_AI_HW2016_Crane(NPC* npc)
	: NPC_AI_Standard(npc)
{ }

NPC_AI_HW2016_Crane::~NPC_AI_HW2016_Crane()
{ }

bool NPC_AI_HW2016_Crane::Dying()
{
	this->npc->hp = this->npc->ENF().hp;
	return true;
}

void NPC_AI_HW2016_Crane::Act()
{
	Character *attacker = this->PickTarget(true);

	if (attacker && IsNextTo(attacker->x, attacker->y))
	{
		this->npc->Attack(attacker);
	}
	else
	{
		if (this->npc->map->world->hw2016_hallway <= 40)
		{
			if (this->npc->y >= 86 - this->npc->map->world->hw2016_hallway)
			{
				if (!this->npc->Walk(DIRECTION_DOWN))
					this->RandomWalk();
			}
			else if (this->npc->y > 90 - this->npc->map->world->hw2016_hallway)
			{
				if (!this->npc->Walk(DIRECTION_UP))
					this->RandomWalk();
			}
			else
			{
				this->RandomWalk();
			}
		}
		else
		{
			if (this->npc->y > 40)
			{
				if (!this->npc->Walk(DIRECTION_UP))
					this->RandomWalk();
			}
			else
			{
				this->RandomWalk();
			}
		}
	}
}

