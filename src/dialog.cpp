
/* $Id$
 * EOSERV is released under the zlib license.
 * See LICENSE.txt for more info.
 */

#include "character.hpp"
#include "dialog.hpp"

#include "packet.hpp"

#include "util.hpp"
#include "util/rpn.hpp"

#include <cstddef>
#include <list>
#include <string>
#include <unordered_map>
#include <utility>

static std::string var_interpolate(std::string str_value, Character* character)
{
	std::list<std::pair<std::size_t, std::size_t>> replacements;

	std::size_t pos = 0;

	while (pos < str_value.length())
	{
		std::size_t open = str_value.find_first_of('{', pos);
		std::size_t close = str_value.find_first_of('}', open);

		if (open == std::string::npos || close == std::string::npos)
			break;

		replacements.push_back(std::make_pair(open, close - open));

		pos = close + 1;
	}

	if (replacements.size() > 0)
	{
		std::unordered_map<std::string, double> vars;
		character->FormulaVars(vars);

		long offset = 0;

		UTIL_FOREACH(replacements, range)
		{
			std::string id = str_value.substr(range.first + offset + 1, range.second - 1);
			std::string replacement = util::to_string(util::rpn_eval(util::rpn_parse(id), vars));

			str_value = str_value.substr(0, range.first + offset)
			          + replacement
			          + str_value.substr(range.first + range.second + offset + 1);

			offset += long(replacement.length()) - long(id.length());
		}
	}

	return str_value;
}

Dialog::Dialog()
{

}

void Dialog::AddPage(const std::string& text)
{
	this->pages.push_back(text);
}

void Dialog::AddLink(int id, const std::string& text)
{
	this->links.insert(std::make_pair(id, text));
}

bool Dialog::CheckLink(int id) const
{
	return this->links.find(id) != this->links.end();
}

int Dialog::PacketLength() const
{
	std::size_t size = this->pages.size() * 3;
	size += this->links.size() * 5;

	UTIL_FOREACH(this->pages, page)
	{
		size += page.length()*2;
	}

	UTIL_FOREACH(this->links, link)
	{
		size += link.second.length()*2;
	}

	return size;
}

void Dialog::BuildPacket(PacketBuilder& builder, Character* character) const
{
	builder.ReserveMore(this->PacketLength());

	UTIL_FOREACH(this->pages, page)
	{
		builder.AddShort(DIALOG_TEXT);
		builder.AddBreakString(var_interpolate(page, character));
	}

	UTIL_FOREACH(this->links, link)
	{
		builder.AddShort(DIALOG_LINK);
		builder.AddShort(link.first);
		builder.AddBreakString(var_interpolate(link.second, character));
	}
}

Dialog::~Dialog()
{

}
