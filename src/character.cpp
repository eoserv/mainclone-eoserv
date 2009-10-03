
/* $Id$
 * EOSERV is released under the zlib license.
 * See LICENSE.txt for more info.
 */

#include "character.hpp"

#include <string>
#include <list>
#include <vector>
#include <limits>

#include "database.hpp"
#include "util.hpp"
#include "console.hpp"

// TODO: Clean up these functions
std::string ItemSerialize(std::list<Character_Item> list)
{
	std::string serialized;
	serialized.reserve(list.size()*10); // Reserve some space to stop some mass-reallocations
	UTIL_LIST_FOREACH_ALL(list, Character_Item, item)
	{
		serialized.append(util::to_string(item.id));
		serialized.append(",");
		serialized.append(util::to_string(item.amount));
		serialized.append(";");
	}
	serialized.reserve(0); // Clean up the reserve to save memory
	return serialized;
}

std::list<Character_Item> ItemUnserialize(std::string serialized)
{
	std::list<Character_Item> list;
	std::size_t p = 0;
	std::size_t lastp = std::numeric_limits<std::size_t>::max();
	Character_Item newitem;

	while ((p = serialized.find_first_of(';', p+1)) != std::string::npos)
	{
		std::string part = serialized.substr(lastp+1, p-lastp-1);
		std::size_t pp = 0;
		pp = part.find_first_of(',', 0);
		if (pp == std::string::npos)
		{
			continue;
		}
		newitem.id = util::to_int(part.substr(0, pp));
		newitem.amount = util::to_int(part.substr(pp+1));

		list.push_back(newitem);

		lastp = p;
	}

	return list;
}

std::string DollSerialize(util::array<int, 15> list)
{
	std::string serialized;

	serialized.reserve(15*5); // Reserve some space to stop some mass-reallocations

	UTIL_ARRAY_FOREACH_ALL(list, int, 15, item)
	{
		serialized.append(util::to_string(item));
		serialized.append(",");
	}

	serialized.reserve(0); // Clean up the reserve to save memory

	return serialized;
}

util::array<int, 15> DollUnserialize(std::string serialized)
{
	util::array<int, 15> list(0);
	std::size_t p = 0;
	std::size_t lastp = std::numeric_limits<std::size_t>::max();
	int i = 0;
	while ((p = serialized.find_first_of(',', p+1)) != std::string::npos)
	{
		list[i++] = util::to_int(serialized.substr(lastp+1, p-lastp-1));
		lastp = p;
	}
	return list;
}

template <typename T> T GetRow(std::map<std::string, util::variant> &row, const char *col)
{
	return row[col];
}

