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

#ifndef _DIR_H_
#define _DIR_H_

typedef struct DIR_ENTRY {
        char szFileName[2000];
        struct DIR_ENTRY *next;
        struct stat sp;
} DIR_ENTRY;

DIR_ENTRY *add_entry(DIR_ENTRY *root,DIR_ENTRY *add);
DIR_ENTRY *free_list(DIR_ENTRY *root);
int find(char *szStartDir,char *szNextDir,DIR_ENTRY **list);

#endif //       _DIR_H_
