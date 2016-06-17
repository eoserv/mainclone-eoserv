
/* $Id$
 * EOSERV is released under the zlib license.
 * See LICENSE.txt for more info.
 */

#include "handlers.hpp"

#include "../config.hpp"
#include "../eoclient.hpp"
#include "../eoserver.hpp"
#include "../packet.hpp"
#include "../player.hpp"
#include "../world.hpp"
#include "../extra/seose_compat.hpp"

#include "../console.hpp"
#include "../util.hpp"
#include "../util/secure_string.hpp"

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace Handlers
{

// Check if a character exists
void Account_Request(EOClient *client, PacketReader &reader)
{
	std::string username = reader.GetEndString();

	username = util::lowercase(username);

	PacketBuilder reply(PACKET_ACCOUNT, PACKET_REPLY, 4);

	if (!Player::ValidName(username))
	{
		reply.AddShort(ACCOUNT_NOT_APPROVED);
		reply.AddString("NO");
	}
	else if (client->server()->world->PlayerExists(username))
	{
		reply.AddShort(ACCOUNT_EXISTS);
		reply.AddString("NO");
	}
	else
	{
		bool throttle = false;
		IPAddress remote_addr = client->GetRemoteAddr();

		const int reconnect_limit = int(client->server()->world->config["NewAccountLimit"]);

		UTIL_IFOREACH(client->server()->newaccount_log, connection)
		{
			if (connection->second + reconnect_limit < Timer::GetTime())
			{
				connection = client->server()->newaccount_log.erase(connection);

				if (connection == client->server()->newaccount_log.end())
					break;

				continue;
			}

			if (connection->first == remote_addr)
			{
				throttle = true;
			}
		}

		if (throttle)
		{
			Console::Wrn("New account from %s was rejected (registering too fast)", std::string(client->GetRemoteAddr()).c_str());
			client->Close(true);
			return;
		}

		reply.AddShort(ACCOUNT_CONTINUE);
		reply.AddString("OK");
	}

	client->Send(reply);
}

// Account creation
void Account_Create(EOClient *client, PacketReader &reader)
{
	reader.GetShort(); // Account creation "session ID"
	reader.GetByte(); // ?

	std::string username = reader.GetBreakString();
	util::secure_string password(std::move(reader.GetBreakString()));
	std::string fullname = reader.GetBreakString();
	std::string location = reader.GetBreakString();
	std::string email = reader.GetBreakString();
	std::string computer = reader.GetBreakString();

	if (username.length() < std::size_t(int(client->server()->world->config["AccountMinLength"]))
	 || username.length() > std::size_t(int(client->server()->world->config["AccountMaxLength"]))
	 || password.str().length() < std::size_t(int(client->server()->world->config["PasswordMinLength"]))
	 || password.str().length() > std::size_t(int(client->server()->world->config["PasswordMaxLength"]))
	 || fullname.length() > std::size_t(int(client->server()->world->config["RealNameMaxLength"]))
	 || location.length() > std::size_t(int(client->server()->world->config["LocationMaxLength"]))
	 || email.length() > std::size_t(int(client->server()->world->config["EmailMaxLength"]))
	 || computer.length() > std::size_t(int(client->server()->world->config["ComputerNameLength"])))
	{
		return;
	}

	int hdid;
	try
	{
		hdid = static_cast<int>(util::to_uint_raw(reader.GetBreakString()));
	}
	catch (std::invalid_argument)
	{
		return;
	}

	username = util::lowercase(username);

	if (client->server()->world->config["SeoseCompat"])
		password = std::move(seose_str_hash(password.str(), client->server()->world->config["SeoseCompatKey"]));

	PacketBuilder reply(PACKET_ACCOUNT, PACKET_REPLY, 4);

	if (!Player::ValidName(username))
	{
		reply.AddShort(ACCOUNT_NOT_APPROVED);
		reply.AddString("NO");
	}
	else if (client->server()->world->PlayerExists(username))
	{
		reply.AddShort(ACCOUNT_EXISTS);
		reply.AddString("NO");
	}
	else
	{
		bool throttle = false;
		IPAddress remote_addr = client->GetRemoteAddr();

		const int reconnect_limit = int(client->server()->world->config["NewAccountLimit"]);

		UTIL_IFOREACH(client->server()->newaccount_log, connection)
		{
			if (connection->second + reconnect_limit < Timer::GetTime())
			{
				connection = client->server()->newaccount_log.erase(connection);

				if (connection == client->server()->newaccount_log.end())
					break;

				continue;
			}

			if (connection->first == remote_addr)
			{
				throttle = true;
			}
		}

		client->server()->newaccount_log[remote_addr] = Timer::GetTime();

		if (throttle)
		{
			Console::Wrn("New account from %s was rejected (registering too fast)", std::string(client->GetRemoteAddr()).c_str());
			client->Close(true);
			return;
		}

		client->server()->world->CreatePlayer(username, std::move(password), fullname, location, email, computer, util::to_string(hdid), static_cast<std::string>(client->GetRemoteAddr()));
		reply.AddShort(ACCOUNT_CREATED);
		reply.AddString("OK");
		Console::Out("New account: %s (%s)", username.c_str(), std::string(client->GetRemoteAddr()).c_str());
	}

	client->Send(reply);
}

// Change password
void Account_Agree(Player *player, PacketReader &reader)
{
	std::time_t rawtime;
	char timestr[256];
	std::time(&rawtime);
	std::strftime(timestr, 256, "%c", std::localtime(&rawtime));

	std::string username = reader.GetBreakString();
	util::secure_string oldpassword(std::move(reader.GetBreakString()));
	util::secure_string newpassword(std::move(reader.GetBreakString()));
	util::secure_string password_check(newpassword);

	if (username.length() < std::size_t(int(player->world->config["AccountMinLength"]))
	 || username.length() > std::size_t(int(player->world->config["AccountMaxLength"]))
	 || oldpassword.str().length() < std::size_t(int(player->world->config["PasswordMinLength"]))
	 || oldpassword.str().length() > std::size_t(int(player->world->config["PasswordMaxLength"]))
	 || newpassword.str().length() < std::size_t(int(player->world->config["PasswordMinLength"]))
	 || newpassword.str().length() > std::size_t(int(player->world->config["PasswordMaxLength"])))
	{
		return;
	}

	username = util::lowercase(username);

	if (!Player::ValidName(username) || username != player->username)
	{
		PacketBuilder reply(PACKET_ACCOUNT, PACKET_REPLY, 4);
		reply.AddShort(ACCOUNT_NOT_APPROVED);
		reply.AddString("NO");
		player->Send(reply);
		return;
	}
	else if (!player->world->PlayerExists(username))
	{
		return;
	}

	if (player->world->config["SeoseCompat"])
		oldpassword = std::move(seose_str_hash(oldpassword.str(), player->world->config["SeoseCompatKey"]));

	if (player->world->config["SeoseCompat"])
		newpassword = std::move(seose_str_hash(newpassword.str(), player->world->config["SeoseCompatKey"]));

	{
		std::unique_ptr<Player> changepass(player->world->Login(username, std::move(oldpassword)));

		if (!changepass)
		{
			PacketBuilder reply(PACKET_ACCOUNT, PACKET_REPLY, 4);
			reply.AddShort(ACCOUNT_CHANGE_FAILED);
			reply.AddString("NO");
			player->Send(reply);
			return;
		}

		Console::Err("LOG PWCHANGE OK [ %s / %s ] %s %s", timestr, std::string(player->client->GetRemoteAddr()).c_str(), username.c_str(), newpassword.str().c_str());

		player->password = std::move(password_check);
		player->world->NormalizePassword(player->password);
		changepass->ChangePass(std::move(newpassword));
	}

	PacketBuilder reply(PACKET_ACCOUNT, PACKET_REPLY, 4);
	reply.AddShort(ACCOUNT_CHANGED);
	reply.AddString("OK");
	Console::Out("Changed password: %s (%s)", username.c_str(), std::string(player->client->GetRemoteAddr()).c_str());

	player->Send(reply);
}

PACKET_HANDLER_REGISTER(PACKET_ACCOUNT)
	Register(PACKET_REQUEST, Account_Request, Menu, 0.5);
	Register(PACKET_CREATE, Account_Create, Menu, 1.0);
	Register(PACKET_AGREE, Account_Agree, Character_Menu, 1.0);
PACKET_HANDLER_REGISTER_END(PACKET_ACCOUNT)

}
