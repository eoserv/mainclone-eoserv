
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

#include "../console.hpp"
#include "../util.hpp"

NPC_AI_HW2016_Apozen::NPC_AI_HW2016_Apozen(NPC* npc)
	: NPC_AI_Standard(npc)
	, skull{}
	, num_skulls(0)
	, charging(-6)
	, chase(0)
{ }

NPC_AI_HW2016_Apozen::~NPC_AI_HW2016_Apozen()
{ }

bool NPC_AI_HW2016_Apozen::IsInRange(int x, int y, int range) const
{
	return util::path_length(this->npc->x, this->npc->y, x, y) <= range;
}

void NPC_AI_HW2016_Apozen::Spawn()
{
	int speed = 2;
	int direction = 0;

	for (int i = 0; i < 4; ++i)
	{
		unsigned char index = this->npc->map->GenerateNPCIndex();

		if (index > 250)
			break;

		int xoff, yoff;
		
		switch (i)
		{
			case 0: xoff =  2; yoff =  2; break;
			case 1: xoff =  2; yoff = -2; break;
			case 2: xoff = -2; yoff =  2; break;
			case 3: xoff = -2; yoff = -2; break;
		}
		
		if (this->npc->map->world->hw2016_spawncount > i)
		{
			NPC *npc = new NPC(this->npc->map, 330, this->npc->x + xoff, this->npc->y + yoff, speed, direction, index, true);
			this->npc->map->npcs.push_back(npc);
			npc->ai.reset(new NPC_AI_HW2016_ApozenSkull(npc, this->npc));
			npc->Spawn();
			
			this->skull[i] = npc;
			++num_skulls;
		}
	}
}

bool NPC_AI_HW2016_Apozen::Dying()
{
	for (int i = 0; i < 4; ++i)
	{
		if (this->skull[i])
			this->skull[i]->Die();
	}
	
	return false;
}

void NPC_AI_HW2016_Apozen::Act()
{
	const int monster_imp1 = 347;
	const int monster_imp2 = 348;
	const int monster_twin_demon = 349;

	const int speed_fast = 0;
	const int speed_slow = 2;
	
	auto spawn_npc = [&](int mapid, int x, int y, int id, int speed) -> NPC*
	{
		Map* map = this->npc->map->world->GetMap(mapid);

		unsigned char index = map->GenerateNPCIndex();

		if (index > 251)
			return nullptr;

		NPC *npc = new NPC(map, id, x, y, speed, DIRECTION_DOWN, index, true);
		map->npcs.push_back(npc);
		npc->Spawn();
		
		return npc;
	};

	this->npc->hw2016_aposhield = (this->npc->map->npcs.size() > (this->num_skulls + 1));

	++this->charging;
	
	if (this->charging >= 200)
	{
		if (this->charging == 201)
			this->npc->Say("RUN FOOLS");

		if (this->charging == 220)
			this->charging = -15;
	}
	else if (this->charging >= 0)
	{
		switch (util::rand(0,4))
		{
			case 0: // summon imps
				if (!this->npc->hw2016_aposhield && --this->spawn_cooldown <= 0)
				{
					this->npc->Say("ARISE");
					this->npc->map->world->hw2016_apospawn = 1;
						
					this->charging = -15;
					
					this->spawn_cooldown = 1;
				}
				break;

			case 1: // summon demons
				if (!this->npc->hw2016_aposhield && --this->spawn_cooldown <= 0)
				{
					this->npc->Say("ARISE");
					this->npc->map->world->hw2016_apospawn = 2;

					this->charging = -15;
					
					this->spawn_cooldown = 1;
				}
				break;

			case 2: // teleport
				this->chase = 0;
				this->npc->Effect(3);
				this->npc->x = util::rand(10,17);
				this->npc->y = util::rand(5,17) - 1;
				for (int i = 0; i < 4; ++i)
				{
					if (this->skull[i])
					{
						int xoff, yoff;
						
						switch (i)
						{
							case 0: xoff =  2; yoff =  2; break;
							case 1: xoff =  2; yoff = -2; break;
							case 2: xoff = -2; yoff =  2; break;
							case 3: xoff = -2; yoff = -2; break;
						}

						this->skull[i]->Effect(3);
						this->skull[i]->x = this->npc->x + xoff;
						this->skull[i]->y = this->npc->y + yoff;
					}
				}
				for (Character* c : this->npc->map->characters)
					c->Refresh();
				this->npc->Effect(2);
				for (int i = 0; i < 4; ++i)
				{
					if (this->skull[i])
						this->skull[i]->Effect(3);
				}
				// no break!

			case 3: // aoe attack special!
				this->charging = 200;
				for (int i = 0; i < 4; ++i)
				{
					if (this->skull[i])
					{
						NPC_AI_HW2016_ApozenSkull* ai = static_cast<NPC_AI_HW2016_ApozenSkull*>(skull[i]->ai.get());
						
						if (ai)
							ai->charging = 200;
					}
				}
				break;
			
			case 4:
				chase = 0;
				this->charging = -10;
		}
	}
	else
	{
		Character* attacker = this->PickTargetRandomMD();

		if (attacker)
		{
			this->target = attacker;
			chase = 0;
		}
		else
		{
			++chase;
			
			this->target = this->PickTargetRandom();
			
			if (chase < 10)
			{
				if (this->target)
					this->SmartChase(this->target);
				
				{
					NPC* skull = this->skull[util::rand(0,3)];
					
					if (!skull) return;

					NPC_AI_HW2016_ApozenSkull* ai = static_cast<NPC_AI_HW2016_ApozenSkull*>(skull->ai.get());

					if (!ai) return;

					if (ai->charging < -1)
					{
						ai->charging = -1;
						this->charging++;
					}
				}
			}
		}

		if (this->target)
		{
			if (util::rand(0, 3) < num_skulls)
				this->npc->Attack(this->target);
		}
		else
		{
			NPC* skull = this->skull[util::rand(0,3)];
			
			if (!skull) return;

			NPC_AI_HW2016_ApozenSkull* ai = static_cast<NPC_AI_HW2016_ApozenSkull*>(skull->ai.get());			

			if (!ai) return;

			if (ai->charging < -1)
			{
				ai->charging = -1;
				this->charging++;
			}
		}
	}
}

