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

#include <stddef.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include "dir.h"

DIR_ENTRY *add_entry(DIR_ENTRY *root,DIR_ENTRY *add) {

	DIR_ENTRY *next;
	

	if(add == NULL) return NULL;

	if(root == NULL) {
		root = (DIR_ENTRY *)malloc(sizeof(DIR_ENTRY));
		memset(root,0,sizeof(DIR_ENTRY));
		memcpy(root,add,sizeof(DIR_ENTRY));
	} else {
		next = root;
		while(next->next != NULL) {
			next = next->next;
		}
		next->next = (DIR_ENTRY *)malloc(sizeof(DIR_ENTRY));
		memset(next->next,0,sizeof(DIR_ENTRY));
		memcpy(next->next,add,sizeof(DIR_ENTRY));
	}

	return root;

	
}

DIR_ENTRY *free_list(DIR_ENTRY *root) {
	
	DIR_ENTRY *next,*delete;
	
	if(root == NULL) return NULL;
	
	next = delete = root;
	while(next->next != NULL) {
		delete = next;
		next = next->next;
		free(delete);
	}
	free(next);

	return NULL;
}

int find(char *szStartDir,char *szNextDir,DIR_ENTRY **list) {
	DIR *dp;
	struct dirent *ep;
	char szDir[20000];
	DIR_ENTRY add;

	if(szNextDir != NULL) {
		sprintf(szDir,"%s/%s",szStartDir,szNextDir);
	} else {
		sprintf(szDir,"%s",szStartDir);
	}
	dp = opendir(szDir);
	if (dp != NULL) {
		while ((ep = readdir(dp))) {
			memset(&add,0,sizeof(DIR_ENTRY));
			if(strcmp(ep->d_name,".") == 0 || strcmp(ep->d_name,"..") == 0) {
				continue;
			}
			sprintf(add.szFileName,"%s/%s",szDir,ep->d_name);
			stat(add.szFileName,&add.sp);
			*list = add_entry(*list,&add);
			if(ep->d_type == 4) {
				find(szDir,ep->d_name,list);
			}
		}
		(void) closedir (dp);
	}
	return 0;
}

