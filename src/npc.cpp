
/* $Id$
 * EOSERV is released under the zlib license.
 * See LICENSE.txt for more info.
 */

#include "npc.hpp"

#include "character.hpp"
#include "config.hpp"
#include "eodata.hpp"
#include "map.hpp"
#include "npc_ai.hpp"
#include "npc_ai/npc_ai_ranged.hpp"
#include "npc_ai/npc_ai_hw2016_apozen.hpp"
#include "npc_ai/npc_ai_hw2016_banshee.hpp"
#include "npc_ai/npc_ai_hw2016_crane.hpp"
#include "npc_ai/npc_ai_hw2016_cursed_mask.hpp"
#include "npc_ai/npc_ai_hw2016_dark_magician.hpp"
#include "npc_ai/npc_ai_hw2016_inferno_grenade.hpp"
#include "npc_ai/npc_ai_hw2016_skeleton_warlock.hpp"
#include "npc_ai/npc_ai_hw2016_tentacle.hpp"
#include "npc_data.hpp"
#include "packet.hpp"
#include "party.hpp"
#include "quest.hpp"
#include "timer.hpp"
#include "world.hpp"

#include "console.hpp"
#include "util.hpp"
#include "util/rpn.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <list>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

static double speed_table[8] = {0.9, 0.6, 1.3, 1.9, 3.7, 7.5, 15.0, 0.0};

void NPC::SetSpeedTable(std::array<double, 7> speeds)
{
	for (std::size_t i = 0; i < 7; ++i)
	{
		if (speeds[i] != 0.0)
			speed_table[i] = speeds[i];
	}
}

NPC::NPC(Map *map, short id, unsigned char x, unsigned char y, unsigned char spawn_type, short spawn_time, unsigned char index, bool temporary)
{
	this->id = id;
	this->map = map;
	this->temporary = temporary;
	this->index = index;
	this->spawn_x = this->x = x;
	this->spawn_y = this->y = y;
	this->alive = false;
	this->totaldamage = 0;

	if (spawn_type > 7)
	{
		spawn_type = 7;
	}

	this->spawn_type = spawn_type;
	this->spawn_time = spawn_time;

	if (spawn_type == 7)
	{
		this->direction = static_cast<Direction>(spawn_time & 0x03);
		this->spawn_time = 0;
	}
	else
	{
		this->direction = DIRECTION_DOWN;
	}

	this->parent = 0;
}

const NPC_Data& NPC::Data() const
{
	return *this->map->world->GetNpcData(this->id);
}

const ENF_Data& NPC::ENF() const
{
	return this->Data().ENF();
}

