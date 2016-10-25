
/* $Id$
 * EOSERV is released under the zlib license.
 * See LICENSE.txt for more info.
 */

#ifndef NPC_AI_RANGED_HPP_INCLUDED
#define NPC_AI_RANGED_HPP_INCLUDED

#include "../fwd/character.hpp"
#include "../fwd/npc.hpp"

#include "../npc_ai.hpp"

class NPC_AI_Ranged : public NPC_AI_Standard
{
	protected:
		Character* PickTargetRanged(int range, bool legacy = false) const;
		bool IsInStraightRange(int x, int y, int range) const;

	public:
		NPC_AI_Ranged(NPC* npc);
		~NPC_AI_Ranged();

		virtual void Act();
};

#endif // NPC_AI_RANGED_HPP_INCLUDED