Character::Character(std::string name, World *world)
{
	this->world = world;

	Database_Result res = this->world->db.Query("SELECT `name`, `title`, `home`, `partner`, `admin`, `class`, `gender`, `race`, `hairstyle`, `haircolor`, `map`,"
	"`x`, `y`, `direction`, `spawnmap`, `spawnx`, `spawny`, `level`, `exp`, `hp`, `tp`, `str`, `int`, `wis`, `agi`, `con`, `cha`, `statpoints`, `skillpoints`, "
	"`karma`, `sitting`, `bankmax`, `goldbank`, `usage`, `inventory`, `bank`, `paperdoll`, `spells`, `guild`, `guild_rank` FROM `characters` WHERE `name` = '$'", name.c_str());
	std::map<std::string, util::variant> row = res.front();

	this->login_time = std::time(0);

	this->online = false;
	this->nowhere = false;
	this->id = this->world->GenerateCharacterID();

	this->admin = static_cast<AdminLevel>(GetRow<int>(row, "admin"));
	this->name = GetRow<std::string>(row, "name");
	this->title = GetRow<std::string>(row, "title");
	this->home = GetRow<std::string>(row, "home");
	this->partner = GetRow<std::string>(row, "partner");

	this->clas = GetRow<int>(row, "class");
	this->gender = static_cast<Gender>(GetRow<int>(row, "gender"));
	this->race = static_cast<Skin>(GetRow<int>(row, "race"));
	this->hairstyle = GetRow<int>(row, "hairstyle");
	this->haircolor = GetRow<int>(row, "haircolor");

	this->mapid = GetRow<int>(row, "map");
	this->x = GetRow<int>(row, "x");
	this->y = GetRow<int>(row, "y");
	this->direction = GetRow<int>(row, "direction");

	this->spawnmap = GetRow<int>(row, "spawnmap");
	this->spawnx = GetRow<int>(row, "spawnx");
	this->spawny = GetRow<int>(row, "spawny");

	this->level = GetRow<int>(row, "level");
	this->exp = GetRow<int>(row, "exp");

	this->hp = GetRow<int>(row, "hp");
	this->tp = GetRow<int>(row, "tp");

	this->str = GetRow<int>(row, "str");
	this->intl = GetRow<int>(row, "int");
	this->wis = GetRow<int>(row, "wis");
	this->agi = GetRow<int>(row, "agi");
	this->con = GetRow<int>(row, "con");
	this->cha = GetRow<int>(row, "cha");
	this->statpoints = GetRow<int>(row, "statpoints");
	this->skillpoints = GetRow<int>(row, "skillpoints");
	this->karma = GetRow<int>(row, "karma");

	this->weight = 0;
	this->maxweight = 0;

	this->maxhp = 0;
	this->maxtp = 0;
	this->maxsp = 0;

	this->mindam = 0;
	this->maxdam = 0;

	this->accuracy = 0;
	this->evade = 0;
	this->armor = 0;

	this->trading = false;
	this->trade_partner = 0;
	this->trade_agree = false;

	this->party_trust_send = 0;
	this->party_trust_recv = 0;

	this->shop_npc = 0;
	this->bank_npc = 0;

	this->next_arena = 0;
	this->arena = 0;

	this->warp_anim = WARP_ANIMATION_INVALID;

	this->sitting = static_cast<SitAction>(GetRow<int>(row, "sitting"));

	this->bankmax = GetRow<int>(row, "bankmax");

	// EOSERV originally set the default bankmax to 20
	if (this->bankmax > static_cast<int>(world->config["MaxBankUpgrades"]))
	{
		this->bankmax = 0;
	}

	this->goldbank = GetRow<int>(row, "goldbank");

	this->usage = GetRow<int>(row, "usage");

	this->inventory = ItemUnserialize(row["inventory"]);
	this->bank = ItemUnserialize(row["bank"]);
	this->paperdoll = DollUnserialize(row["paperdoll"]);

	this->player = 0;
	this->guild = 0;
	this->guild_tag = util::trim(static_cast<std::string>(row["guild"]));
	this->guild_rank = static_cast<int>(row["guild_rank"]);
	this->party = 0;
	this->map = this->world->GetMap(0);
}

bool Character::ValidName(std::string name)
{
	if (name.length() < 4)
	{
		return false;
	}

	if (name.length() > 12)
	{
		return false;
	}

	for (std::size_t i = 0; i < name.length(); ++i)
	{
		if (name[i] < 'a' || name[i] > 'z')
		{
			return false;
		}
	}

	return true;
}

void Character::Msg(Character *from, std::string message)
{
	PacketBuilder builder;

	builder.SetID(PACKET_TALK, PACKET_TELL);
	builder.AddBreakString(from->name);
	builder.AddBreakString(message);
	this->player->client->SendBuilder(builder);
}

bool Character::Walk(Direction direction)
{
	return this->map->Walk(this, direction);
}

bool Character::AdminWalk(Direction direction)
{
	return this->map->Walk(this, direction, true);
}

void Character::Attack(Direction direction)
{
	this->map->Attack(this, direction);
}

void Character::Sit(SitAction sit_type)
{
	this->map->Sit(this, sit_type);
}

void Character::Stand()
{
	this->map->Stand(this);
}

void Character::Emote(enum Emote emote, bool relay)
{
	this->map->Emote(this, emote, relay);
}