void NPC::Spawn(NPC *parent)
{
	if (this->alive)
		return;

	if (this->ENF().boss && !parent)
	{
		UTIL_FOREACH(this->map->npcs, npc)
		{
			if (npc->ENF().child)
			{
				npc->Spawn(this);
			}
		}
	}

	if (parent)
	{
		this->parent = parent;
	}

	if (this->spawn_type < 7)
	{
		bool found = false;
		for (int i = 0; i < 200; ++i)
		{
			if (this->temporary && i == 0)
			{
				this->x = this->spawn_x;
				this->y = this->spawn_y;
			}
			else
			{
				this->x = util::rand(this->spawn_x-2, this->spawn_x+2);
				this->y = util::rand(this->spawn_y-2, this->spawn_y+2);
			}

			if (this->map->Walkable(this->x, this->y, true) && (i > 100 || !this->map->Occupied(this->x, this->y, Map::NPCOnly)))
			{
				this->direction = static_cast<Direction>(util::rand(0,3));
				found = true;
				break;
			}
		}

		if (!found)
		{
			Console::Wrn("An NPC on map %i at %i,%i is being placed by linear scan of spawn area (%s)", this->map->id, this->spawn_x, this->spawn_y, this->map->world->enf->Get(this->id).name.c_str());
			for (this->x = this->spawn_x-2; this->x <= spawn_x+2; ++this->x)
			{
				for (this->y = this->spawn_y-2; this->y <= this->spawn_y+2; ++this->y)
				{
					if (this->map->Walkable(this->x, this->y, true))
					{
						Console::Wrn("Placed at valid location: %i,%i", this->x, this->y);
						found = true;
						goto end_linear_scan;
					}
				}
			}
		}
		end_linear_scan:

		if (!found)
		{
			Console::Err("NPC couldn't spawn anywhere valid!");
		}
	}

	this->alive = true;
	this->hp = this->ENF().hp;
	this->last_act = Timer::GetTime();
	this->act_speed = speed_table[this->spawn_type];
	
	if (this->map->id >= 286 && this->map->id <= 289 && this->map->world->hw2016_state > 0)
	{
		if (this->id != 350)
			this->hp *= this->map->world->hw2016_monstermod;
	}
	
	this->peakhp = this->hp;

	if (this->peakhp == 0)
		this->peakhp = 1;

	PacketBuilder builder(PACKET_APPEAR, PACKET_REPLY, 8);
	builder.AddChar(0);
	builder.AddByte(255);
	builder.AddChar(this->index);
	builder.AddShort(this->id);
	builder.AddChar(this->x);
	builder.AddChar(this->y);
	builder.AddChar(this->direction);

	UTIL_FOREACH(this->map->characters, character)
	{
		if (character->InRange(this))
		{
			character->Send(builder);
		}
	}

	if (!ai)
	{
		if (parent)
			ai.reset(new NPC_AI_Legacy(this));
		else
		{
			switch (this->id)
			{
				case 329:
					ai.reset(new NPC_AI_HW2016_Apozen(this));
					break;
				
				case 337: // BANSHEE
					ai.reset(new NPC_AI_HW2016_Banshee(this));
					break;

				case 342: // CRANE
					ai.reset(new NPC_AI_HW2016_Crane(this));
					break;

				case 343: // Cursed mask
					ai.reset(new NPC_AI_HW2016_Cursed_Mask(this));
					break;

				case 336: // Dark Magician
					ai.reset(new NPC_AI_HW2016_Dark_Magician(this));
					break;

				case 338: // Headless hunter
					ai.reset(new NPC_AI_Ranged(this));
					break;

				case 340: // inferno grenade
					ai.reset(new NPC_AI_HW2016_Inferno_Grenade(this));
					break;

				case 334: // Skeleton warlock
					ai.reset(new NPC_AI_HW2016_Skeleton_Warlock(this));
					break;

				case 341: // Tanktentacle
					ai.reset(new NPC_AI_HW2016_Tentacle(this));
					break;

				default:
					ai.reset(new NPC_AI_Standard(this));
			}
		}
	}

	ai->Spawn();
}

void NPC::Act()
{
	// Needed for the server startup spawn to work properly
	if (this->ENF().child && !this->parent)
	{
		UTIL_FOREACH(this->map->npcs, npc)
		{
			if (npc->ENF().boss)
			{
				this->parent = npc;
				break;
			}
		}
	}

	this->last_act += double(util::rand(int(this->act_speed * 750.0), int(this->act_speed * 1250.0))) / 1000.0;

	if (this->spawn_type == 7)
	{
		return;
	}

	if (ai) ai->Act();
}

bool NPC::Walk(Direction direction, bool playerghost)
{
	return this->map->Walk(this, direction, playerghost);
}

