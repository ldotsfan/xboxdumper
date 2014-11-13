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

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include "util.h"

/**
 * Report an error
 */
void error(char *fmt, ...) {
  va_list argp;
  fprintf(stderr, "error: ");
  va_start(argp, fmt);
  vfprintf(stderr, fmt, argp);
  va_end(argp);
  fprintf(stderr, "\n");
  exit(1);
}


/**
 * Load a DOS date and time stamp
 *
 * @param dateTime Place to put data
 * @param date Raw DOS date value
 * @param time Raw DOS time value
 */
void loadDosDateTime(DosDateTime* dateTime, 
                     u_int16_t date, u_int16_t time) {
  dateTime->day = date & 0x1f;
  dateTime->month = (date & 0x1e0) >> 5;
  dateTime->year = ((date & 0xfe00) >> 9) + 1980;
  dateTime->secs = (time & 0x1f) * 2;
  dateTime->mins = (time & 0x7e0) >> 5;
  dateTime->hours = (time & 0xf800) >> 11;
}



/**
 * Format a DOSDateTime for printing
 *
 * @param dateTime DosDateTime to format
 *
 * @return Formatted string
 */
char* formatDosDate(DosDateTime* dateTime) {
  static char formatDosDateSTORE[256];
  
  sprintf(formatDosDateSTORE, "%02i:%02i:%02i-%i/%i/%i",
          dateTime->hours, dateTime->mins, dateTime->secs, 
          dateTime->day, dateTime->month, dateTime->year);
  return formatDosDateSTORE;
}
