
/* $Id$
 * EOSERV is released under the zlib license.
 * See LICENSE.txt for more info.
 */

#include "handlers.h"

#include "map.hpp"

static void add_common(PacketBuilder &reply, Character *character, short item, int amount)
{
	character->DelItem(item, amount);

	character->CalculateStats();

	reply.SetID(PACKET_LOCKER, PACKET_REPLY);
	reply.AddShort(item);
	reply.AddInt(character->HasItem(item));
	reply.AddChar(character->weight);
	reply.AddChar(character->maxweight);

	UTIL_PTR_LIST_FOREACH(character->bank, Character_Item, item)
	{
		reply.AddShort(item->id);
		reply.AddThree(item->amount);
	}
}

CLIENT_F_FUNC(Locker)
{
	PacketBuilder reply;

	switch (action)
	{
		case PACKET_ADD: // Placing an item in a bank locker
		{
			if (this->state < EOClient::Playing) return false;

			unsigned char x = reader.GetChar();
			unsigned char y = reader.GetChar();
			short item = reader.GetShort();
			int amount = reader.GetThree();

			if (item == 1) return true;
			if (amount <= 0) return true;
			if (this->player->character->HasItem(item) < amount) return true;

			std::size_t lockermax = static_cast<int>(this->server->world->config["BaseBankSize"]) + this->player->character->bankmax * static_cast<int>(this->server->world->config["BankSizeStep"]);

			if (util::path_length(this->player->character->x, this->player->character->y, x, y) <= 1)
			{
				if (this->player->character->map->GetSpec(x, y) == Map_Tile::BankVault)
				{
					UTIL_PTR_LIST_FOREACH(this->player->character->bank, Character_Item, it)
					{
						if (it->id == item)
						{
							if (it->amount + amount < 0)
							{
								return true;
							}

							amount = std::min<int>(amount, static_cast<int>(this->server->world->config["MaxBank"]) - it->amount);

							it->amount += amount;

							add_common(reply, this->player->character, item, amount);
							CLIENT_SEND(reply);
							return true;
						}
					}

					if (this->player->character->bank.size() >= lockermax)
					{
						return true;
					}

					amount = std::min<int>(amount, static_cast<int>(this->server->world->config["MaxBank"]));

					Character_Item *newitem(new Character_Item);

					newitem->id = item;
					newitem->amount = amount;

					this->player->character->bank.push_back(newitem);
					newitem->Release();

					add_common(reply, this->player->character, item, amount);
					CLIENT_SEND(reply);
				}
			}
		}
		break;

		case PACKET_TAKE: // Taking an item from a bank locker
		{
			if (this->state < EOClient::Playing) return false;

			unsigned char x = reader.GetChar();
			unsigned char y = reader.GetChar();
			short item = reader.GetShort();

			if (util::path_length(this->player->character->x, this->player->character->y, x, y) <= 1)
			{
				if (this->player->character->map->GetSpec(x, y) == Map_Tile::BankVault)
				{
					UTIL_PTR_LIST_FOREACH(this->player->character->bank, Character_Item, it)
					{
						if (it->id == item)
						{
							this->player->character->AddItem(item, it->amount);

							this->player->character->CalculateStats();

							reply.SetID(PACKET_LOCKER, PACKET_GET);
							reply.AddShort(item);
							reply.AddThree(it->amount);
							reply.AddChar(this->player->character->weight);
							reply.AddChar(this->player->character->maxweight);

							this->player->character->bank.erase(it);

							UTIL_PTR_LIST_FOREACH(this->player->character->bank, Character_Item, item)
							{
								reply.AddShort(item->id);
								reply.AddThree(item->amount);
							}
							CLIENT_SEND(reply);

							break;
						}
					}
				}
			}
		}
		break;

		case PACKET_OPEN: // Opening a bank locker
		{
			if (this->state < EOClient::Playing) return false;

			unsigned char x = reader.GetChar();
			unsigned char y = reader.GetChar();

			if (util::path_length(this->player->character->x, this->player->character->y, x, y) <= 1)
			{
				if (this->player->character->map->GetSpec(x, y) == Map_Tile::BankVault)
				{
					reply.SetID(PACKET_LOCKER, PACKET_OPEN);
					reply.AddChar(x);
					reply.AddChar(y);
					UTIL_PTR_LIST_FOREACH(this->player->character->bank, Character_Item, item)
					{
						reply.AddShort(item->id);
						reply.AddThree(item->amount);
					}
					CLIENT_SEND(reply);
				}
			}
		}
		break;

		case PACKET_BUY: // Purchasing a locker space upgrade
		{
			if (this->state < EOClient::Playing) return false;

			if (this->player->character->npc_type == ENF::Bank)
			{
				int cost = static_cast<int>(this->server->world->config["BankUpgradeBase"]) + this->player->character->bankmax * static_cast<int>(this->server->world->config["BankUpgradeStep"]);

				if (this->player->character->bankmax >= static_cast<int>(this->server->world->config["MaxBankUpgrades"]))
				{
					return true;
				}

				if (this->player->character->HasItem(1) < cost)
				{
					return true;
				}

				++this->player->character->bankmax;
				this->player->character->DelItem(1, cost);

				reply.SetID(PACKET_LOCKER, PACKET_BUY);
				reply.AddInt(this->player->character->HasItem(1));
				reply.AddChar(this->player->character->bankmax);
				CLIENT_SEND(reply);
			}
		}
		break;

		default:
			return false;
	}

	return true;
}