int Character::HasItem(short item)
{
	UTIL_LIST_FOREACH_ALL(this->inventory, Character_Item, it)
	{
		if (it.id == item)
		{
			if (this->trading)
			{
				UTIL_LIST_FOREACH_ALL(this->trade_inventory, Character_Item, tit)
				{
					if (tit.id == item)
					{
						return std::max(it.amount - tit.amount, 0);
					}
				}

				return it.amount;
			}
			else
			{
				return it.amount;
			}
		}
	}

	return 0;
}

bool Character::AddItem(short item, int amount)
{
	Character_Item newitem;

	if (amount <= 0)
	{
		return false;
	}

	if (item <= 0 || static_cast<std::size_t>(item) >= this->world->eif->data.size())
	{
		return false;
	}

	UTIL_LIST_IFOREACH_ALL(this->inventory, Character_Item, it)
	{
		if (it->id == item)
		{
			if (it->amount + amount < 0)
			{
				return false;
			}
			it->amount += amount;

			it->amount = std::min<int>(it->amount, this->world->config["MaxItem"]);

			this->CalculateStats();
			return true;
		}
	}

	newitem.id = item;
	newitem.amount = amount;

	this->inventory.push_back(newitem);
	this->CalculateStats();
	return true;
}

bool Character::DelItem(short item, int amount)
{
	if (amount <= 0)
	{
		return false;
	}

	UTIL_LIST_IFOREACH_ALL(this->inventory, Character_Item, it)
	{
		if (it->id == item)
		{
			if (it->amount < 0 || it->amount - amount <= 0)
			{
				this->inventory.erase(it);
			}
			else
			{
				it->amount -= amount;
			}

			this->CalculateStats();
			return true;
		}
	}

	return false;
}

bool Character::AddTradeItem(short item, int amount)
{
	Character_Item newitem;

	if (amount <= 0)
	{
		return false;
	}

	if (item <= 0 || static_cast<std::size_t>(item) >= this->world->eif->data.size())
	{
		return false;
	}

	int hasitem = this->HasItem(item);

	if (hasitem - amount < 0)
	{
		return false;
	}

	UTIL_LIST_IFOREACH_ALL(this->trade_inventory, Character_Item, it)
	{
		if (it->id == item)
		{
			it->amount += amount;
			return true;
		}
	}

	newitem.id = item;
	newitem.amount = amount;

	this->trade_inventory.push_back(newitem);
	return true;
}

bool Character::DelTradeItem(short item)
{
	UTIL_LIST_IFOREACH_ALL(this->trade_inventory, Character_Item, it)
	{
		if (it->id == item)
		{
			this->trade_inventory.erase(it);
			return true;
		}
	}

	return false;
}

bool Character::Unequip(short item, unsigned char subloc)
{
	if (item == 0)
	{
		return false;
	}

	for (std::size_t i = 0; i < this->paperdoll.size(); ++i)
	{
		if (this->paperdoll[i] == item)
		{
			if (((i == Character::Ring2 || i == Character::Armlet2 || i == Character::Bracer2) ? 1 : 0) == subloc)
			{
				this->paperdoll[i] = 0;
				this->AddItem(item, 1);
				this->CalculateStats();
				return true;
			}
		}
	}

	return false;
}

