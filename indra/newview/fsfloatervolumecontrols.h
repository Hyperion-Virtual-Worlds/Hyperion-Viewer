/**
 * @file fsfloatervolumecontrols.cpp
 * @brief Class for the standalone volume controls floater in Firestorm
 *
 * $LicenseInfo:firstyear=2012&license=viewerlgpl$
 * Starbird Viewer Source Code
 * Copyright (c) 2013 Ansariel Hiller @ Second Life
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 * Virtual World Research, Inc., 109 Marion Drive, Room #306, Van Buren, Maine 04785-1267 USA
 * https://www.hyperionvirtual.com
 * $/LicenseInfo$
 */

#ifndef FS_FLOATERVOLUMECONTROLS_H
#define FS_FLOATERVOLUMECONTROLS_H

#include "llfloater.h"

class FSFloaterVolumeControls
:	public LLFloater
{
public:
	FSFloaterVolumeControls(const LLSD& key);
	BOOL postBuild();

private:
	~FSFloaterVolumeControls();
	
	/*virtual*/ void onVisibilityChange(BOOL new_visibility);
};

#endif // FS_FLOATERVOLUMECONTROLS_H
