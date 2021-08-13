/** 
 * @file fsnamelistavatarmenu.h
 * @brief Special avatar menu used LLNameListCtrls
 *
 * $LicenseInfo:firstyear=2015&license=viewerlgpl$
 * Starbird Viewer Source Code
 * Copyright (c) 2015 Ansariel Hiller @ Second Life
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

#ifndef FS_FSNAMELISTAVATARMENU_H
#define FS_FSNAMELISTAVATARMENU_H

#include "lllistcontextmenu.h"

class FSNameListAvatarMenu : public LLListContextMenu
{
public:
	/*virtual*/ LLContextMenu* createMenu();
private:
	bool enableContextMenuItem(const LLSD& userdata);
	void offerTeleport();
	void teleportToAvatar();
	void onTrackAvatarMenuItemClick();
	void addToContactSet();
	void copyNameToClipboard(const LLUUID& id);
	void copySLURLToClipboard(const LLUUID& id);
};

extern FSNameListAvatarMenu gFSNameListAvatarMenu;

#endif // FS_FSNAMELISTAVATARMENU_H
