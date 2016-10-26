
/* $Id$
 * EOSERV is released under the zlib license.
 * See LICENSE.txt for more info.
 */

#include "world.hpp"

#include "character.hpp"
#include "command_source.hpp"
#include "config.hpp"
#include "database.hpp"
#include "eoclient.hpp"
#include "eodata.hpp"
#include "eoplus.hpp"
#include "eoserver.hpp"
#include "guild.hpp"
#include "i18n.hpp"
#include "map.hpp"
#include "npc.hpp"
#include "npc_data.hpp"
#include "packet.hpp"
#include "party.hpp"
#include "player.hpp"
#include "quest.hpp"
#include "timer.hpp"
#include "commands/commands.hpp"
#include "handlers/handlers.hpp"
#include "npc_ai/npc_ai_hw2016_apozen.hpp"
#include "npc_ai/npc_ai_hw2016_apozenskull.hpp"

#include "console.hpp"
#include "hash.hpp"
#include "util.hpp"
#include "util/secure_string.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <ctime>
#include <cstring>
#include <limits>
#include <list>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

void world_spawn_npcs(void *world_void)
{
	World *world(static_cast<World *>(world_void));

	double spawnrate = world->config["SpawnRate"];
	double current_time = Timer::GetTime();
	UTIL_FOREACH(world->maps, map)
	{
		UTIL_FOREACH(map->npcs, npc)
		{
			if ((!npc->alive && npc->dead_since + (double(npc->spawn_time) * spawnrate) < current_time)
			 && (!npc->ENF().child || (npc->parent && npc->parent->alive && world->config["RespawnBossChildren"])))
			{
#ifdef DEBUG
				Console::Dbg("Spawning NPC %i on map %i", npc->id, map->id);
#endif // DEBUG
				npc->Spawn();
			}
		}
	}
}

void world_act_npcs(void *world_void)
{
	World *world(static_cast<World *>(world_void));

	double current_time = Timer::GetTime();
	UTIL_FOREACH(world->maps, map)
	{
		NPC* check;
		
		if (map->npcs.size() > 0)
			check = map->npcs[0];

		UTIL_FOREACH(map->npcs, npc)
		{
			if (npc->alive && npc->last_act + npc->act_speed < current_time)
			{
				npc->Act();

				if (check != map->npcs[0])
					break;
			}
		}
	}
}

void world_recover(void *world_void)
{
	World *world(static_cast<World *>(world_void));

	UTIL_FOREACH(world->characters, character)
	{
		bool updated = false;

		if (character->hp != character->maxhp)
		{
			if (character->sitting != SIT_STAND) character->hp += character->maxhp * double(world->config["SitHPRecoverRate"]);
			else                                 character->hp += character->maxhp * double(world->config["HPRecoverRate"]);

			character->hp = std::min(character->hp, character->maxhp);
			updated = true;

			if (character->party)
			{
				character->party->UpdateHP(character);
			}
		}

		if (character->tp != character->maxtp)
		{
			if (character->sitting != SIT_STAND) character->tp += character->maxtp * double(world->config["SitTPRecoverRate"]);
			else                                 character->tp += character->maxtp * double(world->config["TPRecoverRate"]);

			character->tp = std::min(character->tp, character->maxtp);
			updated = true;
		}

		if (updated)
		{
			PacketBuilder builder(PACKET_RECOVER, PACKET_PLAYER, 6);
			builder.AddShort(character->hp);
			builder.AddShort(character->tp);
			builder.AddShort(0); // ?
			character->Send(builder);
		}
	}
}

void world_npc_recover(void *world_void)
{
	World *world(static_cast<World *>(world_void));

	UTIL_FOREACH(world->maps, map)
	{
		UTIL_FOREACH(map->npcs, npc)
		{
			if (npc->alive && npc->hp < npc->ENF().hp)
			{
				npc->hp += npc->ENF().hp * double(world->config["NPCRecoverRate"]);

				npc->hp = std::min(npc->hp, npc->ENF().hp);
			}
		}
	}
}

void world_warp_suck(void *world_void)
{
	struct Warp_Suck_Action
	{
		Character *character;
		short map;
		unsigned char x;
		unsigned char y;

		Warp_Suck_Action(Character *character, short map, unsigned char x, unsigned char y)
			: character(character)
			, map(map)
			, x(x)
			, y(y)
		{ }
	};

	std::vector<Warp_Suck_Action> actions;

	World *world(static_cast<World *>(world_void));

	double now = Timer::GetTime();
	double delay = world->config["WarpSuck"];

	UTIL_FOREACH(world->maps, map)
	{
		UTIL_FOREACH(map->characters, character)
		{
			if (character->last_walk + delay >= now)
				continue;

			auto check_warp = [&](bool test, unsigned char x, unsigned char y)
			{
				if (!test || !map->InBounds(x, y))
					return;

				const Map_Warp& warp = map->GetWarp(x, y);

				if (!warp || warp.levelreq > character->level || (warp.spec != Map_Warp::Door && warp.spec != Map_Warp::NoDoor))
					return;

				actions.push_back({character, warp.map, warp.x, warp.y});
			};

			character->last_walk = now;

			check_warp(true,                       character->x,     character->y);
			check_warp(character->x > 0,           character->x - 1, character->y);
			check_warp(character->x < map->width,  character->x + 1, character->y);
			check_warp(character->y > 0,           character->x,     character->y - 1);
			check_warp(character->y < map->height, character->x,     character->y + 1);
		}
	}

	UTIL_FOREACH(actions, act)
	{
		if (act.character->SourceAccess() < ADMIN_GUIDE && world->GetMap(act.map)->evacuate_lock)
		{
			act.character->StatusMsg(world->i18n.Format("map_evacuate_block"));
			act.character->Refresh();
		}
		else
		{
			act.character->Warp(act.map, act.x, act.y);
		}
	}
}

void world_despawn_items(void *world_void)
{
	World *world = static_cast<World *>(world_void);

	UTIL_FOREACH(world->maps, map)
	{
		restart_loop:
		UTIL_FOREACH(map->items, item)
		{
			if (item->unprotecttime < (Timer::GetTime() - static_cast<double>(world->config["ItemDespawnRate"])))
			{
				map->DelItem(item->uid, 0);
				goto restart_loop;
			}
		}
	}
}

void world_timed_save(void *world_void)
{
	World *world = static_cast<World *>(world_void);

	if (!world->config["TimedSave"])
		return;

	UTIL_FOREACH(world->characters, character)
	{
		character->Save();
	}

	world->guildmanager->SaveAll();

	try
	{
		world->CommitDB();
	}
	catch (Database_Exception& e)
	{
		Console::Wrn("Database commit failed - no data was saved!");
		world->db.Rollback();
	}

	world->BeginDB();
}

void world_spikes(void *world_void)
{
	World *world = static_cast<World *>(world_void);
	
	for (Map* map : world->maps)
	{
		if (map->exists)
			map->TimedSpikes();
	}
}

void world_drains(void *world_void)
{
	World *world = static_cast<World *>(world_void);
	
	for (Map* map : world->maps)
	{
		if (map->exists)
			map->TimedDrains();
	}
}

void world_quakes(void *world_void)
{
	World *world = static_cast<World *>(world_void);
	
	for (Map* map : world->maps)
	{
		if (map->exists)
			map->TimedQuakes();
	}
}

static int world_hw2016_hour()
{
	return ((std::time(nullptr) + 60) / 3600);
}