void NPC::Damage(Character *from, int amount, int spell_id)
{
	int limitamount = std::min(this->hp, amount);

	if (this->map->world->config["LimitDamage"])
	{
		amount = limitamount;
	}

	if (this->ENF().type == ENF::Passive || this->ENF().type == ENF::Aggressive)
	{
		this->hp -= amount;
	}
	else
	{
		this->hp = 0;
		amount = 0;
	}

	if (this->totaldamage + limitamount > this->totaldamage)
		this->totaldamage += limitamount;

	std::unique_ptr<NPC_Opponent> opponent(new NPC_Opponent);
	bool found = false;

	UTIL_FOREACH_CREF(this->damagelist, checkopp)
	{
		if (checkopp->attacker == from)
		{
			found = true;

			if (checkopp->damage + limitamount > checkopp->damage)
				checkopp->damage += limitamount;

			checkopp->last_hit = Timer::GetTime();
		}
	}

	if (!found)
	{
		opponent->attacker = from;
		opponent->damage = limitamount;
		opponent->last_hit = Timer::GetTime();
		opponent->attacker->RegisterNPC(this);
		this->damagelist.emplace_back(std::move(opponent));
	}

	if (ai) ai->HitBy(from, spell_id != -1, amount);

	if (this->hp > 0)
	{
		PacketBuilder builder(spell_id == -1 ? PACKET_NPC : PACKET_CAST, PACKET_REPLY, 14);

		if (spell_id != -1)
			builder.AddShort(spell_id);

		builder.AddShort(from->PlayerID());
		builder.AddChar(from->direction);
		builder.AddShort(this->index);
		builder.AddThree(amount);
		builder.AddShort(util::clamp<int>(double(this->hp) / double(this->peakhp) * 100.0, 0, 100));

		if (spell_id != -1)
			builder.AddShort(from->tp);
		else
			builder.AddChar(1); // ?

		UTIL_FOREACH(this->map->characters, character)
		{
			if (character->InRange(this))
			{
				character->Send(builder);
			}
		}
	}
	else
	{
		this->Killed(from, amount, spell_id);
		// *this may not be valid here
		return;
	}
}

void NPC::RemoveFromView(Character *target)
{
	PacketBuilder builder(PACKET_NPC, PACKET_PLAYER, 7);
	builder.AddChar(this->index);
	if (target->x > 200 && target->y > 200)
	{
		builder.AddChar(0); // x
		builder.AddChar(0); // y
	}
	else
	{
		builder.AddChar(252); // x
		builder.AddChar(252); // y
	}
	builder.AddChar(0); // direction
	builder.AddByte(255);
	builder.AddByte(255);
	builder.AddByte(255);

	PacketBuilder builder2(PACKET_NPC, PACKET_SPEC, 5);
	builder2.AddShort(0); // killer pid
	builder2.AddChar(0); // killer direction
	builder2.AddShort(this->index);
/*
	builder2.AddShort(0); // dropped item uid
	builder2.AddShort(0); // dropped item id
	builder2.AddChar(this->x);
	builder2.AddChar(this->y);
	builder2.AddInt(0); // dropped item amount
	builder2.AddThree(0); // damage
*/

	target->Send(builder);
	target->Send(builder2);
}

