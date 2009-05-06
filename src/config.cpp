
#include "config.hpp"

#include <cstdio>
#include <stdexcept>

#include "util.hpp"

Config::Config()
{

}

Config::Config(std::string filename)
{
	this->Read(filename);
}

void Config::Read(std::string filename)
{
	std::FILE *fh;
	char buf[Config::MaxLineLength];
	std::string line;
	std::string key;
	std::string val;
	std::size_t eqloc;
	util::variant vval;

	this->filename = filename;

	fh = std::fopen(filename.c_str(), "rt");
	if (!fh)
	{
		throw std::runtime_error("Configuration file not found.");
	}

	while (!std::feof(fh))
	{
		if (std::fgets(buf, Config::MaxLineLength, fh))
		{
			line = util::trim(buf);
		}
		else
		{
			line = "";
		}

		if (line.length() < 1)
		{
			continue;
		}

		if (line[0] == '#')
		{
			continue;
		}

		eqloc = line.find('=');

		if (eqloc == std::string::npos)
		{
			continue;
		}

		key = util::rtrim(line.substr(0, eqloc));

		if (line.length() > eqloc+1)
		{
			val = util::ltrim(line.substr(eqloc+1));
		}
		else
		{
			val = std::string("");
		}

		if (key == "REQUIRE")
		{
			this->Read(val);
		}
		else if (key == "INCLUDE")
		{
			try
			{
				this->Read(val);
			}
			catch (std::runtime_error)
			{
				fprintf(stderr, "INCLUDEd configuration file not found: %s\n", val.c_str());
			}
		}
		else
		{
			std::size_t loc = val.find('\\');
			while (loc != std::string::npos && loc != val.length())
			{
				if (val[loc + 1] == 't')
				{
					val.replace(loc, 2, "\t");
				}
				else if (val[loc + 1] == 'r')
				{
					val.replace(loc, 2, "\r");
				}
				else if (val[loc + 1] == 'n')
				{
					val.replace(loc, 2, "\n");
				}
				else if (val[loc + 1] == '\\')
				{
					val.replace(loc, 2, "\\");
				}

				loc = val.find('\\', loc+1);
			}

			this->operator[](key) = static_cast<util::variant>(val);
		}
	}

	std::fclose(fh);
}