void world_hw2016(void *world_void)
{
	World *world = static_cast<World *>(world_void);
	
	if (!world->maps[5-1] || !world->maps[286-1] || !world->maps[287-1] || !world->maps[288-1] || !world->maps[289-1])
	{
		Console::Dbg("Halloween event is broken! Maps not found");
		return;
	}
	
	auto clear_map_npcs = [&](int mapid)
	{
		std::vector<NPC*> despawn_npcs;

		for (NPC* npc : world->maps[mapid-1]->npcs)
		{
			despawn_npcs.push_back(npc);
		}

		for (NPC* npc : despawn_npcs)
		{
			npc->Die();
		}
	};
	
	auto clear_map_items = [&](int mapid)
	{
		world->maps[mapid-1]->items.clear();
	};
	
	auto clear_map_characters = [&](int mapid)
	{
		auto chars = world->maps[mapid-1]->characters;
		world->maps[mapid-1]->TimedDrains();
		
		for (Character* character : chars)
		{
			character->SpikeDamage(std::max<int>(character->maxhp, 9999));
			
			if (character->hp == 0)
				character->DeathRespawn();
		}
	};
	
	auto warp_map_characters = [&](int mapid, int newmapid)
	{
		auto chars = world->maps[mapid-1]->characters;
		world->maps[mapid-1]->TimedDrains();
		
		for (Character* character : chars)
		{
			character->Warp(newmapid, character->x, character->y, WARP_ANIMATION_NONE);
		}
	};
	
	auto warp_map_characters_xy = [&](int mapid, int newmapid, int x, int y)
	{
		auto chars = world->maps[mapid-1]->characters;
		world->maps[mapid-1]->TimedDrains();
		
		for (Character* character : chars)
		{
			character->Warp(newmapid, x, y, WARP_ANIMATION_NONE);
		}
	};
	
	auto abort_halloween = [&]()
	{
		for (int i = 286; i <= 289; ++i)
		{
			clear_map_npcs(i);
			clear_map_characters(i);
		}
		
		world->hw2016_state = 0;
	};
	
	auto spawn_npc = [&](int mapid, int x, int y, int id, int speed) -> NPC*
	{
		Map* map = world->GetMap(mapid);

		unsigned char index = map->GenerateNPCIndex();

		if (index > 251)
			return nullptr;

		NPC *npc = new NPC(map, id, x, y, speed, DIRECTION_DOWN, index, true);
		map->npcs.push_back(npc);
		npc->Spawn();
		
		return npc;
	};
	
	const int monster_apozen = 329;
	const int monster_bat = 331;
	const int monster_banshee = 337;
	const int monster_chaos = 335;
	const int monster_crane = 342;
	const int monster_mask = 343;
	const int monster_flyman = 346;
	const int monster_imp1 = 347;
	const int monster_imp2 = 348;
	const int monster_inferno = 340;
	const int monster_puppet = 344;
	const int monster_reaper = 345;
	const int monster_skel_warlock = 334;
	const int monster_skel_warrior = 333;
	const int monster_spider = 332;
	const int monster_tentacle = 341;
	const int monster_twin_demon = 349;
	const int monster_wraith = 339;
	const int monster_chest = 350;
	
	const int speed_veryfast = 1;
	const int speed_fast = 0;
	const int speed_slow = 2;
	//const int speed_veryslow = 3;
	
	const int cave_x[4] = {4, 4, 16, 17};
	const int cave_y[4] = {6, 15, 18, 4};
	
	int hour = world_hw2016_hour();
	
	if (world->hw2016_hour != hour)
	{
		Console::Dbg("It's a new hour!");

		if (world->hw2016_state == -1)
		{
			world->hw2016_state = 0;
			world->hw2016_hour = hour; // test event
		}
		else if (world->hw2016_state == 0)
		{
			if (!world->config["Halloween2016"])
			{
				Console::Dbg("Halloween event is disabled.");
				world->hw2016_hour = hour;
				return;
			}

			Console::Dbg("Halloween event activated!");
			world->hw2016_state = 1;
			world->hw2016_tick = -5;
			world->hw2016_apospawn = 0;
			world->hw2016_dyingskull.clear();
			world->hw2016_dyingapozen = nullptr;
		}
	}
	
	if (world->hw2016_state == 0)
		return;

	world->hw2016_tick += 5;
	
	world->hw2016_hour = hour;
	
	Console::Dbg("Halloween event debug: state:%d, tick:%d", world->hw2016_state, world->hw2016_tick);

	switch (world->hw2016_state)
	{
		case 1:
			if (world->hw2016_tick == 0)
			{
				world->maps[5-1]->Effect(MAP_EFFECT_QUAKE, 2);
			}
			else if (world->hw2016_tick == 30)
			{
				world->maps[5-1]->Effect(MAP_EFFECT_QUAKE, 4);
			}
			else if (world->hw2016_tick == 60)
			{
				clear_map_npcs(286);

				world->maps[5-1]->Effect(MAP_EFFECT_QUAKE, 6);
				
				if (world->config["Halloween2016Noisy"])
					world->ServerMsg("A sinkhole is about to open up in the center of Aeven!");
			}
			else if (world->hw2016_tick == 90)
			{
				world->maps[5-1]->Effect(MAP_EFFECT_QUAKE, 8);
				world->maps[5-1]->ReloadAs(std::string(world->config["MapDir"]) + "00005h-r7-sinkhole.emf", Map::ReloadIgnoreNPC);
			}
			else if (world->hw2016_tick == 150)
			{
				if (world->maps[286-1]->characters.size() == 0)
				{
					abort_halloween();
				}

				world->maps[5-1]->ReloadAs(std::string(world->config["MapDir"]) + "00005h-r7-nospooks.emf", Map::ReloadIgnoreNPC);
				world->hw2016_spawncount = std::min<int>((world->maps[286-1]->characters.size() + 1) / 2, 5);
				world->hw2016_monstermod = 1.0 + std::max<int>(world->maps[286-1]->characters.size() - 10, 0) * 0.1;
				
				if (world->maps[286-1]->characters.size() == 1)
					world->maps[286-1]->characters.front()->hw2016_points = 50;
				
				if (world->maps[286-1]->characters.size() == 2)
					world->hw2016_monstermod = 1.5;
				else if (world->maps[286-1]->characters.size() == 4)
					world->hw2016_monstermod = 1.3;
				else if (world->maps[286-1]->characters.size() == 6)
					world->hw2016_monstermod = 1.2;
				else if (world->maps[286-1]->characters.size() == 8)
					world->hw2016_monstermod = 1.15;
				
				world->hw2016_state = 10;
				world->hw2016_tick = -5;
			}
			break;

		case 10:
			if (world->hw2016_tick == 0 || world->hw2016_tick == 15)
			{
				if (world->hw2016_tick == 0)
					world->maps[286-1]->Effect(MAP_EFFECT_QUAKE, 2);

				for (int i = 0; i < 4; ++i)
				{
					int x = cave_x[i];
					int y = cave_y[i];
					
					for (int ii = 0; ii < world->hw2016_spawncount; ++ii)
					{
						if (util::rand(0,1))
							spawn_npc(286, x, y, monster_bat, speed_fast);
						else
							spawn_npc(286, x, y, monster_spider, speed_slow);
					}
				}
			}
			else if (world->hw2016_tick == 900)
			{
				abort_halloween();
			}
			else
			{
				if (world->maps[286-1]->npcs.size() < world->hw2016_spawncount)
				{
					world->hw2016_state = 11;
					world->hw2016_tick = -5;
				}
			}
			break;

		case 11:
			if (world->hw2016_tick == 0 || world->hw2016_tick == 15)
			{
				if (world->hw2016_tick == 0)
					world->maps[286-1]->Effect(MAP_EFFECT_QUAKE, 3);

				for (int i = 0; i < 4; ++i)
				{
					int x = cave_x[i];
					int y = cave_y[i];
					
					for (int ii = 0; ii < world->hw2016_spawncount; ++ii)
					{
						if (util::rand(0,1))
							spawn_npc(286, x, y, monster_skel_warrior, speed_fast);
						else
							if (util::rand(0,3) == 0)
								spawn_npc(286, x, y, monster_reaper, speed_fast);
							else
								spawn_npc(286, x, y, monster_skel_warlock, speed_slow);
					}
				}
			}
			else if (world->hw2016_tick == 900)
			{
				abort_halloween();
			}
			else
			{
				if (world->maps[286-1]->npcs.size() < world->hw2016_spawncount)
				{
					world->hw2016_state = 12;
					world->hw2016_tick = -5;
				}
			}
			break;

		case 12:
			if (world->hw2016_tick == 0 || world->hw2016_tick == 15)
			{
				if (world->hw2016_tick == 0)
					world->maps[286-1]->Effect(MAP_EFFECT_QUAKE, 4);

				for (int i = 0; i < 4; ++i)
				{
					int x = cave_x[i];
					int y = cave_y[i];
					
					for (int ii = 0; ii < world->hw2016_spawncount; ++ii)
					{
						if (util::rand(0,1))
							spawn_npc(286, x, y, monster_banshee, speed_slow);
						else
							if (util::rand(0,2) == 0)
								spawn_npc(286, x, y, monster_reaper, speed_fast);
							else
								spawn_npc(286, x, y, monster_chaos, speed_slow);
					}
				}
			}
			else if (world->hw2016_tick == 900)
			{
				abort_halloween();
			}
			else
			{
				if (world->maps[286-1]->npcs.size() == 0)
				{
					world->maps[286-1]->Effect(MAP_EFFECT_QUAKE, 3);
					world->maps[286-1]->ReloadAs(std::string(world->config["MapDir"]) + "00286-hole.emf", Map::ReloadIgnoreNPC);

					clear_map_npcs(287);

					world->hw2016_state = 20;
					world->hw2016_tick = -5;
					
					spawn_npc(287, 8, 29, monster_tentacle, speed_fast);
					spawn_npc(287, 9, 28, monster_tentacle, speed_fast);
					spawn_npc(287, 1, 35, monster_tentacle, speed_fast);
					spawn_npc(287, 6, 17, monster_tentacle, speed_fast);
					spawn_npc(287, 7, 16, monster_tentacle, speed_fast);
					
					spawn_npc(287, 20, 21, monster_tentacle, speed_fast);
				
					world->hw2016_hallway_spawnrow = 86;
					world->hw2016_hallway = 0;
				}
			}
			break;
		
		case 20:
			if (world->hw2016_tick >= 5 && world->hw2016_tick <= 30)
			{
				world->maps[286-1]->Effect(MAP_EFFECT_QUAKE, 5);
				
				auto chars = world->maps[286-1]->characters;
				
				for (Character* character : chars)
				{
					if (world->hw2016_tick < 30)
						character->SpikeDamage(character->maxhp * (0.1 * (world->hw2016_tick / 5)));
					else
						character->SpikeDamage(std::min<int>(character->maxhp, 9999));
					
					if (character->hp == 0)
						character->DeathRespawn();
				}
			}
			
			if (world->hw2016_tick == 30)
			{				
				world->maps[286-1]->ReloadAs(std::string(world->config["MapDir"]) + "00286.emf", Map::ReloadIgnoreNPC);

				clear_map_npcs(288);

				world->hw2016_state = 21;
				world->hw2016_tick = -5;
				
				for (int i = 0; i < 10; ++i)
				{
					int x = util::rand(4, 19);
					int y = util::rand(88, 94);

					spawn_npc(287, x, y, monster_crane, speed_veryfast);
				}
			}
			break;
		
		case 21:
			{
				++world->hw2016_hallway;
				
				if (world->hw2016_hallway_spawnrow >= 82 - world->hw2016_hallway / 4 * 2)
				{
					if (world->hw2016_hallway_spawnrow > 42)
					{
						world->hw2016_hallway_spawnrow -= 4;
						world->hw2016_spawn_hallway_row(world->hw2016_hallway_spawnrow);
					}
				}
			}

			// Too many cranes :(
			/*if (world->hw2016_tick % 30 == 0)
			{
				for (int i = 0; i < 5; ++i)
				{
					int x = util::rand(4, 19);
					int y = util::rand(86, 94);

					spawn_npc(287, x, y, monster_crane, speed_fast);
				}
			}*/
			
			if (world->hw2016_tick >= 900)
			{
				Console::Dbg("Halloween event state 30 forced: Noone reached darkness in 15 mins");
				world->hw2016_state = 30;
				world->hw2016_tick = -5;
			}
			break;
		
		case 30: // Triggered by first player to enter the next zone
			if (world->hw2016_tick <= 30)
				world->maps[287-1]->Effect(MAP_EFFECT_QUAKE, 3);

			if (world->hw2016_tick == 20)
			{
				for (int i = 0; i < 2; ++i)
					for (int ii = 0; i < world->hw2016_spawncount; ++i)
					{
						int x = util::rand(2, 19);
						int y = util::rand(2, 20);

						spawn_npc(288, x, y, monster_puppet, speed_slow);
					}
			}
			
			if (world->hw2016_tick == 30)
			{
				for (int i = 0; i < 2; ++i)
					for (int ii = 0; i < world->hw2016_spawncount; ++i)
					{
						int x = util::rand(2, 19);
						int y = util::rand(2, 20);

						spawn_npc(288, x, y, monster_puppet, speed_slow);
					}

				world->hw2016_state = 31;
				world->hw2016_tick = -5;
			}
			break;
		
		case 31:
			if (world->hw2016_tick % 10 == 0)
			{
				if (world->hw2016_tick <= 90)
				{
					for (int ii = 0; ii < world->hw2016_spawncount; ++ii)
					{
						int x = util::rand(2, 19);
						int y = util::rand(2, 20);

						switch (util::rand(0,4))
						{
							case 0:
								spawn_npc(288, x, y, monster_mask, speed_fast);
								break;
							
							case 1:
								spawn_npc(288, x, y, monster_reaper, speed_veryfast);
								break;
							
							case 2:
							case 3:
								spawn_npc(288, x, y, monster_wraith, speed_fast);
								break;
						}
					}
				}
				else if (world->hw2016_tick <= 180)
				{
					for (int ii = 0; ii < world->hw2016_spawncount; ++ii)
					{
						int x = util::rand(2, 19);
						int y = util::rand(2, 20);

						switch (util::rand(0,5))
						{
							case 0:
								spawn_npc(288, x, y, monster_mask, speed_fast);
								break;
							
							case 1:
								spawn_npc(288, x, y, monster_flyman, speed_fast);
								break;
							
							case 2:
							case 3:
								spawn_npc(288, x, y, monster_reaper, speed_veryfast);
								break;
						}
					}
				}
				
				if (world->maps[288-1]->npcs.size() > 1 && util::rand(0,3) == 3)
				{
					int x = util::rand(2, 19);
					int y = util::rand(2, 20);

					spawn_npc(288, x, y, monster_inferno, speed_veryfast);
				}
			}
			
			if (world->hw2016_tick > 180 && world->maps[288-1]->npcs.size() == 0)
			{
				clear_map_characters(287);
				clear_map_npcs(287);

				clear_map_npcs(289);

				world->hw2016_state = 40;
				world->hw2016_tick = -5;
				
				for (int i = 0; i < 2; ++i)
					for (int ii = 0; ii < world->hw2016_spawncount; ++ii)
					{
						int x = util::rand(2, 19);
						int y = util::rand(2, 20);

						spawn_npc(289, x, y, monster_imp1, speed_veryfast);
					}
				
				for (int i = 0; i < 2; ++i)
					for (int ii = 0; ii < world->hw2016_spawncount; ++ii)
					{
						int x = util::rand(2, 19);
						int y = util::rand(2, 20);

						spawn_npc(289, x, y, monster_imp2, speed_veryfast);
					}

				warp_map_characters(288, 289);
			}
			else if (world->hw2016_tick == 900)
			{
				abort_halloween();
			}
			break;
		
		case 40:
			if (world->maps[289-1]->npcs.size() == 0)
			{
				world->hw2016_state = 41;
				world->hw2016_tick = -5;
				
				world->maps[5-1]->Effect(MAP_EFFECT_QUAKE, 8);
				NPC* apozen = spawn_npc(289, 13, 11, monster_apozen, speed_fast);
				
				if (apozen)
					apozen->Say("YOU DARE BRING LIGHT TO MY LAIR?!");
				else
					abort_halloween();
				
				NPC_AI_HW2016_Apozen* apozen_ai = static_cast<NPC_AI_HW2016_Apozen*>(apozen->ai.get());
				
				// skullz
				{
					int speed = speed_fast;
					int direction = 0;

					for (int i = 0; i < 4; ++i)
					{
						unsigned char index = apozen->map->GenerateNPCIndex();

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
						
						if (world->hw2016_spawncount > i)
						{
							NPC *npc = new NPC(apozen->map, 330, apozen->x + xoff, apozen->y + yoff, speed, direction, index, true);
							apozen->map->npcs.push_back(npc);
							npc->ai.reset(new NPC_AI_HW2016_ApozenSkull(npc, apozen));
							npc->Spawn();
							
							apozen_ai->skull[i] = npc;
							++apozen_ai->num_skulls;
						}
					}
				}
			}
			else if (world->hw2016_tick == 600)
			{
				abort_halloween();
			}
			break;
		
		case 41:
			if (world->hw2016_apospawn == 1)
			{
				for (int i = 0; i < 2; ++i)
					for (int ii = 0; ii < world->hw2016_spawncount; ++ii)
					{
						int x,y;
						
						if (util::rand(0,1))
							x = util::rand(2, 6);
						else
							x = util::rand(15, 19);

						y = util::rand(2, 20);

						spawn_npc(289, x, y, monster_imp1, speed_fast);
					}
			
				for (int i = 0; i < 2; ++i)
					for (int ii = 0; ii < world->hw2016_spawncount; ++ii)
					{
						int x, y;
						
						x = util::rand(2, 19);
						
						if (util::rand(0,1))
							y = util::rand(2, 6);
						else
							y = util::rand(16, 20);

						spawn_npc(289, x, y, monster_imp2, speed_fast);
					}

				world->hw2016_apospawn = 0;
			}
			
			if (world->hw2016_apospawn == 2)
			{
				if (world->hw2016_spawncount == 1)
					spawn_npc(289, 4 + 13 * util::rand(0,1), 4 + 13 * util::rand(0,1), monster_twin_demon, speed_slow);
					
				if (world->hw2016_spawncount > 1)
					spawn_npc(289, 4, 4, monster_twin_demon, speed_slow);

				if (world->hw2016_spawncount >= 2)
					spawn_npc(289, 17, 17, monster_twin_demon, speed_slow);

				if (world->hw2016_spawncount >= 3)
					spawn_npc(289, 4, 17, monster_twin_demon, speed_slow);

				if (world->hw2016_spawncount >= 4)
					spawn_npc(289, 17, 4, monster_twin_demon, speed_slow);

				world->hw2016_apospawn = 0;
			}
			
			/*
			for (int i = 0; i < 4; ++i)
			{
				if (world->hw2016_dyingskull[i])
				{
					NPC* skull = world->hw2016_dyingskull[i];

					unsigned char index = skull->map->GenerateNPCIndex();

					if (index > 250)
					{
						NPC_AI_HW2016_ApozenSkull* skull_ai = static_cast<NPC_AI_HW2016_ApozenSkull*>(skull->ai.get());
						if (skull_ai)
						{
							skull_ai->really_die = true;
							skull->Die();
						}
						world->hw2016_dyingskull[i] = nullptr;
						continue;
					}

					NPC *npc = new NPC(skull->map, 297, skull->x, skull->y, 7, DIRECTION_DOWN, index, true);
					skull->map->npcs.push_back(npc);
					npc->Spawn();
					
					int weak = 0;
					
					NPC_AI_HW2016_ApozenSkull* skull_ai = static_cast<NPC_AI_HW2016_ApozenSkull*>(skull->ai.get());
					
					if (skull_ai)
					{
						for (int i = 0; i < 4; ++i)
						{
							NPC_AI_HW2016_Apozen* ai = static_cast<NPC_AI_HW2016_Apozen*>(skull_ai->apozen->ai.get());

							if (ai && ai->skull[i] == skull)
								ai->skull[i] = nullptr;
							
							if (ai && ai->skull[i] == nullptr)
								++weak;
						}
					
						skull_ai->apozen->hw2016_apoweak = weak;
					}
					
					if (skull_ai)
					{
						skull->hp = 1;
						skull_ai->really_die = true;
						skull->Die();
					}
					
					world->hw2016_dyingskull[i] = nullptr;
				}	
			}
			*/
			
			for (auto ice : world->hw2016_dyingskull)
			{
				spawn_npc(289, ice.first, ice.second, 297, 7); // dream crystal
			}
			
			world->hw2016_dyingskull.clear();
			
			if (world->hw2016_dyingapozen)
			{
				NPC* apozen = world->hw2016_dyingapozen;
				NPC_AI_HW2016_Apozen* apozen_ai = static_cast<NPC_AI_HW2016_Apozen*>(apozen->ai.get());
				
				if (apozen_ai)
				{
					for (int i = 0; i < 4; ++i)
					{
						NPC* skull = apozen_ai->skull[i];
						
						if (skull)
						{
							skull->Die();
							apozen_ai->skull[i] = nullptr;
						}
					}

					apozen->hp = 1;
					apozen->Effect(2);
					apozen_ai->really_die = true;

					UTIL_FOREACH(apozen->map->characters, character)
					{
						if (character->InRange(apozen))
						{
							apozen->RemoveFromView(character);
						}
					}

					apozen->ApozenLoot();
					apozen->Die(false);
				}
				
				world->hw2016_dyingapozen = nullptr;
			}
			
			if (world->hw2016_tick == 1800)
			{
				abort_halloween();
			}
			break;
		
		case 50:
			if (world->hw2016_tick == 0)
			{
				world->maps[289-1]->ReloadAs(std::string(world->config["MapDir"]) + "00289-exit.emf");
				
				for (int i = 0; i < world->hw2016_numchests; ++i)
				{
					int x, y;

					if (i < 11*12)
					{
						x = 8 + (i % 12);
						y = 5 + (i / 12);
					}
					else if (i < 11*12 + 4*7)
					{
						x = 3 + ((i - 11*12) % 4);
						y = 6 + ((i - 11*12) / 4);
					}
					else
						break;
					
					spawn_npc(289, x, y, monster_chest, 7); 
				}
			}
			else if (world->hw2016_tick == 300)
			{
				warp_map_characters_xy(289, 5, 40, 40);
				clear_map_npcs(289);
				clear_map_items(289);
				world->maps[289-1]->ReloadAs(std::string(world->config["MapDir"]) + "00289.emf");
				world->hw2016_state = 0;
			}
			break;
			
		default:
			break;
	}
}

