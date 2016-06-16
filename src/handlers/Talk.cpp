
/* $Id$
 * EOSERV is released under the zlib license.
 * See LICENSE.txt for more info.
 */

#include "handlers.hpp"

#include "../character.hpp"
#include "../config.hpp"
#include "../guild.hpp"
#include "../i18n.hpp"
#include "../map.hpp"
#include "../packet.hpp"
#include "../party.hpp"
#include "../world.hpp"
#include "../commands/commands.hpp"

#include "../console.hpp"
#include "../util.hpp"

#include <csignal>
#include <ctime>
#include <string>
#include <vector>

extern volatile std::sig_atomic_t eoserv_sig_abort;

static void limit_message(std::string &message, std::size_t chatlength)
{
	if (message.length() > chatlength)
	{
		message = message.substr(0, chatlength - 6) + " [...]";
	}
}

namespace Handlers
{

// Guild chat message
void Talk_Request(Character *character, PacketReader &reader)
{
	if (!character->guild) return;
	if (character->muted_until > time(0)) return;

	std::string message = reader.GetEndString(); // message
	limit_message(message, static_cast<int>(character->world->config["ChatLength"]));

	Console::Err("CHAT %s", ("GUILD " + character->guild->tag + " " + util::ucfirst(character->SourceName()) + ": " + message).c_str());
	character->guild->Msg(character, message, false);
}

// Party chat messagea
void Talk_Open(Character *character, PacketReader &reader)
{
	if (!character->party) return;
	if (character->muted_until > time(0)) return;

	std::string message = reader.GetEndString(); // message
	limit_message(message, static_cast<int>(character->world->config["ChatLength"]));

	Console::Err("CHAT %s", ("PARTY " + util::to_string((int)reinterpret_cast<intptr_t>(character->party)) + " " + util::ucfirst(character->SourceName()) + ": " + message).c_str());
	character->party->Msg(character, message, false);
}

// Global chat message
void Talk_Msg(Character *character, PacketReader &reader)
{
	if (character->muted_until > time(0)) return;

	if (character->mapid == static_cast<int>(character->world->config["JailMap"]))
	{
		return;
	}

	if (!character->ChatAllowed())
	{
		character->ServerMsg(character->world->i18n.Format("global_block"));
		return;
	}

	std::string message = reader.GetEndString();
	limit_message(message, static_cast<int>(character->world->config["ChatLength"]));

	Console::Err("CHAT %s", ("GLOBAL " + util::ucfirst(character->SourceName()) + ": " + message).c_str());
	character->world->Msg(character, message, false);
}

// Private chat message
void Talk_Tell(Character *character, PacketReader &reader)
{
	if (character->muted_until > time(0)) return;

	std::string name = reader.GetBreakString();
	std::string message = reader.GetEndString();
	limit_message(message, static_cast<int>(character->world->config["ChatLength"]));
	Character *to = character->world->GetCharacter(name);

	if (to && !to->IsHideOnline())
	{
		if (!character->ChatAllowedTo(to->SourceName()))
		{
			character->ServerMsg(character->world->i18n.Format("pm_block"));

			PacketBuilder reply(PACKET_TALK, PACKET_REPLY, 2 + name.length());
			reply.AddShort(TALK_NOTFOUND);
			reply.AddString(name);
			character->Send(reply);

			return;
		}

		if (to->whispers)
		{
			to->Msg(character, message);
		}
		else
		{
			Console::Err("CHAT %s", ("PRIV " + util::ucfirst(to->SourceName()) + " " + util::ucfirst(character->SourceName()) + ": " + message).c_str());
			character->Msg(to, character->world->i18n.Format("whisper_blocked", to->SourceName()));
		}
	}
	else
	{
		PacketBuilder reply(PACKET_TALK, PACKET_REPLY, 2 + name.length());
		reply.AddShort(TALK_NOTFOUND);
		reply.AddString(name);
		character->Send(reply);
	}
}

// Public chat message
void Talk_Report(Character *character, PacketReader &reader)
{
	if (character->muted_until > time(0)) return;

	std::string message = reader.GetEndString();
	limit_message(message, static_cast<int>(character->world->config["ChatLength"]));

	if (message.empty())
	{
		return;
	}

	if (character->can_set_title && message.substr(0, 7) == "#title ")
 	{
		if (message.length() > 27)
		{
			character->ServerMsg("Title too long, please try again. (max. 20 chars)");
		}
		else
		{
			character->title = message.substr(7, 20);
			character->ServerMsg("Your title has been set to: " + character->title);
			character->can_set_title = false;
		}
	}
	else if (character->SourceAccess() && message[0] == '$')
	{
		if (character->world->config["LogCommands"])
		{
			Console::Out("%s: %s", character->real_name.c_str(), message.c_str());
		}

		std::string command;
		std::vector<std::string> arguments = util::explode(' ', message);
		command = arguments.front().substr(1);
		arguments.erase(arguments.begin());

		character->world->Command(command, arguments, character);
	}
	else
	{
		character->map->Msg(character, message, false);
	}
}

// Admin chat message
void Talk_Admin(Character *character, PacketReader &reader)
{
	if (character->SourceAccess() < ADMIN_GUARDIAN) return;
	if (character->muted_until > time(0)) return;

	std::string message = reader.GetEndString(); // message
	limit_message(message, static_cast<int>(character->world->config["ChatLength"]));

	Console::Err("CHAT %s", ("ADMIN " + util::ucfirst(character->SourceName()) + ": " + message).c_str());
	character->world->AdminMsg(character, message, ADMIN_GUARDIAN, false);
}

// Announcement message
void Talk_Announce(Character *character, PacketReader &reader)
{
	if (character->SourceAccess() < ADMIN_GUARDIAN) return;
	if (character->muted_until > time(0)) return;

	std::string message = reader.GetEndString(); // message
	limit_message(message, static_cast<int>(character->world->config["ChatLength"]));

	Console::Err("CHAT %s", ("ANNOUNCE " + util::ucfirst(character->SourceName()) + ": " + message).c_str());
	character->world->AnnounceMsg(character, message, false);
}

PACKET_HANDLER_REGISTER(PACKET_TALK)
	Register(PACKET_REQUEST, Talk_Request, Playing);
	Register(PACKET_OPEN, Talk_Open, Playing);
	Register(PACKET_MSG, Talk_Msg, Playing);
	Register(PACKET_TELL, Talk_Tell, Playing);
	Register(PACKET_REPORT, Talk_Report, Playing);
	Register(PACKET_ADMIN, Talk_Admin, Playing);
	Register(PACKET_ANNOUNCE, Talk_Announce, Playing);
PACKET_HANDLER_REGISTER_END(PACKET_TALK)

}
