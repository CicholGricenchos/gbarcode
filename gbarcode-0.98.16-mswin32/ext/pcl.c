/*
 * pcl.c -- printing the "partial" bar encoding in PCL format
 *
 * Copyright (c) 1999 Alessandro Rubini (rubini@gnu.org)
 * Copyright (c) 1999 Prosa Srl. (prosa@prosa.it)
 * Copyright (c) 2001 Andrea Scopece (a.scopece@tin.it)
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "barcode.h"

#define SHRINK_AMOUNT 0.15  /* shrink the bars to account for ink spreading */


/*
 * How do the "partial" and "textinfo" strings work? See file "ps.c"
 */


int Barcode_pcl_print(struct Barcode_Item *bc, FILE *f)
{
    int i, j, k, barlen;
    double f1, f2, fsav=0;
    int mode = '-'; /* text below bars */
    double scalef=1, xpos, x0, y0, yr;
    unsigned char *ptr;
    unsigned char c;

    char font_id[6];           /* default font, should be "scalable" */
    /* 0     Line printer,    use on older LJet II, isn't scalable   */
    /* 4148  Univers,         use on LJet III series, and Lj 4L, 5L  */
    /* 16602 Arial,           default LJ family 4, 5, 6, Color, Djet */

    if (!bc->partial || !bc->textinfo) {
	bc->error = EINVAL;
	return -1;
    }

    /*
     * Maybe this first part can be made common to several printing back-ends,
     * we'll see how that works when other ouput engines are added
     */

    /* First, calculate barlen */
    barlen = bc->partial[0] - '0';
    for (ptr = bc->partial+1; *ptr; ptr++)
	if (isdigit(*ptr)) 
	    barlen += (*ptr - '0');
	else if (islower(*ptr))
	    barlen += (*ptr - 'a'+1);

    /* The scale factor depends on bar length */
    if (!bc->scalef) {
        if (!bc->width) bc->width = barlen; /* default */
        scalef = bc->scalef = (double)bc->width / (double)barlen;
    }

    /* The width defaults to "just enough" */
    if (!bc->width) bc->width = barlen * scalef +1;

    /* But it can be too small, in this case enlarge and center the area */
    if (bc->width < barlen * scalef) {
        int wid = barlen * scalef + 1;
        bc->xoff -= (wid - bc->width)/2 ;
        bc->width = wid;
        /* Can't extend too far on the left */
        if (bc->xoff < 0) {
            bc->width += -bc->xoff;
            bc->xoff = 0;
        }
    }

    /* The height defaults to 80 points (rescaled) */
    if (!bc->height) bc->height = 80 * scalef;

#if 0
    /* If too small (5 + text), enlarge and center */
    i = 5 + 10 * ((bc->flags & BARCODE_NO_ASCII)==0);
    if (bc->height < i * scalef ) {
        int hei = i * scalef;
        bc->yoff -= (hei-bc->height)/2;
        bc->height = hei;
        if (bc->yoff < 0) {
            bc->height += -bc->yoff;
            bc->yoff = 0;
        }
    }
#else
    /* If too small (5 + text), reduce the scale factor and center */
    i = 5 + 10 * ((bc->flags & BARCODE_NO_ASCII)==0);
    if (bc->height < i * scalef ) {
        double scaleg = ((double)bc->height) / i;
        int wid = bc->width * scaleg / scalef;
        bc->xoff += (bc->width - wid)/2;
        bc->width = wid;
        scalef = scaleg;
    }
#endif

    /*
     * deal with PCL output
     */

    xpos = bc->margin + (bc->partial[0]-'0') * scalef;
    for (ptr = bc->partial+1, i=1; *ptr; ptr++, i++) {
	/* special cases: '+' and '-' */
	if (*ptr == '+' || *ptr == '-') {
	    mode = *ptr; /* don't count it */ i++; continue;
	}

	/* j is the width of this bar/space */
	if (isdigit (*ptr))   j = *ptr-'0';
	else                  j = *ptr-'a'+1;
	if (i%2) { /* bar */
	    x0 = bc->xoff + xpos;
            y0 = bc->yoff + bc->margin;
            yr = bc->height;
            if (!(bc->flags & BARCODE_NO_ASCII)) { /* leave space for text */
		if (mode == '-') {
		    /* text below bars: 10 points or five points */
		    yr -= (isdigit(*ptr) ? 10 : 5) * scalef;
		} else { /* '+' */
		    /* text above bars: 10 or 0 from bottom, and 10 from top */
		    y0 += (isdigit(*ptr) ? 10 : 0) * scalef;
		    yr -= (isdigit(*ptr) ? 20 : 10) * scalef; 
		}
	    }

	    fprintf(f,"%c&a%.0fH", 27, x0 * 10.0);
	    fprintf(f,"%c&a%.0fV", 27, y0 * 10.0);
	    fprintf(f,"%c*c%.0fH", 27, ((j*scalef)-SHRINK_AMOUNT) * 10.0);
	    fprintf(f,"%c*c%.0fV", 27, yr * 10.0);
	    fprintf(f,"%c*c0P\n", 27);
	}
	xpos += j * scalef;
    }

    /* the text */

    mode = '-'; /* reinstantiate default */
    if (!(bc->flags & BARCODE_NO_ASCII)) {
        k=0; /* k is the "previous font size" */
        for (ptr = bc->textinfo; ptr; ptr = strchr(ptr, ' ')) {
            while (*ptr == ' ') ptr++;
            if (!*ptr) break;
	    if (*ptr == '+' || *ptr == '-') {
		mode = *ptr; continue;
	    }
            if (sscanf(ptr, "%lf:%lf:%c", &f1, &f2, &c) != 3) {
		fprintf(stderr, "barcode: impossible data: %s\n", ptr);
                continue;
            }

    /* select a Scalable Font */

	    if (fsav != f2)
	    {   
    	        if ((bc->flags & BARCODE_OUT_PCL_III) == BARCODE_OUT_PCL_III)
		{	strcpy(font_id, "4148");	/* font Univers */
		}
		else
		{	strcpy(font_id, "16602");	/* font Arial */
		}

		fprintf(f,"%c(8U%c(s1p%5.2fv0s0b%sT", 27, 27, f2 * scalef, font_id);
	    }
	    fsav = f2;
	
	    fprintf(f,"%c&a%.0fH", 27, (bc->xoff + f1 * scalef + bc->margin) * 10.0);
	    fprintf(f,"%c&a%.0fV", 27,
		    mode != '-'
                       ? ((double)bc->yoff + bc->margin              + 8*scalef) * 10.0
		       : ((double)bc->yoff + bc->margin + bc->height           ) * 10.0);

		fprintf(f, "%c", c);
	}

    }

    return 0;
}
