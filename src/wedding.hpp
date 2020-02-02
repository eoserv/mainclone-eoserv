
/* $Id$
 * EOSERV is released under the zlib license.
 * See LICENSE.txt for more info.
 */

#ifndef WEDDING_HPP_INCLUDED
#define WEDDING_HPP_INCLUDED

#include "fwd/wedding.hpp"

#include "fwd/character.hpp"
#include "fwd/map.hpp"
#include "fwd/npc.hpp"
#include "fwd/timer.hpp"

#include <string>

class Wedding
{
	private:
		unsigned char priest_idx;

		NPC* GetPriest();
		Character* GetPartner1();
		Character* GetPartner2();

		void PriestSay(const std::string& message);

		void StartTimer();
		void StopTimer();

		void NextState();

		bool Check();

		void Reset();
		void ErrorOut();

	public:
		int state;
		int tick;

		std::string partner1;
		std::string partner2;

		TimeEvent *tick_timer;
		Map *map;

		Wedding(Map *map, unsigned char priest_idx);

		void Tick();
		bool StartWedding(const std::string& player1, const std::string& player2);

		bool Busy();

		void IDo(Character* character);

		~Wedding();
};

#endif // WEDDING_HPP_INCLUDED
