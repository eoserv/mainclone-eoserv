
/* $Id$
 * EOSERV is released under the zlib license.
 * See LICENSE.txt for more info.
 */

#include "wedding.hpp"

#include "character.hpp"
#include "map.hpp"
#include "npc.hpp"
#include "packet.hpp"
#include "timer.hpp"
#include "world.hpp"

#include "console.hpp"
#include "util.hpp"

#include <string>

static void wedding_tick(void *wedding_void)
{
	Wedding* wedding = static_cast<Wedding*>(wedding_void);

	wedding->Tick();
}

NPC* Wedding::GetPriest()
{
	return this->map->GetNPCIndex(this->priest_idx);
}

Character* Wedding::GetPartner1()
{
	return this->map->GetCharacter(this->partner1);
}

Character* Wedding::GetPartner2()
{
	return this->map->GetCharacter(this->partner2);
}

void Wedding::PriestSay(const std::string& message)
{
	NPC* priest = this->GetPriest();

	if (priest)
		priest->Say(message);
}

void Wedding::StartTimer()
{
	if (!this->tick_timer)
	{
		this->tick_timer = new TimeEvent(wedding_tick, this, 1.5, Timer::FOREVER);
		this->map->world->timer.Register(this->tick_timer);
	}
}

void Wedding::StopTimer()
{	
	if (this->tick_timer)
	{
		this->map->world->timer.Unregister(this->tick_timer);
		this->tick_timer = nullptr;
	}
}

void Wedding::NextState()
{
	if (!this->Check())
		return;

	++this->state;
	this->tick = -1;
	this->Tick();
}

bool Wedding::Check()
{
	if (!this->GetPartner1() || !this->GetPartner2() || !this->GetPriest())
	{
		this->ErrorOut();
		return false;
	}

	return true;
}

void Wedding::Reset()
{
	this->partner1.clear();
	this->partner2.clear();
	this->state = 0;
	this->tick = 0;

	this->StopTimer();
}

void Wedding::ErrorOut()
{
	this->PriestSay(this->map->world->i18n.Format("wedding_error"));
	this->Reset();
}

Wedding::Wedding(Map *map, unsigned char priest_idx)
	: priest_idx(priest_idx)
	, state(0)
	, tick(0)
	, tick_timer(nullptr)
	, map(map)
{ }