void World::hw2016_spawn_hallway_row(int row)
{
	const int monster_magician = 336;
	const int monster_headless = 338;
	const int monster_inferno = 340;
	const int monster_wraith = 339;

	const int speed_fast = 0;
	const int speed_slow = 2;
	
	const int hallway1_x[4] = {7, 9, 11};
	
	const int hallway2_x[4] = {9, 11, 13};
	
	Console::Dbg("Spawning row %d", row);
	
	auto spawn_npc = [&](int mapid, int x, int y, int id, int speed) -> NPC*
	{
		Map* map = this->GetMap(mapid);

		unsigned char index = map->GenerateNPCIndex();

		if (index > 251)
			return nullptr;

		NPC *npc = new NPC(map, id, x, y, speed, DIRECTION_DOWN, index, true);
		map->npcs.push_back(npc);
		npc->Spawn();
		
		return npc;
	};

	for (int i = 0; i < 3; ++i)
	{
		int x = hallway1_x[i];
		int y = row;
		
		if (row < 60)
			x = hallway2_x[i];
		
		for (int ii = 0; ii < this->hw2016_spawncount; ++ii)
		{
			if (util::rand(0, 25 + this->hw2016_spawncount * 3) == 0)
			{
				spawn_npc(287, x, y, monster_inferno, speed_fast);
			}
			else
			{
				switch (util::rand(0,4))
				{
					case 0:
						spawn_npc(287, x, y, monster_magician, speed_slow);
						break;

					case 1:
						spawn_npc(287, x, y, monster_headless, speed_fast);
						break;
					
					case 2:
						spawn_npc(287, x, y, monster_wraith, speed_fast);
						break;
				}
			}
		}
	}
}

