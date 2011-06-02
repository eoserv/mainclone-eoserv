
/* $Id$
 * EOSERV is released under the zlib license.
 * See LICENSE.txt for more info.
 */

#ifndef TIMER_HPP_INCLUDED
#define TIMER_HPP_INCLUDED

#include "fwd/timer.hpp"

#include <set>

/**
 * Manages and calls TimerEvent objects
 */
class Timer
{
	protected:
		/**
		 * List of TimeEvent objects a Timer controls
		 */
		std::set<TimeEvent *> timers;

		/**
		 * Copy of timers which is used to allow timers to unregister themselves safely
		 */
		std::set<TimeEvent *> execlist;

		/**
		 * Used to track when execlist needs to be remade
		 */
		bool changed;

#ifdef WIN32
		static bool use_gtc;
#endif // WIN32

	public:
		/**
		 * TimeEvent lifetime that will never expire
		 */
		static const int FOREVER = -1;

		double resolution;

		Timer();

		/**
		 * Helper function to get the current time in seconds
		 * This is not guaranteed to start at any particular number, or be a UNIX timestamp
		 */
		static double GetTime();

		/**
		 * Check all contained TimeEvent objects and call any which are ready
		 */
		void Tick();

		/**
		 * Register a TimeEvent object with the Timer object
		 */
		void Register(TimeEvent *);

		/**
		 * Unregister a TimeEvent object with the Timer object
		 */
		void Unregister(TimeEvent *);

		/**
		 * Delete any remaining autofree TimeEvent objects
		 */
		~Timer();
};

/**
 * A timed event that should be managed by a Timer object
 */
struct TimeEvent
{
	/**
	 * Pointer to the Timer object that owns it
	 * Set once it has been passed to Timer::Register
	 */
	Timer *manager;

	/**
	 * Function that is called each tick of the timer
	 */
	TimerCallback callback;

	/**
	 * Parameter that's passed to the callback function
	 */
	void *param;

	/**
	 * Time between ticks in seconds
	 */
	double speed;

	/**
	 * Time that the event last "ticked"
	 */
	double lasttime;

	/**
	 * Number of ticks before the Timer will stop calling it
	 */
	int lifetime;

	/**
	 * Construct a new TimeEvent object
	 */
	TimeEvent(TimerCallback callback, void *param, double speed, int lifetime = 1);

	/**
	 * Unregister the object from it's owning Timer object if it has one
	 */
	~TimeEvent();
};

#endif // TIMER_HPP_INCLUDED
