/** 
 * @file fslslpreprocviewer.h
 * @brief Specialized LLScriptEditor class for displaying LSL preprocessor output
 *
 * $LicenseInfo:firstyear=2016&license=viewerlgpl$
 * Starbird Viewer Source Code
 * Copyright (c) 2016 Ansariel Hiller @ Second Life
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

#ifndef FS_LSLPREPROCVIEWER_H
#define FS_LSLPREPROCVIEWER_H

#include "llscripteditor.h"

class FSLSLPreProcViewer : public LLScriptEditor
{
public:
	
	struct Params : public LLInitParam::Block<Params, LLScriptEditor::Params>
	{
		Params()
		{}
	};
	
	virtual ~FSLSLPreProcViewer() {};

	virtual BOOL	handleKeyHere(KEY key, MASK mask );
	virtual BOOL	handleUnicodeCharHere(llwchar uni_char);

	virtual BOOL	canCut() const { return false; }
	virtual BOOL	canPaste() const { return false; }
	virtual BOOL	canUndo() const { return false; }
	virtual BOOL	canRedo() const { return false; }
	virtual BOOL	canPastePrimary() const { return false; }
	virtual BOOL	canDoDelete() const { return false; }

protected:
	friend class LLUICtrlFactory;
	FSLSLPreProcViewer(const Params& p);
};

#endif // FS_LSLPREPROCVIEWER_H
