
/* $Id$
 * EOSERV is released under the zlib license.
 * See LICENSE.txt for more info.
 */

#include "commands.hpp"

#include <functional>

#include "../arena.hpp"
#include "../character.hpp"
#include "../config.hpp"
#include "../map.hpp"
#include "../packet.hpp"
#include "../player.hpp"
#include "../quest.hpp"
#include "../world.hpp"

namespace Commands
{

void SetX(const std::vector<std::string>& arguments, Command_Source* from, std::string set)
{
	Character *victim = from->SourceWorld()->GetCharacter(arguments[0]);

	if (!victim || victim->nowhere)
	{
		from->ServerMsg(from->SourceWorld()->i18n.Format("character_not_found"));
	}
	else if (victim->admin >= from->SourceAccess() && victim != from->SourceCharacter())
	{
		from->ServerMsg(from->SourceWorld()->i18n.Format("command_access_denied"));
	}
	else
	{
		std::string title_string = "";

		for (std::size_t i = 1; i < arguments.size(); ++i)
		{
			title_string += arguments[i];

			if (i < arguments.size() - 1)
				title_string += " ";
		}

		bool appearance = false;
		bool failure = false;
		bool level = false;
		bool stats = false;
		bool karma = false;

		bool statpoints = false;
		bool skillpoints = false;

			 if (set == "level") (level = true, victim->level) = std::min(std::max(util::to_int(arguments[1]), 0), int(from->SourceWorld()->config["MaxLevel"]));
		else if (set == "exp") (level = true, victim->exp) = std::min(std::max(util::to_int(arguments[1]), 0), int(from->SourceWorld()->config["MaxExp"]));
		else if (set == "str") (stats = true, victim->str) = std::min(std::max(util::to_int(arguments[1]), 0), int(from->SourceWorld()->config["MaxStat"]));
		else if (set == "int") (stats = true, victim->intl) = std::min(std::max(util::to_int(arguments[1]), 0), int(from->SourceWorld()->config["MaxStat"]));
		else if (set == "wis") (stats = true, victim->wis) = std::min(std::max(util::to_int(arguments[1]), 0), int(from->SourceWorld()->config["MaxStat"]));
		else if (set == "agi") (stats = true, victim->agi) = std::min(std::max(util::to_int(arguments[1]), 0), int(from->SourceWorld()->config["MaxStat"]));
		else if (set == "con") (stats = true, victim->con) = std::min(std::max(util::to_int(arguments[1]), 0), int(from->SourceWorld()->config["MaxStat"]));
		else if (set == "cha") (stats = true, victim->cha) = std::min(std::max(util::to_int(arguments[1]), 0), int(from->SourceWorld()->config["MaxStat"]));
		else if (set == "statpoints") (statpoints = true, victim->statpoints) = std::min(std::max(util::to_int(arguments[1]), 0), int(from->SourceWorld()->config["MaxLevel"]) * int(from->SourceWorld()->config["StatPerLevel"]));
		else if (set == "skillpoints") (skillpoints = true, victim->skillpoints) = std::min(std::max(util::to_int(arguments[1]), 0), int(from->SourceWorld()->config["MaxLevel"]) * int(from->SourceWorld()->config["SkillPerLevel"]));
		else if (set == "admin")
		{
			AdminLevel level = std::min(std::max(AdminLevel(util::to_int(arguments[1])), ADMIN_PLAYER), ADMIN_HGM);

			if (level < from->SourceAccess() && victim != from->SourceCharacter())
			{
				if (level == ADMIN_PLAYER && victim->admin != ADMIN_PLAYER)
					victim->world->DecAdminCount();
				else if (level != ADMIN_PLAYER && victim->admin == ADMIN_PLAYER)
					victim->world->IncAdminCount();

				victim->admin = level;
			}
			else
			{
				from->ServerMsg(from->SourceWorld()->i18n.Format("command_access_denied"));
			}
		}
		else if (set == "title") victim->title = title_string;
		else if (set == "fiance") victim->fiance = (arguments.size() > 1) ? arguments[1] : "";
		else if (set == "partner") victim->partner = (arguments.size() > 1) ? arguments[1] : "";
		else if (set == "home") victim->home = (arguments.size() > 1) ? arguments[1] : "";
		else if (set == "gender") (appearance = true, victim->gender) = Gender(std::min(std::max(util::to_int(arguments[1]), 0), 1));
		else if (set == "hairstyle") (appearance = true, victim->hairstyle) = std::min(std::max(util::to_int(arguments[1]), 0), int(from->SourceWorld()->config["MaxHairStyle"]));
		else if (set == "haircolor") (appearance = true, victim->haircolor) = std::min(std::max(util::to_int(arguments[1]), 0), int(from->SourceWorld()->config["MaxHairColor"]));
		else if (set == "race") (appearance = true, victim->race) = Skin(std::min(std::max(util::to_int(arguments[1]), 0), int(from->SourceWorld()->config["MaxSkin"])));
		else if (set == "guildrank") victim->guild_rank = std::min(std::max(util::to_int(arguments[1]), 0), 9);
		else if (set == "karma") (karma = true, victim->karma) = std::min(std::max(util::to_int(arguments[1]), 0), 2000);
		else if (set == "class") (stats = true, victim->clas) = std::min(std::max(util::to_int(arguments[1]), 0), int(from->SourceWorld()->ecf->data.size() - 1));
		else failure = true;

		if (failure)
		{
			from->ServerMsg(from->SourceWorld()->i18n.Format("invalid_setx"));
		}
		else
		{
			// Easiest way to get the character to update on everyone nearby's screen
			if (appearance)
				victim->Warp(victim->map->id, victim->x, victim->y);

			// TODO: Good way of updating skillpoints
			(void)skillpoints;

			if (stats || statpoints)
			{
				victim->CalculateStats();

				PacketBuilder builder(PACKET_RECOVER, PACKET_LIST, 32);

				if (statpoints)
				{
					builder.SetID(PACKET_STATSKILL, PACKET_PLAYER);
					builder.AddShort(victim->statpoints);
				}
				else
				{
					builder.AddShort(victim->clas);
				}

				builder.AddShort(victim->display_str);
				builder.AddShort(victim->display_intl);
				builder.AddShort(victim->display_wis);
				builder.AddShort(victim->display_agi);
				builder.AddShort(victim->display_con);
				builder.AddShort(victim->display_cha);
				builder.AddShort(victim->maxhp);
				builder.AddShort(victim->maxtp);
				builder.AddShort(victim->maxsp);
				builder.AddShort(victim->maxweight);
				builder.AddShort(victim->mindam);
				builder.AddShort(victim->maxdam);
				builder.AddShort(victim->accuracy);
				builder.AddShort(victim->evade);
				builder.AddShort(victim->armor);
				victim->Send(builder);
			}

			if (karma || level)
			{
				PacketBuilder builder(PACKET_RECOVER, PACKET_REPLY, 7);
				builder.AddInt(victim->exp);
				builder.AddShort(victim->karma);
				builder.AddChar(level ? victim->level : 0);
				victim->Send(builder);
			}

			if (!stats && !skillpoints && !appearance)
			{
				victim->CheckQuestRules();
			}
		}
	}
}

void Strip(const std::vector<std::string>& arguments, Command_Source* from)
{
	Character *victim = from->SourceWorld()->GetCharacter(arguments[0]);

	if (!victim || victim->nowhere)
	{
		from->ServerMsg(from->SourceWorld()->i18n.Format("character_not_found"));
	}
	else
	{
		if (victim->admin < from->SourceAccess() || victim == from->SourceCharacter())
		{
			for (std::size_t i = 0; i < victim->paperdoll.size(); ++i)
			{
				if (victim->paperdoll[i] != 0)
				{
					int itemid = victim->paperdoll[i];
					int subloc = ((i == Character::Ring2 || i == Character::Armlet2 || i == Character::Bracer2) ? 1 : 0);

					if (victim->Unequip(victim->paperdoll[i], subloc))
					{
						PacketBuilder builder(PACKET_PAPERDOLL, PACKET_REMOVE, 43);
						builder.AddShort(victim->player->id);
						builder.AddChar(SLOT_CLOTHES);
						builder.AddChar(0); // ?
						victim->AddPaperdollData(builder, "B000A0HSW");
						builder.AddShort(itemid);
						builder.AddChar(subloc);
						builder.AddShort(victim->maxhp);
						builder.AddShort(victim->maxtp);
						builder.AddShort(victim->display_str);
						builder.AddShort(victim->display_intl);
						builder.AddShort(victim->display_wis);
						builder.AddShort(victim->display_agi);
						builder.AddShort(victim->display_con);
						builder.AddShort(victim->display_cha);
						builder.AddShort(victim->mindam);
						builder.AddShort(victim->maxdam);
						builder.AddShort(victim->accuracy);
						builder.AddShort(victim->evade);
						builder.AddShort(victim->armor);
						victim->Send(builder);
					}
				}
			}

			PacketBuilder builder(PACKET_AVATAR, PACKET_AGREE, 14);
			builder.AddShort(victim->player->id);
			builder.AddChar(SLOT_CLOTHES);
			builder.AddChar(0);
			builder.AddShort(0);
			builder.AddShort(0);
			builder.AddShort(0);
			builder.AddShort(0);
			builder.AddShort(0);

			UTIL_FOREACH(victim->map->characters, updatecharacter)
			{
				if (updatecharacter == victim || !victim->InRange(updatecharacter))
					continue;

				updatecharacter->Send(builder);
			}
		}
		else
		{
			from->ServerMsg(from->SourceWorld()->i18n.Format("command_access_denied"));
		}
	}
}

void Dress(const std::vector<std::string>& arguments, Command_Source* from)
{
	int argp = 0;
	Character *victim;

	if (arguments.size() > 1)
		victim = from->SourceWorld()->GetCharacter(arguments[argp++]);
	else
		victim = from->SourceCharacter();

	if (!victim || victim->nowhere)
	{
		from->ServerMsg(from->SourceWorld()->i18n.Format("character_not_found"));
		return;
	}

	if (victim->admin >= int(from->SourceWorld()->admin_config["cmdprotect"]) && victim->admin > from->SourceAccess())
	{
		from->ServerMsg(from->SourceWorld()->i18n.Format("command_access_denied"));
		return;
	}

	int item = util::to_int(arguments[argp++]);
	const EIF_Data &eif = from->SourceWorld()->eif->Get(item);

	if (eif.type == EIF::Armor && eif.gender != victim->gender)
	{
		from->ServerMsg(from->SourceWorld()->i18n.Format("can_not_dress"));
		return;
	}

	     if (eif.type == EIF::Hat)    victim->Dress(Character::Hat, eif.dollgraphic);
	else if (eif.type == EIF::Armor)  victim->Dress(Character::Armor, eif.dollgraphic);
	else if (eif.type == EIF::Boots)  victim->Dress(Character::Boots, eif.dollgraphic);
	else if (eif.type == EIF::Weapon) victim->Dress(Character::Weapon, eif.dollgraphic);
	else if (eif.type == EIF::Shield) victim->Dress(Character::Shield, eif.dollgraphic);
	else from->ServerMsg(from->SourceWorld()->i18n.Format("can_not_dress"));
}

void Dress2(const std::vector<std::string>& arguments, Command_Source* from)
{
	int argp = 0;
	Character *victim;

	if (arguments.size() > 2)
		victim = from->SourceWorld()->GetCharacter(arguments[argp++]);
	else
		victim = from->SourceCharacter();

	if (!victim || victim->nowhere)
	{
		from->ServerMsg(from->SourceWorld()->i18n.Format("character_not_found"));
		return;
	}

	if (victim->admin >= int(from->SourceWorld()->admin_config["cmdprotect"]) && victim->admin > from->SourceAccess())
	{
		from->ServerMsg(from->SourceWorld()->i18n.Format("command_access_denied"));
		return;
	}

	std::string slot = util::lowercase(arguments[argp++]);
	int gfx_id = util::to_int(arguments[argp++]);

		 if (slot == "hat")    victim->Dress(Character::Hat, gfx_id);
	else if (slot == "armor")  victim->Dress(Character::Armor, gfx_id);
	else if (slot == "boots")  victim->Dress(Character::Boots, gfx_id);
	else if (slot == "weapon") victim->Dress(Character::Weapon, gfx_id);
	else if (slot == "shield") victim->Dress(Character::Shield, gfx_id);
	else from->ServerMsg(from->SourceWorld()->i18n.Format("invalid_dress_slot"));
}

void Undress(const std::vector<std::string>& arguments, Command_Source* from)
{
	int argp = 0;
	Character *victim;

	if (arguments.size() > 1)
		victim = from->SourceWorld()->GetCharacter(arguments[argp++]);
	else
		victim = from->SourceCharacter();

	if (!victim)
	{
		from->ServerMsg(from->SourceWorld()->i18n.Format("character_not_found"));
		return;
	}

	if (victim->admin >= int(from->SourceWorld()->admin_config["cmdprotect"]) && victim->admin > from->SourceAccess())
	{
		from->ServerMsg(from->SourceWorld()->i18n.Format("command_access_denied"));
		return;
	}

	if (arguments.size() > 0)
	{
		std::string slot = util::lowercase(arguments[argp++]);

		     if (slot == "hat")    victim->Undress(Character::Hat);
		else if (slot == "armor")  victim->Undress(Character::Armor);
		else if (slot == "boots")  victim->Undress(Character::Boots);
		else if (slot == "weapon") victim->Undress(Character::Weapon);
		else if (slot == "shield") victim->Undress(Character::Shield);
		else if (slot == "all")    victim->Undress();
		else from->ServerMsg(from->SourceWorld()->i18n.Format("invalid_dress_slot"));
	}
	else
	{
		victim->Undress();
	}
}

COMMAND_HANDLER_REGISTER(char_mod)
	using namespace std::placeholders;
	Register({"setlevel", {"victim", "value"}, {}, 4}, std::bind(SetX, _1, _2, "level"));
	Register({"setexp", {"victim", "value"}, {}, 4}, std::bind(SetX, _1, _2, "exp"));
	Register({"setstr", {"victim", "value"}, {}, 4}, std::bind(SetX, _1, _2, "str"));
	Register({"setint", {"victim", "value"}, {}, 4}, std::bind(SetX, _1, _2, "int"));
	Register({"setwis", {"victim", "value"}, {}, 4}, std::bind(SetX, _1, _2, "wis"));
	Register({"setagi", {"victim", "value"}, {}, 4}, std::bind(SetX, _1, _2, "agi"));
	Register({"setcon", {"victim", "value"}, {}, 4}, std::bind(SetX, _1, _2, "con"));
	Register({"setcha", {"victim", "value"}, {}, 4}, std::bind(SetX, _1, _2, "cha"));
	Register({"setstatpoints", {"victim", "value"}, {}, 7}, std::bind(SetX, _1, _2, "statpoints"));
	Register({"setskillpoints", {"victim", "value"}, {}, 8}, std::bind(SetX, _1, _2, "skillpoints"));
	Register({"setadmin", {"victim", "value"}, {}, 8}, std::bind(SetX, _1, _2, "admin"));
	Register({"settitle", {"victim", "value"}, {}, 4}, std::bind(SetX, _1, _2, "title"));
	Register({"setfiance", {"victim", "value"}, {}, 4}, std::bind(SetX, _1, _2, "fiance"));
	Register({"setpartner", {"victim", "value"}, {}, 4}, std::bind(SetX, _1, _2, "partner"));
	Register({"sethome", {"victim", "value"}, {}, 4}, std::bind(SetX, _1, _2, "home"));
	Register({"setgender", {"victim", "value"}, {}, 4}, std::bind(SetX, _1, _2, "gender"));
	Register({"sethairstyle", {"victim", "value"}, {}, 8}, std::bind(SetX, _1, _2, "hairstyle"));
	Register({"sethaircolor", {"victim", "value"}, {}, 8}, std::bind(SetX, _1, _2, "haircolor"));
	Register({"setrace", {"victim", "value"}, {}, 4}, std::bind(SetX, _1, _2, "race"));
	Register({"setguildrank", {"victim", "value"}, {}, 4}, std::bind(SetX, _1, _2, "guildrank"));
	Register({"setkarma", {"victim", "value"}, {}, 4}, std::bind(SetX, _1, _2, "karma"));
	Register({"setclass", {"victim", "value"}, {}, 4}, std::bind(SetX, _1, _2, "class"));
	Register({"strip", {"victim"}, {}, 2}, Strip);
	Register({"dress", {"victim"}, {"id"}}, Dress); // victim is the actual optional argument
	Register({"dress2", {"victim", "slot"}, {"id"}, 6}, Dress2); // victim is the actual optional argument
	Register({"undress", {}, {"victim", "slot"}, 3}, Undress);

	RegisterAlias("d2", "dress2");
COMMAND_HANDLER_REGISTER_END(char_mod)

}
