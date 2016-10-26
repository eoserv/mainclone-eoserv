
/* $Id$
 * EOSERV is released under the zlib license.
 * See LICENSE.txt for more info.
 */

#ifndef NPC_AI_HW2016_APOZEN_HPP_INCLUDED
#define NPC_AI_HW2016_APOZEN_HPP_INCLUDED

#include "../fwd/npc.hpp"

#include "../npc_ai.hpp"

#include <memory>

class NPC_AI_HW2016_ApozenSkull;

class NPC_AI_HW2016_Apozen : public NPC_AI_Standard
{
	public:
		NPC* skull[4];
	
	protected:
		int num_skulls;
		int charging;
		int chase;
		int spawn_cooldown = 1;
		bool IsInRange(int x, int y, int range) const;

	public:
		NPC_AI_HW2016_Apozen(NPC* npc);
		~NPC_AI_HW2016_Apozen();
		void SmartChase(Character* attacker, int range = 0);

		virtual void Spawn();
		virtual bool Dying();
		virtual void Act();
	
	friend class NPC_AI_HW2016_ApozenSkull;
};

#endif // NPC_AI_HW2016_APOZEN_HPP_INCLUDED
