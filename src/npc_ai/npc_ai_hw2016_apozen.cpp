
/* $Id$
 * EOSERV is released under the zlib license.
 * See LICENSE.txt for more info.
 */

#include "npc_ai_hw2016_apozen.hpp"

#include "../character.hpp"
#include "../config.hpp"
#include "../map.hpp"
#include "../npc.hpp"
#include "../timer.hpp"
#include "../world.hpp"
#include "npc_ai_hw2016_apozenskull.hpp"

#include "../util.hpp"

NPC_AI_HW2016_Apozen::NPC_AI_HW2016_Apozen(NPC* npc)
	: NPC_AI_Standard(npc)
	, skull{}
	, charging(-6)
{ }

NPC_AI_HW2016_Apozen::~NPC_AI_HW2016_Apozen()
{ }

bool NPC_AI_HW2016_Apozen::IsInRange(int x, int y, int range) const
{
	return util::path_length(this->npc->x, this->npc->y, x, y) <= range;
}

void NPC_AI_HW2016_Apozen::Spawn()
{
	int speed = 3;
	int direction = 0;

	for (int i = 0; i < 4; ++i)
	{
		unsigned char index = this->npc->map->GenerateNPCIndex();

		if (index > 250)
			break;

		int xoff, yoff;
		
		switch (i)
		{
			case 0: xoff = -1; yoff = -1; break;
			case 1: xoff =  1; yoff = -1; break;
			case 2: xoff = -1; yoff =  1; break;
			case 3: xoff =  1; yoff =  1; break;
		}
		
		NPC *npc = new NPC(this->npc->map, 330, this->npc->x + xoff, this->npc->y + yoff, speed, direction, index, true);
		this->npc->map->npcs.push_back(npc);
		npc->ai.reset(new NPC_AI_HW2016_ApozenSkull(npc));
		npc->Spawn();
		
		this->skull[i] = npc;
	}
}

bool NPC_AI_HW2016_Apozen::Dying()
{
	for (int i = 0; i < 4; ++i)
	{
		if (this->skull[i] && this->skull[i]->alive)
			this->skull[i]->Die();
	}
	
	return false;
}

void NPC_AI_HW2016_Apozen::Act()
{
	this->npc->hp = 10;

	if (charging == 0)
	{
		switch (util::rand(0,3))
		{
			case 0:
			case 1:
			case 2:
				this->npc->Effect(12);
				this->charging = -5;
				break;
		}
	}
	else
	{
		Character* attacker = this->PickTarget();
	
		if (this->target != attacker)
		{
			this->target = attacker;
			this->path.clear();
		}

		if (this->target)
		{
			this->npc->Attack(this->target);
		}
	}
}

