
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


#if 0

Potential target table:
	Character* character;
	int damage_inflicted;
	double last_hit;

PickTarget():
	Pick the closest player with damage_inflicted > 0,
	otherwise Pick the closest player

No target:
	If passive:
		Walk around aimlessly
	If aggressive:
		PickTarget(),
		otherwise Walk around aimlessly

If target:
	If path:
		Chase target
		If path blocked:
			If blocker is attacker:
				Target blocker
			Else if lastblocker == blocker:
				Target blocker
			Else:
				Set lastblocker = blocker
		If attacker next to:
			Attack attacker (based on who has done the most damage)
	If no path:
		Path becomes equal to [target.x,target.y]

Event (hit or target moved off of path):
	If target and walked at least one step of current path:
		PickTarget()
	If no target:
		target = hitter




NPC AI PSEUDO CODE

struct NPC_Opponent
{
	Character *attacker;
	int damage;
	double last_hit;
};

struct NPC_AI
{
	NPC* npc;
	list<NPC_Opponent> opponents;
	Character* target;
	Path path;
};

NPC_AI::SetTarget(new_target)
{
	target = ;
	path = FindPathTo(new_target)

	if (path)
		target = new_target
		return true
	else
		return false
}

NPC_AI::PickTarget()
{
	auto nearby = npc->GetNearbyCharacters();
	// find distance by pathfinding
	// find damage influcted by target table lookup
	sort(nearby, damage_inflicted, distance)
	
	if !nearby.empty
		return SetTarget(nearby[0])
	else
		return false
}

NPC_AI::Step()
{
	auto nearby = npc->GetNearbyCharacters();

	if (any nearby characters next to me)
		Attack()

	if (target)
	{
		if (path.remaining > 1)
		{
			if (Walk(path.nextstep))

			else
			{
				blocker = character at path.nextstep

				if (blocker on opponent list)
				{
					target = blocker;
					Attack()
				}
			}
		}
	}
	else
	{
		if (aggressive)
		{
			if PickTarget()
				WalkRandom()
		}
		else
		{
			WalkRandom()
		}
	}
}

#endif


#endif // NPC_AI_HPP_INCLUDED
