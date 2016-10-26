
/* $Id$
 * EOSERV is released under the zlib license.
 * See LICENSE.txt for more info.
 */

#ifndef NPC_AI_HPP_INCLUDED
#define NPC_AI_HPP_INCLUDED

#include "fwd/npc_ai.hpp"

#include "fwd/character.hpp"
#include "fwd/npc.hpp"

#include <deque>
#include <map>
#include <utility>
#include <vector>

class NPC_AI
{
	protected:
		NPC* npc;

	public:
		NPC_AI(NPC* npc);
		virtual ~NPC_AI();

		virtual void HitBy(Character* character, bool spell, int damage);

		virtual void Spawn();
		virtual void Act();
		virtual bool Dying();

		// Character has potentially logged out and is no longer valid
		// If the NPC is targetting this character they should re-calculate a new target
		virtual void Unregister(Character* character);
};

class NPC_AI_Legacy : public NPC_AI
{
	private:
		int walk_idle_for;

	protected:
		Character* PickTarget(bool legacy = true) const;
		Character* PickTargetRandom() const;
		Character* PickTargetRandomMD() const;
		bool IsNextTo(int x, int y) const;
		void DumbChase(int x, int y);
		void RandomWalk();

	public:
		NPC_AI_Legacy(NPC* npc);
		~NPC_AI_Legacy();

		virtual void Spawn();
		virtual void Act();
};

class NPC_AI_Standard : public NPC_AI_Legacy
{
	protected:
		Character* target;
		std::deque<std::pair<int,int>> path;
		void SmartChase(Character* attacker, int range = 0);

	public:
		NPC_AI_Standard(NPC* npc);
		~NPC_AI_Standard();

		virtual void HitBy(Character* character, bool spell, int damage);

		virtual void Act();

		virtual void Unregister(Character* character);
};

#endif // NPC_AI_HPP_INCLUDED