void World::NormalizePassword(std::string& s)
{
	auto pw_isnum = [](char c){ return c >= '0' && c <= '9'; };
	auto pw_islower = [](char c){ return c >= 'a' && c <= 'z'; };
	auto pw_isupper = [](char c){ return c >= 'A' && c <= 'Z'; };
	auto pw_isalpha = [&pw_islower, &pw_isupper](char c){ return pw_islower(c) || pw_isupper(c); };
	auto pw_tolower = [&pw_isupper](char c){ if (pw_isupper(c)) { return char(c - 'A' + 'a'); } else { return c; } };
	auto pw_suffix = [](char a, char b, char c){ return ((a << 16) + (b << 8) + c); };

	std::size_t len = s.length();

	if (len < 6)
	{
		std::fill(s.begin() + len, s.end(), '\0');
		s.resize(0);
		s = "@short";
		return;
	}

	// foobar1995, foobar2020 -> foobar
	long suffix = (len >= 4 ? pw_suffix(s[len - 4], s[len - 3], 0) : 0);
	if (len >= 4 && (
		   suffix == pw_suffix('1', '9', 0)
		|| suffix == pw_suffix('2', '0', 0)
	) && pw_isnum(s[len - 2]) && pw_isnum(s[len - 1]))
	{
		len -= 4;
	}

	// foobar420, foobar007 -> foobar
	suffix = (len >= 3 ? pw_suffix(s[len - 3], s[len - 2], s[len - 1]) : 0);
	if (len >= 3 && (
		   suffix == pw_suffix('0', '0', '7')
		|| suffix == pw_suffix('4', '2', '0')
	))
	{
		len -= 3;
	}

	// foobar111, foobar222, ... -> foobar
	if (len >= 3 && (s[len - 3] == s[len - 2]) && (s[len - 2] == s[len - 1]))
		len -= 3;

	// foobar123, ... -> foobar
	else if (len >= 3 && (s[len - 3] == (s[len - 2] + 1)) && (s[len - 2] == (s[len - 1] + 1)))
		len -= 3;

	// foobar321, ... -> foobar
	else if (len >= 3 && (s[len - 3] == (s[len - 2] - 1)) && (s[len - 2] == (s[len - 1] - 1)))
		len -= 3;

	// foobar69 -> foobar
	else if (len >= 2 && pw_isnum(s[len - 1]) && pw_isnum(s[len - 2]))
		len -= 2;

	// 1foobar -> foobar
	if (len >= 1 && pw_isnum(s[0]))
	{
		std::memmove(&s[0], &s[1], len-1);
		len -= 1;
	}

	// Foobar -> foobar
	if (len >= 1 && pw_isupper(s[0]))
		s[0] = pw_tolower(s[0]);

	// FOOBAR -> foobar
	bool all_upper = (len > 0);
	for (size_t i = 0; i < len; ++i)
	{
		if (pw_islower(s[i]))
			all_upper = false;
	}

	if (all_upper)
	{
		for (size_t i = 0; i < len; ++i)
			s[i] = pw_tolower(s[i]);
	}

	// aaaaaa -> @repeating
	bool repeating = (len > 0);
	char repeat_c = (len > 0) ? s[0] : 0;

	for (size_t i = 1; i < len; ++i)
	{
		if (s[i] != repeat_c)
			repeating = false;
	}

	// abcdef, 654321 -> @sequential
	bool sequential = (len > 0);
	char sequential_c = (len > 0) ? s[0] : 0;

	for (size_t i = 1; i < len; ++i)
	{
		if (s[i] == (sequential_c + 1))
		{
			sequential_c = (sequential_c + 1);
		}
		else if (s[i] == (sequential_c - 1))
		{
			sequential_c = (sequential_c - 1);
		}
		else
		{
			sequential = false;
		}
	}

	if (repeating || sequential)
		len = 0;

	std::fill(s.begin() + len, s.begin() + s.length(), '\0');
	s.resize(len);

	     if (repeating)  s = "@repeating";
	else if (sequential) s = "@sequential";
	else if (len == 0)   s = "@empty";
}

void World::NormalizePassword(util::secure_string& ss)
{
	std::string s = ss.str();

	this->NormalizePassword(s);

	ss = std::move(s);

	std::fill(UTIL_RANGE(s), '\0');
	s.erase();
}

