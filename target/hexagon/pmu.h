/*
 *
 *  Copyright(c) 2022 Qualcomm Innovation Center, Inc. All Rights Reserved.
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
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef HEXAGON_PMU_H
#define HEXAGON_PMU_H

#define HEX_PMU_EVENTS \
    DECL_PMU_EVENT(PMU_NO_EVENT, 0)

#define DECL_PMU_EVENT(name, val) name = val,
enum {
    HEX_PMU_EVENTS
};
#undef DECL_PMU_EVENT

#endif /* HEXAGON_PMU_H */
