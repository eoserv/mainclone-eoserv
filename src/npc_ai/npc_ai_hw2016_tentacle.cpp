
/* $Id$
 * EOSERV is released under the zlib license.
 * See LICENSE.txt for more info.
 */

#include "npc_ai_hw2016_tentacle.hpp"

#include "../character.hpp"
#include "../config.hpp"
#include "../map.hpp"
#include "../npc.hpp"
#include "../timer.hpp"
#include "../world.hpp"

#include "../util.hpp"

NPC_AI_HW2016_Tentacle::NPC_AI_HW2016_Tentacle(NPC* npc)
	: NPC_AI_Standard(npc)
	, charging(-1)
{ }

NPC_AI_HW2016_Tentacle::~NPC_AI_HW2016_Tentacle()
{ }

bool NPC_AI_HW2016_Tentacle::IsInRange(int x, int y, int range) const
{
	return util::path_length(this->npc->x, this->npc->y, x, y) <= range;
}

void NPC_AI_HW2016_Tentacle::Act()
{
	Character* attacker = this->PickTargetRandomMD();
	
	if (!attacker)
		attacker = this->PickTargetRandomRange();
	
	if (this->target != attacker)
	{
		this->target = attacker;
	}

	if (this->target)
	{
		if (this->IsNextTo(this->target->x, this->target->y))
		{
			this->charging = -1;
			this->npc->Attack(this->target);
		}
		else if (this->IsInRange(this->target->x, this->target->y, 8))
		{
			++this->charging;
			
			if (this->charging == 102)
			{
				this->npc->map->SpellAttackNPC(this->npc, this->target, 26);
				this->charging = -1;
			}
			else if (this->charging == 0)
			{
				this->charging = 100;
				
				if (util::rand(0, 300) == 69)
					this->npc->Say("I know you've seen enough hentai to know where this is going");
			}
		}
		else
		{
			this->charging = -1;
		}
	}
}