void NPC::Killed(Character *from, int amount, int spell_id)
{
	double droprate = this->map->world->config["DropRate"];
	double exprate = this->map->world->config["ExpRate"];
	int sharemode = this->map->world->config["ShareMode"];
	int partysharemode = this->map->world->config["PartyShareMode"];
	int dropratemode = this->map->world->config["DropRateMode"];
	std::set<Party *> parties;

	int most_damage_counter = 0;
	Character *most_damage = nullptr;
	NPC_Drop *drop = nullptr;

	if (ai)
	{
		if (ai->Dying())
			return;
	}

	this->alive = false;

	this->dead_since = int(Timer::GetTime());

	if (dropratemode == 1)
	{
		std::vector<NPC_Drop *> drops;

		UTIL_FOREACH_CREF(this->Data().drops, checkdrop)
		{
			if (util::rand(0.0, 100.0) <= checkdrop->chance * droprate)
			{
				drops.push_back(checkdrop.get());
			}
		}

		if (drops.size() > 0)
		{
			drop = drops[util::rand(0, drops.size()-1)];
		}
	}
	else if (dropratemode == 2)
	{
		UTIL_FOREACH_CREF(this->Data().drops, checkdrop)
		{
			if (util::rand(0.0, 100.0) <= checkdrop->chance * droprate)
			{
				drop = checkdrop.get();
				break;
			}
		}
	}
	else if (dropratemode == 3)
	{
		double roll = util::rand(0.0, this->Data().drops_chance_total);

		UTIL_FOREACH_CREF(this->Data().drops, checkdrop)
		{
			if (roll >= checkdrop->chance_offset && roll < checkdrop->chance_offset+checkdrop->chance)
			{
				drop = checkdrop.get();
				break;
			}
		}
	}

	if (sharemode == 1)
	{
		UTIL_FOREACH_CREF(this->damagelist, opponent)
		{
			if (opponent->damage > most_damage_counter)
			{
				most_damage_counter = opponent->damage;
				most_damage = opponent->attacker;
			}
		}
	}

	int dropuid = 0;
	int dropid = 0;
	int dropamount = 0;
	Character* drop_winner = nullptr;

	if (drop)
	{
		dropid = drop->id;
		dropamount = std::min<int>(util::rand(drop->min, drop->max), this->map->world->config["MaxItem"]);

		if (dropid <= 0 || static_cast<std::size_t>(dropid) >= this->map->world->eif->data.size() || dropamount <= 0)
			goto abort_drop;

		dropuid = this->map->GenerateItemID();

		int protect_time = static_cast<int>(this->map->world->config["ProtectNPCDrop"]);
		
		if (this->id == 350)
			protect_time = 900;
		
		std::shared_ptr<Map_Item> newitem(std::make_shared<Map_Item>(dropuid, dropid, dropamount, this->x, this->y, from->PlayerID(), Timer::GetTime() + protect_time));
		this->map->items.push_back(newitem);

		// Selects a random number between 0 and maxhp, and decides the winner based on that
		switch (sharemode)
		{
			case 0:
				drop_winner = from;
				break;

			case 1:
				drop_winner = most_damage;
				break;

			case 2:
			{
				int rewarded_hp = util::rand(0, this->totaldamage - 1);
				int count_hp = 0;
				UTIL_FOREACH_CREF(this->damagelist, opponent)
				{
					if (opponent->attacker->InRange(this))
					{
						if (rewarded_hp >= count_hp && rewarded_hp < opponent->damage)
						{
							drop_winner = opponent->attacker;
							break;
						}

						count_hp += opponent->damage;
					}
				}
			}
				break;

			case 3:
			{
				int rand = util::rand(0, this->damagelist.size() - 1);
				int i = 0;
				UTIL_FOREACH_CREF(this->damagelist, opponent)
				{
					if (opponent->attacker->InRange(this))
					{
						if (rand == i++)
						{
							drop_winner = opponent->attacker;
							break;
						}
					}
				}
			}
				break;
		}
	}
	abort_drop:

	if (drop_winner)
		this->map->items.back()->owner = drop_winner->PlayerID();

	int is_hw2016 = this->map->id >= 286 && this->map->id <= 288;
	int is_hw2016_apozen = (this->map->world->config["Halloween2016"] || this->map->id == 289) && this->id == 329;
	
	if (is_hw2016 && most_damage)
		most_damage->hw2016_points += 2;
	
	if (is_hw2016_apozen)
	{
		int highest_score = -1;
		Character* highest_scorer = nullptr;
		int num_chests = 0;

		UTIL_FOREACH(this->map->characters, character)
		{
			std::list<std::unique_ptr<NPC_Opponent>>::iterator findopp = this->damagelist.begin();
			for (; findopp != this->damagelist.end() && (*findopp)->attacker != character; ++findopp); // no loop body
			
			if (this->map->id != 289 && findopp == this->damagelist.end())
				continue;
			
			if (this->map->id == 289 && character->hw2016_points > highest_score)
			{
				highest_score = character->hw2016_points;
				highest_scorer = character;
			}

			character->exp += 1000 + character->hw2016_points * 2;
			character->exp = std::min(character->exp, static_cast<int>(this->map->world->config["MaxExp"]));
			
			bool level_up = false;
			PacketBuilder builder(spell_id == -1 ? PACKET_NPC : PACKET_CAST, PACKET_SPEC, 26);

			while (character->level < static_cast<int>(this->map->world->config["MaxLevel"]) && character->exp >= this->map->world->exp_table[character->level+1])
			{
				level_up = true;
				++character->level;
				character->statpoints += static_cast<int>(this->map->world->config["StatPerLevel"]);
				character->skillpoints += static_cast<int>(this->map->world->config["SkillPerLevel"]);
				character->CalculateStats();
			}

			if (level_up)
			{
				builder.SetID(spell_id == -1 ? PACKET_NPC : PACKET_CAST, PACKET_ACCEPT);
				builder.ReserveMore(33);
			}

			if (spell_id != -1)
				builder.AddShort(spell_id);

			builder.AddShort(drop_winner ? drop_winner->PlayerID() : from->PlayerID());
			builder.AddChar(drop_winner ? drop_winner->direction : from->direction);
			builder.AddShort(this->index);
			builder.AddShort(dropuid);
			builder.AddShort(dropid);
			builder.AddChar(this->x);
			builder.AddChar(this->y);
			builder.AddInt(dropamount);
			builder.AddThree(amount);

			if (spell_id != -1)
				builder.AddShort(from->tp);

			if ((sharemode == 0 && character == from) || (sharemode != 0 && findopp != this->damagelist.end()))
			{
				builder.AddInt(character->exp);
			}

			if (level_up)
			{
				builder.AddChar(character->level);
				builder.AddShort(character->statpoints);
				builder.AddShort(character->skillpoints);
				builder.AddShort(character->maxhp);
				builder.AddShort(character->maxtp);
				builder.AddShort(character->maxsp);
			}

			character->Send(builder);
			
			character->ServerMsg("Apozen's assault has been contained! Your score was: " + util::to_string(character->hw2016_points));
		}
		
		UTIL_FOREACH(this->map->characters, character)
		{
			if (this->map->id == 289)
			{
				if (character == highest_scorer)
				{
					num_chests += 3;
					character->hw2016_chests += 3;
					character->ServerMsg("You may open three reward chests!");
				}
				else
				{
					num_chests += 2;
					character->hw2016_chests += 2;
					character->ServerMsg("You may open two reward chests!");
				}
			}
		}
		
		if (highest_scorer)
		{
			UTIL_FOREACH(this->map->characters, character)
			{
				character->ServerMsg("Highest score: " + util::ucfirst(highest_scorer->SourceName()) + " (" + util::to_string(highest_score) + ")");
			}
		}

		this->map->world->hw2016_numchests = num_chests;
		this->map->world->hw2016_state = 50;
		this->map->world->hw2016_tick = -5;
	}
	else // not indented to make merging easier
	UTIL_FOREACH(this->map->characters, character)
	{
		std::list<std::unique_ptr<NPC_Opponent>>::iterator findopp = this->damagelist.begin();
		for (; findopp != this->damagelist.end() && (*findopp)->attacker != character; ++findopp); // no loop body

		if (findopp != this->damagelist.end() || character->InRange(this))
		{
			bool level_up = false;

			PacketBuilder builder(spell_id == -1 ? PACKET_NPC : PACKET_CAST, PACKET_SPEC, 26);

			if (this->ENF().exp != 0)
			{
				if (findopp != this->damagelist.end())
				{
					int reward;
					switch (sharemode)
					{
						case 0:
							if (character == from)
							{
								reward = int(std::ceil(double(this->ENF().exp) * exprate));

								if (reward > 0)
								{
									if (partysharemode && !is_hw2016)
									{
										if (character->party)
										{
											character->party->ShareEXP(reward, partysharemode, this->map);
										}
										else
										{
											character->exp += reward;
										}
									}
									else
									{
										character->exp += reward;
										if (is_hw2016) character->hw2016_points += std::min(reward / 2, 1);
									}
								}
							}
							break;

						case 1:
							if (character == most_damage)
							{
								reward = int(std::ceil(double(this->ENF().exp) * exprate));

								if (reward > 0)
								{
									if (partysharemode && !is_hw2016)
									{
										if (character->party && !is_hw2016)
										{
											character->party->ShareEXP(reward, partysharemode, this->map);
										}
										else
										{
											character->exp += reward;
										}
									}
									else
									{
										character->exp += reward;
										if (is_hw2016) character->hw2016_points += std::min(reward / 2, 1);
									}
								}
							}
							break;

						case 2:
							reward = int(std::ceil(double(this->ENF().exp) * exprate * (double((*findopp)->damage) / double(this->totaldamage))));

							if (reward > 0)
							{
								if (partysharemode && !is_hw2016)
								{
									if (character->party && !is_hw2016)
									{
										character->party->temp_expsum += reward;
										parties.insert(character->party);
									}
									else
									{
										character->exp += reward;
									}
								}
								else
								{
									character->exp += reward;
									if (is_hw2016) character->hw2016_points += std::min(reward / 2, 1);
								}
							}
							break;

						case 3:
							reward = int(std::ceil(double(this->ENF().exp) * exprate * (double(this->damagelist.size()) / 1.0)));

							if (reward > 0)
							{
								if (partysharemode && !is_hw2016)
								{
									if (character->party)
									{
										character->party->temp_expsum += reward;
									}
									else
									{
										character->exp += reward;
									}
								}
								else
								{
									character->exp += reward;
									if (is_hw2016) character->hw2016_points += std::min(reward / 2, 1);
								}
							}
							break;
					}

					character->exp = std::min(character->exp, static_cast<int>(this->map->world->config["MaxExp"]));

					while (character->level < static_cast<int>(this->map->world->config["MaxLevel"]) && character->exp >= this->map->world->exp_table[character->level+1])
					{
						level_up = true;
						++character->level;
						character->statpoints += static_cast<int>(this->map->world->config["StatPerLevel"]);
						character->skillpoints += static_cast<int>(this->map->world->config["SkillPerLevel"]);
						character->CalculateStats();
					}

					if (level_up)
					{
						builder.SetID(spell_id == -1 ? PACKET_NPC : PACKET_CAST, PACKET_ACCEPT);
						builder.ReserveMore(33);
					}
				}
			}

			if (spell_id != -1)
				builder.AddShort(spell_id);

			builder.AddShort(drop_winner ? drop_winner->PlayerID() : from->PlayerID());
			builder.AddChar(drop_winner ? drop_winner->direction : from->direction);
			builder.AddShort(this->index);
			builder.AddShort(dropuid);
			builder.AddShort(dropid);
			builder.AddChar(this->x);
			builder.AddChar(this->y);
			builder.AddInt(dropamount);
			builder.AddThree(amount);

			if (spell_id != -1)
				builder.AddShort(from->tp);

			if ((sharemode == 0 && character == from) || (sharemode != 0 && findopp != this->damagelist.end()))
			{
				builder.AddInt(character->exp);
			}

			if (level_up)
			{
				builder.AddChar(character->level);
				builder.AddShort(character->statpoints);
				builder.AddShort(character->skillpoints);
				builder.AddShort(character->maxhp);
				builder.AddShort(character->maxtp);
				builder.AddShort(character->maxsp);
			}

			character->Send(builder);
		}
	}

	UTIL_FOREACH(parties, party)
	{
		party->ShareEXP(party->temp_expsum, partysharemode, this->map);
		party->temp_expsum = 0;
	}

	UTIL_FOREACH_CREF(this->damagelist, opponent)
	{
		opponent->attacker->UnregisterNPC(this);
		if (ai) ai->Unregister(opponent->attacker);
	}

	this->damagelist.clear();
	this->totaldamage = 0;

	short childid = -1;

	if (this->ENF().boss)
	{
		std::vector<NPC*> child_npcs;

		UTIL_FOREACH(this->map->npcs, npc)
		{
			if (npc->ENF().child && !npc->ENF().boss && npc->alive)
			{
				child_npcs.push_back(npc);
			}
		}

		UTIL_FOREACH(child_npcs, npc)
		{
			if (!npc->temporary && (childid == -1 || childid == npc->id))
			{
				npc->Die(false);
				childid = npc->id;
			}
			else
			{
				npc->Die(true);
			}
		}
	}

	if (childid != -1)
	{
		PacketBuilder builder(PACKET_NPC, PACKET_JUNK, 2);
		builder.AddShort(childid);

		UTIL_FOREACH(this->map->characters, character)
		{
			character->Send(builder);
		}
	}

	if (this->temporary)
	{
		this->map->npcs.erase(
			std::remove(this->map->npcs.begin(), this->map->npcs.end(), this),
			this->map->npcs.end()
		);
	}

	UTIL_FOREACH(from->quests, q)
	{
		if (!q.second || q.second->GetQuest()->Disabled())
			continue;

		q.second->KilledNPC(this->ENF().id);
	}

	if (this->temporary)
	{
		delete this;
		return;
	}
}

