
/* $Id$
 * EOSERV is released under the zlib license.
 * See LICENSE.txt for more info.
 */

#include "handlers.hpp"

#include "../character.hpp"
#include "../config.hpp"
#include "../eodata.hpp"
#include "../map.hpp"
#include "../npc.hpp"
#include "../npc_data.hpp"
#include "../packet.hpp"
#include "../timer.hpp"
#include "../wedding.hpp"
#include "../world.hpp"

#include "../util.hpp"

namespace Handlers
{

// Talking to a priest NPC
void Priest_Open(Character *character, PacketReader &reader)
{
	short id = reader.GetShort();

	if (!character->partner.empty())
		return;

	if (!character->map->wedding)
		return;

	UTIL_FOREACH(character->map->npcs, npc)
	{
		if (npc->index == id && (npc->ENF().type == ENF::Priest))
		{
			if (character->map->wedding->Busy())
			{
				PacketBuilder reply(PACKET_PRIEST, PACKET_REPLY, 2);
				reply.AddShort(PRIEST_BUSY);
				character->Send(reply);
				return;
			}

			character->npc = npc;
			character->npc_type = ENF::Priest;

			PacketBuilder reply(PACKET_PRIEST, PACKET_OPEN, 4);
			reply.AddInt(npc->id);
			character->Send(reply);
		}
	}
}

// Requesting marriage at a priest
void Priest_Request(Character *character, PacketReader &reader)
{
	/*int session_id = */reader.GetInt();
	reader.GetByte();
	std::string name = reader.GetEndString();

	if (character->npc_type == ENF::Priest)
	{
		if (name != character->fiance || name == character->SourceName())
		{
			PacketBuilder reply(PACKET_PRIEST, PACKET_REPLY, 2);
			reply.AddShort(PRIEST_NO_PERMISSION);
			character->Send(reply);
			return;
		}

		Character* partner = character->world->GetCharacter(character->fiance);

		if (!partner || partner->map != character->map)
		{
			PacketBuilder reply(PACKET_PRIEST, PACKET_REPLY, 2);
			reply.AddShort(PRIEST_PARTNER_NOT_PRESENT);
			character->Send(reply);
			return;
		}

		if (partner->fiance != character->SourceName())
		{
			PacketBuilder reply(PACKET_PRIEST, PACKET_REPLY, 2);
			reply.AddShort(PRIEST_NO_PERMISSION);
			character->Send(reply);
			return;
		}

		if (!partner->partner.empty())
		{
			PacketBuilder reply(PACKET_PRIEST, PACKET_REPLY, 2);
			reply.AddShort(PRIEST_PARTNER_ALREADY_MARRIED);
			character->Send(reply);
			return;
		}

		int armor = character->paperdoll[Character::EquipLocation::Armor];
		int partner_armor = partner->paperdoll[Character::EquipLocation::Armor];

		int male_outfit = character->world->config["WeddingOutfitMale"];
		int female_outfit = character->world->config["WeddingOutfitFemale"];

		if ((character->gender == GENDER_MALE && male_outfit != 0 && armor != male_outfit)
		 || (character->gender == GENDER_FEMALE && female_outfit != 0 && armor != female_outfit))
		{
			PacketBuilder reply(PACKET_PRIEST, PACKET_REPLY, 2);
			reply.AddShort(PRIEST_NOT_DRESSED);
			character->Send(reply);
			return;
		}

		if ((partner->gender == GENDER_MALE && male_outfit != 0 && partner_armor != male_outfit)
		 || (partner->gender == GENDER_FEMALE && female_outfit != 0 && partner_armor != female_outfit))
		{
			PacketBuilder reply(PACKET_PRIEST, PACKET_REPLY, 2);
			reply.AddShort(PRIEST_PARTNER_NOT_DRESSED);
			character->Send(reply);
			return;
		}

		character->map->wedding->StartWedding(character->SourceName(), partner->SourceName());
	}
}

// Saying "I do" at a wedding
void Priest_Use(Character *character, PacketReader &reader)
{
	/*int session_id = */reader.GetInt();

	if (!character->map->wedding)
		return;

	character->map->wedding->IDo(character);
}

PACKET_HANDLER_REGISTER(PACKET_PRIEST)
	Register(PACKET_OPEN, Priest_Open, Playing);
	Register(PACKET_REQUEST, Priest_Request, Playing);
	Register(PACKET_USE, Priest_Use, Playing);
PACKET_HANDLER_REGISTER_END(PACKET_PRIEST)

}