bool Character::Equip(short item, unsigned char subloc)
{
	if (!this->HasItem(item))
	{
		return false;
	}

	switch (this->world->eif->Get(item)->type)
	{
		case EIF::Weapon:
			if (this->paperdoll[Character::Weapon] != 0)
			{
				return false;
			}
			this->paperdoll[Character::Weapon] = item;
			this->DelItem(item, 1);
			break;

		case EIF::Shield:
			if (this->paperdoll[Character::Shield] != 0)
			{
				return false;
			}
			this->paperdoll[Character::Shield] = item;
			this->DelItem(item, 1);
			break;

		case EIF::Armor:
			if (this->world->eif->Get(item)->gender != this->gender)
			{
				return false;
			}
			if (this->paperdoll[Character::Armor] != 0)
			{
				return false;
			}
			this->paperdoll[Character::Armor] = item;
			this->DelItem(item, 1);
			break;

		case EIF::Hat:
			if (this->paperdoll[Character::Hat] != 0)
			{
				return false;
			}
			this->paperdoll[Character::Hat] = item;
			this->DelItem(item, 1);
			break;

		case EIF::Boots:
			if (this->paperdoll[Character::Boots] != 0)
			{
				return false;
			}
			this->paperdoll[Character::Boots] = item;
			this->DelItem(item, 1);
			break;

		case EIF::Gloves:
			if (this->paperdoll[Character::Gloves] != 0)
			{
				return false;
			}
			this->paperdoll[Character::Gloves] = item;
			this->DelItem(item, 1);
			break;

		case EIF::Accessory:
			if (this->paperdoll[Character::Accessory] != 0)
			{
				return false;
			}
			this->paperdoll[Character::Accessory] = item;
			this->DelItem(item, 1);
			break;

		case EIF::Belt:
			if (this->paperdoll[Character::Belt] != 0)
			{
				return false;
			}
			this->paperdoll[Character::Belt] = item;
			this->DelItem(item, 1);
			break;

		case EIF::Necklace:
			if (this->paperdoll[Character::Necklace] != 0)
			{
				return false;
			}
			this->paperdoll[Character::Necklace] = item;
			this->DelItem(item, 1);
			break;

		case EIF::Ring:
			if (subloc == 0)
			{
				if (this->paperdoll[Character::Ring1] != 0)
				{
					return false;
				}
				this->paperdoll[Character::Ring1] = item;
				this->DelItem(item, 1);
				break;
			}
			else
			{
				if (this->paperdoll[Character::Ring2] != 0)
				{
					return false;
				}
				this->paperdoll[Character::Ring2] = item;
				this->DelItem(item, 1);
				break;
			}

		case EIF::Armlet:
			if (subloc == 0)
			{
				if (this->paperdoll[Character::Armlet1] != 0)
				{
					return false;
				}
				this->paperdoll[Character::Armlet1] = item;
				this->DelItem(item, 1);
				break;
			}
			else
			{
				if (this->paperdoll[Character::Armlet2] != 0)
				{
					return false;
				}
				this->paperdoll[Character::Armlet2] = item;
				this->DelItem(item, 1);
				break;
			}

		case EIF::Bracer:
			if (subloc == 0)
			{
				if (this->paperdoll[Character::Bracer1] != 0)
				{
					return false;
				}
				this->paperdoll[Character::Bracer1] = item;
				this->DelItem(item, 1);
				break;
			}
			else
			{
				if (this->paperdoll[Character::Bracer2] != 0)
				{
					return false;
				}
				this->paperdoll[Character::Bracer2] = item;
				this->DelItem(item, 1);
				break;
			}

		default:
			return false;
	}

	this->CalculateStats();

	return true;
}

bool Character::InRange(unsigned char x, unsigned char y)
{
	return util::path_length(this->x, this->y, x, y) <= static_cast<int>(this->world->config["SeeDistance"]);
}

bool Character::InRange(Character *other)
{
	if (this->nowhere || other->nowhere)
	{
		return false;
	}

	return this->InRange(other->x, other->y);
}

bool Character::InRange(NPC *other)
{
	if (this->nowhere)
	{
		return false;
	}

	return this->InRange(other->x, other->y);
}

bool Character::InRange(Map_Item other)
{
	if (this->nowhere)
	{
		return false;
	}

	return this->InRange(other.x, other.y);
}

