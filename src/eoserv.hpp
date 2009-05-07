#ifndef EOSERV_HPP_INCLUDED
#define EOSERV_HPP_INCLUDED

#include <list>
#include <vector>
#include <ctime>
#include <string>

class World;
class Player;
class Character;
class Guild;
class Party;
class NPC;
class Map;
class ActionQueue;

struct Map_Item;
struct Character_Item;
struct Character_Spell;
struct NPC_Opponent;

#include "database.hpp"
#include "util.hpp"
#include "config.hpp"
#include "eoconst.hpp"
#include "timer.hpp"
#include "socket.hpp"
#include "packet.hpp"
#include "eodata.hpp"

extern World *the_world;

/**
 * Serialize a list of items in to a text format that can be restored with ItemUnserialize
 */
std::string ItemSerialize(std::list<Character_Item> list);

/**
 * Convert a string generated by ItemSerialze back to a list of items
 */
std::list<Character_Item> ItemUnserialize(std::string serialized);

/**
 * Serialize a paperdoll of 15 items in to a string that can be restored with DollUnserialize
 */
std::string DollSerialize(util::array<int, 15> list);

/**
 * Convert a string generated by DollSerialze back to a list of 15 items
 */
util::array<int, 15> DollUnserialize(std::string serialized);

extern Config eoserv_config;
extern Config admin_config;

// ewww
class EOClient;
class EOServer;

/**
 * Object which holds and manages all maps and characters on the server, as well as timed events
 * Only one of these should exist per server
 */
class World
{
	private:
		World();

	public:
		Timer timer;

		EOServer *server;

		std::list<Character *> characters;
		std::list<Guild *> guilds;
		std::list<Party *> partys;
		std::list<NPC *> npcs;
		std::vector<Map *> maps;

		int last_character_id;

		World(util::array<std::string, 5> dbinfo, Config);

		int GenerateCharacterID();
		int GeneratePlayerID();

		void Login(Character *);
		void Logout(Character *);

		void Msg(Character *from, std::string message);
		void AdminMsg(Character *from, std::string message, int minlevel = ADMIN_GUARDIAN);
		void AnnounceMsg(Character *from, std::string message);

		void Reboot();
		void Reboot(int seconds, std::string reason);

		void Kick(Character *from, Character *victim, bool announce = true);
		void Ban(Character *from, Character *victim, double duration, bool announce = true);

		Character *GetCharacter(std::string name);
		Character *GetCharacterPID(unsigned int id);
		Character *GetCharacterCID(unsigned int id);
};

#include "eoclient.hpp"

/**
 * Object representing an item on the floor of a map
 */
struct Map_Item
{
	int uid;
	int id;
	int amount;
	int x;
	int y;
	unsigned int owner; // Player ID
	double unprotecttime;
};

/**
 * Object representing a warp tile on a map, as well as storing door state
 */
struct Map_Warp
{
	int map;
	int x;
	int y;
	int levelreq;

	enum WarpSpec
	{
		NoDoor,
		Door,
		LockedSilver,
		LockedCrystal,
		LockedWraith
	};

	WarpSpec spec;
	bool open;

	Map_Warp() : spec(Map_Warp::NoDoor), open(false) {}
};

/**
 * Object representing one tile on a map
 */
struct Map_Tile
{
	enum TileSpec
	{
		None = -1,
		Wall,
		ChairDown,
		ChairLeft,
		ChairRight,
		ChairUp,
		ChairDownRight,
		ChairUpLeft,
		ChairAll,
		Door,
		Chest,
		Unknown1,
		Unknown2,
		Unknown3,
		Unknown4,
		Unknown5,
		Unknown6,
		BankVault,
		NPCBoundary,
		MapEdge,
		FakeWall,
		Board1,
		Board2,
		Board3,
		Board4,
		Board5,
		Board6,
		Board7,
		Board8,
		Jukebox,
		Jump,
		Water,
		Unknown7,
		Arena,
		AmbientSource,
		Spikes1,
		Spikes2,
		Spikes3
	};

	TileSpec tilespec;

	Map_Warp *warp;

	Map_Tile() : tilespec(Map_Tile::None), warp(0) {}

	bool Walkable(bool npc = false)
	{
		switch (this->tilespec)
		{
			case Wall:
			case ChairDown:
			case ChairLeft:
			case ChairRight:
			case ChairUp:
			case ChairDownRight:
			case ChairUpLeft:
			case ChairAll:
			case Chest:
			case BankVault:
			case MapEdge:
			case Board1:
			case Board2:
			case Board3:
			case Board4:
			case Board5:
			case Board6:
			case Board7:
			case Jukebox:
				return false;
			case NPCBoundary:
				return !npc;
			default:
				return true;
		}
	}

	~Map_Tile()
	{
		if (this->warp)
		{
			delete this->warp;
		}
	}
};

/**
 * Contains all information about a map, holds reference to contained Characters and manages NPCs on it
 */
class Map
{
	public:
		int id;
		char rid[4];
		bool pk;
		int filesize;
		int width;
		int height;
		std::string filename;
		std::list<Character *> characters;
		std::list<NPC *> npcs;
		std::list<Map_Item> items;
		std::map<int, std::map<int, Map_Tile> > tiles;
		bool exists;

		Map(int id);

		int GenerateItemID();

		void Enter(Character *, int animation = WARP_ANIMATION_NONE);
		void Leave(Character *, int animation = WARP_ANIMATION_NONE);

		void Msg(Character *from, std::string message);
		bool Walk(Character *from, int direction, bool admin = false);
		void Attack(Character *from, int direction);
		void Face(Character *from, int direction);
		void Sit(Character *from, int sit_type);
		void Stand(Character *from);
		void Emote(Character *from, int emote);
		bool OpenDoor(Character *from, int x, int y);

