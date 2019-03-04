/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / software 2D rasterizer module
 *
 *  GPAC is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  GPAC is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 *
 */

#include "rast_soft.h"

static s32
mul255(s32 a, s32 b)
{
	return ((a + 1) * b) >> 8;
}


/*
			RGB part
*/

static void overmask_rgb(u32 src, char *dst, u32 alpha, GF_EVGSurface *surf)
{
	s32 srca = (src >> 24) & 0xff;
	u32 srcr = (src >> 16) & 0xff;
	s32 srcg = (src >> 8) & 0xff;
	s32 srcb = (src) & 0xff;

	s32 dstr = dst[surf->idx_r] & 0xFF;
	s32 dstg = dst[surf->idx_g] & 0xFF;
	s32 dstb = dst[surf->idx_b] & 0xFF;

	srca = mul255(srca, alpha);
	dst[surf->idx_r] = mul255(srca, srcr - dstr) + dstr;
	dst[surf->idx_g] = mul255(srca, srcg - dstg) + dstg;
	dst[surf->idx_b] = mul255(srca, srcb - dstb) + dstb;
}

static void overmask_rgb_const_run(u32 src, char *dst, s32 dst_pitch_x, u32 count, GF_EVGSurface *surf)
{
	u8 srca = (src >> 24) & 0xff;
	u8 srcr = (src >> 16) & 0xff;
	u8 srcg = (src >> 8) & 0xff;
	u8 srcb = (src) & 0xff;

	while (count) {
		u8 dstr = dst[surf->idx_r];
		u8 dstg = dst[surf->idx_g];
		u8 dstb = dst[surf->idx_b];
		dst[surf->idx_r] = (u8) mul255(srca, srcr - dstr) + dstr;
		dst[surf->idx_g] = (u8) mul255(srca, srcg - dstg) + dstg;
		dst[surf->idx_b] = (u8) mul255(srca, srcb - dstb) + dstb;
		dst += dst_pitch_x;
		count--;
	}
}

void evg_rgb_fill_const(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf)
{
	u32 col = surf->fill_col;
	u32 a, fin, col_no_a;
	char *dst = surf->pixels + y * surf->pitch_y;
	char *p;
	s32 i;
	u32 len, r, g, b;

	r = GF_COL_R(col);
	g = GF_COL_G(col);
	b = GF_COL_B(col);

	col_no_a = col & 0x00FFFFFF;
	for (i=0; i<count; i++) {
		len = spans[i].len;
		p = dst + spans[i].x * surf->pitch_x;

		if (spans[i].coverage != 0xFF) {
			a = mul255(0xFF, spans[i].coverage);
			fin = (a<<24) | col_no_a;
			overmask_rgb_const_run(fin, p, surf->pitch_x, len, surf);
		} else {
			while (len--) {
				p[surf->idx_r] = r;
				p[surf->idx_g] = g;
				p[surf->idx_b] = b;
				p += surf->pitch_x;
			}
		}
	}
}

void evg_rgb_fill_const_a(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf)
{
	char *dst = surf->pixels + y * surf->pitch_y;
	u32 col = surf->fill_col;
	u32 a, fin;
	s32 i;

	a = (col>>24)&0xFF;
	for (i=0; i<count; i++) {
		fin = mul255(a, spans[i].coverage);
		fin = (fin<<24) | (col&0x00FFFFFF);
		overmask_rgb_const_run(fin, dst + surf->pitch_x * spans[i].x, surf->pitch_x, spans[i].len, surf);
	}
}


void evg_rgb_fill_var(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf)
{
	char *dst = surf->pixels + y * surf->pitch_y;
	u8 spanalpha, col_a;
	s32 i;
	s32 x;
	u32 len;
	u32 *col;

	for (i=0; i<count; i++) {
		len = spans[i].len;
		spanalpha = spans[i].coverage;
		surf->sten->fill_run(surf->sten, surf, spans[i].x, y, len);
		col = surf->stencil_pix_run;
		x = surf->pitch_x * spans[i].x;
		while (len--) {
			col_a = GF_COL_A(*col);
			if (col_a) {
				if ((spanalpha!=0xFF) || (col_a != 0xFF)) {
					overmask_rgb(*col, dst + x, spanalpha, surf);
				} else {
					dst[x + surf->idx_r] = GF_COL_R(*col);
					dst[x + surf->idx_g] = GF_COL_G(*col);
					dst[x + surf->idx_b] = GF_COL_B(*col);
				}
			}
			col++;
			x += surf->pitch_x;
		}
	}
}