void NPC_AI_HW2016_Apozen::SmartChase(Character* attacker, int range)
{
	int ax = this->npc->x;
	int ay = this->npc->y;
	int tx = attacker->x;
	int ty = attacker->y;
	int x = tx;
	int y = ty;
	
	if (range > 0)
	{
		int rx[2] = {
			util::clamp(ax, tx - range, tx + range),
			tx
		};

		int ry[2] = {
			ty,
			util::clamp(ay, ty - range, ty + range)
		};
		
		if (util::path_length(ax, ay, rx[0], ry[0]) <= util::path_length(ax, ay, rx[1], ry[1]))
		{
			x = rx[0];
			y = ry[0];
		}
		else
		{
			x = rx[1];
			y = ry[1];
		}
	}

	auto remove_from_damagelist = [this, attacker]()
	{
		// Will call our Unregister function too
		this->npc->UnregisterCharacter(attacker);
	};
	
	auto recalculate_path = [this, &remove_from_damagelist, x, y]()
	{
		this->path = this->npc->map->world->astar.FindPath({this->npc->x, this->npc->y}, {x, y}, 15,
			[this, x, y](int cx, int cy)
			{
				return (cx == x && cy == y)
				    || (this->npc->map->Walkable(cx, cy, true) && !this->npc->map->Occupied(cx, cy, Map::PlayerAndNPC));
			}
		);
		
		// OLD: Can't find a path to target, remove them from the damage list and do nothing this action
		// NEW: Can't find a path to target, use dumbwalk instead!
		if (this->path.size() < 2)
		{
			this->DumbChase(x, y);
			//remove_from_damagelist();
			return false;
		}
		
		this->path.pop_front();
		
		return true;
	};
	
	if (this->path.size() == 0)
	{
		if (!recalculate_path())
			return;
	}
	
	std::pair<int, int> target = this->path.back();
	
	// Target has moved away from the end of the pathfinding path
	if (util::path_length(target.first, target.second, x, y) > 1)
	{
		if (!recalculate_path())
			return;
	}
	
	std::pair<int, int> next_step = this->path.front();
	
	int direction_no = (next_step.first - this->npc->x) + 2 * (next_step.second - this->npc->y);
	
	Direction direction;
	
	switch (direction_no)
	{
		case -2: direction = DIRECTION_UP; break;
		case -1: direction = DIRECTION_LEFT; break;
		case 1:  direction = DIRECTION_RIGHT; break;
		case 2:  direction = DIRECTION_DOWN; break;
			
		default:
			Console::Wrn("NPC path-finding state is corrupt!");
			recalculate_path();
			return;
	}
	
	if (!this->npc->Walk(direction, true))
	{
		recalculate_path();
	}
	else
	{
		for (int i = 0; i < 4; ++i)
		{
			if (this->skull[i])
				this->skull[i]->Walk(direction, true);
		}
		
		this->path.pop_front();
	}
}
