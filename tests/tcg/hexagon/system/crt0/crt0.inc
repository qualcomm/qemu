/*
 *  Copyright(c) 2024 Qualcomm Innovation Center, Inc. All Rights Reserved.
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

	.macro AddrOf Var, To = r0
	\To\() = ## (\Var)
	.endm

	.macro ReadFrom Var, To = r0
	AddrOf \Var, \To
	\To = memw (\To)
	.endm

	.macro WriteTo Var, From = r0, Ptr = r1
	.ifnc "\From", "\Ptr"
	AddrOf \Var, \Ptr
	memw (\Ptr) = \From
	\From = memw (\Ptr)
	.else
	.print "Macro arguments \"From\" and \"Ptr\" cannot be the same."
	.err
	.endif
	.endm
