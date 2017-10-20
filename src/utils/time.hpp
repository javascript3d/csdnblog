//  SuperTuxKart - a fun racing game with go-kart
//
//  Copyright (C) 2013-2015  SuperTuxKart-Team
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

#ifndef HEADER_TIME_HPP
#define HEADER_TIME_HPP

#include "ITimer.h"

#include <stdexcept>

#ifdef WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <time.h>
#else
#  include <stdint.h>
#  include <sys/time.h>
#  include <unistd.h>
#endif

#include <string>
#include <stdio.h>

class StkTime
{
private:
    /** This objects keeps a copy of irrlicht's null-device timer. This is
    *  important otherwise we can't get the time when resolution is switched
    *  (and the sfx threads needs real time at that time). */
    static irr::ITimer *m_timer;

public:
    typedef time_t TimeType;

    static void init();
    static void getDate(int *day=NULL, int *month=NULL, int *year=NULL);

    /** Converts the time in this object to a human readable string. */
    static std::string toString(const TimeType &tt);
    // ------------------------------------------------------------------------
    /** Returns the number of seconds since 1.1.1970. This function is used
     *  to compare access times of files, e.g. news, addons data etc.
     */
    static TimeType getTimeSinceEpoch()
    {
#ifdef WIN32
        FILETIME ft;
        GetSystemTimeAsFileTime(&ft);
        __int64 t = ft.dwHighDateTime;
        t <<= 32;
        t /= 10;
        // The Unix epoch starts on Jan 1 1970.  Need to subtract
        // the difference in seconds from Jan 1 1601.
#       if defined(_MSC_VER) || defined(_MSC_EXTENSIONS)
#           define DELTA_EPOCH_IN_MICROSECS  11644473600000000Ui64
#       else
#           define DELTA_EPOCH_IN_MICROSECS  11644473600000000ULL
#       endif
        t -= DELTA_EPOCH_IN_MICROSECS;

        t |= ft.dwLowDateTime;
        // Convert to seconds since epoch
        t /= 1000000UL;
        return t;
#else
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return tv.tv_sec;
#endif
    };   // getTimeSinceEpoch

    // ------------------------------------------------------------------------
    /** Returns a time based on an arbitrary 'epoch' (e.g. could be start
     *  time of the application, 1.1.1970, ...).
     *  The value is a double precision floating point value in seconds.
     */
    static double getRealTime(long startAt=0);

    // ------------------------------------------------------------------------
    /**
     * \brief Compare two different times.
     * \return A signed integral indicating the relation between the time.
     */
    static int compareTime(TimeType time1, TimeType time2)
    {
        double diff = difftime(time1, time2);

        if (diff > 0)
            return 1;
        else if (diff < 0)
            return -1;
        else
            return 0;
    };   // compareTime

    // ------------------------------------------------------------------------
    /** Sleeps for the specified amount of time.
     *  \param msec Number of milliseconds to sleep.
     */
    static void sleep(int msec)
    {
#ifdef WIN32
        Sleep(msec);
#else
        usleep(msec*1000);
#endif
    }   // sleep
    // ------------------------------------------------------------------------
    /**
     * \brief Add a interval to a time.
     */
    static TimeType addInterval(TimeType time, int year, int month, int day) {
        struct tm t = *gmtime(&time);
        t.tm_year += year;
        t.tm_mon += month;
        t.tm_mday += day;
        return mktime(&t);
    }   // addInterval

    // ------------------------------------------------------------------------
    class ScopeProfiler
    {
        float m_time;
    public:
        ScopeProfiler(const char* name)
        {
            printf("%s {\n", name);
            m_time = (float)getRealTime();
        }

        ~ScopeProfiler()
        {
            float f2 = (float)getRealTime();
            printf("} // took %f s\n", (f2 - m_time));
        }
    };   // class ScopeProfiler

};   // namespace time
#endif

