
/* $Id$
 * EOSERV is released under the zlib license.
 * See LICENSE.txt for more info.
 */

#ifndef NPC_HPP_INCLUDED
#define NPC_HPP_INCLUDED

#include "fwd/npc.hpp"

#include "fwd/character.hpp"
#include "fwd/eodata.hpp"
#include "fwd/map.hpp"
#include "fwd/npc_ai.hpp"
#include "fwd/npc_data.hpp"

#include <array>
#include <list>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * Used by the NPC class to store information about an attacker
 */
struct NPC_Opponent
{
	Character *attacker;
	int damage;
	double last_hit;
};

/**
 * An instance of an NPC created and managed by a Map
 */
class NPC
{
	public:
		bool temporary;
		Direction direction;
		unsigned char x, y;
		NPC *parent;
		bool alive;
		double dead_since;
		double last_act;
		double act_speed;
		int hp;
		int peakhp = 1;
		int totaldamage;
		std::list<std::unique_ptr<NPC_Opponent>> damagelist;

		Map *map;
		unsigned char index;
		unsigned char spawn_type;
		short spawn_time;
		unsigned char spawn_x, spawn_y;

		std::unique_ptr<NPC_AI> ai;

		int id;
		
		int hw2016_aposhield = 0;
		int hw2016_apoweak = 0;

		static void SetSpeedTable(std::array<double, 7> speeds);

		NPC(Map *map, short id, unsigned char x, unsigned char y, unsigned char spawn_type, short spawn_time, unsigned char index, bool temporary = false);

		const NPC_Data& Data() const;
		const ENF_Data& ENF() const;

		void Spawn(NPC *parent = 0);
		void Act();

		bool Walk(Direction, bool playerghost = false);
		void Damage(Character *from, int amount, int spell_id = -1);
		void RemoveFromView(Character *target);
		void Killed(Character *from, int amount, int spell_id = -1);
		void Die(bool show = true);
		void Effect(int effect);
		
		// Called by the AI in some cases when a character should be (un)registered explicitly/early for a logout event
		void RegisterCharacter(Character*);
		void UnregisterCharacter(Character*);
		
		// Called by Character on logout if logout event is registered
		void UnregisterCharacterEvent(Character*);

		void Attack(Character *target, bool ranged = false);
		
		void Say(const std::string& message);

		void FormulaVars(std::unordered_map<std::string, double> &vars, std::string prefix = "");

		~NPC();
};

#endif // NPC_HPP_INCLUDED