void Character::Warp(short map, unsigned char x, unsigned char y, WarpAnimation animation)
{
	if (!this->world->GetMap(map)->exists)
	{
		return;
	}

	PacketBuilder builder;
	builder.SetID(PACKET_WARP, PACKET_REQUEST);

	if (this->player->character->mapid == map && !this->player->character->nowhere)
	{
		builder.AddChar(WARP_LOCAL);
		builder.AddShort(map);
		builder.AddChar(x);
		builder.AddChar(y);
	}
	else
	{
		builder.AddChar(WARP_SWITCH);
		builder.AddShort(map);

		if (static_cast<int>(this->world->config["GlobalPK"]) && !this->world->PKExcept(map))
		{
			builder.AddByte(0xFF);
			builder.AddByte(0x01);
		}
		else
		{
			builder.AddByte(this->world->GetMap(map)->rid[0]);
			builder.AddByte(this->world->GetMap(map)->rid[1]);
		}

		builder.AddByte(this->world->GetMap(map)->rid[2]);
		builder.AddByte(this->world->GetMap(map)->rid[3]);
		builder.AddThree(this->world->GetMap(map)->filesize);
		builder.AddChar(0); // ?
		builder.AddChar(0); // ?
	}

	if (this->map->exists)
	{
		this->map->Leave(this, animation);
	}

	this->map = this->world->GetMap(map);
	this->mapid = map;
	this->x = x;
	this->y = y;
	this->sitting = SIT_STAND;
	this->map->Enter(this, animation);

	this->warp_anim = animation;
	this->nowhere = false;

	this->player->client->SendBuilder(builder);

	if (this->arena)
	{
		--this->arena->occupants;
		this->arena = 0;
	}

	if (this->next_arena)
	{
		this->arena = this->next_arena;
		++this->arena->occupants;
		this->next_arena = 0;
	}
}

void Character::Refresh()
{
	PacketBuilder builder;

	std::vector<Character *> updatecharacters;
	std::vector<NPC *> updatenpcs;
	std::vector<Map_Item> updateitems;

	UTIL_LIST_FOREACH_ALL(this->map->characters, Character *, character)
	{
		if (this->InRange(character))
		{
			updatecharacters.push_back(character);
		}
	}

	UTIL_VECTOR_FOREACH_ALL(this->map->npcs, NPC *, npc)
	{
		if (this->InRange(npc))
		{
			updatenpcs.push_back(npc);
		}
	}

	UTIL_LIST_FOREACH_ALL(this->map->items, Map_Item, item)
	{
		if (this->InRange(item))
		{
			updateitems.push_back(item);
		}
	}

	builder.SetID(PACKET_REFRESH, PACKET_REPLY);
	builder.AddChar(updatecharacters.size()); // Number of players
	builder.AddByte(255);

	UTIL_VECTOR_FOREACH_ALL(updatecharacters, Character *, character)
	{
		builder.AddBreakString(character->name);
		builder.AddShort(character->player->id);
		builder.AddShort(character->mapid);
		builder.AddShort(character->x);
		builder.AddShort(character->y);
		builder.AddChar(character->direction);
		builder.AddChar(6); // ?
		builder.AddString(character->PaddedGuildTag());
		builder.AddChar(character->level);
		builder.AddChar(character->gender);
		builder.AddChar(character->hairstyle);
		builder.AddChar(character->haircolor);
		builder.AddChar(character->race);
		builder.AddShort(character->maxhp);
		builder.AddShort(character->hp);
		builder.AddShort(character->maxtp);
		builder.AddShort(character->tp);
		// equipment
		builder.AddShort(this->world->eif->Get(character->paperdoll[Character::Boots])->dollgraphic);
		builder.AddShort(0); // ??
		builder.AddShort(0); // ??
		builder.AddShort(0); // ??
		builder.AddShort(this->world->eif->Get(character->paperdoll[Character::Armor])->dollgraphic);
		builder.AddShort(0); // ??
		builder.AddShort(this->world->eif->Get(character->paperdoll[Character::Hat])->dollgraphic);
		builder.AddShort(this->world->eif->Get(character->paperdoll[Character::Shield])->dollgraphic);
		builder.AddShort(this->world->eif->Get(character->paperdoll[Character::Weapon])->dollgraphic);
		builder.AddChar(character->sitting);
		builder.AddChar(0); // visible
		builder.AddByte(255);
	}

	UTIL_VECTOR_FOREACH_ALL(updatenpcs, NPC *, npc)
	{
		if (npc->alive)
		{
			builder.AddChar(npc->index);
			builder.AddShort(npc->data->id);
			builder.AddChar(npc->x);
			builder.AddChar(npc->y);
			builder.AddChar(npc->direction);
		}
	}

	builder.AddByte(255);

	UTIL_VECTOR_FOREACH_ALL(updateitems, Map_Item, item)
	{
		builder.AddShort(item.uid);
		builder.AddShort(item.id);
		builder.AddChar(item.x);
		builder.AddChar(item.y);
		builder.AddThree(item.amount);
	}

	this->player->client->SendBuilder(builder);
}

