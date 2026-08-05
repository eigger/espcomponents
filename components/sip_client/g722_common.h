/*
 * g722_common.h - The ITU G.722 codec, common functions.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2005 Steve Underwood
 *
 * All rights reserved.
 *
 *  Despite my general liking of the GPL, I place my own contributions 
 *  to this code in the public domain for the benefit of all mankind -
 *  even the slimy ones who might try to proprietize my work and use it
 *  to my detriment.
 *
 * Based on a single channel 64kbps only G.722 codec which is:
 *
 *****    Copyright (c) CMU    1993      *****
 * Computer Science, Speech Group
 * Chengxiang Lu and Alex Hauptmann
 *
 * The Carnegie Mellon ADPCM program is Copyright (c) 1993 by Carnegie Mellon
 * University. Use of this program, for any research or commercial purpose, is
 * completely unrestricted. If you make use of or redistribute this material,
 * we would appreciate acknowlegement of its origin.
 *****
 */

#pragma once

#if !defined(FALSE)
#define FALSE 0
#endif
#if !defined(TRUE)
#define TRUE (!FALSE)
#endif

static inline int16_t saturate(int32_t amp)
{
    int16_t amp16;

    /* Hopefully this is optimised for the common case - not clipping */
    amp16 = (int16_t) amp;
    if (amp == amp16)
        return amp16;
    if (amp > INT16_MAX)
        return  INT16_MAX;
    return  INT16_MIN;
}
/*- End of function --------------------------------------------------------*/

static inline void block4(struct g722_band *band, int d)
{
    int wd1;
    int wd2;
    int wd3;
    int i;

    /* Block 4, RECONS */
    band->d[0] = d;
    band->r[0] = saturate(band->s + d);

    /* Block 4, PARREC */
    band->p[0] = saturate(band->sz + d);

    /* Block 4, UPPOL2 */
    for (i = 0;  i < 3;  i++)
        band->sg[i] = band->p[i] >> 15;
    wd1 = saturate(band->a[1] << 2);

    wd2 = (band->sg[0] == band->sg[1])  ?  -wd1  :  wd1;
    if (wd2 > 32767)
        wd2 = 32767;
    wd3 = (wd2 >> 7) + ((band->sg[0] == band->sg[2])  ?  128  :  -128);
    wd3 += (band->a[2]*32512) >> 15;
    if (wd3 > 12288)
        wd3 = 12288;
    else if (wd3 < -12288)
        wd3 = -12288;
    band->ap[2] = wd3;

    /* Block 4, UPPOL1 */
    band->sg[0] = band->p[0] >> 15;
    band->sg[1] = band->p[1] >> 15;
    wd1 = (band->sg[0] == band->sg[1])  ?  192  :  -192;
    wd2 = (band->a[1]*32640) >> 15;

    band->ap[1] = saturate(wd1 + wd2);
    wd3 = saturate(15360 - band->ap[2]);
    if (band->ap[1] > wd3)
        band->ap[1] = wd3;
    else if (band->ap[1] < -wd3)
        band->ap[1] = -wd3;

    /* Block 4, UPZERO */
    wd1 = (d == 0)  ?  0  :  128;
    band->sg[0] = d >> 15;
    for (i = 1;  i < 7;  i++)
    {
        band->sg[i] = band->d[i] >> 15;
        wd2 = (band->sg[i] == band->sg[0])  ?  wd1  :  -wd1;
        wd3 = (band->b[i]*32640) >> 15;
        band->bp[i] = saturate(wd2 + wd3);
    }

    /* Block 4, DELAYA */
    for (i = 6;  i > 0;  i--)
    {
        band->d[i] = band->d[i - 1];
        band->b[i] = band->bp[i];
    }
    
    for (i = 2;  i > 0;  i--)
    {
        band->r[i] = band->r[i - 1];
        band->p[i] = band->p[i - 1];
        band->a[i] = band->ap[i];
    }

    /* Block 4, FILTEP */
    wd1 = saturate(band->r[1] + band->r[1]);
    wd1 = (band->a[1]*wd1) >> 15;
    wd2 = saturate(band->r[2] + band->r[2]);
    wd2 = (band->a[2]*wd2) >> 15;
    band->sp = saturate(wd1 + wd2);

    /* Block 4, FILTEZ */
    band->sz = 0;
    for (i = 6;  i > 0;  i--)
    {
        wd1 = saturate(band->d[i] + band->d[i]);
        band->sz += (band->b[i]*wd1) >> 15;
    }
    band->sz = saturate(band->sz);

    /* Block 4, PREDIC */
    band->s = saturate(band->sp + band->sz);
}
/*- End of function --------------------------------------------------------*/
