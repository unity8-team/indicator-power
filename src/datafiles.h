/*
 * Copyright 2016 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3, as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranties of
 * MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Charles Kerr <charles.kerr@canonical.com>
 */

#ifndef __INDICATOR_POWER_DATAFILES_H__
#define __INDICATOR_POWER_DATAFILES_H__

#include <gio/gio.h>

G_BEGIN_DECLS

typedef enum 
{
  DATAFILE_TYPE_SOUND
}
DatafileType;

gchar* datafile_find(DatafileType type, const char * basename);

G_END_DECLS

#endif /* __INDICATOR_POWER_DATAFILES_H__ */
