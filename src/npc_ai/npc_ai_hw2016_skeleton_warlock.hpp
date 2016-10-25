
/* $Id$
 * EOSERV is released under the zlib license.
 * See LICENSE.txt for more info.
 */

#ifndef NPC_AI_HW2016_SKELETON_WARLOCK_HPP_INCLUDED
#define NPC_AI_HW2016_SKELETON_WARLOCK_HPP_INCLUDED

#include "../fwd/npc.hpp"

#include "../npc_ai.hpp"

class NPC_AI_HW2016_Skeleton_Warlock : public NPC_AI_Standard
{
	protected:
		int charging;
		bool IsInRange(int x, int y, int range) const;

	public:
		NPC_AI_HW2016_Skeleton_Warlock(NPC* npc);
		~NPC_AI_HW2016_Skeleton_Warlock();

		virtual void Act();
};

#endif // NPC_AI_HW2016_SKELETON_WARLOCK_HPP_INCLUDED