void Wedding::Tick()
{
	if (state != 0)
		++this->tick;

	switch (state)
	{
		case 0:
			this->StopTimer();
			return;

		case 1:
			if (this->tick == 0)
			{
				this->PriestSay(this->map->world->i18n.Format("wedding_wait"));
			}
			else if (this->tick == 20)
			{
				this->NextState();
			}
			break;

		case 2:
			// This is called directly after players are checked for validity already
			if (this->tick == 0)
			{
				this->PriestSay(this->map->world->i18n.Format(
					"wedding_text1",
					this->GetPartner1()->SourceName(),
					this->GetPartner2()->SourceName()
				));
			}
			else if (this->tick == 6)
			{
				this->NextState();
			}
			break;

		case 3:
			if (this->tick == 0)
			{
				this->PriestSay(this->map->world->i18n.Format("wedding_text2"));
			}
			else if (this->tick == 6)
			{
				this->NextState();
			}
			break;

		case 4:
			// This is called directly after players are checked for validity already
			if (this->tick == 0)
			{
				this->PriestSay(this->map->world->i18n.Format(
					"wedding_doyou",
					this->GetPartner1()->SourceName(),
					this->GetPartner2()->SourceName()
				));
			}
			else if (this->tick == 3)
			{
				if (this->Check())
				{
					PacketBuilder builder(PACKET_PRIEST, PACKET_REPLY, 4);
					builder.AddShort(PRIEST_DO_YOU);
					this->GetPartner1()->Send(builder);
				}
			}
			else if (this->tick == 13)
			{
				this->ErrorOut();
			}
			break;

		case 5:
			if (this->tick == 6)
			{
				this->NextState();
			}
			break;

		case 6:
			// This is called directly after players are checked for validity already
			if (this->tick == 0)
			{
				this->PriestSay(this->map->world->i18n.Format(
					"wedding_doyou",
					this->GetPartner2()->SourceName(),
					this->GetPartner1()->SourceName()
				));
			}
			else if (this->tick == 3)
			{
				if (this->Check())
				{
					PacketBuilder builder(PACKET_PRIEST, PACKET_REPLY, 4);
					builder.AddShort(PRIEST_DO_YOU);
					this->GetPartner2()->Send(builder);
				}
			}
			else if (this->tick == 13)
			{
				this->ErrorOut();
			}
			break;

		case 7:
			// This is called directly after players are checked for validity already
			if (this->tick == 0)
			{
				Character* p1 = this->GetPartner1();
				Character* p2 = this->GetPartner2();

				p1->partner = p2->SourceName();
				p2->partner = p1->SourceName();

				p1->fiance.clear();
				p2->fiance.clear();

				short ring_id = short(int(this->map->world->config["WeddingRing"]));

				if (ring_id)
				{
					if (p1->AddItem(ring_id, 1))
					{
						PacketBuilder reply(PACKET_ITEM, PACKET_GET, 9);
						reply.AddShort(0); // UID
						reply.AddShort(ring_id);
						reply.AddThree(1);
						reply.AddChar(p1->weight);
						reply.AddChar(p1->maxweight);
						p1->Send(reply);
					}

					if (p2->AddItem(ring_id, 1))
					{
						PacketBuilder reply(PACKET_ITEM, PACKET_GET, 9);
						reply.AddShort(0); // UID
						reply.AddShort(ring_id);
						reply.AddThree(1);
						reply.AddChar(p1->weight);
						reply.AddChar(p1->maxweight);
						p2->Send(reply);
					}
				}
			}
			if (this->tick == 6)
			{
				this->NextState();
			}
			break;

		case 8:
			if (this->tick == 0)
			{
				this->PriestSay(this->map->world->i18n.Format("wedding_ring1"));
			}
			else if (this->tick == 6)
			{
				this->NextState();
			}
			break;

		case 9:
			if (this->tick == 0)
			{
				this->PriestSay(this->map->world->i18n.Format("wedding_ring2"));
			}
			else if (this->tick == 5)
			{
				this->NextState();
			}
			break;

		case 10:
			// This is called directly after players are checked for validity already
			if (this->tick == 0)
			{
				Character* p1 = this->GetPartner1();
				Character* p2 = this->GetPartner2();

				if (p1) p1->Effect(1); // hearts
				if (p2) p2->Effect(1); // hearts
			}
			else if (this->tick == 1)
			{
				this->NextState();
			}
			break;

		case 11:
			// This is called directly after players are checked for validity already
			if (this->tick == 0)
			{
				Character* p1 = this->GetPartner1();
				Character* p2 = this->GetPartner2();

				this->PriestSay(this->map->world->i18n.Format(
					"wedding_finish1",
					p1->SourceName(),
					p2->SourceName()
				));

				// FIXME: This is supposed to use map coordinate effect
				p1->Effect(11); // fizzy
				p2->Effect(11); // fizzy
			}
			else if (this->tick == 5)
			{
				this->NextState();
			}
			break;

		case 12:
			if (this->tick == 0)
			{
				this->PriestSay(this->map->world->i18n.Format("wedding_finish2"));
				this->Reset();
			}
			break;

		default:
			this->ErrorOut();
	}
}

bool Wedding::StartWedding(const std::string& player1, const std::string& player2)
{
	if (this->state > 0)
	{
		return false;
	}

	this->partner1 = player1;
	this->partner2 = player2;
	this->NextState();

	this->Tick();
	this->StartTimer();

	return true;
}

bool Wedding::Busy()
{
	return this->state != 0;
}

void Wedding::IDo(Character* character)
{
	int required_state = 0;

	if (character->SourceName() == partner1)
		required_state = 4;
	else if (character->SourceName() == partner2)
		required_state = 6;

	if (required_state == 0 || this->state != required_state)
		return;

	if (character->map != this->map)
		return;

	character->map->Msg(character, this->map->world->i18n.Format("wedding_ido"), true);

	this->NextState();
}

Wedding::~Wedding()
{
	this->StopTimer();
}
