
#include <string>

#include "eoclient.hpp"
#include "packet.hpp"

#ifdef CLIENT_F_FUNC
#undef CLIENT_F_FUNC
#endif // CLIENT_F_FUNC

#define CLIENT_F_FUNC(FUNC) bool EOClient::Handle_##FUNC(int action, PacketReader &reader)

#define CLIENT_SENDRAW(REPLY) this->Send(REPLY)
#define CLIENT_SEND(REPLY) this->Send(this->processor.Encode(REPLY))

#include "handlers/Account.cpp"
#include "handlers/AdminInteract.cpp"
#include "handlers/Attack.cpp"
#include "handlers/Bank.cpp"
#include "handlers/Board.cpp"
#include "handlers/Book.cpp"
#include "handlers/Chair.cpp"
#include "handlers/Character.cpp"
#include "handlers/Chest.cpp"
#include "handlers/Citizen.cpp"
#include "handlers/Connection.cpp"
#include "handlers/Door.cpp"
#include "handlers/Emote.cpp"
#include "handlers/Face.cpp"
#include "handlers/Global.cpp"
#include "handlers/Guild.cpp"
#include "handlers/Init.cpp"
#include "handlers/Item.cpp"
#include "handlers/Jukebox.cpp"
#include "handlers/Locker.cpp"
#include "handlers/Login.cpp"
#include "handlers/Paperdoll.cpp"
#include "handlers/Party.cpp"
#include "handlers/Ping.cpp"
#include "handlers/Players.cpp"
#include "handlers/Refresh.cpp"
#include "handlers/Shop.cpp"
#include "handlers/Sit.cpp"
#include "handlers/SkillMaster.cpp"
#include "handlers/Talk.cpp"
#include "handlers/Trade.cpp"
#include "handlers/Walk.cpp"
#include "handlers/Warp.cpp"
#include "handlers/Welcome.cpp"