void NPC::Die(bool show)
{
	if (!this->alive)
		return;

	if (ai) ai->Dying();

	this->alive = false;
	this->parent = 0;
	this->dead_since = int(Timer::GetTime());

	UTIL_FOREACH_CREF(this->damagelist, opponent)
	{
		opponent->attacker->UnregisterNPC(this);
		if (ai) ai->Unregister(opponent->attacker);
	}

	this->damagelist.clear();
	this->totaldamage = 0;

	if (show)
	{
		PacketBuilder builder(PACKET_NPC, PACKET_SPEC, 18);
		builder.AddShort(0); // killer pid
		builder.AddChar(0); // killer direction
		builder.AddShort(this->index);
		builder.AddShort(0); // dropped item uid
		builder.AddShort(0); // dropped item id
		builder.AddChar(this->x);
		builder.AddChar(this->y);
		builder.AddInt(0); // dropped item amount
		builder.AddThree(this->hp); // damage

		UTIL_FOREACH(this->map->characters, character)
		{
			if (character->InRange(this))
			{
				character->Send(builder);
			}
		}
	}

	if (this->temporary)
	{
		this->map->npcs.erase(
			std::remove(this->map->npcs.begin(), this->map->npcs.end(), this),
			this->map->npcs.end()
		);

		delete this;
	}
}

