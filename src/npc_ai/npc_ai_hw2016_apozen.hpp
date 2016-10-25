
/* $Id$
 * EOSERV is released under the zlib license.
 * See LICENSE.txt for more info.
 */

#ifndef NPC_AI_HW2016_APOZEN_HPP_INCLUDED
#define NPC_AI_HW2016_APOZEN_HPP_INCLUDED

#include "../fwd/npc.hpp"

#include "../npc_ai.hpp"

#include <memory>

class NPC_AI_HW2016_Apozen : public NPC_AI_Standard
{
	private:
		class Persist
		{
			int timed_spawned;
		};
		
		static std::unique_ptr<Persist> persist;

	protected:
		NPC* skull[4];
		int charging;
		bool IsInRange(int x, int y, int range) const;

	public:
		NPC_AI_HW2016_Apozen(NPC* npc);
		~NPC_AI_HW2016_Apozen();

		virtual void Spawn();
		virtual bool Dying();
		virtual void Act();
};

#endif // NPC_AI_HW2016_APOZEN_HPP_INCLUDED
