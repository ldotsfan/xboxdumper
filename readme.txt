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

xboxdumper v0.13

This program allows files on an xbox hard disk to be dumped out.


Two basic commands are supported:


xboxdumper list <partition number> <xbox image filename>

This will dump the directory tree of the specified partition.

(e.g. "./xboxdumper.sh list 1 xboximage.bin" )




xboxdumper dump <xbox filename> <output filename> <partition number> <image filename>

This will dump the file <xbox filename> into the file <output filename> 
from the specified partition number. The full directory path to the file 
using / or \ should be supplied.

(e.g. "./xboxdumper.sh dump /voice.afs voice.afs 1 xboximage.bin" )


<partition number> may be between 0 and 4 inclusively. Partition 0 is not 
confirmed yet.

I will not supply images of the xbox hard disk, so please don't bother 
asking.
