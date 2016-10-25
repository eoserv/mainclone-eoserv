
/* $Id$
 * EOSERV is released under the zlib license.
 * See LICENSE.txt for more info.
 */

#include "npc_ai_magic.hpp"

#include "../character.hpp"
#include "../config.hpp"
#include "../map.hpp"
#include "../npc.hpp"
#include "../timer.hpp"
#include "../world.hpp"

#include "../util.hpp"

NPC_AI_Magic::NPC_AI_Magic(NPC* npc, int charge_effect_id, int spell_id)
	: NPC_AI_Standard(npc)
	, charge_effect_id(charge_effect_id)
	, spell_id(spell_id)
	, charging(-1)
{ }

NPC_AI_Magic::~NPC_AI_Magic()
{ }

bool NPC_AI_Magic::IsInRange(int x, int y, int range) const
{
	return util::path_length(this->npc->x, this->npc->y, x, y) <= range;
}

void NPC_AI_Magic::Act()
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
			
			if (this->charging == 2)
			{
				this->npc->map->SpellAttackNPC(this->npc, this->target, spell_id);
				this->charging = -3;
			}
			else if (this->charging == 0)
			{
				this->path.clear();
				this->npc->Effect(charge_effect_id);
				//this->npc->Say("tentacles");
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