void Character::ShowBoard(int boardid)
{
	if (static_cast<std::size_t>(boardid) > this->world->boards.size())
	{
		return;
	}

	PacketBuilder builder(PACKET_BOARD, PACKET_OPEN);
	builder.AddChar(boardid);
	builder.AddChar(this->world->boards[boardid]->posts.size());

	int post_count = 0;
	int recent_post_count = 0;

	UTIL_LIST_FOREACH_ALL(this->world->boards[boardid]->posts, Board_Post *, post)
	{
		if (post->author == this->player->character->name)
		{
			++post_count;

			if (post->time + static_cast<int>(this->world->config["BoardRecentPostTime"]) > Timer::GetTime())
			{
				++recent_post_count;
			}
		}
	}

	int posts_remaining = std::min(static_cast<int>(this->world->config["BoardMaxUserPosts"]) - post_count, static_cast<int>(this->world->config["BoardMaxUserRecentPosts"]) - recent_post_count);

	UTIL_LIST_FOREACH_ALL(this->world->boards[boardid]->posts, Board_Post *, post)
	{
		builder.AddShort(post->id);
		builder.AddByte(255);

		std::string author_extra;

		if (posts_remaining > 0)
		{
			author_extra = " ";
		}

		builder.AddBreakString(post->author + author_extra);

		std::string subject_extra;

		if (static_cast<int>(this->world->config["BoardDatePosts"]))
		{
			subject_extra = " (" + util::timeago(post->time, Timer::GetTime()) + ")";
		}

		builder.AddBreakString(post->subject + subject_extra);
	}

	this->player->client->SendBuilder(builder);
}

std::string Character::PaddedGuildTag()
{
	std::string tag;

	if (static_cast<int>(this->world->config["ShowLevel"]))
	{
		tag = util::to_string(this->level);
		if (tag.length() < 3)
		{
			tag.insert(tag.begin(), 'L');
		}
	}
	else
	{
		tag = this->guild_tag;
	}

	for (std::size_t i = tag.length(); i < 3; ++i)
	{
		tag.push_back(' ');
	}

	return tag;
}

int Character::Usage()
{
	return this->usage + (std::time(0) - this->login_time) / 60;
}

// TODO: calculate equipment bonuses, check formulas
void Character::CalculateStats()
{
	short calcstr = this->str;
	short calcintl = this->intl;
	short calcwis = this->wis;
	short calcagi = this->agi;
	short calccon = this->con;
	short calccha = this->cha;
	this->maxweight = 70 + this->str;

	if (this->maxweight < 70 || this->maxweight > 250)
	{
		this->maxweight = 250;
	}

	this->weight = 0;
	this->maxhp = 0;
	this->maxtp = 0;
	this->mindam = 0;
	this->maxdam = 0;
	this->accuracy = 0;
	this->evade = 0;
	this->armor = 0;
	this->maxsp = 0;
	UTIL_LIST_FOREACH_ALL(this->inventory, Character_Item, item)
	{
		this->weight += this->world->eif->Get(item.id)->weight * item.amount;
	}
	UTIL_ARRAY_FOREACH_ALL(this->paperdoll, int, 15, i)
	{
		if (i)
		{
			EIF_Data *item = this->world->eif->Get(i);
			this->weight += item->weight;
			this->maxhp += item->hp;
			this->maxtp += item->tp;
			this->mindam += item->mindam;
			this->maxdam += item->maxdam;
			this->accuracy += item->accuracy;
			this->evade += item->evade;
			this->armor += item->armor;
			calcstr += item->str;
			calcintl += item->intl;
			calcwis += item->wis;
			calcagi += item->agi;
			calccon += item->con;
			calccha += item->cha;
		}
	}
	this->maxhp += 10 + calccon*3;
	this->maxtp += 10 + calcwis*3;
	this->mindam += 1 + calcstr/2;
	this->maxdam += 2 + calcstr/2;
	this->accuracy += 0 + calcagi/2;
	this->evade += 0 + calcagi/2;
	this->armor += 0 + calccon/2;
	this->maxsp += std::min(20 + this->level*2, 100);

	if (this->party)
	{
		this->party->UpdateHP(this);
	}
}

