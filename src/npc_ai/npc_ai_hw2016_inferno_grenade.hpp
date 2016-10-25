
/* $Id$
 * EOSERV is released under the zlib license.
 * See LICENSE.txt for more info.
 */

#ifndef NPC_AI_HW2016_INFERNO_GRENADE_HPP_INCLUDED
#define NPC_AI_HW2016_INFERNO_GRENADE_HPP_INCLUDED

#include "../fwd/npc.hpp"

#include "../npc_ai.hpp"

class NPC_AI_HW2016_Inferno_Grenade : public NPC_AI_Standard
{
	protected:
		int charging;
		bool IsInRange(int x, int y, int range) const;

	public:
		NPC_AI_HW2016_Inferno_Grenade(NPC* npc);
		~NPC_AI_HW2016_Inferno_Grenade();

		virtual bool Dying();
		virtual void Act();
};

#endif // NPC_AI_HW2016_INFERNO_GRENADE_HPP_INCLUDED
