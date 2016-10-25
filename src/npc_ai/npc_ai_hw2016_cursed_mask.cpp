
/* $Id$
 * EOSERV is released under the zlib license.
 * See LICENSE.txt for more info.
 */

#include "npc_ai_hw2016_cursed_mask.hpp"

#include "../character.hpp"
#include "../config.hpp"
#include "../map.hpp"
#include "../npc.hpp"
#include "../timer.hpp"
#include "../world.hpp"

#include "../util.hpp"

NPC_AI_HW2016_Cursed_Mask::NPC_AI_HW2016_Cursed_Mask(NPC* npc)
	: NPC_AI_Standard(npc)
	, charging(-4)
{ }

NPC_AI_HW2016_Cursed_Mask::~NPC_AI_HW2016_Cursed_Mask()
{ }

bool NPC_AI_HW2016_Cursed_Mask::IsInRange(int x, int y, int range) const
{
	return util::path_length(this->npc->x, this->npc->y, x, y) <= range;
}

void NPC_AI_HW2016_Cursed_Mask::Act()
{
	Character* attacker = this->PickTarget();
	
	if (this->target != attacker)
	{
		this->target = attacker;
		this->path.clear();
	}

	if (this->target)
	{
		if (this->IsInRange(this->target->x, this->target->y, 8))
		{
			++this->charging;
			
			if (this->charging == 102)
			{
				this->npc->map->SpellAttackNPC(this->npc, this->target, 29);
				this->charging = -3;
			}
			else if (this->charging == 0)
			{
				this->path.clear();
				
				switch (util::rand(0,2))
				{
					case 0:
					case 1:
						this->npc->Effect(29);
						this->charging = 100;
						break;
					case 3:
						this->charging = -2;

						if (this->IsNextTo(this->target->x, this->target->y))
							this->npc->Attack(this->target);
						else
							this->SmartChase(this->target);

						break;
						
				}
			}
			else if (this->charging < 0)
			{
				if (this->IsNextTo(this->target->x, this->target->y))
					this->npc->Attack(this->target);
				else
					this->SmartChase(this->target);
			}
		}
		else
		{
			this->charging = -1;
			this->SmartChase(this->target);
		}
	}
	else
	{
		this->charging = -1;
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

