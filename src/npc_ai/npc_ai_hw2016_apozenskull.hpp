
/* $Id$
 * EOSERV is released under the zlib license.
 * See LICENSE.txt for more info.
 */

#ifndef NPC_AI_HW2016_APOZENSKULL_HPP_INCLUDED
#define NPC_AI_HW2016_APOZENSKULL_HPP_INCLUDED

#include "../fwd/npc.hpp"

#include "../npc_ai.hpp"

class NPC_AI_HW2016_ApozenSkull : public NPC_AI_Standard
{
	protected:
		int charging;
	
	public:
		NPC* apozen;
		bool really_die = false;
		
	protected:
		bool IsInRange(int x, int y, int range) const;
		NPC* PickHealTarget(int range) const;

	public:
		NPC_AI_HW2016_ApozenSkull(NPC* npc, NPC* apozen);
		~NPC_AI_HW2016_ApozenSkull();

		virtual bool Dying();
		virtual void Act();
	
	friend class NPC_AI_HW2016_Apozen;
};

#endif // NPC_AI_HW2016_APOZENSKULL_HPP_INCLUDED