void NPC::Effect(int effect)
{
	this->map->SpellEffect(this->x, this->y, effect);
}

void NPC::RegisterCharacter(Character* character)
{
	character->RegisterNPC(this);
}

void NPC::UnregisterCharacter(Character* character)
{
	this->UnregisterCharacterEvent(character);
	character->UnregisterNPC(this);
}

void NPC::UnregisterCharacterEvent(Character* character)
{
	std::list<std::unique_ptr<NPC_Opponent>>::iterator findopp = this->damagelist.begin();
	for (; findopp != this->damagelist.end() && (*findopp)->attacker != character; ++findopp); // no loop body
	
	if (findopp != this->damagelist.end())
	{
		this->totaldamage -= (*findopp)->damage;
		this->damagelist.erase(findopp);
	}

	if (ai) ai->Unregister(character);
}

void NPC::Attack(Character *target, bool ranged)
{
	int amount = util::rand(this->ENF().mindam, this->ENF().maxdam + static_cast<int>(this->map->world->config["NPCAdjustMaxDam"]));
	double rand = util::rand(0.0, 1.0);
	
	// Halloween 2016 NPC damage formula
	if (this->id >= 329 && this->id <= 349)
	{
		int mindam = this->ENF().mindam;
		int maxdam = this->ENF().maxdam;
		
		maxdam += this->hw2016_apoweak * 2;
		
		if (this->id == 342 && this->map->world->hw2016_state == 30)
		{
			mindam += this->map->world->hw2016_tick;
			maxdam += this->map->world->hw2016_tick;
		}
		
		mindam = std::max<int>(target->maxhp * (mindam / 100.0), 1);
		maxdam = std::max<int>(target->maxhp * (maxdam / 100.0), 1);
		
		if (target->shielded_until > Timer::GetTime())
		{
			mindam /= 2;
			maxdam /= 2;
			mindam = std::max<int>(mindam, 1);
			maxdam = std::max<int>(maxdam, 1);
		}

		amount = util::rand(mindam, maxdam);
	}
	else
	{
		// Checks if target is facing you
		bool critical = std::abs(int(target->direction) - this->direction) != 2 || rand < static_cast<double>(this->map->world->config["CriticalRate"]);

		std::unordered_map<std::string, double> formula_vars;

		this->FormulaVars(formula_vars);
		target->FormulaVars(formula_vars, "target_");
		formula_vars["modifier"] = 1.0 / static_cast<double>(this->map->world->config["MobRate"]);
		formula_vars["damage"] = amount;
		formula_vars["critical"] = critical;

		amount = rpn_eval(rpn_parse(this->map->world->formulas_config["damage"]), formula_vars);
		double hit_rate = rpn_eval(rpn_parse(this->map->world->formulas_config["hit_rate"]), formula_vars);

		if (rand > hit_rate)
		{
			amount = 0;
		}

		amount = std::max(amount, 0);
	}

	int limitamount = std::min(amount, int(target->hp));

	if (this->map->world->config["LimitDamage"])
	{
		amount = limitamount;
	}

	target->hp -= limitamount;
	if (target->party)
	{
		target->party->UpdateHP(target);
	}

	int xdiff = this->x - target->x;
	int ydiff = this->y - target->y;

	if (std::abs(xdiff) > std::abs(ydiff))
	{
		if (xdiff < 0)
		{
			this->direction = DIRECTION_RIGHT;
		}
		else
		{
			this->direction = DIRECTION_LEFT;
		}
	}
	else
	{
		if (ydiff < 0)
		{
			this->direction = DIRECTION_DOWN;
		}
		else
		{
			this->direction = DIRECTION_UP;
		}
	}

	PacketBuilder builder(PACKET_NPC, PACKET_PLAYER, 18);
	builder.AddByte(255);
	builder.AddChar(this->index);
	builder.AddChar(1 + (target->hp == 0));
	builder.AddChar(this->direction);
	builder.AddShort(target->PlayerID());
	builder.AddThree(amount);
	builder.AddThree(util::clamp<int>(double(target->hp) / double(target->maxhp) * 100.0, 0, 100));
	builder.AddByte(255);
	builder.AddByte(255);

	UTIL_FOREACH(this->map->characters, character)
	{
		if (character == target || !character->InRange(target))
		{
			continue;
		}

		character->Send(builder);
	}

	if (target->hp == 0)
	{
		target->DeathRespawn();
	}

	builder.AddShort(target->hp);
	builder.AddShort(target->tp);

	target->Send(builder);
}