void World::UpdateConfig()
{
	this->timer.SetMaxDelta(this->config["ClockMaxDelta"]);


	double rate_face = this->config["PacketRateFace"];
	double rate_walk = this->config["PacketRateWalk"];
	double rate_attack = this->config["PacketRateAttack"];

	Handlers::SetDelay(PACKET_FACE, PACKET_PLAYER, rate_face);

	Handlers::SetDelay(PACKET_WALK, PACKET_ADMIN, rate_walk);
	Handlers::SetDelay(PACKET_WALK, PACKET_PLAYER, rate_walk);
	Handlers::SetDelay(PACKET_WALK, PACKET_SPEC, rate_walk);

	Handlers::SetDelay(PACKET_ATTACK, PACKET_USE, rate_attack);


	std::array<double, 7> npc_speed_table;

	std::vector<std::string> rate_list = util::explode(',', this->config["NPCMovementRate"]);

	for (std::size_t i = 0; i < std::min<std::size_t>(7, rate_list.size()); ++i)
	{
		if (i < rate_list.size())
			npc_speed_table[i] = util::tdparse(rate_list[i]);
		else
			npc_speed_table[i] = 1.0;
	}

	NPC::SetSpeedTable(npc_speed_table);


	this->i18n.SetLangFile(this->config["ServerLanguage"]);


	this->instrument_ids.clear();

	std::vector<std::string> instrument_list = util::explode(',', this->config["InstrumentItems"]);
	this->instrument_ids.reserve(instrument_list.size());

	for (std::size_t i = 0; i < instrument_list.size(); ++i)
	{
		this->instrument_ids.push_back(int(util::tdparse(instrument_list[i])));
	}

	if (this->db.Pending() && !this->config["TimedSave"])
	{
		try
		{
			this->CommitDB();
		}
		catch (Database_Exception& e)
		{
			Console::Wrn("Database commit failed - no data was saved!");
			this->db.Rollback();
		}
	}

	try
	{
		this->insecure_passwords.clear();

		Database_Result res = this->db.Query("SELECT `password` FROM `insecure_passwords`");

		UTIL_FOREACH_REF(res, row)
		{
			std::string password = row["password"];
			this->insecure_passwords.insert(password);

			if (this->config["CheckNormalizePasswords"])
			{
				this->NormalizePassword(password);
				this->insecure_passwords.insert(password);
			}
		}

		Console::Out("%d insecure passwords loaded.", this->insecure_passwords.size());
	}
	catch (Database_QueryFailed& e)
	{
		Console::Err("Failed to load insecure passwords list");
	}
}

World::World(std::array<std::string, 6> dbinfo, const Config &eoserv_config, const Config &admin_config)
	: i18n(eoserv_config.find("ServerLanguage")->second)
	, admin_count(0)
{
	if (int(this->timer.resolution * 1000.0) > 1)
	{
		Console::Out("Timers set at approx. %i ms resolution", int(this->timer.resolution * 1000.0));
	}
	else
	{
		Console::Out("Timers set at < 1 ms resolution");
	}

	this->config = eoserv_config;
	this->admin_config = admin_config;
	
	this->astar.SetMaxDistance(15);

	Database::Engine engine;

	std::string dbdesc;

	if (util::lowercase(dbinfo[0]).compare("sqlite") == 0)
	{
		engine = Database::SQLite;
		dbdesc = std::string("SQLite: ")
		       + dbinfo[1];
	}
	else
	{
		engine = Database::MySQL;
		dbdesc = std::string("MySQL: ")
		       + dbinfo[2] + "@"
		       + dbinfo[1];

		if (dbinfo[5] != "0" && dbinfo[5] != "3306")
			dbdesc += ":" + dbinfo[5];

		dbdesc += "/" + dbinfo[4];
	}

	Console::Out("Connecting to database (%s)...", dbdesc.c_str());
	this->db.Connect(engine, dbinfo[1], util::to_int(dbinfo[5]), dbinfo[2], dbinfo[3], dbinfo[4]);

	if (config.find("StartUpSQL") != config.end())
	{
		try
		{
			this->db.ExecuteFile(config["StartupSQL"]);
		}
		catch (Database_Exception& e)
		{
			Console::Err(e.error());
		}
	}

	this->BeginDB();

	try
	{
		this->drops_config.Read(this->config["DropsFile"]);
		this->shops_config.Read(this->config["ShopsFile"]);
		this->arenas_config.Read(this->config["ArenasFile"]);
		this->formulas_config.Read(this->config["FormulasFile"]);
		this->home_config.Read(this->config["HomeFile"]);
		this->skills_config.Read(this->config["SkillsFile"]);
	}
	catch (std::runtime_error &e)
	{
		Console::Wrn(e.what());
	}

	this->UpdateConfig();
	this->LoadHome();

	this->eif = new EIF(this->config["EIF"]);
	this->enf = new ENF(this->config["ENF"]);
	this->esf = new ESF(this->config["ESF"]);
	this->ecf = new ECF(this->config["ECF"]);

	std::size_t num_npcs = this->enf->data.size();
	this->npc_data.resize(num_npcs);
	for (std::size_t i = 0; i < num_npcs; ++i)
	{
		auto& npc = this->npc_data[i];
		npc.reset(new NPC_Data(this, i));
		if (npc->id != 0)
			npc->LoadShopDrop();
	}

	this->maps.resize(static_cast<int>(this->config["Maps"]));
	int loaded = 0;
	for (int i = 0; i < static_cast<int>(this->config["Maps"]); ++i)
	{
		this->maps[i] = new Map(i + 1, this);
		if (this->maps[i]->exists)
			++loaded;
	}
	Console::Out("%i/%i maps loaded.", loaded, static_cast<int>(this->maps.size()));

	short max_quest = static_cast<int>(this->config["Quests"]);

	UTIL_FOREACH(this->enf->data, npc)
	{
		if (npc.type == ENF::Quest)
			max_quest = std::max(max_quest, npc.vendor_id);
	}

	for (short i = 0; i <= max_quest; ++i)
	{
		try
		{
			std::shared_ptr<Quest> q = std::make_shared<Quest>(i, this);
			this->quests.insert(std::make_pair(i, std::move(q)));
		}
		catch (...)
		{

		}
	}
	Console::Out("%i/%i quests loaded.", static_cast<int>(this->quests.size()), max_quest);

	this->last_character_id = 0;

	TimeEvent *event = new TimeEvent(world_spawn_npcs, this, 1.0, Timer::FOREVER);
	this->timer.Register(event);

	event = new TimeEvent(world_act_npcs, this, 0.05, Timer::FOREVER);
	this->timer.Register(event);

	if (int(this->config["RecoverSpeed"]) > 0)
	{
		event = new TimeEvent(world_recover, this, double(this->config["RecoverSpeed"]), Timer::FOREVER);
		this->timer.Register(event);
	}

	if (int(this->config["NPCRecoverSpeed"]) > 0)
	{
		event = new TimeEvent(world_npc_recover, this, double(this->config["NPCRecoverSpeed"]), Timer::FOREVER);
		this->timer.Register(event);
	}

	if (int(this->config["WarpSuck"]) > 0)
	{
		event = new TimeEvent(world_warp_suck, this, 1.0, Timer::FOREVER);
		this->timer.Register(event);
	}

	if (this->config["ItemDespawn"])
	{
		event = new TimeEvent(world_despawn_items, this, static_cast<double>(this->config["ItemDespawnCheck"]), Timer::FOREVER);
		this->timer.Register(event);
	}

	if (this->config["TimedSave"])
	{
		event = new TimeEvent(world_timed_save, this, static_cast<double>(this->config["TimedSave"]), Timer::FOREVER);
		this->timer.Register(event);
	}

	if (this->config["SpikeTime"])
	{
		event = new TimeEvent(world_spikes, this, static_cast<double>(this->config["SpikeTime"]), Timer::FOREVER);
		this->timer.Register(event);
	}

	if (this->config["DrainTime"])
	{
		event = new TimeEvent(world_drains, this, static_cast<double>(this->config["DrainTime"]), Timer::FOREVER);
		this->timer.Register(event);
	}

	if (this->config["QuakeRate"])
	{
		event = new TimeEvent(world_quakes, this, static_cast<double>(this->config["QuakeRate"]), Timer::FOREVER);
		this->timer.Register(event);
	}
	
	{
		//event = new TimeEvent(world_hw2016, this, 5.0, Timer::FOREVER);
		event = new TimeEvent(world_hw2016, this, 5.0, Timer::FOREVER); // Overclocked 1x
		this->timer.Register(event);
	}

	exp_table[0] = 0;
	for (std::size_t i = 1; i < this->exp_table.size(); ++i)
	{
		exp_table[i] = int(util::round(std::pow(double(i), 3.0) * 133.1));
	}

	for (std::size_t i = 0; i < this->boards.size(); ++i)
	{
		this->boards[i] = new Board(i);
	}

	this->guildmanager = new GuildManager(this);
}

void World::BeginDB()
{
	if (this->config["TimedSave"])
		this->db.BeginTransaction();
}

void World::CommitDB()
{
	if (this->db.Pending())
		this->db.Commit();
}

void World::UpdateAdminCount(int admin_count)
{
	this->admin_count = admin_count;

	if (admin_count == 0 && this->config["FirstCharacterAdmin"])
	{
		Console::Out("There are no admin characters!");
		Console::Out("The next character created will be given HGM status!");
	}
}

