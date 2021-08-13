/** 
 * @file fsavatarsearchmenu.h
 * @brief Menu used by the avatar picker
 *
 * $LicenseInfo:firstyear=2014&license=viewerlgpl$
 * Starbird Viewer Source Code
 * Copyright (c) 2014 Ansariel Hiller
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

#ifndef FS_AVATARSEARCHMENU_H
#define FS_AVATARSEARCHMENU_H

#include "lllistcontextmenu.h"

class FSAvatarSearchMenu : public LLListContextMenu
{
public:
	/*virtual*/ LLContextMenu* createMenu();
private:
	bool onContextMenuItemEnable(const LLSD& userdata);
	bool onContextMenuItemCheck(const LLSD& userdata);

	void offerTeleport();
	void addToContactSet();
};

extern FSAvatarSearchMenu gFSAvatarSearchMenu;

#endif // FS_AVATARSEARCHMENU_H
