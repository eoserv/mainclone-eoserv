
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
#include "npc_ai_hw2016_apozen.hpp"

#include "../console.hpp"
#include "../util.hpp"

NPC_AI_HW2016_ApozenSkull::NPC_AI_HW2016_ApozenSkull(NPC* npc, NPC* apozen)
	: NPC_AI_Standard(npc)
	, charging(-4)
	, apozen(apozen)
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
	this->npc->Effect(16);

	if (this->apozen)
	{
		int weak = 0;
				
		for (int i = 0; i < 4; ++i)
		{
			NPC_AI_HW2016_Apozen* ai = static_cast<NPC_AI_HW2016_Apozen*>(this->apozen->ai.get());

			if (ai && ai->skull[i] == this->npc)
				ai->skull[i] = nullptr;
			
			if (ai && ai->skull[i] == nullptr)
				++weak;
		}

		this->apozen->hw2016_apoweak = weak;

		this->npc->map->world->hw2016_dyingskull.push_back({this->npc->x, this->npc->y});
	}

	return false;
}

void NPC_AI_HW2016_ApozenSkull::Act()
{
	if (!this->apozen)
		return;

	this->npc->hw2016_aposhield = (apozen->hw2016_aposhield);
		
	if (true)
	{
		this->target = this->PickTargetRandom();
		
		NPC_AI_HW2016_Apozen* apozen_ai = static_cast<NPC_AI_HW2016_Apozen*>(this->apozen->ai.get());
		
		if (apozen_ai)
		{
			for (int i = 0; i < apozen_ai->num_skulls; ++i)
			{
				if (apozen_ai->skull[i])
				{
					NPC_AI_HW2016_ApozenSkull* skull_ai = static_cast<NPC_AI_HW2016_ApozenSkull*>(apozen_ai->skull[i]->ai.get());
					
					if (skull_ai)
					{
						if (skull_ai != this && skull_ai->target == this->target)
							this->target = nullptr;
					}
				}
			}
		}
		
		if (this->charging < 200)
		{
			if (!this->target)
				return;
		}
		
		++this->charging;
		
		if (this->charging >= 204)
		{
			std::list<Character*> burned;

			for (int x = -1; x <= 1; ++x)
			{
				for (int y = -1; y <= 1; ++y)
				{
					if ((x != 0 || y != 0))
					{
						for (Character* c : this->npc->map->CharactersInRange(this->npc->x + x, this->npc->y + y, 0))
							burned.push_back(c);

						this->npc->map->SpellEffect(this->npc->x + x, this->npc->y + y, 0);
					}
				}
			}
			
			for (Character* c : burned)
			{
				int hit = c->maxhp * 0.2;
				
				if (hit > c->hp)
					hit = c->hp - 1;

				if (hit > 0)
					c->SpikeDamage(hit);
			}
			
			if (this->charging >= 206)
				this->charging = -5;
		}
		else if (charging >= 200 && charging % 2 == 0)
		{
			this->npc->Effect(17);
		}
		else if (this->charging == 103)
		{
			this->npc->map->SpellAttackNPC(this->npc, this->target, 10);
			this->charging = -5;
		}
		else if (this->charging >= 0 && this->charging < 100)
		{
			int r = util::rand(0,3);
			
			switch (r)
			{
				case 0:
					this->npc->Effect(29);
					this->charging = 100;
					break;
				case 1:
					this->npc->Effect(29);
					//this->charging = 200;
					this->charging = 100;
					break;
				case 2:
				case 3:
					this->charging = -4;
					
					this->target = this->PickTargetRandomMD();

					if (this->target && this->IsNextTo(this->target->x, this->target->y))
						this->npc->Attack(this->target);

					break;
					
			}
		}
		else if (this->charging < 0)
		{
			this->target = this->PickTargetRandomMD();
			
			if (this->target && this->IsNextTo(this->target->x, this->target->y))
				this->npc->Attack(this->target);
		}
	}
	else
	{
		this->target = this->PickTargetRandomMD();
		
		if (this->target && this->IsNextTo(this->target->x, this->target->y))
			this->npc->Attack(this->target);
	}
}