void World::Command(std::string command, const std::vector<std::string>& arguments, Command_Source* from)
{
	std::unique_ptr<System_Command_Source> system_source;

	if (!from)
	{
		system_source.reset(new System_Command_Source(this));
		from = system_source.get();
	}

	Commands::Handle(util::lowercase(command), arguments, from);
}

void World::LoadHome()
{
	this->homes.clear();

	std::unordered_map<std::string, Home *> temp_homes;

	UTIL_FOREACH(this->home_config, hc)
	{
		std::vector<std::string> parts = util::explode('.', hc.first);

		if (parts.size() < 2)
		{
			continue;
		}

		if (parts[0] == "level")
		{
			int level = util::to_int(parts[1]);

			std::unordered_map<std::string, Home *>::iterator home_iter = temp_homes.find(hc.second);

			if (home_iter == temp_homes.end())
			{
				Home *home = new Home;
				home->id = static_cast<std::string>(hc.second);
				temp_homes[hc.second] = home;
				home->level = level;
			}
			else
			{
				home_iter->second->level = level;
			}

			continue;
		}

		Home *&home = temp_homes[parts[0]];

		if (!home)
		{
			temp_homes[parts[0]] = home = new Home;
			home->id = parts[0];
		}

		if (parts[1] == "name")
		{
			home->name = home->name = static_cast<std::string>(hc.second);
		}
		else if (parts[1] == "location")
		{
			std::vector<std::string> locparts = util::explode(',', hc.second);
			home->map = locparts.size() >= 1 ? util::to_int(locparts[0]) : 1;
			home->x = locparts.size() >= 2 ? util::to_int(locparts[1]) : 0;
			home->y = locparts.size() >= 3 ? util::to_int(locparts[2]) : 0;
		}
	}

	UTIL_FOREACH(temp_homes, home)
	{
		this->homes.push_back(home.second);
	}
}

int World::GenerateCharacterID()
{
	return ++this->last_character_id;
}

int World::GeneratePlayerID()
{
	unsigned int lowest_free_id = 1;
	restart_loop:
	UTIL_FOREACH(this->server->clients, client)
	{
		EOClient *eoclient = static_cast<EOClient *>(client);

		if (eoclient->id == lowest_free_id)
		{
			lowest_free_id = eoclient->id + 1;
			goto restart_loop;
		}
	}
	return lowest_free_id;
}

void World::Login(Character *character)
{
	this->characters.push_back(character);

	if (this->GetMap(character->mapid)->relog_x || this->GetMap(character->mapid)->relog_y)
	{
		     if (character->mapid == 286 && this->hw2016_state >=  1 && this->hw2016_state < 20) { }
		else if (character->mapid == 287 && this->hw2016_state >= 20 && this->hw2016_state < 40) { }
		else if (character->mapid == 288 && this->hw2016_state >= 30 && this->hw2016_state < 40) { }
		else if (character->mapid == 289 && this->hw2016_state >= 40) { }
		else
		{
			character->x = this->GetMap(character->mapid)->relog_x;
			character->y = this->GetMap(character->mapid)->relog_y;
		}
	}

	Map* map = this->GetMap(character->mapid);

	if (character->sitting == SIT_CHAIR)
	{
		Map_Tile::TileSpec spec = map->GetSpec(character->x, character->y);

		if (spec == Map_Tile::ChairDown)
			character->direction = DIRECTION_DOWN;
		else if (spec == Map_Tile::ChairUp)
			character->direction = DIRECTION_UP;
		else if (spec == Map_Tile::ChairLeft)
			character->direction = DIRECTION_LEFT;
		else if (spec == Map_Tile::ChairRight)
			character->direction = DIRECTION_RIGHT;
		else if (spec == Map_Tile::ChairDownRight)
			character->direction = character->direction == DIRECTION_RIGHT ? DIRECTION_RIGHT : DIRECTION_DOWN;
		else if (spec == Map_Tile::ChairUpLeft)
			character->direction = character->direction == DIRECTION_LEFT ? DIRECTION_LEFT : DIRECTION_UP;
		else if (spec != Map_Tile::ChairAll)
			character->sitting = SIT_STAND;
	}

	map->Enter(character);
	character->Login();
}

void World::Logout(Character *character)
{
	if (this->GetMap(character->mapid)->exists)
	{
		this->GetMap(character->mapid)->Leave(character);
	}

	this->characters.erase(
		std::remove(UTIL_RANGE(this->characters), character),
		this->characters.end()
	);
}

void World::Msg(Command_Source *from, std::string message, bool echo)
{
	std::string from_str = from ? from->SourceName() : "server";

	message = util::text_cap(message, static_cast<int>(this->config["ChatMaxWidth"]) - util::text_width(util::ucfirst(from_str) + "  "));

	PacketBuilder builder(PACKET_TALK, PACKET_MSG, 2 + from_str.length() + message.length());
	builder.AddBreakString(from_str);
	builder.AddBreakString(message);

	UTIL_FOREACH(this->characters, character)
	{
		character->AddChatLog("~", from_str, message);

		if (!echo && character == from)
		{
			continue;
		}

		character->Send(builder);
	}
}

void World::AdminMsg(Command_Source *from, std::string message, int minlevel, bool echo)
{
	std::string from_str = from ? from->SourceName() : "server";

	message = util::text_cap(message, static_cast<int>(this->config["ChatMaxWidth"]) - util::text_width(util::ucfirst(from_str) + "  "));

	PacketBuilder builder(PACKET_TALK, PACKET_ADMIN, 2 + from_str.length() + message.length());
	builder.AddBreakString(from_str);
	builder.AddBreakString(message);

	UTIL_FOREACH(this->characters, character)
	{
		character->AddChatLog("+", from_str, message);

		if ((!echo && character == from) || character->SourceAccess() < minlevel)
		{
			continue;
		}

		character->Send(builder);
	}
}

void World::AnnounceMsg(Command_Source *from, std::string message, bool echo)
{
	std::string from_str = from ? from->SourceName() : "server";

	message = util::text_cap(message, static_cast<int>(this->config["ChatMaxWidth"]) - util::text_width(util::ucfirst(from_str) + "  "));

	PacketBuilder builder(PACKET_TALK, PACKET_ANNOUNCE, 2 + from_str.length() + message.length());
	builder.AddBreakString(from_str);
	builder.AddBreakString(message);

	UTIL_FOREACH(this->characters, character)
	{
		character->AddChatLog("@", from_str, message);

		if (!echo && character == from)
		{
			continue;
		}

		character->Send(builder);
	}
}

void World::ServerMsg(std::string message)
{
	message = util::text_cap(message, static_cast<int>(this->config["ChatMaxWidth"]) - util::text_width("Server  "));

	PacketBuilder builder(PACKET_TALK, PACKET_SERVER, message.length());
	builder.AddString(message);

	UTIL_FOREACH(this->characters, character)
	{
		character->Send(builder);
	}
}

void World::AdminReport(Character *from, std::string reportee, std::string message)
{
	message = util::text_cap(message, static_cast<int>(this->config["ChatMaxWidth"]) - util::text_width(util::ucfirst(from->SourceName()) + "  reports: " + reportee + ", "));

	PacketBuilder builder(PACKET_ADMININTERACT, PACKET_REPLY, 5 + from->SourceName().length() + message.length() + reportee.length());
	builder.AddChar(2); // message type
	builder.AddByte(255);
	builder.AddBreakString(from->SourceName());
	builder.AddBreakString(message);
	builder.AddBreakString(reportee);

	UTIL_FOREACH(this->characters, character)
	{
		if (character->SourceAccess() >= static_cast<int>(this->admin_config["reports"]))
		{
			character->Send(builder);
		}
	}

	short boardid = static_cast<int>(this->config["AdminBoard"]) - 1;

	if (static_cast<std::size_t>(boardid) < this->boards.size())
	{
		std::string chat_log_dump;
		Board *admin_board = this->boards[boardid];

		Board_Post *newpost = new Board_Post;
		newpost->id = ++admin_board->last_id;
		newpost->author = from->SourceName();
		newpost->author_admin = from->admin;
		newpost->subject = std::string(" [Report] ") + util::ucfirst(from->SourceName()) + " reports: " + reportee;
		newpost->body = message;
		newpost->time = Timer::GetTime();

		if (int(this->config["ReportChatLogSize"]) > 0)
		{
			chat_log_dump = from->GetChatLogDump();
			newpost->body += "\r\n\r\n";
			newpost->body += chat_log_dump;
		}

		if (this->config["LogReports"])
		{
			try
			{
				this->db.Query("INSERT INTO `reports` (`reporter`, `reported`, `reason`, `time`, `chat_log`) VALUES ('$', '$', '$', #, '$')",
					from->SourceName().c_str(),
					reportee.c_str(),
					message.c_str(),
					int(std::time(0)),
					chat_log_dump.c_str()
				);
			}
			catch (Database_Exception& e)
			{
				Console::Err("Could not save report to database.");
				Console::Err("%s", e.error());
			}
		}

		admin_board->posts.push_front(newpost);

		if (admin_board->posts.size() > static_cast<std::size_t>(static_cast<int>(this->config["AdminBoardLimit"])))
		{
			admin_board->posts.pop_back();
		}
	}
}

