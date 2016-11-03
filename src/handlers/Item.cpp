
/* $Id$
 * EOSERV is released under the zlib license.
 * See LICENSE.txt for more info.
 */

#include "handlers.hpp"

#include "../character.hpp"
#include "../config.hpp"
#include "../eoclient.hpp"
#include "../eodata.hpp"
#include "../map.hpp"
#include "../npc.hpp"
#include "../party.hpp"
#include "../player.hpp"
#include "../quest.hpp"
#include "../world.hpp"

#include "../console.hpp"
#include "../util.hpp"

#include <algorithm>
#include <cstddef>
#include <memory>

namespace Handlers
{

// Player using an item
void Item_Use(Character *character, PacketReader &reader)
{
	if (character->trading) return;

	int id = reader.GetShort();

	if (character->HasItem(id))
	{
		const EIF_Data& item = character->world->eif->Get(id);
		PacketBuilder reply(PACKET_ITEM, PACKET_REPLY, 3);
		reply.AddChar(item.type);
		reply.AddShort(id);

		auto QuestUsedItems = [](Character* character, int id)
		{
			UTIL_FOREACH(character->quests, q)
			{
				if (!q.second || q.second->GetQuest()->Disabled())
					continue;

				q.second->UsedItem(id);
			}
		};

		switch (item.type)
		{
			case EIF::Teleport:
			{
				if (!character->map->scroll)
					break;

				if (item.scrollmap == 0)
				{
					if (character->mapid == character->SpawnMap() && character->x == character->SpawnX() && character->y == character->SpawnY())
						break;
				}
				else
				{
					if (character->mapid == item.scrollmap && character->x == item.scrollx && character->y == item.scrolly)
						break;
				}

				character->DelItem(id, 1);

				reply.ReserveMore(6);
				reply.AddInt(character->HasItem(id));
				reply.AddChar(character->weight);
				reply.AddChar(character->maxweight);

				if (item.scrollmap == 0)
				{
					character->Warp(character->SpawnMap(), character->SpawnX(), character->SpawnY(), WARP_ANIMATION_SCROLL);
				}
				else
				{
					character->Warp(item.scrollmap, item.scrollx, item.scrolly, WARP_ANIMATION_SCROLL);
				}

				character->Send(reply);

				QuestUsedItems(character, id);
			}
			break;

			case EIF::Heal:
			{
				// Title Certificate
				if (id == 494)
				{
					character->DelItem(id, 1);
	
					reply.ReserveMore(14);
					reply.AddInt(character->HasItem(id));
					reply.AddChar(character->weight);
					reply.AddChar(character->maxweight);
					reply.AddInt(0);
					reply.AddShort(character->hp);
					reply.AddShort(character->tp);

					character->can_set_title = true;
					character->ServerMsg("You can now set your title once using the #title command (max. 20 chars). If you log out your ticket will be wasted.");

					character->Send(reply);
					QuestUsedItems(character, id);

					if (character->world->config["LogItemUse"])
						Console::Err("LOG ITEM USE [ %s ] %s Title Certificate", std::string(character->player->client->GetRemoteAddr()).c_str(), character->SourceName().c_str());

					break;
				}

				// Christmas Scroll
				else if (id == 498)
				{
					unsigned char index = character->map->GenerateNPCIndex();

					if (index > 250)
						break;

					character->DelItem(id, 1);

					reply.ReserveMore(14);
					reply.AddInt(character->HasItem(id));
					reply.AddChar(character->weight);
					reply.AddChar(character->maxweight);
					reply.AddInt(0);
					reply.AddShort(character->hp);
					reply.AddShort(character->tp);

					NPC *npc = new NPC(character->map, 328, character->x, character->y, 1, util::rand(0, 3), index, true);
					character->map->npcs.push_back(npc);
					npc->Spawn();

					character->Send(reply);
					QuestUsedItems(character, id);

					if (character->world->config["LogItemUse"])
						Console::Err("LOG ITEM USE [ %s ] %s Christmas Scroll", std::string(character->player->client->GetRemoteAddr()).c_str(), character->SourceName().c_str());

					break;
				}

				// Skin Melter
				else if (id == 500)
				{
					if (character->race != SKIN_SKELETON)
					{
						character->ServerMsg("Spooky!");
						character->race = SKIN_SKELETON;
						character->FakeWarp();
					}

					break;
				}
				
				// Spooky Skull
				else if (id == 501)
				{
					if (character->HasSpell(24))
					{
						character->ServerMsg("You already know this spell!");
					}
					else
					{
						character->AddSpell(24);
						character->ServerMsg("You've learned a new spell!");

						PacketBuilder reply(PACKET_STATSKILL, PACKET_TAKE, 6);
						reply.AddShort(24);
						reply.AddInt(character->HasItem(1));
						character->Send(reply);
					}

					break;
				}

				int hpgain = item.hp;
				int tpgain = item.tp;
				
				if (character->mapid >= 286 && character->mapid <= 289)
				{
					if (id == 16)
					{
						hpgain = 30;
						tpgain = 20;
						
						if (character->maxhp < 50) { hpgain += 10; }
						if (character->maxhp < 40) { hpgain += 10; }
						if (character->maxhp < 30) { hpgain += 10; }
						if (character->maxhp < 20) { hpgain += 10; }
						
						if (character->maxtp < 60) { tpgain += 10; }
						if (character->maxtp < 40) { tpgain += 10; }
						if (character->maxtp < 20) { tpgain += 10; }
					}
					else if (id == 377)
					{
						hpgain = 100;
						tpgain = 100;
						character->Effect(30, 1);
					}
					else
					{
						hpgain /= 2;
					}

					if (hpgain > 0)
						hpgain = std::max<int>(character->maxhp * (hpgain / 100.0), 1);
					
					if (tpgain > 0)
						tpgain = std::max<int>(character->maxtp * (tpgain / 100.0), 1);
				}
				
				if (character->world->config["LimitDamage"])
				{
					hpgain = std::min(hpgain, character->maxhp - character->hp);
					tpgain = std::min(tpgain, character->maxtp - character->tp);
				}

				hpgain = std::max(hpgain, 0);
				tpgain = std::max(tpgain, 0);

				if (hpgain == 0 && tpgain == 0)
					break;

				character->hp += hpgain;
				character->tp += tpgain;

				if (!character->world->config["LimitDamage"])
				{
					character->hp = std::min(character->hp, character->maxhp);
					character->tp = std::min(character->tp, character->maxtp);
				}

				character->DelItem(id, 1);

				reply.ReserveMore(14);
				reply.AddInt(character->HasItem(id));
				reply.AddChar(character->weight);
				reply.AddChar(character->maxweight);

				reply.AddInt(hpgain);
				reply.AddShort(character->hp);
				reply.AddShort(character->tp);

				PacketBuilder builder(PACKET_RECOVER, PACKET_AGREE, 7);
				builder.AddShort(character->PlayerID());
				builder.AddInt(hpgain);
				builder.AddChar(util::clamp<int>(double(character->hp) / double(character->maxhp) * 100.0, 0, 100));

				UTIL_FOREACH(character->map->characters, updatecharacter)
				{
					if (updatecharacter != character && character->InRange(updatecharacter))
					{
						updatecharacter->Send(builder);
					}
				}

				if (character->party)
				{
					character->party->UpdateHP(character);
				}

				character->Send(reply);

				QuestUsedItems(character, id);
			}
			break;

			case EIF::HairDye:
			{
				if (character->haircolor == item.haircolor)
					break;

				character->haircolor = item.haircolor;

				character->DelItem(id, 1);

				reply.ReserveMore(7);
				reply.AddInt(character->HasItem(id));
				reply.AddChar(character->weight);
				reply.AddChar(character->maxweight);

				reply.AddChar(item.haircolor);

				PacketBuilder builder(PACKET_AVATAR, PACKET_AGREE, 5);
				builder.AddShort(character->PlayerID());
				builder.AddChar(SLOT_HAIRCOLOR);
				builder.AddChar(0); // subloc
				builder.AddChar(item.haircolor);

				UTIL_FOREACH(character->map->characters, updatecharacter)
				{
					if (updatecharacter != character && character->InRange(updatecharacter))
					{
						updatecharacter->Send(builder);
					}
				}

				character->Send(reply);

				QuestUsedItems(character, id);
			}
			break;

			case EIF::Beer:
			{
				character->DelItem(id, 1);

				reply.ReserveMore(6);
				reply.AddInt(character->HasItem(id));
				reply.AddChar(character->weight);
				reply.AddChar(character->maxweight);

				character->Send(reply);

				QuestUsedItems(character, id);
			}
			break;

			case EIF::EffectPotion:
			{
				character->DelItem(id, 1);

				reply.ReserveMore(8);
				reply.AddInt(character->HasItem(id));
				reply.AddChar(character->weight);
				reply.AddChar(character->maxweight);
				reply.AddShort(item.effect);

				character->Effect(item.effect, false);

				character->Send(reply);

				QuestUsedItems(character, id);
			}
			break;

			case EIF::CureCurse:
			{
				bool found = false;

				for (std::size_t i = 0; i < character->paperdoll.size(); ++i)
				{
					if (character->world->eif->Get(character->paperdoll[i]).special == EIF::Cursed)
					{
						character->paperdoll[i] = 0;
						found = true;
					}
				}

				if (!found)
					break;

				character->CalculateStats();

				character->DelItem(id, 1);

				reply.ReserveMore(32);
				reply.AddInt(character->HasItem(id));
				reply.AddChar(character->weight);
				reply.AddChar(character->maxweight);

				reply.AddShort(character->maxhp);
				reply.AddShort(character->maxtp);

				reply.AddShort(character->display_str);
				reply.AddShort(character->display_intl);
				reply.AddShort(character->display_wis);
				reply.AddShort(character->display_agi);
				reply.AddShort(character->display_con);
				reply.AddShort(character->display_cha);

				reply.AddShort(character->mindam);
				reply.AddShort(character->maxdam);
				reply.AddShort(character->accuracy);
				reply.AddShort(character->evade);
				reply.AddShort(character->armor);

				PacketBuilder builder(PACKET_AVATAR, PACKET_AGREE, 14);
				builder.AddShort(character->PlayerID());
				builder.AddChar(SLOT_CLOTHES);
				builder.AddChar(0); // sound
				character->AddPaperdollData(builder, "BAHWS");

				UTIL_FOREACH(character->map->characters, updatecharacter)
				{
					if (updatecharacter != character && character->InRange(updatecharacter))
					{
						updatecharacter->Send(builder);
					}
				}

				character->Send(reply);

				QuestUsedItems(character, id);
			}
			break;

			case EIF::EXPReward:
			{
				bool level_up = false;

				character->exp += item.expreward;

				character->exp = std::min(character->exp, static_cast<int>(character->map->world->config["MaxExp"]));

				while (character->level < static_cast<int>(character->map->world->config["MaxLevel"])
				 && character->exp >= character->map->world->exp_table[character->level+1])
				{
					level_up = true;
					++character->level;
					character->statpoints += static_cast<int>(character->map->world->config["StatPerLevel"]);
					character->skillpoints += static_cast<int>(character->map->world->config["SkillPerLevel"]);
					character->CalculateStats();
				}

				character->DelItem(id, 1);

				reply.ReserveMore(21);
				reply.AddInt(character->HasItem(id));
				reply.AddChar(character->weight);
				reply.AddChar(character->maxweight);

				reply.AddInt(character->exp);

				reply.AddChar(level_up ? character->level : 0);
				reply.AddShort(character->statpoints);
				reply.AddShort(character->skillpoints);
				reply.AddShort(character->maxhp);
				reply.AddShort(character->maxtp);
				reply.AddShort(character->maxsp);

				if (level_up)
				{
					PacketBuilder builder(PACKET_RECOVER, PACKET_REPLY, 9);
					builder.AddInt(character->exp);
					builder.AddShort(character->karma);
					builder.AddChar(character->level);
					builder.AddShort(character->statpoints);
					builder.AddShort(character->skillpoints);

					UTIL_FOREACH(character->map->characters, check)
					{
						if (character != check && character->InRange(check))
						{
							PacketBuilder builder(PACKET_ITEM, PACKET_ACCEPT, 2);
							builder.AddShort(character->PlayerID());
							character->Send(builder);
						}
					}
				}

				character->Send(reply);

				QuestUsedItems(character, id);

				if (character->world->config["LogItemUse"])
					Console::Err("LOG ITEM USE [ %s ] %s Exp Reward", std::string(character->player->client->GetRemoteAddr()).c_str(), character->SourceName().c_str());
			}
			break;

			default:
				return;
		}
	}
}

// Drop an item on the ground
void Item_Drop(Character *character, PacketReader &reader)
{
	if (character->trading) return;
	if (!character->CanInteractItems()) return;

	int id = reader.GetShort();
	int amount;

	if (character->world->eif->Get(id).special == EIF::Lore)
	{
		return;
	}

	if (reader.Remaining() == 5)
	{
		amount = reader.GetThree();
	}
	else
	{
		amount = reader.GetInt();
	}
	unsigned char x = reader.GetByte(); // ?
	unsigned char y = reader.GetByte(); // ?

	amount = std::min<int>(amount, character->world->config["MaxDrop"]);

	if (amount <= 0)
	{
		return;
	}

	if (x == 255 && y == 255)
	{
		x = character->x;
		y = character->y;
	}
	else
	{
		x = PacketProcessor::Number(x);
		y = PacketProcessor::Number(y);
	}

	int distance = util::path_length(x, y, character->x, character->y);

	if (distance > static_cast<int>(character->world->config["DropDistance"]))
	{
		return;
	}

	if (!character->map->Walkable(x, y))
	{
		return;
	}

	if (character->HasItem(id) >= amount && character->mapid != static_cast<int>(character->world->config["JailMap"]))
	{
		std::shared_ptr<Map_Item> item = character->map->AddItem(id, amount, x, y, character);

		if (item)
		{
			item->owner = character->PlayerID();
			item->unprotecttime = Timer::GetTime() + static_cast<double>(character->world->config["ProtectPlayerDrop"]);
			item->player_dropped = true;
			character->DelItem(id, amount);

			PacketBuilder reply(PACKET_ITEM, PACKET_DROP, 15);
			reply.AddShort(id);
			reply.AddThree(amount);
			reply.AddInt(character->HasItem(id));
			reply.AddShort(item->uid);
			reply.AddChar(x);
			reply.AddChar(y);
			reply.AddChar(character->weight);
			reply.AddChar(character->maxweight);
			character->Send(reply);

			if (character->world->config["LogItemDrop"])
				Console::Err("LOG ITEM DROP [ %s ] %s %i %i", std::string(character->player->client->GetRemoteAddr()).c_str(), character->SourceName().c_str(), id, amount);
		}
	}
}

// Destroying an item
void Item_Junk(Character *character, PacketReader &reader)
{
	if (character->trading) return;

	int id = reader.GetShort();
	int amount = reader.GetInt();

	if (amount <= 0)
		return;

	if (character->HasItem(id) >= amount)
	{
		character->DelItem(id, amount);

		PacketBuilder reply(PACKET_ITEM, PACKET_JUNK, 11);
		reply.AddShort(id);
		reply.AddThree(amount); // Overflows, does it matter?
		reply.AddInt(character->HasItem(id));
		reply.AddChar(character->weight);
		reply.AddChar(character->maxweight);
		character->Send(reply);

		if (character->world->config["LogItemJunk"])
			Console::Err("LOG ITEM JUNK [ %s ] %s %i %i", std::string(character->player->client->GetRemoteAddr()).c_str(), character->SourceName().c_str(), id, amount);
	}
}

// Retrieve an item from the ground
void Item_Get(Character *character, PacketReader &reader)
{
	int uid = reader.GetShort();

	std::shared_ptr<Map_Item> item = character->map->GetItem(uid);

	if (item)
	{
		int distance = util::path_length(item->x, item->y, character->x, character->y);

		if (distance > static_cast<int>(character->world->config["DropDistance"]))
		{
			return;
		}

		if (item->owner != character->PlayerID() && item->unprotecttime > Timer::GetTime())
		{
			return;
		}

		int taken = character->CanHoldItem(item->id, item->amount);

		if (taken > 0)
		{
			character->AddItem(item->id, taken);

			PacketBuilder reply(PACKET_ITEM, PACKET_GET, 9);
			reply.AddShort(uid);
			reply.AddShort(item->id);
			reply.AddThree(taken);
			reply.AddChar(character->weight);
			reply.AddChar(character->maxweight);
			character->Send(reply);

			if (item->player_dropped && character->world->config["LogItemTake"])
				Console::Err("LOG ITEM TAKE [ %s ] %s %i %i", std::string(character->player->client->GetRemoteAddr()).c_str(), character->SourceName().c_str(), item->id, taken);

			character->map->DelSomeItem(item->uid, taken, character);
		}
	}
}

PACKET_HANDLER_REGISTER(PACKET_ITEM)
	Register(PACKET_USE, Item_Use, Playing);
	Register(PACKET_DROP, Item_Drop, Playing);
	Register(PACKET_JUNK, Item_Junk, Playing);
	Register(PACKET_GET, Item_Get, Playing);
PACKET_HANDLER_REGISTER_END(PACKET_ITEM)

}