GF_Err evg_surface_clear_rgb(GF_EVGSurface *surf, GF_IRect rc, GF_Color col)
{
	u32 x, y, w, h, sx, sy;
	s32 st;
	u8 r, g, b;
	char *o_data;
	GF_EVGSurface *_this = (GF_EVGSurface *)surf;
	st = _this->pitch_y;

	h = rc.height;
	w = rc.width;
	sx = rc.x;
	sy = rc.y;

	r = GF_COL_R(col);
	g = GF_COL_G(col);
	b = GF_COL_B(col);
	o_data = NULL;
	for (y = 0; y < h; y++) {
		char *data = _this ->pixels + (y + sy) * st + _this->pitch_x*sx;
		if (!y) {
			o_data = data;
			for (x = 0; x < w; x++) {
				data[surf->idx_r] = r;
				data[surf->idx_g] = g;
				data[surf->idx_b] = b;
				data += _this->pitch_x;
			}
		} else {
			memcpy(data, o_data, w*3);
		}
	}
	return GF_OK;
}




/*
			grey part
*/

static void overmask_grey(u32 src, char *dst, u32 alpha, u32 grey_type)
{
	s32 srca = (src >> 24) & 0xff;
	u32 srcc;
	s32 dstc = *dst;

	if (grey_type==0) srcc = (src >> 16) & 0xff;
	else if (grey_type==1) srcc = (src >> 8) & 0xff;
	else srcc = (src) & 0xff;


	srca = mul255(srca, alpha);
	*dst = mul255(srca, srcc - dstc) + dstc;
}

static void overmask_grey_const_run(u8 srca, u8 srcc, char *dst, s32 dst_pitch_x, u32 count)
{
	while (count) {
		u8 dstc = *(dst);
		*dst = (u8) mul255(srca, srcc - dstc) + dstc;
		dst += dst_pitch_x;
		count--;
	}
}

void evg_grey_fill_const(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf)
{
	u32 col = surf->fill_col;
	u32 a, c, col_no_a;
	char *dst = surf->pixels + y * surf->pitch_y;
	char *p;
	s32 i;
	u32 len;

	if (surf->grey_type==0) c = GF_COL_R(col);
	else if (surf->grey_type==1) c = GF_COL_G(col);
	else c = GF_COL_B(col);

	col_no_a = col & 0x00FFFFFF;
	for (i=0; i<count; i++) {
		len = spans[i].len;
		p = dst + spans[i].x * surf->pitch_x;

		if (spans[i].coverage != 0xFF) {
			a = mul255(0xFF, spans[i].coverage);
			overmask_grey_const_run(a, c, p, surf->pitch_x, len);
		} else {
			while (len--) {
				*(p) = c;
				p += surf->pitch_x;
			}
		}
	}
}

void evg_grey_fill_const_a(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf)
{
	char *dst = surf->pixels + y * surf->pitch_y;
	u32 a, fin;
	s32 i;
	u8 c;

	if (surf->grey_type==0) c = GF_COL_R(surf->fill_col);
	else if (surf->grey_type==1) c = GF_COL_G(surf->fill_col);
	else c = GF_COL_B(surf->fill_col);

 	a = GF_COL_A(surf->fill_col);

	for (i=0; i<count; i++) {
		fin = mul255(a, spans[i].coverage);
		overmask_grey_const_run(a, c, dst + surf->pitch_x * spans[i].x, surf->pitch_x, spans[i].len);
	}
}


void evg_grey_fill_var(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf)
{
	char *dst = surf->pixels + y * surf->pitch_y;
	u8 spanalpha, col_a;
	s32 i;
	s32 x;
	u32 len;
	u32 *col;

	for (i=0; i<count; i++) {
		len = spans[i].len;
		spanalpha = spans[i].coverage;
		surf->sten->fill_run(surf->sten, surf, spans[i].x, y, len);
		col = surf->stencil_pix_run;
		x = surf->pitch_x * spans[i].x;
		while (len--) {
			col_a = GF_COL_A(*col);
			if (col_a) {
				if ((spanalpha!=0xFF) || (col_a != 0xFF)) {
					overmask_grey(*col, dst + x, spanalpha, surf->grey_type);
				} else {
					u8 c;

					if (surf->grey_type==0) c = GF_COL_R(*col);
					else if (surf->grey_type==1) c = GF_COL_G(*col);
					else c = GF_COL_B(*col);

					*(dst + x) = GF_COL_R(*col);
				}
			}
			col++;
			x += surf->pitch_x;
		}
	}
}

GF_Err evg_surface_clear_grey(GF_EVGSurface *surf, GF_IRect rc, GF_Color col)
{
	u32 y, w, h, sx, sy;
	s32 st;
	u8 r;
	st = surf->pitch_y;

	h = rc.height;
	w = rc.width;
	sx = rc.x;
	sy = rc.y;

	if (surf->grey_type==0) r = GF_COL_R(col);
	else if (surf->grey_type==0) r = GF_COL_G(col);
	else r = GF_COL_B(col);

	for (y = 0; y < h; y++) {
		char *data = surf ->pixels + (y + sy) * st + surf->pitch_x*sx;
		memset(data, r, surf->pitch_y);
	}
	return GF_OK;
}