void World::AdminRequest(Character *from, std::string message)
{
	message = util::text_cap(message, static_cast<int>(this->config["ChatMaxWidth"]) - util::text_width(util::ucfirst(from->SourceName()) + "  needs help: "));

	PacketBuilder builder(PACKET_ADMININTERACT, PACKET_REPLY, 4 + from->SourceName().length() + message.length());
	builder.AddChar(1); // message type
	builder.AddByte(255);
	builder.AddBreakString(from->SourceName());
	builder.AddBreakString(message);

	UTIL_FOREACH(this->characters, character)
	{
		if (character->SourceAccess() >= static_cast<int>(this->admin_config["reports"]))
		{
			character->Send(builder);
		}
	}

	short boardid = static_cast<int>(this->server->world->config["AdminBoard"]) - 1;

	if (static_cast<std::size_t>(boardid) < this->server->world->boards.size())
	{
		Board *admin_board = this->server->world->boards[boardid];

		Board_Post *newpost = new Board_Post;
		newpost->id = ++admin_board->last_id;
		newpost->author = from->SourceName();
		newpost->author_admin = from->admin;
		newpost->subject = std::string(" [Request] ") + util::ucfirst(from->SourceName()) + " needs help";
		newpost->body = message;
		newpost->time = Timer::GetTime();

		admin_board->posts.push_front(newpost);

		if (admin_board->posts.size() > static_cast<std::size_t>(static_cast<int>(this->server->world->config["AdminBoardLimit"])))
		{
			admin_board->posts.pop_back();
		}
	}
}

void World::Rehash()
{
	try
	{
		this->config.Read("config.ini");
		this->admin_config.Read("admin.ini");
		this->drops_config.Read(this->config["DropsFile"]);
		this->shops_config.Read(this->config["ShopsFile"]);
		this->arenas_config.Read(this->config["ArenasFile"]);
		this->formulas_config.Read(this->config["FormulasFile"]);
		this->home_config.Read(this->config["HomeFile"]);
		this->skills_config.Read(this->config["SkillsFile"]);
	}
	catch (std::runtime_error &e)
	{
		Console::Err(e.what());
	}

	this->UpdateConfig();
	this->LoadHome();

	UTIL_FOREACH(this->maps, map)
	{
		map->LoadArena();
	}

	UTIL_FOREACH_CREF(this->npc_data, npc)
	{
		if (npc->id != 0)
			npc->LoadShopDrop();
	}
}

void World::ReloadPub(bool quiet)
{
	auto eif_id = this->eif->rid;
	auto enf_id = this->enf->rid;
	auto esf_id = this->esf->rid;
	auto ecf_id = this->ecf->rid;

	this->eif->Read(this->config["EIF"]);
	this->enf->Read(this->config["ENF"]);
	this->esf->Read(this->config["ESF"]);
	this->ecf->Read(this->config["ECF"]);

	if (eif_id != this->eif->rid || enf_id != this->enf->rid
	 || esf_id != this->esf->rid || ecf_id != this->ecf->rid)
	{
		if (!quiet)
		{
			UTIL_FOREACH(this->characters, character)
			{
				character->ServerMsg("The server has been reloaded, please log out and in again.");
			}
		}
	}

	std::size_t current_npcs = this->npc_data.size();
	std::size_t new_npcs = this->enf->data.size();

	this->npc_data.resize(new_npcs);

	for (std::size_t i = current_npcs; i < new_npcs; ++i)
	{
		npc_data[i]->LoadShopDrop();
	}
}

void World::ReloadQuests()
{
	// Back up character quest states
	UTIL_FOREACH(this->characters, c)
	{
		UTIL_FOREACH(c->quests, q)
		{
			if (!q.second)
				continue;

			short quest_id = q.first;
			std::string quest_name = q.second->StateName();
			std::string progress = q.second->SerializeProgress();

			c->quests_inactive.insert({quest_id, quest_name, progress});
		}
	}

	// Clear character quest states
	UTIL_FOREACH(this->characters, c)
	{
		c->quests.clear();
	}

	// Reload all quests
	short max_quest = static_cast<int>(this->config["Quests"]);

	UTIL_FOREACH(this->enf->data, npc)
	{
		if (npc.type == ENF::Quest)
			max_quest = std::max(max_quest, npc.vendor_id);
	}

	for (short i = 0; i <= max_quest; ++i)
	{
		try
		{
			std::shared_ptr<Quest> q = std::make_shared<Quest>(i, this);
			this->quests[i] = std::move(q);
		}
		catch (...)
		{
			this->quests.erase(i);
		}
	}

	// Reload quests that might still be loaded above the highest quest npc ID
	UTIL_IFOREACH(this->quests, it)
	{
		if (it->first > max_quest)
		{
			try
			{
				std::shared_ptr<Quest> q = std::make_shared<Quest>(it->first, this);
				std::swap(it->second, q);
			}
			catch (...)
			{
				it = this->quests.erase(it);
			}
		}
	}

	// Restore character quest states
	UTIL_FOREACH(this->characters, c)
	{
		c->quests.clear();

		UTIL_FOREACH(c->quests_inactive, state)
		{
			auto quest_it = this->quests.find(state.quest_id);

			if (quest_it == this->quests.end())
			{
				Console::Wrn("Quest not found: %i. Marking as inactive.", state.quest_id);
				continue;
			}

			// WARNING: holds a non-tracked reference to shared_ptr
			Quest* quest = quest_it->second.get();
			auto quest_context(std::make_shared<Quest_Context>(c, quest));

			try
			{
				quest_context->SetState(state.quest_state, false);
				quest_context->UnserializeProgress(UTIL_CRANGE(state.quest_progress));
			}
			catch (EOPlus::Runtime_Error& ex)
			{
				Console::Wrn(ex.what());
				Console::Wrn("Could not resume quest: %i. Marking as inactive.", state.quest_id);

				if (!c->quests_inactive.insert(std::move(state)).second)
					Console::Wrn("Duplicate inactive quest record dropped for quest: %i", state.quest_id);

				continue;
			}

			auto result = c->quests.insert(std::make_pair(state.quest_id, std::move(quest_context)));

			if (!result.second)
			{
				Console::Wrn("Duplicate quest record dropped for quest: %i", state.quest_id);
				continue;
			}
		}
	}

	// Check new quest rules
	UTIL_FOREACH(this->characters, c)
	{
		// TODO: If a character is removed by a quest rule...
		c->CheckQuestRules();
	}

	Console::Out("%i/%i quests loaded.", this->quests.size(), max_quest);
}

Character *World::GetCharacter(std::string name)
{
	name = util::lowercase(name);

	UTIL_FOREACH(this->characters, character)
	{
		if (character->SourceName() == name)
		{
			return character;
		}
	}

	return 0;
}

Character *World::GetCharacterReal(std::string real_name)
{
	real_name = util::lowercase(real_name);

	UTIL_FOREACH(this->characters, character)
	{
		if (character->real_name == real_name)
		{
			return character;
		}
	}

	return 0;
}

Character *World::GetCharacterPID(unsigned int id)
{
	UTIL_FOREACH(this->characters, character)
	{
		if (character->PlayerID() == id)
		{
			return character;
		}
	}

	return 0;
}

Character *World::GetCharacterCID(unsigned int id)
{
	UTIL_FOREACH(this->characters, character)
	{
		if (character->id == id)
		{
			return character;
		}
	}

	return 0;
}

Map *World::GetMap(short id)
{
	try
	{
		return this->maps.at(id - 1);
	}
	catch (...)
	{
		try
		{
			return this->maps.at(0);
		}
		catch (...)
		{
			throw std::runtime_error("Map #" + util::to_string(id) + " and fallback map #1 are unavailable");
		}
	}
}

const NPC_Data* World::GetNpcData(short id) const
{
	if (id >= 0 && id < npc_data.size())
		return npc_data[id].get();
	else
		return npc_data[0].get();
}

Home *World::GetHome(const Character *character) const
{
	Home *home = 0;
	static Home *null_home = new Home;

	UTIL_FOREACH(this->homes, h)
	{
		if (h->id == character->home)
		{
			return h;
		}
	}

	int current_home_level = -2;
	UTIL_FOREACH(this->homes, h)
	{
		if (h->level <= character->level && h->level > current_home_level)
		{
			home = h;
			current_home_level = h->level;
		}
	}

	if (!home)
	{
		home = null_home;
	}

	return home;
}

Home *World::GetHome(std::string id)
{
	UTIL_FOREACH(this->homes, h)
	{
		if (h->id == id)
		{
			return h;
		}
	}

	return 0;
}

bool World::CharacterExists(std::string name)
{
	Database_Result res = this->db.Query("SELECT 1 FROM `characters` WHERE `name` = '$'", name.c_str());
	return !res.empty();
}

