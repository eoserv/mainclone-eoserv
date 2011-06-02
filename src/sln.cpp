
/* $Id$
 * EOSERV is released under the zlib license.
 * See LICENSE.txt for more info.
 */

#include "sln.hpp"

#include <pthread.h>

#include "console.hpp"
#include "eoserver.hpp"
#include "nanohttp.hpp"
#include "timer.hpp"
#include "world.hpp"
#include "character.hpp"

// TODO: Make this safe (race conditions)

SLN::SLN(EOServer *server)
{
	this->server = server;

	this->Request();
}

void SLN::Request()
{
	static pthread_t thread;

	if (pthread_create(&thread, 0, SLN::RequestThread, this) != 0)
	{
		throw std::exception();
	}

	pthread_detach(thread);
}

void *SLN::RequestThread(void *void_sln)
{
	SLN *sln(static_cast<SLN *>(void_sln));

	try
	{
		HTTP *http;

		std::string url = sln->server->world->config["SLNURL"];
		url += "check?software=EOSERV";
		url += std::string("&retry=") + HTTP::URLEncode(util::to_string(static_cast<int>(sln->server->world->config["SLNPeriod"])));

		if (static_cast<std::string>(sln->server->world->config["SLNHost"]).length() > 0)
		{
			url += std::string("&host=") + HTTP::URLEncode(sln->server->world->config["SLNHost"]);
		}

		url += std::string("&port=") + HTTP::URLEncode(sln->server->world->config["Port"]);
		url += std::string("&name=") + HTTP::URLEncode(sln->server->world->config["ServerName"]);

		if (static_cast<std::string>(sln->server->world->config["SLNSite"]).length() > 0)
		{
			url += std::string("&url=") + HTTP::URLEncode(sln->server->world->config["SLNSite"]);
		}

		if (static_cast<std::string>(sln->server->world->config["SLNZone"]).length() > 0)
		{
			url += std::string("&zone=") + HTTP::URLEncode(sln->server->world->config["SLNZone"]);
		}

		if (sln->server->world->config["GlobalPK"])
		{
			url += std::string("&pk=") + HTTP::URLEncode(sln->server->world->config["GlobalPK"]);
		}

		if (sln->server->world->config["Deadly"])
		{
			url += std::string("&deadly=") + HTTP::URLEncode(sln->server->world->config["Deadly"]);
		}

		try
		{
			if (static_cast<std::string>(sln->server->world->config["SLNBind"]) == "0")
			{
				http = HTTP::RequestURL(url);
			}
			else if (static_cast<std::string>(sln->server->world->config["SLNBind"]) == "1")
			{
				http = HTTP::RequestURL(url, IPAddress(static_cast<std::string>(sln->server->world->config["Host"])));
			}
			else
			{
				http = HTTP::RequestURL(url, IPAddress(static_cast<std::string>(sln->server->world->config["SLNBind"])));
			}
		}
		catch (Socket_Exception &e)
		{
			Console::Err(e.error());
			goto end;
		}
		catch (...)
		{
			Console::Err("There was a problem trying to make the HTTP request...");
			goto end;
		}

		while (!http->Done())
		{
			http->Tick(0.01);
		}

		std::vector<std::string> lines = util::explode("\r\n", http->Response());
		UTIL_FOREACH(lines, line)
		{
			if (line.length() == 0)
			{
				continue;
			}

			std::vector<std::string> parts = util::explode('\t', line);

			int code = util::to_int(parts.at(0));
			int maincode = code / 100;

			std::string errmsg = std::string("(") + parts.at(0) + ") ";
			bool resolved = false;

			switch (maincode)
			{
				case 1: // Informational
					break;

				case 2: // Success
					break;

				case 3: // Warning
					errmsg += "SLN Update Warning: ";
					switch (code)
					{
						case 300:
							errmsg += parts.at(4);

							if (parts.at(2) == "retry")
							{
								sln->server->world->config["SLNPeriod"] = util::to_int(parts.at(3));
								resolved = true;
							}
							else if (parts.at(2) == "name")
							{
								sln->server->world->config["ServerName"] = parts.at(3);
								resolved = true;
							}
							else if (parts.at(2) == "url")
							{
								sln->server->world->config["SLNSite"] = parts.at(3);
								resolved = true;
							}
							break;

						case 301:
							errmsg += parts.at(2);
							break;

						case 302:
							errmsg += parts.at(2);
							break;

						default:
							errmsg += "Unknown error code";
							break;
					}

					Console::Wrn(errmsg);
					sln->server->world->AdminMsg(0, errmsg, ADMIN_HGM);
					if (resolved)
					{
						sln->server->world->AdminMsg(0, "EOSERV has automatically resolved this message and the next check-in should succeed.", ADMIN_HGM);
					}
					break;

				case 4: // Client Error
					errmsg += "SLN Update Client Error: ";
					switch (code)
					{
						case 400:
							errmsg += parts.at(3);
							break;

						case 401:
							errmsg += parts.at(3);

							if (parts.at(2) == "url")
							{
								sln->server->world->config["SLNSite"] = "";
								resolved = true;
							}
							break;

						case 402:
							errmsg += parts.at(2);
							break;

						case 403:
							errmsg += parts.at(2);
							break;

						case 404:
							errmsg += parts.at(2);
							break;

						default:
							errmsg += "Unknown error code";
							break;
					}

					Console::Wrn(errmsg);
					sln->server->world->AdminMsg(0, errmsg, ADMIN_HGM);
					if (resolved)
					{
						sln->server->world->AdminMsg(0, "EOSERV has automatically resolved this message and the next check-in should succeed.", ADMIN_HGM);
					}
					break;

				case 5: // Server Error
					errmsg += "SLN Update Server Error: ";

					switch (code)
					{
						case 500:
							errmsg += parts.at(2);
							break;

						default:
							errmsg += "Unknown error code";
							break;

					}

					Console::Wrn(errmsg);
					sln->server->world->AdminMsg(0, errmsg, ADMIN_HGM);
					break;
			}
		}

		delete http;
	}
	catch (...)
	{
		Console::Wrn("There was an unknown SLN error");
	}

end:
	TimeEvent *event = new TimeEvent(SLN::TimedRequest, sln, static_cast<int>(sln->server->world->config["SLNPeriod"]), 1);
	sln->server->world->timer.Register(event);

	return 0;
}

void SLN::TimedRequest(void *void_sln)
{
	SLN *sln(static_cast<SLN *>(void_sln));

	sln->Request();
}
