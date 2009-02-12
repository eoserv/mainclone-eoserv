#ifndef EOCLIENT_HPP_INCLUDED
#define EOCLIENT_HPP_INCLUDED

#include <cstdio>

#include "socket.hpp"
#include "packet.hpp"

#define CLIENT_F_FUNC(FUNC) bool Handle_##FUNC(int action, PacketReader &reader)

class EOClient;

class EOClient : public Client
{
	private:
		void Initialize();

	public:
		enum State
		{
			ReadLen1,
			ReadLen2,
			ReadData
		};

		int state;
		unsigned char raw_length[2];
		int length;
		std::string data;

		PacketProcessor processor;

		EOClient() { this->Initialize(); };
		EOClient(SOCKET s, sockaddr_in sa) : Client(s, sa) { this->Initialize(); };

		void Execute(std::string data);

		CLIENT_F_FUNC(Init);
		CLIENT_F_FUNC(Connection);
		CLIENT_F_FUNC(Account);
		CLIENT_F_FUNC(Character);
		CLIENT_F_FUNC(Login);
		CLIENT_F_FUNC(Welcome);
		CLIENT_F_FUNC(Walk);
		CLIENT_F_FUNC(Face);
		CLIENT_F_FUNC(Chair);
		CLIENT_F_FUNC(Emote);
		CLIENT_F_FUNC(Attack);
		CLIENT_F_FUNC(Shop);
		CLIENT_F_FUNC(Item);
		CLIENT_F_FUNC(SkillMaster);
		CLIENT_F_FUNC(Global);
		CLIENT_F_FUNC(Talk);
		CLIENT_F_FUNC(Warp);
		CLIENT_F_FUNC(Jukebox);
		CLIENT_F_FUNC(Players);
		CLIENT_F_FUNC(Party);
		CLIENT_F_FUNC(Refresh);
		CLIENT_F_FUNC(Paperdoll);
		CLIENT_F_FUNC(Trade);
		CLIENT_F_FUNC(Chest);
		CLIENT_F_FUNC(Door);
		CLIENT_F_FUNC(Ping);
		CLIENT_F_FUNC(Bank);
		CLIENT_F_FUNC(Locker);
		CLIENT_F_FUNC(Guild);
		CLIENT_F_FUNC(Sit);
		CLIENT_F_FUNC(Board);
		//CLIENT_F_FUNC(Arena);
		CLIENT_F_FUNC(AdminInteract);
		CLIENT_F_FUNC(Citizen);
		//CLIENT_F_FUNC(Quest);
		CLIENT_F_FUNC(Book);

};

#endif // EOCLIENT_HPP_INCLUDED