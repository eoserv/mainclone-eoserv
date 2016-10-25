
/* $Id$
 * EOSERV is released under the zlib license.
 * See LICENSE.txt for more info.
 */

#include "npc_ai_hw2016_banshee.hpp"

#include "../character.hpp"
#include "../config.hpp"
#include "../map.hpp"
#include "../npc.hpp"
#include "../timer.hpp"
#include "../world.hpp"

#include "../util.hpp"

NPC_AI_HW2016_Banshee::NPC_AI_HW2016_Banshee(NPC* npc)
	: NPC_AI_Standard(npc)
	, charging(-1)
{ }

NPC_AI_HW2016_Banshee::~NPC_AI_HW2016_Banshee()
{ }

bool NPC_AI_HW2016_Banshee::IsInRange(int x, int y, int range) const
{
	return util::path_length(this->npc->x, this->npc->y, x, y) <= range;
}

NPC* NPC_AI_HW2016_Banshee::PickHealTarget(int range) const
{
	NPC* buddy = nullptr;
	int buddy_hp = 0;
	
	int ax = this->npc->x;
	int ay = this->npc->y;
	
	auto eval_npc = [&](NPC* npc)
	{
		int tx = npc->x;
		int ty = npc->y;

		int distance = util::path_length(ax, ay, tx, ty);

		if ((distance < range) && npc->hp < buddy_hp)
		{
			buddy = npc;
			buddy_hp = npc->hp;
		}
	};
	
	for (NPC* npc : this->npc->map->NPCsInRange(ax, ay, 8))
	{
		eval_npc(npc);
	}
	
	return buddy;
}

void NPC_AI_HW2016_Banshee::Act()
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
				this->npc->map->SpellAttackNPC(this->npc, this->target, 23);
				this->charging = -3;
			}
			else if (this->charging == 202)
			{
				/*NPC* buddy = this->PickHealTarget(8);
				
				if (buddy)
					this->npc->map->SpellAttackNPC(this->npc, buddy, 28);*/
				
				this->charging = -3;
			}
			else if (this->charging == 302)
			{
				/*NPC* buddy = this->PickHealTarget();
				
				if (buddy)
					this->npc->map->SpellAttackNPC(this->npc, buddy, 11);*/
				
				this->charging = -3;
			}
			else if (this->charging == 0)
			{
				this->path.clear();
				
				switch (util::rand(0,3))
				{
					case 0:
					case 1:
					case 2:
						this->npc->Effect(29);
						this->charging = 100;
						break;

					/*
					case 3:
					case 4:
						this->npc->Effect(29);
						this->charging = 200;
						break;

					case 5:
						this->npc->Effect(29);
						this->charging = 300;
						break;
					*/

					case 3:
						this->charging = -1;

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