void NPC::Say(const std::string& message)
{
	PacketBuilder builder(PACKET_NPC, PACKET_PLAYER, 18);
	builder.AddByte(255);
	builder.AddByte(255);
	builder.AddChar(this->index);
	builder.AddChar(message.length());
	builder.AddString(message.c_str());
	builder.AddByte(255);

	UTIL_FOREACH(this->map->characters, character)
	{
		if (!character->InRange(this))
		{
			continue;
		}

		character->Send(builder);
	}
}

#define v(x) vars[prefix + #x] = x;
#define vv(x, n) vars[prefix + n] = x;
#define vd(x) vars[prefix + #x] = data.x;

void NPC::FormulaVars(std::unordered_map<std::string, double> &vars, std::string prefix)
{
	const ENF_Data& data = this->ENF();
	vv(1, "npc") v(hp) vv(data.hp, "maxhp")
	vd(mindam) vd(maxdam)
	vd(accuracy) vd(evade) vd(armor)
	v(x) v(y) v(direction) vv(map->id, "mapid")
}

#undef vd
#undef vv
#undef v

NPC::~NPC()
{
	UTIL_FOREACH(this->map->characters, character)
	{
		if (character->npc == this)
		{
			character->npc = 0;
			character->npc_type = ENF::NPC;
		}
	}

	UTIL_FOREACH_CREF(this->damagelist, opponent)
	{
		opponent->attacker->UnregisterNPC(this);
		if (ai) ai->Unregister(opponent->attacker);
	}
}
