/*
    Xboxdumper - FATX library and utilities.

    Copyright (C) 2005 Andrew de Quincey <adq_dvb@lidskialf.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

// Utility functions

#ifndef UTIL_H
#define UTIL_H 1

#include <sys/types.h>

/**
 * Structure to contain a DOS date and timestamp
 */
typedef struct {
  int year;
  int month;
  int day;
  int hours;
  int mins;
  int secs;
} DosDateTime;

/**
 * Report an error
 */
void error(char *fmt, ...);

/**
 * Load a DOS date and time stamp
 *
 * @param dateTime Place to put data
 * @param date Raw DOS date value
 * @param time Raw DOS time value
 */
void loadDosDateTime(DosDateTime* dateTime, 
                     u_int16_t date, u_int16_t time);



/**
 * Format a DOSDateTime for printing
 *
 * @param dateTime DosDateTime to format
 *
 * @return Formatted string
 */
char* formatDosDate(DosDateTime* dateTime);

#endif