Character *World::CreateCharacter(Player *player, std::string name, Gender gender, int hairstyle, int haircolor, Skin race)
{
	char buffer[1024];
	std::string startmapinfo;
	std::string startmapval;

	if (static_cast<int>(this->config["StartMap"]))
	{
		using namespace std;
		startmapinfo = ", `map`, `x`, `y`";
		snprintf(buffer, 1024, ",%i,%i,%i", static_cast<int>(this->config["StartMap"]), static_cast<int>(this->config["StartX"]), static_cast<int>(this->config["StartY"]));
		startmapval = buffer;
	}

	this->db.Query("INSERT INTO `characters` (`name`, `account`, `gender`, `hairstyle`, `haircolor`, `race`, `inventory`, `bank`, `paperdoll`, `spells`, `quest`, `vars`@) VALUES ('$','$',#,#,#,#,'$','','$','$','',''@)",
		startmapinfo.c_str(), name.c_str(), player->username.c_str(), gender, hairstyle, haircolor, race,
		static_cast<std::string>(this->config["StartItems"]).c_str(), static_cast<std::string>(gender?this->config["StartEquipMale"]:this->config["StartEquipFemale"]).c_str(),
		static_cast<std::string>(this->config["StartSpells"]).c_str(), startmapval.c_str());

	return new Character(name, this);
}

void World::DeleteCharacter(std::string name)
{
	this->db.Query("DELETE FROM `characters` WHERE name = '$'", name.c_str());
}

Player *World::Login(const std::string& username, util::secure_string&& password)
{
	if (LoginCheck(username, std::move(password)) == LOGIN_WRONG_USERPASS)
		return 0;

	return new Player(username, this);
}

Player *World::Login(std::string username)
{
	return new Player(username, this);
}

LoginReply World::LoginCheck(const std::string& username, util::secure_string&& password)
{
	{
		util::secure_string password_buffer(std::move(std::string(this->config["PasswordSalt"]) + username + password.str()));
		password = sha256(password_buffer.str());
	}

	Database_Result res = this->db.Query("SELECT 1 FROM `accounts` WHERE `username` = '$' AND `password` = '$'", username.c_str(), password.str().c_str());

	if (res.empty())
	{
		return LOGIN_WRONG_USERPASS;
	}
	else if (this->PlayerOnline(username))
	{
		return LOGIN_LOGGEDIN;
	}
	else
	{
		return LOGIN_OK;
	}
}

bool World::CreatePlayer(const std::string& username, util::secure_string&& password,
	const std::string& fullname, const std::string& location, const std::string& email,
	const std::string& computer, const std::string& hdid, const std::string& ip)
{
	{
		util::secure_string password_buffer(std::move(std::string(this->config["PasswordSalt"]) + username + password.str()));
		password = sha256(password_buffer.str());
	}

	Database_Result result = this->db.Query("INSERT INTO `accounts` (`username`, `password`, `fullname`, `location`, `email`, `computer`, `hdid`, `regip`, `created`) VALUES ('$','$','$','$','$','$','$','$',#)",
		username.c_str(), password.str().c_str(), fullname.c_str(), location.c_str(), email.c_str(), computer.c_str(), hdid.c_str(), ip.c_str(), int(std::time(0)));

	return !result.Error();
}

bool World::PlayerExists(std::string username)
{
	Database_Result res = this->db.Query("SELECT 1 FROM `accounts` WHERE `username` = '$'", username.c_str());
	return !res.empty();
}

bool World::PlayerOnline(std::string username)
{
	if (!Player::ValidName(username))
	{
		return false;
	}

	UTIL_FOREACH(this->server->clients, client)
	{
		EOClient *eoclient = static_cast<EOClient *>(client);

		if (eoclient->player)
		{
			if (eoclient->player->username.compare(username) == 0)
			{
				return true;
			}
		}
	}

	return false;
}

void World::Kick(Command_Source *from, Character *victim, bool announce)
{
	if (announce)
		this->ServerMsg(i18n.Format("announce_removed", victim->SourceName(), from ? from->SourceName() : "server", i18n.Format("kicked")));

	victim->player->client->Close();
}

void World::Jail(Command_Source *from, Character *victim, bool announce)
{
	if (announce)
		this->ServerMsg(i18n.Format("announce_removed", victim->SourceName(), from ? from->SourceName() : "server", i18n.Format("jailed")));

	bool bubbles = this->config["WarpBubbles"] && !victim->IsHideWarp();

	Character* charfrom = dynamic_cast<Character*>(from);

	if (charfrom && charfrom->IsHideWarp())
		bubbles = false;

	victim->Warp(static_cast<int>(this->config["JailMap"]), static_cast<int>(this->config["JailX"]), static_cast<int>(this->config["JailY"]), bubbles ? WARP_ANIMATION_ADMIN : WARP_ANIMATION_NONE);
}

void World::Unjail(Command_Source *from, Character *victim)
{
	bool bubbles = this->config["WarpBubbles"] && !victim->IsHideWarp();

	Character* charfrom = dynamic_cast<Character*>(from);

	if (charfrom && charfrom->IsHideWarp())
		bubbles = false;

	if (victim->mapid != static_cast<int>(this->config["JailMap"]))
		return;

	victim->Warp(static_cast<int>(this->config["JailMap"]), static_cast<int>(this->config["UnJailX"]), static_cast<int>(this->config["UnJailY"]), bubbles ? WARP_ANIMATION_ADMIN : WARP_ANIMATION_NONE);
}

void World::Ban(Command_Source *from, Character *victim, int duration, bool announce)
{
	std::string from_str = from ? from->SourceName() : "server";

	if (announce)
		this->ServerMsg(i18n.Format("announce_removed", victim->SourceName(), from_str, i18n.Format("banned")));

	std::string query("INSERT INTO bans (username, ip, hdid, expires, setter) VALUES ");

	query += "('" + db.Escape(victim->player->username) + "', ";
	query += util::to_string(static_cast<int>(victim->player->client->GetRemoteAddr())) + ", ";
	query += util::to_string(victim->player->client->hdid) + ", ";

	if (duration == -1)
	{
		query += "0";
	}
	else
	{
		query += util::to_string(int(std::time(0) + duration));
	}

	query += ", '" + db.Escape(from_str) + "')";

	try
	{
		this->db.Query(query.c_str());
	}
	catch (Database_Exception& e)
	{
		Console::Err("Could not save ban to database.");
		Console::Err("%s", e.error());
	}

	victim->player->client->Close();
}

void World::Mute(Command_Source *from, Character *victim, bool announce)
{
	if (announce && !this->config["SilentMute"])
		this->ServerMsg(i18n.Format("announce_muted", victim->SourceName(), from ? from->SourceName() : "server", i18n.Format("banned")));

	victim->Mute(from);
}

int World::CheckBan(const std::string *username, const IPAddress *address, const int *hdid)
{
	std::string query("SELECT COALESCE(MAX(expires),-1) AS expires FROM bans WHERE (");

	if (!username && !address && !hdid)
	{
		return -1;
	}

	if (username)
	{
		query += "username = '";
		query += db.Escape(*username);
		query += "' OR ";
	}

	if (address)
	{
		query += "ip = ";
		query += util::to_string(static_cast<int>(*const_cast<IPAddress *>(address)));
		query += " OR ";
	}

	if (hdid)
	{
		query += "hdid = ";
		query += util::to_string(*hdid);
		query += " OR ";
	}

	Database_Result res = db.Query((query.substr(0, query.length()-4) + ") AND (expires > # OR expires = 0)").c_str(), int(std::time(0)));

	return static_cast<int>(res[0]["expires"]);
}

bool World::IsPasswordSecure(const util::secure_string& password) const
{
	return (this->insecure_passwords.find(password.str()) == this->insecure_passwords.end());
}

static std::list<int> PKExceptUnserialize(std::string serialized)
{
	std::list<int> list;
	std::size_t p = 0;
	std::size_t lastp = std::numeric_limits<std::size_t>::max();

	if (!serialized.empty() && *(serialized.end()-1) != ',')
	{
		serialized.push_back(',');
	}

	while ((p = serialized.find_first_of(',', p+1)) != std::string::npos)
	{
		list.push_back(util::to_int(serialized.substr(lastp+1, p-lastp-1)));
		lastp = p;
	}

	return list;
}

bool World::PKExcept(const Map *map)
{
	return this->PKExcept(map->id);
}

bool World::PKExcept(int mapid)
{
	if (mapid == static_cast<int>(this->config["JailMap"]))
	{
		return true;
	}

	if (this->GetMap(mapid)->arena)
	{
		return true;
	}

	std::list<int> except_list = PKExceptUnserialize(this->config["PKExcept"]);

	return std::find(except_list.begin(), except_list.end(), mapid) != except_list.end();
}

bool World::IsInstrument(int graphic_id)
{
	return std::find(UTIL_RANGE(this->instrument_ids), graphic_id) != this->instrument_ids.end();
}

World::~World()
{
	UTIL_FOREACH(this->maps, map)
	{
		delete map;
	}

	UTIL_FOREACH(this->homes, home)
	{
		delete home;
	}

	UTIL_FOREACH(this->boards, board)
	{
		delete board;
	}

	delete this->eif;
	delete this->enf;
	delete this->esf;
	delete this->ecf;

	delete this->guildmanager;

	if (this->config["TimedSave"])
	{
		this->db.Commit();
	}
}
