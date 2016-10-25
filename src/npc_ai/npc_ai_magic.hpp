
/* $Id$
 * EOSERV is released under the zlib license.
 * See LICENSE.txt for more info.
 */

#ifndef NPC_AI_MAGIC_HPP_INCLUDED
#define NPC_AI_MAGIC_HPP_INCLUDED

#include "../fwd/npc.hpp"

#include "../npc_ai.hpp"

class NPC_AI_Magic : public NPC_AI_Standard
{
	protected:
		int charge_effect_id;
		int spell_id;
		int charging;
		bool IsInRange(int x, int y, int range) const;

	public:
		NPC_AI_Magic(NPC* npc, int charge_effect_id, int spell_id);
		~NPC_AI_Magic();

		virtual void Act();
};

#endif // NPC_AI_MAGIC_HPP_INCLUDED