void Character::DropAll(Character *killer)
{
	// TODO: This could be more efficient
	restart_loop:
	UTIL_LIST_FOREACH_ALL(this->inventory, Character_Item, item)
	{
		if (this->world->eif->Get(item.id)->special == EIF::Lore)
		{
			continue;
		}

		Map_Item *map_item = this->player->character->map->AddItem(item.id, item.amount, this->x, this->y, 0);
		this->DelItem(item.id, item.amount);

		if (map_item)
		{
			if (killer)
			{
				map_item->owner = killer->player->id;
				map_item->unprotecttime = Timer::GetTime() + static_cast<double>(this->world->config["ProtectPKDrop"]);
			}
			else
			{
				map_item->owner = this->player->id;
				map_item->unprotecttime = Timer::GetTime() + static_cast<double>(this->world->config["ProtectDeathDrop"]);
			}

			PacketBuilder builder(PACKET_ITEM, PACKET_DROP);
			builder.AddShort(item.id);
			builder.AddThree(item.amount);
			builder.AddInt(0);
			builder.AddShort(map_item->uid);
			builder.AddChar(this->x);
			builder.AddChar(this->y);
			builder.AddChar(this->weight);
			builder.AddChar(this->maxweight);
			this->player->client->SendBuilder(builder);
		}

		goto restart_loop;
	}

	int i = 0;
	UTIL_ARRAY_FOREACH_ALL(this->paperdoll, int, 15, id)
	{
		if (id == 0 || this->world->eif->Get(id)->special == EIF::Lore || this->world->eif->Get(id)->special == EIF::Cursed)
		{
			++i;
			continue;
		}

		Map_Item *map_item = this->player->character->map->AddItem(id, 1, this->x, this->y, 0);

		if (map_item)
		{
			if (killer)
			{
				map_item->owner = killer->player->id;
				map_item->unprotecttime = Timer::GetTime() + static_cast<double>(this->world->config["ProtectPKDrop"]);
			}
			else
			{
				map_item->owner = this->player->id;
				map_item->unprotecttime = Timer::GetTime() + static_cast<double>(this->world->config["ProtectDeathDrop"]);
			}

			int subloc = 0;

			if (i == Ring2 || i == Armlet2 || i == Bracer2)
			{
				subloc = 1;
			}

			if (this->player->character->Unequip(id, subloc))
			{
				PacketBuilder builder(PACKET_PAPERDOLL, PACKET_REMOVE);
				builder.AddShort(this->player->id);
				builder.AddChar(SLOT_CLOTHES);
				builder.AddChar(0); // ?
				builder.AddShort(this->world->eif->Get(this->paperdoll[Character::Boots])->dollgraphic);
				builder.AddShort(this->world->eif->Get(this->paperdoll[Character::Armor])->dollgraphic);
				builder.AddShort(this->world->eif->Get(this->paperdoll[Character::Hat])->dollgraphic);
				builder.AddShort(this->world->eif->Get(this->paperdoll[Character::Weapon])->dollgraphic);
				builder.AddShort(this->world->eif->Get(this->paperdoll[Character::Shield])->dollgraphic);
				builder.AddShort(id);
				builder.AddChar(subloc);
				builder.AddShort(this->maxhp);
				builder.AddShort(this->maxtp);
				builder.AddShort(this->str);
				builder.AddShort(this->intl);
				builder.AddShort(this->wis);
				builder.AddShort(this->agi);
				builder.AddShort(this->con);
				builder.AddShort(this->cha);
				builder.AddShort(this->mindam);
				builder.AddShort(this->maxdam);
				builder.AddShort(this->accuracy);
				builder.AddShort(this->evade);
				builder.AddShort(this->armor);
				this->player->client->SendBuilder(builder);
			}

			this->player->character->DelItem(id, 1);

			PacketBuilder builder(PACKET_ITEM, PACKET_DROP);
			builder.AddShort(id);
			builder.AddThree(1);
			builder.AddInt(0);
			builder.AddShort(map_item->uid);
			builder.AddChar(this->x);
			builder.AddChar(this->y);
			builder.AddChar(this->weight);
			builder.AddChar(this->maxweight);
			this->player->client->SendBuilder(builder);
		}

		++i;
	}
}

