
CLIENT_F_FUNC(Shop)
{
	PacketBuilder reply;

	switch (action)
	{
		case PACKET_CREATE: // Crafting an item
		{
			if (this->state < EOClient::Playing) return false;

			short item = reader.GetShort();
			/*int shopid = reader.GetInt();*/

			if (this->player->character->shop_npc)
			{
				UTIL_VECTOR_FOREACH_ALL(this->player->character->shop_npc->shop_craft, NPC_Shop_Craft_Item, checkitem)
				{
					if (checkitem.id == item)
					{
						bool hasitems = true;

						UTIL_VECTOR_FOREACH_ALL(checkitem.ingredients, NPC_Shop_Craft_Ingredient, ingredient)
						{
							if (this->player->character->HasItem(ingredient.id) < ingredient.amount)
							{
								hasitems = false;
							}
						}

						if (hasitems)
						{
							reply.SetID(PACKET_SHOP, PACKET_CREATE);
							reply.AddShort(item);
							reply.AddChar(this->player->character->weight);
							reply.AddChar(this->player->character->maxweight);
							UTIL_VECTOR_FOREACH_ALL(checkitem.ingredients, NPC_Shop_Craft_Ingredient, ingredient)
							{
								this->player->character->DelItem(ingredient.id, ingredient.amount);
								reply.AddShort(ingredient.id);
								reply.AddInt(this->player->character->HasItem(ingredient.id));
							}
							this->player->character->AddItem(checkitem.id, 1);
							CLIENT_SEND(reply);
						}
					}
				}
			}
		}
		break;

		case PACKET_BUY: // Purchasing an item from a store
		{
			if (this->state < EOClient::Playing) return false;

			short item = reader.GetShort();
			int amount = reader.GetInt();
			/*int shopid = reader.GetInt();*/

			if (this->player->character->shop_npc)
			{
				UTIL_VECTOR_FOREACH_ALL(this->player->character->shop_npc->shop_trade, NPC_Shop_Trade_Item, checkitem)
				{
					if (checkitem.id == item && checkitem.buy != 0 && this->player->character->HasItem(1) >= amount * checkitem.buy)
					{
						this->player->character->DelItem(1, amount * checkitem.buy);
						this->player->character->AddItem(item, amount);

						reply.SetID(PACKET_SHOP, PACKET_BUY);
						reply.AddInt(this->player->character->HasItem(1));
						reply.AddShort(item);
						reply.AddInt(amount);
						reply.AddChar(this->player->character->weight);
						reply.AddChar(this->player->character->maxweight);
						CLIENT_SEND(reply);
						break;
					}
				}
			}
		}
		break;

		case PACKET_SELL: // Selling an item to a store
		{
			if (this->state < EOClient::Playing) return false;

			short item = reader.GetShort();
			int amount = reader.GetInt();
			/*int shopid = reader.GetInt();*/

			if (this->player->character->shop_npc)
			{
				UTIL_VECTOR_FOREACH_ALL(this->player->character->shop_npc->shop_trade, NPC_Shop_Trade_Item, checkitem)
				{
					if (checkitem.id == item && checkitem.sell != 0 && this->player->character->HasItem(item) >= amount)
					{
						this->player->character->DelItem(item, amount);
						this->player->character->AddItem(1, amount * checkitem.sell);

						reply.SetID(PACKET_SHOP, PACKET_SELL);
						reply.AddInt(this->player->character->HasItem(item));
						reply.AddShort(item);
						reply.AddInt(this->player->character->HasItem(1));
						reply.AddChar(this->player->character->weight);
						reply.AddChar(this->player->character->maxweight);
						CLIENT_SEND(reply);
						break;
					}
				}

			}
		}
		break;

		case PACKET_OPEN: // Talking to a store NPC
		{
			if (this->state < EOClient::Playing) return false;

			short id = reader.GetShort();

			UTIL_VECTOR_FOREACH_ALL(this->player->character->map->npcs, NPC *, npc)
			{
				if (npc->index == id && (npc->shop_trade.size() > 0 || npc->shop_craft.size() > 0))
				{
					this->player->character->shop_npc = npc;

					reply.SetID(PACKET_SHOP, PACKET_OPEN);
					reply.AddShort(npc->id);
					reply.AddBreakString(npc->shop_name.c_str());

					UTIL_VECTOR_FOREACH_ALL(npc->shop_trade, NPC_Shop_Trade_Item, item)
					{
						reply.AddShort(item.id);
						reply.AddThree(item.buy);
						reply.AddThree(item.sell);
						reply.AddChar(4); // ?
					}
					reply.AddByte(255);

					UTIL_VECTOR_FOREACH_ALL(npc->shop_craft, NPC_Shop_Craft_Item, item)
					{
						std::size_t i = 0;

						reply.AddShort(item.id);

						for (; i < item.ingredients.size(); ++i)
						{
							reply.AddShort(item.ingredients[i].id);
							reply.AddChar(item.ingredients[i].amount);
						}

						for (; i < 4; ++i)
						{
							reply.AddShort(0);
							reply.AddChar(0);
						}
					}
					reply.AddByte(255);

					CLIENT_SEND(reply);

					break;
				}
			}
		}
		break;

		default:
			return false;
	}

	return true;
}
