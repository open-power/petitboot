/*
 *  cfg.h - config file parsing definitions
 *
 *  Copyright (C) 1999 Benjamin Herrenschmidt
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef CFG_H
#define CFG_H

extern int	cfg_parse(char *cfg_file, char *buff, int len);
extern char*	cfg_get_strg(char *image, char *item);
extern int	cfg_get_flag(char *image, char *item);
extern void	cfg_print_images(void);
extern char*	cfg_get_default(void);
extern char*	cfg_next_image(char *);
#endif
