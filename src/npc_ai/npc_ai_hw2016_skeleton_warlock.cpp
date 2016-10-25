
/* $Id$
 * EOSERV is released under the zlib license.
 * See LICENSE.txt for more info.
 */

#include "npc_ai_hw2016_skeleton_warlock.hpp"

#include "../character.hpp"
#include "../config.hpp"
#include "../map.hpp"
#include "../npc.hpp"
#include "../timer.hpp"
#include "../world.hpp"

#include "../util.hpp"

NPC_AI_HW2016_Skeleton_Warlock::NPC_AI_HW2016_Skeleton_Warlock(NPC* npc)
	: NPC_AI_Standard(npc)
	, charging(-4)
{ }

NPC_AI_HW2016_Skeleton_Warlock::~NPC_AI_HW2016_Skeleton_Warlock()
{ }

bool NPC_AI_HW2016_Skeleton_Warlock::IsInRange(int x, int y, int range) const
{
	return util::path_length(this->npc->x, this->npc->y, x, y) <= range;
}

void NPC_AI_HW2016_Skeleton_Warlock::Act()
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
				this->npc->map->SpellAttackNPC(this->npc, this->target, 4);
				this->charging = -3;
			}
			else if (this->charging == 202)
			{
				this->npc->map->SpellAttackNPC(this->npc, this->target, 5);
				this->charging = -3;
			}
			else if (this->charging == 0)
			{
				this->path.clear();
				
				switch (util::rand(0,4))
				{
					case 0:
						this->npc->Effect(29);
						this->charging = 100;
						break;
					case 1:
						this->npc->Effect(29);
						this->charging = 200;
						break;
					case 3:
					case 4:
						this->charging = -4;

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

