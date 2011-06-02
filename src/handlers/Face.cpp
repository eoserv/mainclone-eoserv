
/* $Id$
 * EOSERV is released under the zlib license.
 * See LICENSE.txt for more info.
 */

#include "handlers.hpp"

#include "character.hpp"
#include "map.hpp"

namespace Handlers
{

// Player changing direction
void Face_Player(Character *character, PacketReader &reader)
{
	Direction direction = static_cast<Direction>(reader.GetChar());

	if (character->sitting != SIT_STAND)
	{
		return;
	}

	if (direction >= 0 && direction <= 3)
	{
		character->map->Face(character, direction);
	}
}

PACKET_HANDLER_REGISTER(PACKET_FACE)
	Register(PACKET_PLAYER, Face_Player, Playing, 0.09);
PACKET_HANDLER_REGISTER_END()

}
