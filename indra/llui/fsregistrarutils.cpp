/**
 * @file fsregistrarutils.cpp
 * @brief Utility class to allow registrars access information from dependent projects
 *
 * $LicenseInfo:firstyear=2013&license=viewerlgpl$
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

#include "linden_common.h"
#include "fsregistrarutils.h"

FSRegistrarUtils::FSRegistrarUtils() :
	mEnableCheckFunction(NULL)
{
}

bool FSRegistrarUtils::checkIsEnabled(LLUUID av_id, EFSRegistrarFunctionActionType action)
{
	if (mEnableCheckFunction)
	{
		return mEnableCheckFunction(av_id, action);
	}
	return false;
}

FSRegistrarUtils gFSRegistrarUtils;
