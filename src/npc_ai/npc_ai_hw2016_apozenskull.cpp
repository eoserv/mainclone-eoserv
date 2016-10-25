
/* $Id$
 * EOSERV is released under the zlib license.
 * See LICENSE.txt for more info.
 */

#include "npc_ai_hw2016_apozenskull.hpp"

#include "../character.hpp"
#include "../config.hpp"
#include "../map.hpp"
#include "../npc.hpp"
#include "../timer.hpp"
#include "../world.hpp"

#include "../util.hpp"

NPC_AI_HW2016_ApozenSkull::NPC_AI_HW2016_ApozenSkull(NPC* npc)
	: NPC_AI_Standard(npc)
	, charging(-2000)
{ }

NPC_AI_HW2016_ApozenSkull::~NPC_AI_HW2016_ApozenSkull()
{ }

bool NPC_AI_HW2016_ApozenSkull::IsInRange(int x, int y, int range) const
{
	return util::path_length(this->npc->x, this->npc->y, x, y) <= range;
}

NPC* NPC_AI_HW2016_ApozenSkull::PickHealTarget(int range) const
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

bool NPC_AI_HW2016_ApozenSkull::Dying()
{
	if (this->charging < 0)
		this->charging = 0;

	this->npc->hp = this->npc->ENF().hp;

	return true;
}

void NPC_AI_HW2016_ApozenSkull::Act()
{
	++charging;

	if (charging > 6)
	{
		for (int x = -2; x <= 2; ++x)
		{
			for (int y = -2; y <= 2; ++y)
			{
				if (x != 0 || y != 0)
				{
					this->npc->map->SpellEffect(this->npc->x + x, this->npc->y + y, 0);
				}
			}
		}
		
		this->npc->Die();
		
		for (Character* c : this->npc->map->characters)
			c->SpikeDamage(666);
	}
	else if (charging > 0 && charging % 2 ==0)
	{
		this->npc->Effect(17);
		this->npc->hp = this->npc->ENF().hp;
	}
}