void Character::Save()
{

#ifdef DEBUG
	Console::Dbg("Saving character '%s' (session lasted %i minutes)", this->name.c_str(), int(std::time(0) - this->login_time) / 60);
#endif // DEBUG
	this->world->db.Query("UPDATE `characters` SET `title` = '$', `home` = '$', `partner` = '$', `class` = #, `gender` = #, `race` = #, "
		"`hairstyle` = #, `haircolor` = #, `map` = #, `x` = #, `y` = #, `direction` = #, `level` = #, `exp` = #, `hp` = #, `tp` = #, "
		"`str` = #, `int` = #, `wis` = #, `agi` = #, `con` = #, `cha` = #, `statpoints` = #, `skillpoints` = #, `karma` = #, `sitting` = #, "
		"`bankmax` = #, `goldbank` = #, `usage` = #, `inventory` = '$', `bank` = '$', `paperdoll` = '$', "
		"`spells` = '$', `guild` = '$', guild_rank = # WHERE `name` = '$'",
		this->title.c_str(), this->home.c_str(), this->partner.c_str(), this->clas, this->gender, this->race,
		this->hairstyle, this->haircolor, this->mapid, this->x, this->y, this->direction, this->level, this->exp, this->hp, this->tp,
		this->str, this->intl, this->wis, this->agi, this->con, this->cha, this->statpoints, this->skillpoints, this->karma, this->sitting,
		this->bankmax, this->goldbank, this->Usage(), ItemSerialize(this->inventory).c_str(), ItemSerialize(this->bank).c_str(),
		DollSerialize(this->paperdoll).c_str(), "", this->guild_tag.c_str(), this->guild_rank, this->name.c_str());
}

Character::~Character()
{
	if (!this->online)
	{
		return;
	}

	if (this->trading)
	{
		PacketBuilder builder(PACKET_TRADE, PACKET_CLOSE);
		builder.AddShort(this->id);
		this->trade_partner->player->client->SendBuilder(builder);

		this->player->client->state = EOClient::Playing;
		this->trading = false;
		this->trade_inventory.clear();
		this->trade_agree = false;

		this->trade_partner->player->client->state = EOClient::Playing;
		this->trade_partner->trading = false;
		this->trade_partner->trade_inventory.clear();
		this->trade_agree = false;

		this->trade_partner->trade_partner = 0;
		this->trade_partner = 0;
	}

	if (this->party)
	{
		this->party->Leave(this);
	}

	if (this->arena)
	{
		--this->arena->occupants;
	}

	UTIL_LIST_FOREACH_ALL(this->unregister_npc, NPC *, npc)
	{
		UTIL_LIST_IFOREACH_ALL(npc->damagelist, NPC_Opponent, checkopp)
		{
			if (checkopp->attacker == this)
			{
				npc->totaldamage -= checkopp->damage;
				npc->damagelist.erase(checkopp);
				break;
			}
		}
	}

	this->world->Logout(this);

	this->Save();
}
