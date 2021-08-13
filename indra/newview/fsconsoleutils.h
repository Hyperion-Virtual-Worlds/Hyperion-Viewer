/**
 * @file fsconsoleutils.h
 * @brief Class for console-related functions in Starbird
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

#ifndef FS_CONSOLEUTILS_H
#define FS_CONSOLEUTILS_H

class LLAvatarName;
class LLChat;

class FSConsoleUtils
{
public:
	static bool ProcessChatMessage(const LLChat& chat_msg, const LLSD &args);
	static bool ProcessInstantMessage(const LLUUID& session_id, const LLUUID& from_id, const std::string& message);

protected:
	static void onProcessChatAvatarNameLookup(const LLUUID& agent_id, const LLAvatarName& av_name, const LLChat& chat_msg);
	static void onProccessInstantMessageNameLookup(const LLUUID& agent_id, const LLAvatarName& av_name, const std::string& message_str, const std::string& group, const LLUUID& session_id);

};

#endif // FS_CONSOLEUTILS_H
