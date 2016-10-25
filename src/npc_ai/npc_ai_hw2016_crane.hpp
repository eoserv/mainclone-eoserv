
/* $Id$
 * EOSERV is released under the zlib license.
 * See LICENSE.txt for more info.
 */

#ifndef NPC_AI_HW2016_CRANE_HPP_INCLUDED
#define NPC_AI_HW2016_CRANE_HPP_INCLUDED

#include "../fwd/npc.hpp"

#include "../npc_ai.hpp"

class NPC_AI_HW2016_Crane : public NPC_AI_Standard
{
	public:
		NPC_AI_HW2016_Crane(NPC* npc);
		~NPC_AI_HW2016_Crane();

		virtual bool Dying();
		virtual void Act();
};

#endif // NPC_AI_HW2016_CRANE_HPP_INCLUDED
