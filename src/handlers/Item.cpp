
CLIENT_F_FUNC(Item)
{
	PacketBuilder reply;

	switch (action)
	{
		case PACKET_USE: // Player using an item
		{
			if (!this->player || !this->player->character) return false;

			int id = reader.GetShort();

			if (this->player->character->HasItem(id))
			{
				EIF_Data *item = eoserv_items->Get(id);
				switch (item->type)
				{
					case EIF::Teleport:
						if (this->player->character->modal)
						{
							break;
						}

						if (this->player->character->mapid == static_cast<int>(eoserv_config["JailMap"]))
						{
							break;
						}

						if (item->scrollmap == 0)
						{
							this->player->character->Warp(this->player->character->spawnmap, this->player->character->spawnx, this->player->character->spawny, WARP_ANIMATION_ADMIN);
						}
						else
						{
							this->player->character->Warp(item->scrollmap, item->scrollx, item->scrolly, WARP_ANIMATION_ADMIN);
						}
						this->player->character->DelItem(id, 1);
						break;

					default:
						break;
				}
				reply.SetID(PACKET_ITEM, PACKET_REPLY);
				reply.AddChar(4); // ?
				reply.AddShort(id);
				reply.AddInt(this->player->character->HasItem(id));
				reply.AddChar(this->player->character->weight);
				reply.AddChar(this->player->character->maxweight);
				CLIENT_SEND(reply);
			}
		}
		break;

		case PACKET_DROP: // Drop an item on the ground
		{
			if (!this->player || !this->player->character) return false;

			int id = reader.GetShort();
			int amount;

			if (eoserv_items->Get(id)->special == EIF::Lore)
			{
				return true;
			}

			if (reader.Length() == 8)
			{
				amount = reader.GetThree();
			}
			else
			{
				amount = reader.GetInt();
			}
			int x = reader.GetByte(); // ?
			int y = reader.GetByte(); // ?

			amount = std::min<int>(amount, eoserv_config["MaxDrop"]);

			if (amount == 0)
			{
				return true;
			}

			if (x == 255 && y == 255)
			{
				x = this->player->character->x;
				y = this->player->character->y;
			}
			else
			{
				x = PacketProcessor::Number(x);
				y = PacketProcessor::Number(y);
			}

			int distance = std::abs(x + y - this->player->character->x - this->player->character->y);

			if (distance > static_cast<int>(eoserv_config["DropDistance"]))
			{
				return true;
			}

			if (!this->player->character->map->Walkable(x, y))
			{
				return true;
			}

			if (this->player->character->HasItem(id) >= amount && this->player->character->mapid != static_cast<int>(eoserv_config["JailMap"]))
			{
				Map_Item *item = this->player->character->map->AddItem(id, amount, x, y, this->player->character);
				if (item)
				{
					item->owner = this->player->id;
					item->unprotecttime = Timer::GetTime() + static_cast<double>(eoserv_config["ProctectPlayerDrop"]);
					this->player->character->DelItem(id, amount);

					reply.SetID(PACKET_ITEM, PACKET_DROP);
					reply.AddShort(id);
					reply.AddThree(amount);
					reply.AddInt(this->player->character->HasItem(id));
					reply.AddShort(item->uid);
					reply.AddChar(x);
					reply.AddChar(y);
					reply.AddChar(this->player->character->weight);
					reply.AddChar(this->player->character->maxweight);
					CLIENT_SEND(reply);
				}
			}
		}
		break;

		case PACKET_JUNK: // Destroying an item
		{
			if (!this->player || !this->player->character) return false;

			int id = reader.GetShort();
			int amount = reader.GetInt();

			if (this->player->character->HasItem(id) >= amount)
			{
				this->player->character->DelItem(id, amount);

				reply.SetID(PACKET_ITEM, PACKET_JUNK);
				reply.AddShort(id);
				reply.AddThree(amount); // Overflows, does it matter?
				reply.AddInt(this->player->character->HasItem(id));
				reply.AddChar(this->player->character->weight);
				reply.AddChar(this->player->character->maxweight);
				CLIENT_SEND(reply);
			}
		}
		break;

		case PACKET_GET: // Retrieve an item from the ground
		{
			if (!this->player || !this->player->character || this->player->character->modal) return false;

			int uid = reader.GetShort();

			UTIL_LIST_FOREACH_ALL(this->player->character->map->items, Map_Item, item)
			{
				if (item.uid == uid)
				{
					int distance = std::abs(item.x + item.y - this->player->character->x - this->player->character->y);

					if (distance > static_cast<int>(eoserv_config["DropDistance"]))
					{
						break;
					}

					if (item.owner != this->player->id && item.unprotecttime > Timer::GetTime())
					{
						break;
					}

					this->player->character->AddItem(item.id, item.amount);
					this->player->character->map->DelItem(uid, this->player->character);

					reply.SetID(PACKET_ITEM, PACKET_GET);
					reply.AddShort(uid);
					reply.AddShort(item.id);
					reply.AddThree(item.amount);
					reply.AddChar(this->player->character->weight);
					reply.AddChar(this->player->character->maxweight);
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