		Map_Item *AddItem(int id, int amount, int x, int y, Character *from = 0);
		void DelItem(int uid, Character *from = 0);

		bool InBounds(int x, int y);
		bool Walkable(int x, int y, bool npc = false);
		Map_Tile::TileSpec GetSpec(int x, int y);
		Map_Warp *GetWarp(int x, int y);

		void Effect(int effect, int param);

		Character *GetCharacter(std::string name);
		Character *GetCharacterPID(unsigned int id);
		Character *GetCharacterCID(unsigned int id);

		~Map();
};

/**
 * Object representing a player, but not a character
 */
class Player
{
	public:
		int login_time;
		bool online;
		unsigned int id;
		std::string username;
		std::string password;

		Player(std::string username);

		std::list<Character *> characters;
		Character *character;

		static bool ValidName(std::string username);
		static Player *Login(std::string username, std::string password);
		static bool Create(std::string username, std::string password, std::string fullname, std::string location, std::string email, std::string computer, std::string hdid, std::string ip);
		static bool Exists(std::string username);
		bool AddCharacter(std::string name, int gender, int hairstyle, int haircolor, int race);
		void ChangePass(std::string password);
		static bool Online(std::string username);

		EOClient *client;

		~Player();
};

/**
 * One type of item in a Characters inventory
 */
struct Character_Item
{
	int id;
	int amount;
};

/**
 * One spell that a Character knows
 */
struct Character_Spell
{
	int id;
	int level;
};

class Character
{
	public:
		int login_time;
		bool online;
		unsigned int id;
		int admin;
		std::string name;
		std::string title;
		std::string home;
		std::string partner;
		int clas;
		int gender;
		int race;
		int hairstyle, haircolor;
		int mapid, x, y, direction;
		int spawnmap, spawnx, spawny;
		int level, exp;
		int hp, tp;
		int str, intl, wis, agi, con, cha;
		int statpoints, skillpoints;
		int weight, maxweight;
		int karma;
		int sitting, visible;
		int bankmax;
		int goldbank;
		int usage;

		int maxsp;
		int maxhp, maxtp;
		int accuracy, evade, armor;
		int mindam, maxdam;

		bool modal;

		bool trading;
		Character *trade_partner;
		bool trade_agree;
		std::list<Character_Item> trade_inventory;

		int warp_anim;

		enum EquipLocation
		{
			Boots,
			Accessory,
			Gloves,
			Belt,
			Armor,
			Necklace,
			Hat,
			Shield,
			Weapon,
			Ring1,
			Ring2,
			Armlet1,
			Armlet2,
			Bracer1,
			Bracer2
		};

		std::list<Character_Item> inventory;
		std::list<Character_Item> bank;
		util::array<int, 15> paperdoll;
		std::list<Character_Spell> spells;
		std::list<NPC *> unregister_npc;

		Character(std::string name);

		static bool ValidName(std::string name);
		static bool Exists(std::string name);
		static Character *Create(Player *, std::string name, int gender, int hairstyle, int haircolor, int race);
		static void Delete(std::string name);

		void Msg(Character *from, std::string message);
		bool Walk(int direction);
		bool AdminWalk(int direction);
		void Attack(int direction);
		void Sit(int sit_type);
		void Stand();
		void Emote(int emote);
		int HasItem(int item);
		bool AddItem(int item, int amount);
		bool DelItem(int item, int amount);
		bool AddTradeItem(int item, int amount);
		bool DelTradeItem(int item);
		bool Unequip(int item, int subloc);
		bool Equip(int item, int subloc);
		bool InRange(int x, int y);
		bool InRange(Character *);
		bool InRange(NPC *);
		bool InRange(Map_Item);
		void Warp(int map, int x, int y, int animation = WARP_ANIMATION_NONE);
		void Refresh();
		std::string PaddedGuildTag();
		int Usage();
		void CalculateStats();

		void Save();

		~Character();

		Player *player;
		Guild *guild;
		std::string guild_tag;
		int guild_rank;
		Party *party;
		Map *map;
};

/**
 * Used by the NPC class to store information about an attacker
 */
struct NPC_Opponent
{
	Character *attacker;
	int damage;
	bool first;
};

/**
 * An instance of an NPC created and managed by a Map
 */
class NPC
{
	public:
		int id;
		ENF_Data *data;
		int x, y;
		int direction;
		int distance;
		int spawn_time;
		int spawn_x, spawn_y;
		int min_x, min_y, max_x, max_y;
		int tries;
		NPC *parent;

		bool alive;
		int dead_since;
		bool attack;
		int hp;
		std::list<NPC_Opponent> damagelist;

		Map *map;
		int index;

		NPC(Map *map, int id, int x, int y, int distance, int spawn_time, int index);

		bool SpawnReady();
		void Spawn();

		void Follow(Character *target = 0);
};

/**
 * Stores guild information and references to online members
 * Created by the World object when a member of the guild logs in, and destroyed when the last member logs out
 */
class Guild
{
	public:
		std::string tag;
		std::string name;
		std::list<Character *> members;
		util::array<std::string, 9> ranks;
		std::time_t created;

		void Msg(Character *from, std::string message);
};

/**
 * A temporary group of Characters
 */
class Party
{
	public:
		Party(Character *host, Character *other);

		Character *host;
		std::list<Character *> members;

		void Msg(Character *from, std::string message);
		void Join(Character *);
		void Part(Character *);
};

#endif // EOSERV_HPP_INCLUDED
