
/* $Id$
 * EOSERV is released under the zlib license.
 * See LICENSE.txt for more info.
 */

#ifndef EOCLIENT_HPP_INCLUDED
#define EOCLIENT_HPP_INCLUDED

#include "fwd/eoclient.hpp"

#include <cstddef>
#include <cstdio>
#include <queue>
#include <string>

#include "socket.hpp"

#include "fwd/character.hpp"
#include "fwd/player.hpp"
#include "fwd/eodata.hpp"
#include "eoserver.hpp"
#include "packet.hpp"

/**
 * An action the server will execute for the client
 */
struct ActionQueue_Action
{
	PacketReader reader;
	double time;
	bool auto_queue;

	ActionQueue_Action(PacketReader reader_, double time_, bool auto_queue_ = false)
		: reader(reader_)
		, time(time_)
		, auto_queue(auto_queue_)
	{ }
};

/**
 * A list of actions a client needs to eventually have executed for it
 */
class ActionQueue
{
	public:
		std::queue<ActionQueue_Action *> queue;

		double next;
		void AddAction(PacketReader reader, double time, bool auto_queue = false);

		ActionQueue() : next(0) {};

		~ActionQueue();
};

/**
 * A connection between an EO Client and EOSERV
 */
class EOClient : public Client
{
	public:
		enum PacketState
		{
			ReadLen1,
			ReadLen2,
			ReadData
		};

		enum ClientState
		{
			Uninitialized,
			Initialized,
			LoggedIn,
			Playing
		};

	private:
		void Initialize();
		EOClient();

		FileType upload_type;
		std::FILE *upload_fh;
		std::size_t upload_pos;
		std::size_t upload_size;

		std::string send_buffer2;
		std::size_t send_buffer2_gpos;
		std::size_t send_buffer2_ppos;
		std::size_t send_buffer2_used;

	public:
		EOServer *server() { return static_cast<EOServer *>(Client::server); };
		int version;
		Player *player;
		unsigned int id;
		bool needpong;
		int hdid;
		ClientState state;

		ActionQueue queue;

		PacketState packet_state;
		unsigned char raw_length[2];
		unsigned int length;
		std::string data;

		PacketProcessor processor;

		EOClient(EOServer *server_) : Client(server_)
		{
			this->Initialize();
		}

		EOClient(const Socket &sock, EOServer *server_) : Client(sock, server_)
		{
			this->Initialize();
		}

		virtual bool NeedTick();

		void Tick();

		void Execute(const std::string &data);

		bool Upload(FileType type, int id, InitReply init_reply);
		bool Upload(FileType type, const std::string &filename, InitReply init_reply);
		void Send(const PacketBuilder &packet);

		~EOClient();
};

#endif // EOCLIENT_HPP_INCLUDED
