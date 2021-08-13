/**
 * @file fscorehttputil.h
 * @brief Core HTTP utility classes.
 *
 * $LicenseInfo:firstyear=2015&license=viewerlgpl$
 * Starbird Viewer Source Code
 * Copyright (c) 2015 Nicky Dasmijn
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

#ifndef FS_COREHTTPUTIL_H
#define FS_COREHTTPUTIL_H

#include "llcorehttputil.h"

namespace FSCoreHttpUtil
{
	typedef boost::function<void(const LLSD &)> completionCallback_t;

	void callbackHttpPostRaw(const std::string &url, std::string postData, completionCallback_t success = NULL, completionCallback_t failure = NULL,
							 LLCore::HttpHeaders::ptr_t aHeader = LLCore::HttpHeaders::ptr_t(), LLCore::HttpOptions::ptr_t options = LLCore::HttpOptions::ptr_t());
	void callbackHttpGetRaw(const std::string &url, completionCallback_t success = NULL, completionCallback_t failure = NULL,
							LLCore::HttpHeaders::ptr_t aHeader = LLCore::HttpHeaders::ptr_t(), LLCore::HttpOptions::ptr_t options = LLCore::HttpOptions::ptr_t());
	void callbackHttpGet(const std::string &url, const time_t& last_modified, completionCallback_t success, completionCallback_t failure);

	void trivialGetCoroRaw(std::string url, LLCore::HttpRequest::policy_t policyId, LLCore::HttpHeaders::ptr_t aHeader, LLCore::HttpOptions::ptr_t options, completionCallback_t success, completionCallback_t failure);
	void trivialPostCoroRaw(std::string url, LLCore::HttpRequest::policy_t policyId, LLCore::BufferArray::ptr_t postData, LLCore::HttpHeaders::ptr_t aHeader, LLCore::HttpOptions::ptr_t options,
							completionCallback_t success, completionCallback_t failure);
	void trivialGetCoro(std::string url, time_t last_modified, completionCallback_t success, completionCallback_t failure);
}

#endif // FS_COREHTTPUTIL_H
