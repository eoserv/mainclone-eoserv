
/* $Id$
 * EOSERV is released under the zlib license.
 * See LICENSE.txt for more info.
 */

#ifndef FWD_PACKET_HPP_INCLUDED
#define FWD_PACKET_HPP_INCLUDED

class PacketProcessor;
class PacketReader;
class PacketBuilder;

enum PacketFamily : unsigned char
{
	PACKET_INTERNAL = 0,
	PACKET_CONNECTION = 1,
	PACKET_ACCOUNT = 2,
	PACKET_CHARACTER = 3,
	PACKET_LOGIN = 4,
	PACKET_WELCOME = 5,
	PACKET_WALK = 6,
	PACKET_FACE = 7,
	PACKET_CHAIR = 8,
	PACKET_EMOTE = 9,
	PACKET_ATTACK = 11,
	PACKET_SPELL = 12,
	PACKET_SHOP = 13,
	PACKET_ITEM = 14,
	PACKET_STATSKILL = 16,
	PACKET_GLOBAL = 17,
	PACKET_TALK = 18,
	PACKET_WARP = 19,
	PACKET_JUKEBOX = 21,
	PACKET_PLAYERS = 22,
	PACKET_AVATAR = 23,
	PACKET_PARTY = 24,
	PACKET_REFRESH = 25,
	PACKET_NPC = 26,
	PACKET_PLAYER_AUTOREFRESH = 27,
	PACKET_NPC_AUTOREFRESH = 28,
	PACKET_APPEAR = 29,
	PACKET_PAPERDOLL = 30,
	PACKET_EFFECT = 31,
	PACKET_TRADE = 32,
	PACKET_CHEST = 33,
	PACKET_DOOR = 34,
	PACKET_MESSAGE = 35,
	PACKET_BANK = 36,
	PACKET_LOCKER = 37,
	PACKET_BARBER = 38,
	PACKET_GUILD = 39,
	PACKET_MUSIC = 40,
	PACKET_SIT = 41,
	PACKET_RECOVER = 42,
	PACKET_BOARD = 43,
	PACKET_CAST = 44,
	PACKET_ARENA = 45,
	PACKET_PRIEST = 46,
	PACKET_MARRIAGE = 47,
	PACKET_ADMININTERACT = 48,
	PACKET_CITIZEN = 49,
	PACKET_QUEST = 50,
	PACKET_BOOK = 51,
	PACKET_F_INIT = 255
};

enum PacketAction : unsigned char
{
	PACKET_REQUEST = 1,
	PACKET_ACCEPT = 2,
	PACKET_REPLY = 3,
	PACKET_REMOVE = 4,
	PACKET_AGREE = 5,
	PACKET_CREATE = 6,
	PACKET_ADD = 7,
	PACKET_PLAYER = 8,
	PACKET_TAKE = 9,
	PACKET_USE = 10,
	PACKET_BUY = 11,
	PACKET_SELL = 12,
	PACKET_OPEN = 13,
	PACKET_CLOSE = 14,
	PACKET_MSG = 15,
	PACKET_SPEC = 16,
	PACKET_ADMIN = 17,
	PACKET_LIST = 18,
	PACKET_TELL = 20,
	PACKET_REPORT = 21,
	PACKET_ANNOUNCE = 22,
	PACKET_SERVER = 23,
	PACKET_DROP = 24,
	PACKET_JUNK = 25,
	PACKET_OBTAIN = 26,
	PACKET_GET = 27,
	PACKET_KICK = 28,
	PACKET_RANK = 29,
	PACKET_TARGET_SELF = 30,
	PACKET_TARGET_OTHER = 31,
	PACKET_TARGET_GROUP = 33, // Tentative name
	PACKET_DIALOG = 34,

	PACKET_INTERNAL_NULL = 128,
	PACKET_INTERNAL_WARP = 129,

	PACKET_PING = 240,
	PACKET_PONG = 241,
	PACKET_NET3 = 242,
	PACKET_A_INIT = 255
};

#endif // FWD_PACKET_HPP_INCLUDED
