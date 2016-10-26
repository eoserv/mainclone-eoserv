
/* $Id$
 * EOSERV is released under the zlib license.
 * See LICENSE.txt for more info.
 */

#include "commands.hpp"

#include "../character.hpp"
#include "../command_source.hpp"
#include "../config.hpp"
#include "../eoserver.hpp"
#include "../map.hpp"
#include "../timer.hpp"
#include "../world.hpp"

#include "../console.hpp"
#include "../util.hpp"

#include <csignal>
#include <string>
#include <vector>

extern volatile std::sig_atomic_t eoserv_sig_abort;

namespace Commands
{

void ReloadMap(const std::vector<std::string>& arguments, Character* from)
{
	World* world = from->SourceWorld();
	Map* map = from->map;
	bool isnew = false;
	int reloadflags = 0;
	std::string filename;

	if (arguments.size() >= 1)
	{
		int mapid = util::to_int(arguments[0]);

		if (mapid < 1)
			mapid = 1;

		if (world->maps.size() > mapid - 1)
		{
			map = world->maps[mapid - 1];
		}
		else if (mapid <= static_cast<int>(world->config["Maps"]))
		{
			isnew = true;

			while (world->maps.size() < mapid)
			{
				int newmapid = world->maps.size() + 1;
				world->maps.push_back(new Map(newmapid, world));
			}
		}
	}

	if (map && !isnew)
	{
		if (arguments.size() >= 2)
			filename = map->MakeFilename(arguments[1].c_str());
		
		if (arguments.size() >= 3)
			reloadflags = util::to_int(arguments[2]) ? Map::ReloadIgnoreNPC : 0;

		if (filename.empty())
			map->Reload(reloadflags);
		else
			map->ReloadAs(filename, reloadflags);
	}
}

void ReloadPub(const std::vector<std::string>& arguments, Command_Source* from)
{
	(void)arguments;

	Console::Out("Pub files reloaded by %s", from->SourceName().c_str());

	bool quiet = true;

	if (arguments.size() >= 1)
		quiet = (arguments[0] != "announce");

	from->SourceWorld()->ReloadPub(quiet);
}

void ReloadConfig(const std::vector<std::string>& arguments, Command_Source* from)
{
	(void)arguments;

	Console::Out("Config reloaded by %s", from->SourceName().c_str());
	from->SourceWorld()->Rehash();
}

void ReloadQuest(const std::vector<std::string>& arguments, Command_Source* from)
{
	(void)arguments;

	Console::Out("Quests reloaded by %s", from->SourceName().c_str());
	from->SourceWorld()->ReloadQuests();
}

void Shutdown(const std::vector<std::string>& arguments, Command_Source* from)
{
	(void)arguments;

	Console::Wrn("Server shut down by %s", from->SourceName().c_str());
	eoserv_sig_abort = true;
}

void ForceHW2016(const std::vector<std::string>& arguments, Command_Source* from)
{
	World* world = from->SourceWorld();

	if (arguments.size() >= 1)
	{
		int state = util::to_int(arguments[0]);
		int tick = -5;
		
		if (arguments.size() >= 2)
			tick = util::to_int(arguments[1]);
		
		if (world->hw2016_state <= 1)
		{
			int pc = world->maps[286-1]->characters.size() + world->maps[287-1]->characters.size() + world->maps[288-1]->characters.size() + world->maps[289-1]->characters.size();

			world->hw2016_spawncount = std::min<int>((pc + 1) / 2, 5);
			world->hw2016_monstermod = 1.0 + std::max<int>(pc - 10, 0) * 0.1;
			
			if (world->maps[286-1]->characters.size() == 1)
				world->maps[286-1]->characters.front()->hw2016_points = 50;
			
			if (pc == 2)
				world->hw2016_monstermod = 1.5;
			else if (pc == 4)
				world->hw2016_monstermod = 1.3;
			else if (pc == 6)
				world->hw2016_monstermod = 1.2;
			else if (pc == 8)
				world->hw2016_monstermod = 1.15;
		}
		
		world->hw2016_state = state;
		world->hw2016_tick = tick;
	}
	else
		world->hw2016_hour = 0;
}

void Uptime(const std::vector<std::string>& arguments, Command_Source* from)
{
	(void)arguments;

	std::string buffer = "Server started ";
	buffer += util::timeago(from->SourceWorld()->server->start, Timer::GetTime());
	from->ServerMsg(buffer);
}

void AdminSecret(const std::vector<std::string>& arguments, Character* from)
{
	from->ServerMsg("ok");
	from->adminsecret = true;
}

COMMAND_HANDLER_REGISTER(server)
	RegisterCharacter({"remap", {}, {"mapid", "suffix", "ignorenpc"}, 3}, ReloadMap);
	Register({"repub", {}, {"announce"}, 3}, ReloadPub);
	Register({"rehash"}, ReloadConfig);
	Register({"request", {}, {}, 3}, ReloadQuest);
	Register({"shutdown", {}, {}, 8}, Shutdown);
	Register({"uptime"}, Uptime);
	Register({"nezapo", {}, {"state", "tick"}, 6}, ForceHW2016);
	RegisterCharacter({"mods=gods", {}, {}, 9}, AdminSecret);
COMMAND_HANDLER_REGISTER_END(server)

}
