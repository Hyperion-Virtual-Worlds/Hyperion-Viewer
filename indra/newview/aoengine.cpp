/**
 * @file aoengine.cpp
 * @brief The core Animation Overrider engine
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2011, Zi Ree @ Second Life
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
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"


#include "roles_constants.h"

#include "aoengine.h"
#include "aoset.h"

#include "llagent.h"
#include "llagentcamera.h"
#include "llanimationstates.h"
#include "llassetstorage.h"
#include "llfilesystem.h"
#include "llinventoryfunctions.h"		// for ROOT_Starbird_FOLDER
#include "llinventorymodel.h"
#include "llnotificationsutil.h"
#include "llstring.h"
#include "llviewercontrol.h"
#include "llviewerinventory.h"

#define ROOT_AO_FOLDER			"#AO"
#include <boost/graph/graph_concepts.hpp>

const F32 INVENTORY_POLLING_INTERVAL = 5.0f;

AOEngine::AOEngine() :
	LLSingleton<AOEngine>(),
	mCurrentSet(nullptr),
	mDefaultSet(nullptr),
	mEnabled(false),
	mEnabledStands(false),
	mInMouselook(false),
	mUnderWater(false),
	mImportSet(nullptr),
	mImportCategory(LLUUID::null),
	mAOFolder(LLUUID::null),
	mLastMotion(ANIM_AGENT_STAND),
	mLastOverriddenMotion(ANIM_AGENT_STAND)
{
	gSavedPerAccountSettings.getControl("UseAO")->getCommitSignal()->connect(boost::bind(&AOEngine::onToggleAOControl, this));
	gSavedPerAccountSettings.getControl("UseAOStands")->getCommitSignal()->connect(boost::bind(&AOEngine::onToggleAOStandsControl, this));
	gSavedPerAccountSettings.getControl("PauseAO")->getCommitSignal()->connect(boost::bind(&AOEngine::onPauseAO, this));

	mRegionChangeConnection = gAgent.addRegionChangedCallback(boost::bind(&AOEngine::onRegionChange, this));
}

AOEngine::~AOEngine()
{
	clear(false);

	if (mRegionChangeConnection.connected())
	{
		mRegionChangeConnection.disconnect();
	}
}

void AOEngine::init()
{
	BOOL do_enable = gSavedPerAccountSettings.getBOOL("UseAO");
	BOOL do_enable_stands = gSavedPerAccountSettings.getBOOL("UseAOStands");
	if (do_enable)
	{
		// enable_stands() calls enable(), but we need to set the
		// mEnabled variable properly
		mEnabled = true;
		// Enabling the AO always enables stands to start with
		enableStands(true);
	}
	else
	{
		enableStands(do_enable_stands);
		enable(false);
	}
}

// static
void AOEngine::onLoginComplete()
{
	AOEngine::instance().init();
}

void AOEngine::onToggleAOControl()
{
	enable(gSavedPerAccountSettings.getBOOL("UseAO"));
	if (mEnabled)
	{
		// Enabling the AO always enables stands to start with
		gSavedPerAccountSettings.setBOOL("UseAOStands", TRUE);
	}
}

void AOEngine::onToggleAOStandsControl()
{
	enableStands(gSavedPerAccountSettings.getBOOL("UseAOStands"));
}

void AOEngine::onPauseAO()
{
	// can't use mEnabled here as that gets switched over by enable()
	if (gSavedPerAccountSettings.getBOOL("UseAO"))
	{
		enable(!gSavedPerAccountSettings.getBOOL("PauseAO"));
	}
}

void AOEngine::clear(bool from_timer)
{
	mOldSets.insert(mOldSets.end(), mSets.begin(), mSets.end());
	mSets.clear();

	mCurrentSet = nullptr;

	//<ND/> FIRE-3801; We cannot delete any AOSet object if we're called from a timer tick. AOSet is derived from LLEventTimer and destruction will
	// fail in ~LLInstanceTracker when a destructor runs during iteration.
	if (!from_timer)
	{
		std::for_each(mOldSets.begin(), mOldSets.end(), DeletePointer());
		mOldSets.clear();

		std::for_each(mOldImportSets.begin(), mOldImportSets.end(), DeletePointer());
		mOldImportSets.clear();
	}
}

void AOEngine::stopAllStandVariants()
{
	LL_DEBUGS("AOEngine") << "stopping all STAND variants." << LL_ENDL;
	gAgent.sendAnimationRequest(ANIM_AGENT_STAND_1, ANIM_REQUEST_STOP);
	gAgent.sendAnimationRequest(ANIM_AGENT_STAND_2, ANIM_REQUEST_STOP);
	gAgent.sendAnimationRequest(ANIM_AGENT_STAND_3, ANIM_REQUEST_STOP);
	gAgent.sendAnimationRequest(ANIM_AGENT_STAND_4, ANIM_REQUEST_STOP);
	gAgentAvatarp->LLCharacter::stopMotion(ANIM_AGENT_STAND_1);
	gAgentAvatarp->LLCharacter::stopMotion(ANIM_AGENT_STAND_2);
	gAgentAvatarp->LLCharacter::stopMotion(ANIM_AGENT_STAND_3);
	gAgentAvatarp->LLCharacter::stopMotion(ANIM_AGENT_STAND_4);
}

void AOEngine::stopAllSitVariants()
{
	LL_DEBUGS("AOEngine") << "stopping all SIT variants." << LL_ENDL;
	gAgent.sendAnimationRequest(ANIM_AGENT_SIT_FEMALE, ANIM_REQUEST_STOP);
	gAgent.sendAnimationRequest(ANIM_AGENT_SIT_GENERIC, ANIM_REQUEST_STOP);
	gAgentAvatarp->LLCharacter::stopMotion(ANIM_AGENT_SIT_FEMALE);
	gAgentAvatarp->LLCharacter::stopMotion(ANIM_AGENT_SIT_GENERIC);

	// scripted seats that use ground_sit as animation need special treatment
	const LLViewerObject* agentRoot = dynamic_cast<LLViewerObject*>(gAgentAvatarp->getRoot());
	if (agentRoot && agentRoot->getID() != gAgentID)
	{
		LL_DEBUGS("AOEngine") << "Not stopping ground sit animations while sitting on a prim." << LL_ENDL;
		return;
	}

	gAgent.sendAnimationRequest(ANIM_AGENT_SIT_GROUND, ANIM_REQUEST_STOP);
	gAgent.sendAnimationRequest(ANIM_AGENT_SIT_GROUND_CONSTRAINED, ANIM_REQUEST_STOP);

	gAgentAvatarp->LLCharacter::stopMotion(ANIM_AGENT_SIT_GROUND);
	gAgentAvatarp->LLCharacter::stopMotion(ANIM_AGENT_SIT_GROUND_CONSTRAINED);
}

void AOEngine::setLastMotion(const LLUUID& motion)
{
	if (motion != ANIM_AGENT_TYPE)
	{
		mLastMotion = motion;
	}
}

void AOEngine::setLastOverriddenMotion(const LLUUID& motion)
{
	if (motion != ANIM_AGENT_TYPE)
	{
		mLastOverriddenMotion = motion;
	}
}

bool AOEngine::foreignAnimations()
{
	// checking foreign animations only makes sense when smart sit is enabled
	if (!mCurrentSet->getSmart())
	{
		return false;
	}

	// get the seat the avatar is sitting on
	const LLViewerObject* agentRoot = dynamic_cast<LLViewerObject*>(gAgentAvatarp->getRoot());
	if (!agentRoot)
	{
		// this should not happen, ever
		return false;
	}

	LLUUID seat = agentRoot->getID();
	if (seat == gAgentID)
	{
		LL_DEBUGS("AOEngine") << "Not checking for foreign animation when not sitting." << LL_ENDL;
		return false;
	}

	LL_DEBUGS("AOEngine") << "Checking for foreign animation on seat " << seat << LL_ENDL;

	for (LLVOAvatar::AnimSourceIterator sourceIterator = gAgentAvatarp->mAnimationSources.begin();
		sourceIterator != gAgentAvatarp->mAnimationSources.end(); ++sourceIterator)
	{
		// skip animations run by the avatar itself
		if (sourceIterator->first != gAgentID)
		{
			// find the source object where the animation came from
			LLViewerObject* source = gObjectList.findObject(sourceIterator->first);

			// proceed if it's not an attachment
			if (source && !source->isAttachment())
			{
				LL_DEBUGS("AOEngine") << "Source " << sourceIterator->first << " is running animation " << sourceIterator->second << LL_ENDL;

				// get the source's root prim
				LLViewerObject* sourceRoot = dynamic_cast<LLViewerObject*>(source->getRoot());

				// if the root prim is the same as the animation source, report back as TRUE
				if (sourceRoot && sourceRoot->getID() == seat)
				{
					LL_DEBUGS("AOEngine") << "foreign animation " << sourceIterator->second << " found on seat." << LL_ENDL;
					return true;
				}
			}
		}
	}

	return false;
}

// map motion to underwater state, return nullptr if not applicable
AOSet::AOState* AOEngine::mapSwimming(const LLUUID& motion) const
{
	S32 stateNum;

	if (motion == ANIM_AGENT_HOVER)
	{
		stateNum = AOSet::Floating;
	}
	else if (motion == ANIM_AGENT_FLY)
	{
		stateNum = AOSet::SwimmingForward;
	}
	else if (motion == ANIM_AGENT_HOVER_UP)
	{
		stateNum = AOSet::SwimmingUp;
	}
	else if (motion == ANIM_AGENT_HOVER_DOWN)
	{
		stateNum = AOSet::SwimmingDown;
	}
	else
	{
		// motion not applicable for underwater mapping
		return nullptr;
	}

	return mCurrentSet->getState(stateNum);
}

// switch between swimming and flying on transition in and out of Linden region water
void AOEngine::checkBelowWater(bool check_underwater)
{
	// there was no transition, do nothing
	if (mUnderWater == check_underwater)
	{
		return;
	}

	// only applies to motions that actually change underwater and have animations inside
	AOSet::AOState* mapped = mapSwimming(mLastMotion);
	if (!mapped || mapped->mAnimations.empty())
	{
		// set underwater status but do nothing else
		mUnderWater = check_underwater;
		return;
	}

	// find animation id to stop when transitioning
	LLUUID id = override(mLastMotion, false);
	if (id.isNull())
	{
		// no animation in overrider for this state, use Linden Lab motion
		id = mLastMotion;
	}

	// stop currently running animation
	gAgent.sendAnimationRequest(id, ANIM_REQUEST_STOP);

	if (!mUnderWater)
	{
		// remember which animation we stopped while going under water, to catch the stop
		// request later in the overrider - this prevents the overrider from triggering itself
		// after the region comes back with the stop request for Linden Lab motion ids
		mTransitionId = id;
	}

	// set requested underwater status for overrider
	mUnderWater = check_underwater;

	// find animation id to start when transitioning
	id = override(mLastMotion, true);
	if (id.isNull())
	{
		// no animation in overrider for this state, use Linden Lab motion
		id = mLastMotion;
	}

	// start new animation
	gAgent.sendAnimationRequest(id, ANIM_REQUEST_START);
}

// find the correct animation state for the requested motion, mapping flying to
// swimming where necessary
AOSet::AOState* AOEngine::getStateForMotion(const LLUUID& motion) const
{
	// get default state for this motion
	AOSet::AOState* defaultState = mCurrentSet->getStateByRemapID(motion);
	if (!mUnderWater)
	{
		return defaultState;
	}

	// get state for underwater motion
	AOSet::AOState* mapped = mapSwimming(motion);
	if (!mapped)
	{
		// not applicable for underwater motion, so use default state
		return defaultState;
	}

	// check if the underwater state has any animations to play
	if (mapped->mAnimations.empty())
	{
		// no animations in underwater state, return default
		return defaultState;
	}
	return mapped;
}

void AOEngine::enableStands(bool enable_stands)
{
	mEnabledStands = enable_stands;
	// let the main enable routine decide if we need to change animations
	// but don't actually change the state of the enabled flag
	enable(mEnabled);
}

void AOEngine::enable(bool enable)
{
	LL_DEBUGS("AOEngine") << "using " << mLastMotion << " enable " << enable << LL_ENDL;
	mEnabled = enable;

	if (!mCurrentSet)
	{
		LL_DEBUGS("AOEngine") << "enable(" << enable << ") without animation set loaded." << LL_ENDL;
		return;
	}

	AOSet::AOState* state = mCurrentSet->getStateByRemapID(mLastMotion);
	if (mEnabled)
	{
		if (state && !state->mAnimations.empty())
		{
			LL_DEBUGS("AOEngine") << "Enabling animation state " << state->mName << LL_ENDL;

			// do not stop underlying sit animations when re-enabling the AO
			if (mLastOverriddenMotion != ANIM_AGENT_SIT_GROUND_CONSTRAINED && mLastOverriddenMotion != ANIM_AGENT_SIT)
			{
				gAgent.sendAnimationRequest(mLastOverriddenMotion, ANIM_REQUEST_STOP);
			}

			LLUUID animation = override(mLastMotion, true);
			if (animation.isNull())
			{
				return;
			}

			if (mLastMotion == ANIM_AGENT_STAND)
			{
				if (!mEnabledStands)
				{
					LL_DEBUGS("AOEngine") << "Last motion was a STAND, but disabled for stands, ignoring." << LL_ENDL;
					return;
				}
				stopAllStandVariants();
			}
			else if (mLastMotion == ANIM_AGENT_WALK)
			{
				LL_DEBUGS("AOEngine") << "Last motion was a WALK, stopping all variants." << LL_ENDL;
				gAgent.sendAnimationRequest(ANIM_AGENT_WALK_NEW, ANIM_REQUEST_STOP);
				gAgent.sendAnimationRequest(ANIM_AGENT_FEMALE_WALK, ANIM_REQUEST_STOP);
				gAgent.sendAnimationRequest(ANIM_AGENT_FEMALE_WALK_NEW, ANIM_REQUEST_STOP);
				gAgentAvatarp->LLCharacter::stopMotion(ANIM_AGENT_WALK_NEW);
				gAgentAvatarp->LLCharacter::stopMotion(ANIM_AGENT_FEMALE_WALK);
				gAgentAvatarp->LLCharacter::stopMotion(ANIM_AGENT_FEMALE_WALK_NEW);
			}
			else if (mLastMotion == ANIM_AGENT_RUN)
			{
				LL_DEBUGS("AOEngine") << "Last motion was a RUN, stopping all variants." << LL_ENDL;
				gAgent.sendAnimationRequest(ANIM_AGENT_RUN_NEW, ANIM_REQUEST_STOP);
				gAgent.sendAnimationRequest(ANIM_AGENT_FEMALE_RUN_NEW, ANIM_REQUEST_STOP);
				gAgentAvatarp->LLCharacter::stopMotion(ANIM_AGENT_RUN_NEW);
				gAgentAvatarp->LLCharacter::stopMotion(ANIM_AGENT_FEMALE_RUN_NEW);
			}
			else if (mLastMotion == ANIM_AGENT_SIT)
			{
				stopAllSitVariants();
				gAgent.sendAnimationRequest(ANIM_AGENT_SIT_GENERIC, ANIM_REQUEST_START);
			}
			else
			{
				LL_WARNS("AOEngine") << "Unhandled last motion id " << mLastMotion << LL_ENDL;
			}

			gAgent.sendAnimationRequest(animation, ANIM_REQUEST_START);
			mAnimationChangedSignal(state->mAnimations[state->mCurrentAnimation].mInventoryUUID);
		}
	}
	else
	{
		mAnimationChangedSignal(LLUUID::null);

		if (mLastOverriddenMotion == ANIM_AGENT_SIT)
		{
			// remove sit cycle cover up
			gAgent.sendAnimationRequest(ANIM_AGENT_SIT_GENERIC, ANIM_REQUEST_STOP);
		}

		// stop all overriders, catch leftovers
		for (S32 index = 0; index < AOSet::AOSTATES_MAX; ++index)
		{
			state = mCurrentSet->getState(index);
			if (state)
			{
				LLUUID animation = state->mCurrentAnimationID;
				if (animation.notNull())
				{
					LL_DEBUGS("AOEngine") << "Stopping leftover animation from state " << state->mName << LL_ENDL;
					gAgent.sendAnimationRequest(animation, ANIM_REQUEST_STOP);
					gAgentAvatarp->LLCharacter::stopMotion(animation);
					state->mCurrentAnimationID.setNull();
				}
			}
			else
			{
				LL_DEBUGS("AOEngine") << "state "<< index <<" returned NULL." << LL_ENDL;
			}
		}

		// restore Linden animation if applicable
		if (mLastOverriddenMotion != ANIM_AGENT_SIT || !foreignAnimations())
		{
			gAgent.sendAnimationRequest(mLastMotion, ANIM_REQUEST_START);
		}

		mCurrentSet->stopTimer();
	}
}

void AOEngine::setStateCycleTimer(const AOSet::AOState* state)
{
	F32 timeout = state->mCycleTime;
	LL_DEBUGS("AOEngine") << "Setting cycle timeout for state " << state->mName << " of " << timeout << LL_ENDL;
	if (timeout > 0.0f)
	{
		mCurrentSet->startTimer(timeout);
	}
}

const LLUUID AOEngine::override(const LLUUID& pMotion, bool start)
{
	LL_DEBUGS("AOEngine") << "override(" << pMotion << "," << start << ")" << LL_ENDL;

	LLUUID animation;

	LLUUID motion = pMotion;

	if (!mEnabled)
	{
		if (start && mCurrentSet)
		{
			const AOSet::AOState* state = mCurrentSet->getStateByRemapID(motion);
			if (state)
			{
				setLastMotion(motion);
				LL_DEBUGS("AOEngine") << "(disabled AO) setting last motion id to " <<  gAnimLibrary.animationName(mLastMotion) << LL_ENDL;
				if (!state->mAnimations.empty())
				{
					setLastOverriddenMotion(motion);
					LL_DEBUGS("AOEngine") << "(disabled AO) setting last overridden motion id to " <<  gAnimLibrary.animationName(mLastOverriddenMotion) << LL_ENDL;
				}
			}
		}
		return animation;
	}

	if (mSets.empty())
	{
		LL_DEBUGS("AOEngine") << "No sets loaded. Skipping overrider." << LL_ENDL;
		return animation;
	}

	if (!mCurrentSet)
	{
		LL_DEBUGS("AOEngine") << "No current AO set chosen. Skipping overrider." << LL_ENDL;
		return animation;
	}

	// we don't distinguish between these two
	if (motion == ANIM_AGENT_SIT_GROUND)
	{
		motion = ANIM_AGENT_SIT_GROUND_CONSTRAINED;
	}

	// map the requested motion to an animation state, taking underwater
	// swimming into account where applicable
	AOSet::AOState* state = getStateForMotion(motion);
	if (!state)
	{
		LL_DEBUGS("AOEngine") << "No current AO state for motion " << motion << " (" << gAnimLibrary.animationName(motion) << ")." << LL_ENDL;
//		This part of the code was added to capture an edge case where animations got stuck
//		However, it seems it isn't needed anymore and breaks other, more important cases.
//		So we disable this code for now, unless bad things happen and the stuck animations
//		come back again. -Zi
//		if (!gAnimLibrary.animStateToString(motion) && !start)
//		{
//			state = mCurrentSet->getStateByRemapID(mLastOverriddenMotion);
//			if (state && state->mCurrentAnimationID == motion)
//			{
//				LL_DEBUGS("AOEngine") << "Stop requested for current overridden animation UUID " << motion << " - Skipping." << LL_ENDL;
//			}
//			else
//			{
//				LL_DEBUGS("AOEngine") << "Stop requested for unknown UUID " << motion << " - Stopping it just in case." << LL_ENDL;
//				gAgent.sendAnimationRequest(motion, ANIM_REQUEST_STOP);
//				gAgentAvatarp->LLCharacter::stopMotion(motion);
//			}
//		}
		return animation;
	}

	mAnimationChangedSignal(LLUUID::null);

	// clean up stray animations as an additional safety measure
	// unless current motion is ANIM_AGENT_TYPE, which is a special
	// case, as it plays at the same time as other motions
	if (motion != ANIM_AGENT_TYPE)
	{
		const S32 cleanupStates[]=
		{
			AOSet::Standing,
			AOSet::Walking,
			AOSet::Running,
			AOSet::Sitting,
			AOSet::SittingOnGround,
			AOSet::Crouching,
			AOSet::CrouchWalking,
			AOSet::Falling,
			AOSet::FlyingDown,
			AOSet::FlyingUp,
			AOSet::Flying,
			AOSet::FlyingSlow,
			AOSet::Hovering,
			AOSet::Jumping,
			AOSet::TurningRight,
			AOSet::TurningLeft,
			AOSet::Floating,
			AOSet::SwimmingForward,
			AOSet::SwimmingUp,
			AOSet::SwimmingDown,
			AOSet::AOSTATES_MAX		// end marker, guaranteed to be different from any other entry
		};

		S32 index = 0;
		S32 stateNum;

		// loop through the list of states
		while ((stateNum = cleanupStates[index]) != AOSet::AOSTATES_MAX)
		{
			// check if the next state is the one we are currently animating and skip that
			AOSet::AOState* stateToCheck = mCurrentSet->getState(stateNum);
			if (stateToCheck != state)
			{
				// check if there is an animation left over for that state
				if (!stateToCheck->mCurrentAnimationID.isNull())
				{
					LL_WARNS() << "cleaning up animation in state " << stateToCheck->mName << LL_ENDL;

					// stop  the leftover animation locally and in the region for everyone
					gAgent.sendAnimationRequest(stateToCheck->mCurrentAnimationID, ANIM_REQUEST_STOP);
					gAgentAvatarp->LLCharacter::stopMotion(stateToCheck->mCurrentAnimationID);

					// mark the state as clean
					stateToCheck->mCurrentAnimationID.setNull();
				}
			}
			index++;
		}
	}

	mCurrentSet->stopTimer();
	if (start)
	{
		setLastMotion(motion);
		LL_DEBUGS("AOEngine") << "(enabled AO) setting last motion id to " <<  gAnimLibrary.animationName(mLastMotion) << LL_ENDL;

		// Disable start stands in Mouselook
		if (mCurrentSet->getMouselookStandDisable() &&
			motion == ANIM_AGENT_STAND &&
			mInMouselook)
		{
			LL_DEBUGS("AOEngine") << "(enabled AO, mouselook stand stopped) setting last motion id to " <<  gAnimLibrary.animationName(mLastMotion) << LL_ENDL;
			return animation;
		}

		// Don't override start and turning stands if stand override is disabled
		if (!mEnabledStands &&
			(motion == ANIM_AGENT_STAND || motion == ANIM_AGENT_TURNRIGHT || motion == ANIM_AGENT_TURNLEFT))
		{
			LL_DEBUGS("AOEngine") << "(enabled AO, stands disabled) setting last motion id to " <<  gAnimLibrary.animationName(mLastMotion) << LL_ENDL;
			return animation;
		}

		// Do not start override sits if not selected
		if (!mCurrentSet->getSitOverride() && motion == ANIM_AGENT_SIT)
		{
			LL_DEBUGS("AOEngine") << "(enabled AO, sit override stopped) setting last motion id to " <<  gAnimLibrary.animationName(mLastMotion) << LL_ENDL;
			return animation;
		}

		// scripted seats that use ground_sit as animation need special treatment
		if (motion == ANIM_AGENT_SIT_GROUND_CONSTRAINED)
		{
			const LLViewerObject* agentRoot = dynamic_cast<LLViewerObject*>(gAgentAvatarp->getRoot());
			if (agentRoot && agentRoot->getID() != gAgentID)
			{
				LL_DEBUGS("AOEngine") << "Ground sit animation playing but sitting on a prim - disabling overrider." << LL_ENDL;
				return animation;
			}
		}

		if (!state->mAnimations.empty())
		{
			setLastOverriddenMotion(motion);
			LL_DEBUGS("AOEngine") << "(enabled AO) setting last overridden motion id to " <<  gAnimLibrary.animationName(mLastOverriddenMotion) << LL_ENDL;
		}

		// do not remember typing as set-wide motion
		if (motion != ANIM_AGENT_TYPE)
		{
			mCurrentSet->setMotion(motion);
		}

		if (animation.isNull())
		{
			animation = mCurrentSet->getAnimationForState(state);
		}

		if (state->mCurrentAnimationID.notNull())
		{
			LL_DEBUGS("AOEngine")	<< "Previous animation for state "
						<< gAnimLibrary.animationName(motion)
						<< " was not stopped, but we were asked to start a new one. Killing old animation." << LL_ENDL;
			gAgent.sendAnimationRequest(state->mCurrentAnimationID, ANIM_REQUEST_STOP);
			gAgentAvatarp->LLCharacter::stopMotion(state->mCurrentAnimationID);
		}

		state->mCurrentAnimationID = animation;
		LL_DEBUGS("AOEngine")	<< "overriding " <<  gAnimLibrary.animationName(motion)
					<< " with " << animation
					<< " in state " << state->mName
					<< " of set " << mCurrentSet->getName()
					<< " (" << mCurrentSet << ")" << LL_ENDL;

		if (animation.notNull() && state->mCurrentAnimation < state->mAnimations.size())
		{
			mAnimationChangedSignal(state->mAnimations[state->mCurrentAnimation].mInventoryUUID);
		}

		setStateCycleTimer(state);

		if (motion == ANIM_AGENT_SIT)
		{
			// Use ANIM_AGENT_SIT_GENERIC, so we don't create an overrider loop with ANIM_AGENT_SIT
			// while still having a base sitting pose to cover up cycle points
			gAgent.sendAnimationRequest(ANIM_AGENT_SIT_GENERIC, ANIM_REQUEST_START);
			if (mCurrentSet->getSmart())
			{
				mSitCancelTimer.oneShot();
			}
		}
		// special treatment for "transient animations" because the viewer needs the Linden animation to know the agent's state
		else if (motion == ANIM_AGENT_SIT_GROUND_CONSTRAINED ||
				motion == ANIM_AGENT_PRE_JUMP ||
				motion == ANIM_AGENT_STANDUP ||
				motion == ANIM_AGENT_LAND ||
				motion == ANIM_AGENT_MEDIUM_LAND)
		{
			gAgent.sendAnimationRequest(animation, ANIM_REQUEST_START);
			return LLUUID::null;
		}
	}
	else
	{
		// check for previously remembered transition motion from/to underwater movement and don't
		// allow the overrider to stop it, or it will cancel the transition to underwater motion
		// immediately after starting it
		if (motion == mTransitionId)
		{
			// clear transition motion id and return a null UUID to allow the stock Linden animation
			// system to take over
			mTransitionId.setNull();
			return LLUUID::null;
		}

		// clear transition motion id here as well, to make sure there is no stray id left behind
		mTransitionId.setNull();

		animation = state->mCurrentAnimationID;
		state->mCurrentAnimationID.setNull();

		// for typing animaiton, just return the stored animation, reset the state timer, and don't memorize anything else
		if (motion == ANIM_AGENT_TYPE)
		{
			AOSet::AOState* previousState = mCurrentSet->getStateByRemapID(mLastMotion);
			if (previousState)
			{
				setStateCycleTimer(previousState);
			}
			return animation;
		}

		if (motion != mCurrentSet->getMotion())
		{
			LL_WARNS("AOEngine") << "trying to stop-override motion " <<  gAnimLibrary.animationName(motion)
					<< " but the current set has motion " <<  gAnimLibrary.animationName(mCurrentSet->getMotion()) << LL_ENDL;
			return animation;
		}

		mCurrentSet->setMotion(LLUUID::null);

		// again, special treatment for "transient" animations to make sure our own animation gets stopped properly
		if (motion == ANIM_AGENT_SIT_GROUND_CONSTRAINED ||
			motion == ANIM_AGENT_PRE_JUMP ||
			motion == ANIM_AGENT_STANDUP ||
			motion == ANIM_AGENT_LAND ||
			motion == ANIM_AGENT_MEDIUM_LAND)
		{
			gAgent.sendAnimationRequest(animation, ANIM_REQUEST_STOP);
			gAgentAvatarp->LLCharacter::stopMotion(animation);
			setStateCycleTimer(state);
			return LLUUID::null;
		}

		// stop the underlying Linden Lab motion, in case it's still running.
		// frequently happens with sits, so we keep it only for those currently.
		if (mLastMotion == ANIM_AGENT_SIT)
		{
			stopAllSitVariants();
		}
	}

	return animation;
}

void AOEngine::checkSitCancel()
{
	if (foreignAnimations())
	{
		AOSet::AOState* aoState = mCurrentSet->getStateByRemapID(ANIM_AGENT_SIT);
		if (aoState)
		{
			LLUUID animation = aoState->mCurrentAnimationID;
			if (animation.notNull())
			{
				LL_DEBUGS("AOEngine") << "Stopping sit animation due to foreign animations running" << LL_ENDL;
				gAgent.sendAnimationRequest(animation, ANIM_REQUEST_STOP);
				// remove cycle point cover-up
				gAgent.sendAnimationRequest(ANIM_AGENT_SIT_GENERIC, ANIM_REQUEST_STOP);
				gAgentAvatarp->LLCharacter::stopMotion(animation);
				mSitCancelTimer.stop();
				// stop cycle tiemr
				mCurrentSet->stopTimer();
			}
		}
	}
}

void AOEngine::cycleTimeout(const AOSet* set)
{
	if (!mEnabled)
	{
		return;
	}

	if (set != mCurrentSet)
	{
		LL_WARNS("AOEngine") << "cycleTimeout for set " << set->getName() << " but current set is " << mCurrentSet->getName() << LL_ENDL;
		return;
	}

	cycle(CycleAny);
}

void AOEngine::cycle(eCycleMode cycleMode)
{
	if (!mEnabled)
	{
		return;
	}

	if (!mCurrentSet)
	{
		LL_DEBUGS("AOEngine") << "cycle without set." << LL_ENDL;
		return;
	}

	// do not cycle if we're sitting and sit-override is off
	if (mLastMotion == ANIM_AGENT_SIT && !mCurrentSet->getSitOverride())
	{
		return;
	}
	// do not cycle if we're standing and mouselook stand override is disabled while being in mouselook
	else if (mLastMotion == ANIM_AGENT_STAND && mCurrentSet->getMouselookStandDisable() && mInMouselook)
	{
		return;
	}

	AOSet::AOState* state = mCurrentSet->getStateByRemapID(mLastMotion);
	if (!state)
	{
		LL_DEBUGS("AOEngine") << "cycle without state." << LL_ENDL;
		return;
	}

	if (!state->mAnimations.size())
	{
		LL_DEBUGS("AOEngine") << "cycle without animations in state." << LL_ENDL;
		return;
	}

	// make sure we disable cycling only for timed cycle, so manual cycling still works, even with cycling switched off
	if (!state->mCycle && cycleMode == CycleAny)
	{
		LL_DEBUGS("AOEngine") << "cycle timeout, but state is set to not cycling." << LL_ENDL;
		return;
	}

	LLUUID oldAnimation = state->mCurrentAnimationID;
	LLUUID animation;

	if (cycleMode == CycleAny)
	{
		animation = mCurrentSet->getAnimationForState(state);
	}
	else
	{
		if (cycleMode == CyclePrevious)
		{
			if (state->mCurrentAnimation == 0)
			{
				state->mCurrentAnimation = state->mAnimations.size() - 1;
			}
			else
			{
				state->mCurrentAnimation--;
			}
		}
		else if (cycleMode == CycleNext)
		{
			state->mCurrentAnimation++;
			if (state->mCurrentAnimation == state->mAnimations.size())
			{
				state->mCurrentAnimation = 0;
			}
		}
		animation = state->mAnimations[state->mCurrentAnimation].mAssetUUID;
	}

	// don't do anything if the animation didn't change
	if (animation == oldAnimation)
	{
		return;
	}

	mAnimationChangedSignal(LLUUID::null);

	state->mCurrentAnimationID = animation;
	if (animation.notNull())
	{
		LL_DEBUGS("AOEngine") << "requesting animation start for motion " << gAnimLibrary.animationName(mLastMotion) << ": " << animation << LL_ENDL;
		gAgent.sendAnimationRequest(animation, ANIM_REQUEST_START);
		mAnimationChangedSignal(state->mAnimations[state->mCurrentAnimation].mInventoryUUID);
	}
	else
	{
		LL_DEBUGS("AOEngine") << "overrider came back with NULL animation for motion " << gAnimLibrary.animationName(mLastMotion) << "." << LL_ENDL;
	}

	if (oldAnimation.notNull())
	{
		LL_DEBUGS("AOEngine") << "Cycling state " << state->mName << " - stopping animation " << oldAnimation << LL_ENDL;
		gAgent.sendAnimationRequest(oldAnimation, ANIM_REQUEST_STOP);
		gAgentAvatarp->LLCharacter::stopMotion(oldAnimation);
	}
}

void AOEngine::updateSortOrder(AOSet::AOState* state)
{
	for (U32 index = 0; index < state->mAnimations.size(); ++index)
	{
		U32 sortOrder = state->mAnimations[index].mSortOrder;

		if (sortOrder != index)
		{
			std::ostringstream numStr("");
			numStr << index;

			LL_DEBUGS("AOEngine")	<< "sort order is " << sortOrder << " but index is " << index
						<< ", setting sort order description: " << numStr.str() << LL_ENDL;

			state->mAnimations[index].mSortOrder = index;

			LLViewerInventoryItem* item = gInventory.getItem(state->mAnimations[index].mInventoryUUID);
			if (!item)
			{
				LL_WARNS("AOEngine") << "NULL inventory item found while trying to copy " << state->mAnimations[index].mInventoryUUID << LL_ENDL;
				continue;
			}
			LLPointer<LLViewerInventoryItem> newItem = new LLViewerInventoryItem(item);

			newItem->setDescription(numStr.str());
			newItem->setComplete(TRUE);
			newItem->updateServer(FALSE);

			gInventory.updateItem(newItem);
		}
	}
}

LLUUID AOEngine::addSet(const std::string& name, bool reload)
{
	if (mAOFolder.isNull())
	{
		LL_WARNS("AOEngine") << ROOT_AO_FOLDER << " folder not there yet. Requesting recreation." << LL_ENDL;
		tick();
		return LLUUID::null;
	}

	BOOL wasProtected = gSavedPerAccountSettings.getBOOL("LockAOFolders");
	gSavedPerAccountSettings.setBOOL("LockAOFolders", FALSE);
	LL_DEBUGS("AOEngine") << "adding set folder " << name << LL_ENDL;
	LLUUID newUUID = gInventory.createNewCategory(mAOFolder, LLFolderType::FT_NONE, name);
	gSavedPerAccountSettings.setBOOL("LockAOFolders", wasProtected);

	if (reload)
	{
		mTimerCollection.enableReloadTimer(true);
	}
	return newUUID;
}

bool AOEngine::createAnimationLink(const AOSet* set, AOSet::AOState* state, const LLInventoryItem* item)
{
	LL_DEBUGS("AOEngine") << "Asset ID " << item->getAssetUUID() << " inventory id " << item->getUUID() << " category id " << state->mInventoryUUID << LL_ENDL;
	if (state->mInventoryUUID.isNull())
	{
		LL_DEBUGS("AOEngine") << "no " << state->mName << " folder yet. Creating ..." << LL_ENDL;
		gInventory.createNewCategory(set->getInventoryUUID(), LLFolderType::FT_NONE, state->mName);

		LL_DEBUGS("AOEngine") << "looking for folder to get UUID ..." << LL_ENDL;
		LLUUID newStateFolderUUID;

		LLInventoryModel::item_array_t* items;
		LLInventoryModel::cat_array_t* cats;
		gInventory.getDirectDescendentsOf(set->getInventoryUUID(), cats, items);

		if (cats)
		{
			for (S32 index = 0; index < cats->size(); ++index)
			{
				if (cats->at(index)->getName().compare(state->mName) == 0)
				{
					LL_DEBUGS("AOEngine") << "UUID found!" << LL_ENDL;
					newStateFolderUUID = cats->at(index)->getUUID();
					state->mInventoryUUID = newStateFolderUUID;
					break;
				}
			}
		}
	}

	if (state->mInventoryUUID.isNull())
	{
		LL_DEBUGS("AOEngine") << "state inventory UUID not found, failing." << LL_ENDL;
		return false;
	}

	LLInventoryObject::const_object_list_t obj_array;
	obj_array.push_back(LLConstPointer<LLInventoryObject>(item));
	link_inventory_array(state->mInventoryUUID,
							obj_array,
							LLPointer<LLInventoryCallback>(nullptr));

	return true;
}

bool AOEngine::addAnimation(const AOSet* set, AOSet::AOState* state, const LLInventoryItem* item, bool reload)
{
	AOSet::AOAnimation anim;
	anim.mAssetUUID = item->getAssetUUID();
	anim.mInventoryUUID = item->getUUID();
	anim.mName = item->getName();
	anim.mSortOrder = state->mAnimations.size() + 1;
	state->mAnimations.push_back(anim);

	BOOL wasProtected = gSavedPerAccountSettings.getBOOL("LockAOFolders");
	gSavedPerAccountSettings.setBOOL("LockAOFolders", FALSE);
	createAnimationLink(set, state, item);
	gSavedPerAccountSettings.setBOOL("LockAOFolders", wasProtected);

	if (reload)
	{
		mTimerCollection.enableReloadTimer(true);
	}
	return true;
}

bool AOEngine::findForeignItems(const LLUUID& uuid) const
{
	bool moved = false;

	LLInventoryModel::item_array_t* items;
	LLInventoryModel::cat_array_t* cats;

	gInventory.getDirectDescendentsOf(uuid, cats, items);

	if (cats)
	{
		for (const auto& cat : *cats)
		{
			if (findForeignItems(cat->getUUID()))
			{
				moved = true;
			}
		}
	}

	// count backwards in case we have to remove items
	BOOL wasProtected = gSavedPerAccountSettings.getBOOL("LockAOFolders");
	gSavedPerAccountSettings.setBOOL("LockAOFolders", FALSE);

	if (items)
	{
		for (S32 index = items->size() - 1; index >= 0; --index)
		{
			bool move = false;

			LLPointer<LLViewerInventoryItem> item = items->at(index);
			if (item->getIsLinkType())
			{
				if (item->getInventoryType() != LLInventoryType::IT_ANIMATION)
				{
					LL_DEBUGS("AOEngine") << item->getName() << " is a link but does not point to an animation." << LL_ENDL;
					move = true;
				}
				else
				{
					LL_DEBUGS("AOEngine") << item->getName() << " is an animation link." << LL_ENDL;
				}
			}
			else
			{
				LL_DEBUGS("AOEngine") << item->getName() << " is not a link!" << LL_ENDL;
				move = true;
			}

			if (move)
			{
				moved = true;
				gInventory.changeItemParent(item, gInventory.findCategoryUUIDForType(LLFolderType::FT_LOST_AND_FOUND), false);
				LL_DEBUGS("AOEngine") << item->getName() << " moved to lost and found!" << LL_ENDL;
			}
		}
	}

	gSavedPerAccountSettings.setBOOL("LockAOFolders", wasProtected);

	return moved;
}

// needs a three-step process, since purge of categories only seems to work from trash
void AOEngine::purgeFolder(const LLUUID& uuid) const
{
	// unprotect it
	BOOL wasProtected = gSavedPerAccountSettings.getBOOL("LockAOFolders");
	gSavedPerAccountSettings.setBOOL("LockAOFolders", FALSE);

	// move everything that's not an animation link to "lost and found"
	if (findForeignItems(uuid))
	{
		LLNotificationsUtil::add("AOForeignItemsFound", LLSD());
	}

	// trash it
	gInventory.removeCategory(uuid);

	// clean it
	purge_descendents_of(uuid, nullptr);
	gInventory.notifyObservers();

	// purge it
	remove_inventory_object(uuid, nullptr);
	gInventory.notifyObservers();

	// protect it
	gSavedPerAccountSettings.setBOOL("LockAOFolders", wasProtected);
}

bool AOEngine::removeSet(AOSet* set)
{
	purgeFolder(set->getInventoryUUID());

	mTimerCollection.enableReloadTimer(true);
	return true;
}

bool AOEngine::removeAnimation(const AOSet* set, AOSet::AOState* state, S32 index)
{
	if (index < 0)
	{
		return false;
	}

	S32 numOfAnimations = state->mAnimations.size();
	if (numOfAnimations == 0)
	{
		return false;
	}

	LLViewerInventoryItem* item = gInventory.getItem(state->mAnimations[index].mInventoryUUID);

	// check if this item is actually an animation link
	bool move = true;
	if (item->getIsLinkType())
	{
		if (item->getInventoryType() == LLInventoryType::IT_ANIMATION)
		{
			// it is an animation link, so mark it to be purged
			move = false;
		}
	}

	// this item was not an animation link, move it to lost and found
	if (move)
	{
		LLInventoryModel* model = &gInventory;
		model->changeItemParent(item, gInventory.findCategoryUUIDForType(LLFolderType::FT_LOST_AND_FOUND), false);
		LLNotificationsUtil::add("AOForeignItemsFound", LLSD());
		update();
		return false;
	}

	// purge the item from inventory
	LL_DEBUGS("AOEngine") << __LINE__ << " purging: " << state->mAnimations[index].mInventoryUUID << LL_ENDL;
	remove_inventory_object(state->mAnimations[index].mInventoryUUID, nullptr); // item->getUUID());
	gInventory.notifyObservers();

	state->mAnimations.erase(state->mAnimations.begin() + index);

	if (state->mAnimations.size() == 0)
	{
		LL_DEBUGS("AOEngine") << "purging folder " << state->mName << " from inventory because it's empty." << LL_ENDL;

		LLInventoryModel::item_array_t* items;
		LLInventoryModel::cat_array_t* cats;
		gInventory.getDirectDescendentsOf(set->getInventoryUUID(), cats, items);

		if (cats)
		{
			for (LLInventoryModel::cat_array_t::iterator it = cats->begin(); it != cats->end(); ++it)
			{
				LLPointer<LLInventoryCategory> cat = (*it);
				std::vector<std::string> params;
				LLStringUtil::getTokens(cat->getName(), params, ":");
				std::string stateName = params[0];

				if (state->mName.compare(stateName) == 0)
				{
					LL_DEBUGS("AOEngine") << "folder found: " << cat->getName() << " purging uuid " << cat->getUUID() << LL_ENDL;

					purgeFolder(cat->getUUID());
					state->mInventoryUUID.setNull();
					break;
				}
			}
		}
	}
	else
	{
		updateSortOrder(state);
	}

	return true;
}

bool AOEngine::swapWithPrevious(AOSet::AOState* state, S32 index)
{
	S32 numOfAnimations = state->mAnimations.size();
	if (numOfAnimations < 2 || index == 0)
	{
		return false;
	}

	AOSet::AOAnimation tmpAnim = state->mAnimations[index];
	state->mAnimations.erase(state->mAnimations.begin() + index);
	state->mAnimations.insert(state->mAnimations.begin() + index - 1, tmpAnim);

	updateSortOrder(state);

	return true;
}

bool AOEngine::swapWithNext(AOSet::AOState* state, S32 index)
{
	S32 numOfAnimations = state->mAnimations.size();
	if (numOfAnimations < 2 || index == (numOfAnimations - 1))
	{
		return false;
	}

	AOSet::AOAnimation tmpAnim = state->mAnimations[index];
	state->mAnimations.erase(state->mAnimations.begin() + index);
	state->mAnimations.insert(state->mAnimations.begin() + index + 1, tmpAnim);

	updateSortOrder(state);

	return true;
}

void AOEngine::reloadStateAnimations(AOSet::AOState* state)
{
	LLInventoryModel::item_array_t* items;
	LLInventoryModel::cat_array_t* dummy;

	state->mAnimations.clear();

	gInventory.getDirectDescendentsOf(state->mInventoryUUID, dummy, items);

	if (items)
	{
		for (S32 num = 0; num < items->size(); ++num)
		{
			LL_DEBUGS("AOEngine") << "Found animation link " << items->at(num)->LLInventoryItem::getName()
				<< " desc " << items->at(num)->LLInventoryItem::getDescription()
				<< " asset " << items->at(num)->getAssetUUID() << LL_ENDL;

			AOSet::AOAnimation anim;
			anim.mAssetUUID = items->at(num)->getAssetUUID();
			LLViewerInventoryItem* linkedItem = items->at(num)->getLinkedItem();
			if (!linkedItem)
			{
				LL_WARNS("AOEngine") << "linked item for link " << items->at(num)->LLInventoryItem::getName() << " not found (broken link). Skipping." << LL_ENDL;
				continue;
			}
			anim.mName = linkedItem->LLInventoryItem::getName();
			anim.mInventoryUUID = items->at(num)->getUUID();

			S32 sortOrder;
			if (!LLStringUtil::convertToS32(items->at(num)->LLInventoryItem::getDescription(), sortOrder))
			{
				sortOrder = -1;
			}
			anim.mSortOrder = sortOrder;

			LL_DEBUGS("AOEngine") << "current sort order is " << sortOrder << LL_ENDL;

			if (sortOrder == -1)
			{
				LL_WARNS("AOEngine") << "sort order was unknown so append to the end of the list" << LL_ENDL;
				state->mAnimations.push_back(anim);
			}
			else
			{
				bool inserted = false;
				for (U32 index = 0; index < state->mAnimations.size(); ++index)
				{
					if (state->mAnimations[index].mSortOrder > sortOrder)
					{
						LL_DEBUGS("AOEngine") << "inserting at index " << index << LL_ENDL;
						state->mAnimations.insert(state->mAnimations.begin() + index, anim);
						inserted = true;
						break;
					}
				}
				if (!inserted)
				{
					LL_DEBUGS("AOEngine") << "not inserted yet, appending to the list instead" << LL_ENDL;
					state->mAnimations.push_back(anim);
				}
			}
			LL_DEBUGS("AOEngine") << "Animation count now: " << state->mAnimations.size() << LL_ENDL;
		}
	}

	updateSortOrder(state);
}

void AOEngine::update()
{
	if (mAOFolder.isNull())
	{
		return;
	}

	// move everything that's not an animation link to "lost and found"
	if (findForeignItems(mAOFolder))
	{
		LLNotificationsUtil::add("AOForeignItemsFound", LLSD());
	}

	LLInventoryModel::cat_array_t* categories;
	LLInventoryModel::item_array_t* items;

	bool allComplete = true;
	mTimerCollection.enableSettingsTimer(false);

	gInventory.getDirectDescendentsOf(mAOFolder, categories, items);

	if (categories)
	{
		for (S32 index = 0; index < categories->size(); ++index)
		{
			LLViewerInventoryCategory* currentCategory = categories->at(index);
			const std::string& setFolderName = currentCategory->getName();

			if (setFolderName.empty())
			{
				LL_WARNS("AOEngine") << "Folder with emtpy name in AO folder" << LL_ENDL;
				continue;
			}

			std::vector<std::string> params;
			LLStringUtil::getTokens(setFolderName, params, ":");

			AOSet* newSet = getSetByName(params[0]);
			if (!newSet)
			{
				LL_DEBUGS("AOEngine") << "Adding set " << setFolderName << " to AO." << LL_ENDL;
				newSet = new AOSet(currentCategory->getUUID());
				newSet->setName(params[0]);
				mSets.push_back(newSet);
			}
			else
			{
				if (newSet->getComplete())
				{
					LL_DEBUGS("AOEngine") << "Set " << params[0] << " already complete. Skipping." << LL_ENDL;
					continue;
				}
				LL_DEBUGS("AOEngine") << "Updating set " << setFolderName << " in AO." << LL_ENDL;
			}
			allComplete = false;

			for (U32 num = 1; num < params.size(); ++num)
			{
				if (params[num].size() != 2)
				{
					LL_WARNS("AOEngine") << "Unknown AO set option " << params[num] << LL_ENDL;
				}
				else if (params[num] == "SO")
				{
					newSet->setSitOverride(true);
				}
				else if (params[num] == "SM")
				{
					newSet->setSmart(true);
				}
				else if (params[num] == "DM")
				{
					newSet->setMouselookStandDisable(true);
				}
				else if (params[num] == "**")
				{
					mDefaultSet = newSet;
					mCurrentSet = newSet;
				}
				else
				{
					LL_WARNS("AOEngine") << "Unknown AO set option " << params[num] << LL_ENDL;
				}
			}

			if (gInventory.isCategoryComplete(currentCategory->getUUID()))
			{
				LL_DEBUGS("AOEngine") << "Set " << params[0] << " is complete, reading states ..." << LL_ENDL;

				LLInventoryModel::cat_array_t* stateCategories;
				gInventory.getDirectDescendentsOf(currentCategory->getUUID(), stateCategories, items);
				newSet->setComplete(true);

				for (S32 state_index = 0; state_index < stateCategories->size(); ++state_index)
				{
					std::vector<std::string> state_params;
					LLStringUtil::getTokens(stateCategories->at(state_index)->getName(), state_params, ":");
					std::string stateName = state_params[0];

					AOSet::AOState* state = newSet->getStateByName(stateName);
					if (!state)
					{
						LL_WARNS("AOEngine") << "Unknown state " << stateName << ". Skipping." << LL_ENDL;
						continue;
					}
					LL_DEBUGS("AOEngine") << "Reading state " << stateName << LL_ENDL;

					state->mInventoryUUID = stateCategories->at(state_index)->getUUID();
					for (U32 num = 1; num < state_params.size(); ++num)
					{
						if (state_params[num] == "CY")
						{
							state->mCycle = true;
							LL_DEBUGS("AOEngine") << "Cycle on" << LL_ENDL;
						}
						else if (state_params[num] == "RN")
						{
							state->mRandom = true;
							LL_DEBUGS("AOEngine") << "Random on" << LL_ENDL;
						}
						else if (state_params[num].substr(0, 2) == "CT")
						{
							LLStringUtil::convertToS32(state_params[num].substr(2, state_params[num].size() - 2), state->mCycleTime);
							LL_DEBUGS("AOEngine") << "Cycle Time specified:" << state->mCycleTime << LL_ENDL;
						}
						else
						{
							LL_WARNS("AOEngine") << "Unknown AO set option " << state_params[num] << LL_ENDL;
						}
					}

					if (!gInventory.isCategoryComplete(state->mInventoryUUID))
					{
						LL_DEBUGS("AOEngine") << "State category " << stateName << " is incomplete, fetching descendents" << LL_ENDL;
						gInventory.fetchDescendentsOf(state->mInventoryUUID);
						allComplete = false;
						newSet->setComplete(false);
						continue;
					}
					reloadStateAnimations(state);
				}
			}
			else
			{
				LL_DEBUGS("AOEngine") << "Set " << params[0] << " is incomplete, fetching descendents" << LL_ENDL;
				gInventory.fetchDescendentsOf(currentCategory->getUUID());
			}
		}
	}

	if (allComplete)
	{
		mEnabled = gSavedPerAccountSettings.getBOOL("UseAO");

		if (!mCurrentSet && !mSets.empty())
		{
			LL_DEBUGS("AOEngine") << "No default set defined, choosing the first in the list." << LL_ENDL;
			selectSet(mSets[0]);
		}

		mTimerCollection.enableInventoryTimer(false);
		mTimerCollection.enableSettingsTimer(true);

		LL_INFOS("AOEngine") << "sending update signal" << LL_ENDL;
		mUpdatedSignal();
		enable(mEnabled);
	}
}

void AOEngine::reload(bool aFromTimer)
{
	bool wasEnabled = mEnabled;

	mTimerCollection.enableReloadTimer(false);

	if (wasEnabled)
	{
		enable(false);
	}

	gAgent.stopCurrentAnimations();
	mLastOverriddenMotion = ANIM_AGENT_STAND;

	clear(aFromTimer);
	mAOFolder.setNull();
	mTimerCollection.enableInventoryTimer(true);
	tick();

	if (wasEnabled)
	{
		enable(true);
	}
}

AOSet* AOEngine::getSetByName(const std::string& name) const
{
	AOSet* found = nullptr;
	for (U32 index = 0; index < mSets.size(); ++index)
	{
		if (mSets[index]->getName().compare(name) == 0)
		{
			found = mSets[index];
			break;
		}
	}
	return found;
}

const std::string AOEngine::getCurrentSetName() const
{
	if (mCurrentSet)
	{
		return mCurrentSet->getName();
	}
	return std::string();
}

const AOSet* AOEngine::getDefaultSet() const
{
	return mDefaultSet;
}

void AOEngine::selectSet(AOSet* set)
{
	if (mEnabled && mCurrentSet)
	{
		AOSet::AOState* state = mCurrentSet->getStateByRemapID(mLastOverriddenMotion);
		if (state)
		{
			gAgent.sendAnimationRequest(state->mCurrentAnimationID, ANIM_REQUEST_STOP);
			state->mCurrentAnimationID.setNull();
			mCurrentSet->stopTimer();
		}
	}

	mCurrentSet = set;

	if (mEnabled)
	{
		LL_DEBUGS("AOEngine") << "enabling with motion " << gAnimLibrary.animationName(mLastMotion) << LL_ENDL;
		gAgent.sendAnimationRequest(override(mLastMotion, true), ANIM_REQUEST_START);
	}
}

AOSet* AOEngine::selectSetByName(const std::string& name)
{
	AOSet* set = getSetByName(name);
	if (set)
	{
		selectSet(set);
		return set;
	}
	LL_WARNS("AOEngine") << "Could not find AO set " << name << LL_ENDL;
	return nullptr;
}

const std::vector<AOSet*> AOEngine::getSetList() const
{
	return mSets;
}

void AOEngine::saveSet(const AOSet* set)
{
	if (!set)
	{
		return;
	}

	std::string setParams=set->getName();
	if (set->getSitOverride())
	{
		setParams += ":SO";
	}
	if (set->getSmart())
	{
		setParams += ":SM";
	}
	if (set->getMouselookStandDisable())
	{
		setParams += ":DM";
	}
	if (set == mDefaultSet)
	{
		setParams += ":**";
	}

/*
	// This works fine, but LL seems to have added a few helper functions in llinventoryfunctions.h
	// so let's make use of them. This code is just for reference

	LLViewerInventoryCategory* cat=gInventory.getCategory(set->getInventoryUUID());
	LL_WARNS("AOEngine") << cat << LL_ENDL;
	cat->rename(setParams);
	cat->updateServer(FALSE);
	gInventory.addChangedMask(LLInventoryObserver::LABEL, cat->getUUID());
	gInventory.notifyObservers();
*/
	BOOL wasProtected = gSavedPerAccountSettings.getBOOL("LockAOFolders");
	gSavedPerAccountSettings.setBOOL("LockAOFolders", FALSE);
	rename_category(&gInventory, set->getInventoryUUID(), setParams);
	gSavedPerAccountSettings.setBOOL("LockAOFolders", wasProtected);

	LL_INFOS("AOEngine") << "sending update signal" << LL_ENDL;
	mUpdatedSignal();
}

bool AOEngine::renameSet(AOSet* set, const std::string& name)
{
	if (name.empty() || name.find(":") != std::string::npos)
	{
		return false;
	}
	set->setName(name);
	set->setDirty(true);

	return true;
}

void AOEngine::saveState(const AOSet::AOState* state)
{
	std::string stateParams = state->mName;
	F32 time = state->mCycleTime;
	if (time > 0.0f)
	{
		std::ostringstream timeStr;
		timeStr << ":CT" << state->mCycleTime;
		stateParams += timeStr.str();
	}
	if (state->mCycle)
	{
		stateParams += ":CY";
	}
	if (state->mRandom)
	{
		stateParams += ":RN";
	}

	BOOL wasProtected = gSavedPerAccountSettings.getBOOL("LockAOFolders");
	gSavedPerAccountSettings.setBOOL("LockAOFolders", FALSE);
	rename_category(&gInventory, state->mInventoryUUID, stateParams);
	gSavedPerAccountSettings.setBOOL("LockAOFolders", wasProtected);
}

void AOEngine::saveSettings()
{
	for (U32 index = 0; index < mSets.size(); ++index)
	{
		AOSet* set = mSets[index];
		if (set->getDirty())
		{
			saveSet(set);
			LL_INFOS("AOEngine") << "dirty set saved " << set->getName() << LL_ENDL;
			set->setDirty(false);
		}

		for (S32 stateIndex = 0; stateIndex < AOSet::AOSTATES_MAX; ++stateIndex)
		{
			AOSet::AOState* state = set->getState(stateIndex);
			if (state->mDirty)
			{
				saveState(state);
				LL_INFOS("AOEngine") << "dirty state saved " << state->mName << LL_ENDL;
				state->mDirty = false;
			}
		}
	}
}

void AOEngine::inMouselook(bool mouselook)
{
	if (mInMouselook == mouselook)
	{
		return;
	}

	mInMouselook = mouselook;

	if (!mCurrentSet)
	{
		return;
	}

	if (!mCurrentSet->getMouselookStandDisable())
	{
		return;
	}

	if (!mEnabled)
	{
		return;
	}

	if (mLastMotion != ANIM_AGENT_STAND)
	{
		return;
	}

	if (mouselook)
	{
		AOSet::AOState* state = mCurrentSet->getState(AOSet::Standing);
		if (!state)
		{
			return;
		}

		LLUUID animation = state->mCurrentAnimationID;
		if (animation.notNull())
		{
			gAgent.sendAnimationRequest(animation, ANIM_REQUEST_STOP);
			gAgentAvatarp->LLCharacter::stopMotion(animation);
			state->mCurrentAnimationID.setNull();
			LL_DEBUGS("AOEngine") << " stopped animation " << animation << " in state " << state->mName << LL_ENDL;
		}
		gAgent.sendAnimationRequest(ANIM_AGENT_STAND, ANIM_REQUEST_START);
	}
	else
	{
		stopAllStandVariants();
		gAgent.sendAnimationRequest(override(ANIM_AGENT_STAND, true), ANIM_REQUEST_START);
	}
}

void AOEngine::setDefaultSet(AOSet* set)
{
	mDefaultSet = set;
	for (U32 index = 0; index < mSets.size(); ++index)
	{
		mSets[index]->setDirty(true);
	}
}

void AOEngine::setOverrideSits(AOSet* set, bool override_sit)
{
	set->setSitOverride(override_sit);
	set->setDirty(true);

	if (mCurrentSet != set)
	{
		return;
	}

	if (mLastMotion != ANIM_AGENT_SIT)
	{
		return;
	}

	if (!mEnabled)
	{
		return;
	}

	if (override_sit)
	{
		stopAllSitVariants();
		gAgent.sendAnimationRequest(ANIM_AGENT_SIT_GENERIC, ANIM_REQUEST_START);
	}
	else
	{
		// remove sit cycle cover up
		gAgent.sendAnimationRequest(ANIM_AGENT_SIT_GENERIC, ANIM_REQUEST_STOP);

		AOSet::AOState* state = mCurrentSet->getState(AOSet::Sitting);
		if (state)
		{
			LLUUID animation = state->mCurrentAnimationID;
			if (animation.notNull())
			{
				gAgent.sendAnimationRequest(animation, ANIM_REQUEST_STOP);
				state->mCurrentAnimationID.setNull();
			}
		}

		if (!foreignAnimations())
		{
			gAgent.sendAnimationRequest(ANIM_AGENT_SIT, ANIM_REQUEST_START);
		}
	}
}

void AOEngine::setSmart(AOSet* set, bool smart)
{
	set->setSmart(smart);
	set->setDirty(true);

	if (!mEnabled)
	{
		return;
	}

	if (smart)
	{
		// make sure to restart the sit cancel timer to fix sit overrides when the object we are
		// sitting on is playing its own animation
		const LLViewerObject* agentRoot = dynamic_cast<LLViewerObject*>(gAgentAvatarp->getRoot());
		if (agentRoot && agentRoot->getID() != gAgentID)
		{
			mSitCancelTimer.oneShot();
		}
	}
}

void AOEngine::setDisableMouselookStands(AOSet* set, bool disabled)
{
	set->setMouselookStandDisable(disabled);
	set->setDirty(true);

	if (mCurrentSet != set)
	{
		return;
	}

	if (!mEnabled)
	{
		return;
	}

	// make sure an update happens if needed
	mInMouselook = !gAgentCamera.cameraMouselook();
	inMouselook(!mInMouselook);
}

void AOEngine::setCycle(AOSet::AOState* state, bool cycle)
{
	state->mCycle = cycle;
	state->mDirty = true;
}

void AOEngine::setRandomize(AOSet::AOState* state, bool randomize)
{
	state->mRandom = randomize;
	state->mDirty = true;
}

void AOEngine::setCycleTime(AOSet::AOState* state, F32 time)
{
	state->mCycleTime = time;
	state->mDirty = true;
}

void AOEngine::tick()
{
	// <FS:ND> make sure agent is alive and kicking before doing anything
	if (!isAgentAvatarValid())
	{
		return;
	}
	// </FS:ND>

	const LLUUID categoryID = gInventory.findCategoryByName(ROOT_Starbird_FOLDER);

	if (categoryID.isNull())
	{
		LL_WARNS("AOEngine") << "no " << ROOT_Starbird_FOLDER << " folder yet. Creating ..." << LL_ENDL;
		gInventory.createNewCategory(gInventory.getRootFolderID(), LLFolderType::FT_NONE, ROOT_Starbird_FOLDER);
		mAOFolder.setNull();
	}
	else
	{
		LLInventoryModel::cat_array_t* categories;
		LLInventoryModel::item_array_t* items;
		gInventory.getDirectDescendentsOf(categoryID, categories, items);
		LL_DEBUGS("AOEngine") << "cat " << categories->size() << " items " << items->size() << LL_ENDL;

		for (S32 index = 0; index < categories->size(); ++index)
		{
			const std::string& catName = categories->at(index)->getName();
			if (catName.compare(ROOT_AO_FOLDER) == 0)
			{
				mAOFolder = categories->at(index)->getUUID();
				break;
			}
		}

		if (mAOFolder.isNull())
		{
			LL_WARNS("AOEngine") << "no " << ROOT_AO_FOLDER << " folder yet. Creating ..." << LL_ENDL;
			gInventory.createNewCategory(categoryID, LLFolderType::FT_NONE, ROOT_AO_FOLDER);
		}
		else
		{
			LL_INFOS("AOEngine") << "AO basic folder structure intact." << LL_ENDL;
			update();
		}
	}
}

bool AOEngine::importNotecard(const LLInventoryItem* item)
{
	if (item)
	{
		LL_INFOS("AOEngine") << "importing AO notecard: " << item->getName() << LL_ENDL;
		if (getSetByName(item->getName()))
		{
			LLNotificationsUtil::add("AOImportSetAlreadyExists", LLSD());
			return false;
		}

		if (!gAgent.allowOperation(PERM_COPY, item->getPermissions(), GP_OBJECT_MANIPULATE) && !gAgent.isGodlike())
		{
			LLNotificationsUtil::add("AOImportPermissionDenied", LLSD());
			return false;
		}

		if (item->getAssetUUID().notNull())
		{
			mImportSet = new AOSet(item->getParentUUID());
			mImportSet->setName(item->getName());

			LLUUID* newUUID = new LLUUID(item->getAssetUUID());
			const LLHost sourceSim;

			gAssetStorage->getInvItemAsset(
				sourceSim,
				gAgent.getID(),
				gAgent.getSessionID(),
				item->getPermissions().getOwner(),
				LLUUID::null,
				item->getUUID(),
				item->getAssetUUID(),
				item->getType(),
				&onNotecardLoadComplete,
				(void*) newUUID,
				TRUE
			);

			return true;
		}
	}
	return false;
}

// static
void AOEngine::onNotecardLoadComplete(const LLUUID& assetUUID, LLAssetType::EType type,
											void* userdata, S32 status, LLExtStat extStatus)
{
	if (status != LL_ERR_NOERR)
	{
		// AOImportDownloadFailed
		LLNotificationsUtil::add("AOImportDownloadFailed", LLSD());
		// NULL tells the importer to cancel all operations and free the import set memory
		AOEngine::instance().parseNotecard(nullptr);
		return;
	}
	LL_DEBUGS("AOEngine") << "Downloading import notecard complete." << LL_ENDL;

	LLFileSystem file(assetUUID, type, LLFileSystem::READ);

	S32 notecardSize = file.getSize();
	char* buffer = new char[notecardSize + 1];
	buffer[notecardSize] = 0;

	S32 ret = file.read((U8*)buffer, notecardSize);
	if (ret > 0)
	{
		AOEngine::instance().parseNotecard(buffer);
	}
	else
	{
		delete[] buffer;
	}
}

void AOEngine::parseNotecard(const char* buffer)
{
	LL_DEBUGS("AOEngine") << "parsing import notecard" << LL_ENDL;

	bool isValid = false;

	if (!buffer)
	{
		LL_WARNS("AOEngine") << "buffer==NULL - aborting import" << LL_ENDL;
		// NOTE: cleanup is always the same, needs streamlining
		delete mImportSet;
		mImportSet = 0;
		mUpdatedSignal();
		return;
	}

	std::string text(buffer);
	delete[] buffer;

	std::vector<std::string> lines;
	LLStringUtil::getTokens(text, lines, "\n");

	S32 found = -1;
	for (U32 index = 0; index < lines.size(); ++index)
	{
		if (lines[index].find("Text length ") == 0)
		{
			found = index;
			break;
		}
	}

	if (found == -1)
	{
		LLNotificationsUtil::add("AOImportNoText", LLSD());
		delete mImportSet;
		mImportSet = 0;
		mUpdatedSignal();
		return;
	}

	LLViewerInventoryCategory* importCategory = gInventory.getCategory(mImportSet->getInventoryUUID());
	if (!importCategory)
	{
		LLNotificationsUtil::add("AOImportNoFolder", LLSD());
		delete mImportSet;
		mImportSet = 0;
		mUpdatedSignal();
		return;
	}

	std::map<std::string, LLUUID> animationMap;
	LLInventoryModel::cat_array_t* dummy;
	LLInventoryModel::item_array_t* items;

	gInventory.getDirectDescendentsOf(mImportSet->getInventoryUUID(), dummy, items);
	for (U32 index = 0; index < items->size(); ++index)
	{
		animationMap[items->at(index)->getName()] = items->at(index)->getUUID();
		LL_DEBUGS("AOEngine")	<<	"animation " << items->at(index)->getName() <<
						" has inventory UUID " << animationMap[items->at(index)->getName()] << LL_ENDL;
	}

	// [ State ]Anim1|Anim2|Anim3
	for (U32 index = found + 1; index < lines.size(); ++index)
	{
		std::string line = lines[index];

		// cut off the trailing } of a notecard asset's text portion in the last line
		if (index == lines.size() - 1)
		{
			line = line.substr(0, line.size() - 1);
		}

		LLStringUtil::trim(line);

		if (line.empty())
		{
			continue;
		}

		if (line[0] == '#') // <ND/> FIRE-3801; skip comments to reduce spam to local chat.
		{
			continue;
		}

		if (line.find("[") != 0)
		{
			LLSD args;
			args["LINE"] = (S32)index;
			LLNotificationsUtil::add("AOImportNoStatePrefix", args);
			continue;
		}

		if (line.find("]") == std::string::npos)
		{
			LLSD args;
			args["LINE"] = (S32)index;
			LLNotificationsUtil::add("AOImportNoValidDelimiter", args);
			continue;
		}
		U32 endTag = line.find("]");

		std::string stateName = line.substr(1, endTag - 1);
		LLStringUtil::trim(stateName);

		AOSet::AOState* newState = mImportSet->getStateByName(stateName);
		if (!newState)
		{
			LLSD args;
			args["NAME"] = stateName;
			LLNotificationsUtil::add("AOImportStateNameNotFound", args);
			continue;
		}

		std::string animationLine = line.substr(endTag + 1);
		std::vector<std::string> animationList;
		LLStringUtil::getTokens(animationLine, animationList, "|,");

		for (U32 animIndex = 0; animIndex < animationList.size(); ++animIndex)
		{
			AOSet::AOAnimation animation;
			animation.mName = animationList[animIndex];
			animation.mInventoryUUID = animationMap[animation.mName];
			if (animation.mInventoryUUID.isNull())
			{
				LLSD args;
				args["NAME"] = animation.mName;
				LLNotificationsUtil::add("AOImportAnimationNotFound", args);
				continue;
			}
			animation.mSortOrder = animIndex;
			newState->mAnimations.push_back(animation);
			isValid = true;
		}
	}

	if (!isValid)
	{
		LLNotificationsUtil::add("AOImportInvalid", LLSD());
		// NOTE: cleanup is always the same, needs streamlining
		delete mImportSet;
		mImportSet = nullptr;
		mUpdatedSignal();
		return;
	}

	mTimerCollection.enableImportTimer(true);
	mImportRetryCount = 0;
	processImport(false);
}

void AOEngine::processImport(bool from_timer)
{
	if (mImportCategory.isNull())
	{
		mImportCategory = addSet(mImportSet->getName(), false);
		if (mImportCategory.isNull())
		{
			mImportRetryCount++;
			if (mImportRetryCount == 5)
			{
				// NOTE: cleanup is the same as at the end of this function. Needs streamlining.
				mTimerCollection.enableImportTimer(false);
				delete mImportSet;
				mImportSet = nullptr;
				mImportCategory.setNull();
				mUpdatedSignal();
				LLSD args;
				args["NAME"] = mImportSet->getName();
				LLNotificationsUtil::add("AOImportAbortCreateSet", args);
			}
			else
			{
				LLSD args;
				args["NAME"] = mImportSet->getName();
				LLNotificationsUtil::add("AOImportRetryCreateSet", args);
			}
			return;
		}
		mImportSet->setInventoryUUID(mImportCategory);
	}

	bool allComplete = true;
	for (S32 index = 0; index < AOSet::AOSTATES_MAX; ++index)
	{
		AOSet::AOState* state = mImportSet->getState(index);
		if (state->mAnimations.size())
		{
			allComplete = false;
			LL_DEBUGS("AOEngine") << "state " << state->mName << " still has animations to link." << LL_ENDL;

			for (S32 animationIndex = state->mAnimations.size() - 1; animationIndex >= 0; --animationIndex)
			{
				LL_DEBUGS("AOEngine") << "linking animation " << state->mAnimations[animationIndex].mName << LL_ENDL;
				if (createAnimationLink(mImportSet, state, gInventory.getItem(state->mAnimations[animationIndex].mInventoryUUID)))
				{
					LL_DEBUGS("AOEngine")	<< "link success, size "<< state->mAnimations.size() << ", removing animation "
								<< (*(state->mAnimations.begin() + animationIndex)).mName << " from import state" << LL_ENDL;
					state->mAnimations.erase(state->mAnimations.begin() + animationIndex);
					LL_DEBUGS("AOEngine") << "deleted, size now: " << state->mAnimations.size() << LL_ENDL;
				}
				else
				{
					LLSD args;
					args["NAME"] = state->mAnimations[animationIndex].mName;
					LLNotificationsUtil::add("AOImportLinkFailed", args);
				}
			}
		}
	}

	if (allComplete)
	{
		mTimerCollection.enableImportTimer(false);
		mOldImportSets.push_back(mImportSet); //<ND/> FIRE-3801; Cannot delete here, or LLInstanceTracker gets upset. Just remember and delete mOldImportSets once we can. 
		mImportSet = nullptr;
		mImportCategory.setNull();
		reload(from_timer);
	}
}

const LLUUID& AOEngine::getAOFolder() const
{
	return mAOFolder;
}

void AOEngine::onRegionChange()
{
	// do nothing if the AO is off
	if (!mEnabled)
	{
		return;
	}

	// catch errors without crashing
	if (!mCurrentSet)
	{
		LL_DEBUGS("AOEngine") << "Current set was NULL" << LL_ENDL;
		return;
	}

	// sitting needs special attention
	if (mLastMotion == ANIM_AGENT_SIT)
	{
		// do nothing if sit overrides was disabled
		if (!mCurrentSet->getSitOverride())
		{
			return;
		}

		// do nothing if the last overridden motion wasn't a sit.
		// happens when sit override is enabled but there were no
		// sit animations added to the set yet
		if (mLastOverriddenMotion != ANIM_AGENT_SIT)
		{
			return;
		}

		AOSet::AOState* state = mCurrentSet->getState(AOSet::Sitting);
		if (!state)
		{
			return;
		}

		// do nothing if no AO animation is playing (e.g. smart sit cancel)
		LLUUID animation = state->mCurrentAnimationID;
		if (animation.isNull())
		{
			return;
		}
	}

	// restart current animation on region crossing
	gAgent.sendAnimationRequest(mLastMotion, ANIM_REQUEST_START);
}

// ----------------------------------------------------

AOSitCancelTimer::AOSitCancelTimer()
:	LLEventTimer(0.1f),
	mTickCount(0)
{
	mEventTimer.stop();
}

AOSitCancelTimer::~AOSitCancelTimer()
{
}

void AOSitCancelTimer::oneShot()
{
	mTickCount = 0;
	mEventTimer.start();
}

void AOSitCancelTimer::stop()
{
	mEventTimer.stop();
}

BOOL AOSitCancelTimer::tick()
{
	mTickCount++;
	AOEngine::instance().checkSitCancel();
	if (mTickCount == 10)
	{
		mEventTimer.stop();
	}
	return FALSE;
}

// ----------------------------------------------------

AOTimerCollection::AOTimerCollection()
:	LLEventTimer(INVENTORY_POLLING_INTERVAL),
	mInventoryTimer(true),
	mSettingsTimer(false),
	mReloadTimer(false),
	mImportTimer(false)
{
	updateTimers();
}

AOTimerCollection::~AOTimerCollection()
{
}

BOOL AOTimerCollection::tick()
{
	if (mInventoryTimer)
	{
		LL_DEBUGS("AOEngine") << "Inventory timer tick()" << LL_ENDL;
		AOEngine::instance().tick();
	}
	if (mSettingsTimer)
	{
		LL_DEBUGS("AOEngine") << "Settings timer tick()" << LL_ENDL;
		AOEngine::instance().saveSettings();
	}
	if (mReloadTimer)
	{
		LL_DEBUGS("AOEngine") << "Reload timer tick()" << LL_ENDL;
		AOEngine::instance().reload(true);
	}
	if (mImportTimer)
	{
		LL_DEBUGS("AOEngine") << "Import timer tick()" << LL_ENDL;
		AOEngine::instance().processImport(true);
	}

	// always return FALSE or the LLEventTimer will be deleted -> crash
	return FALSE;
}

void AOTimerCollection::enableInventoryTimer(bool enable)
{
	mInventoryTimer = enable;
	updateTimers();
}

void AOTimerCollection::enableSettingsTimer(bool enable)
{
	mSettingsTimer = enable;
	updateTimers();
}

void AOTimerCollection::enableReloadTimer(bool enable)
{
	mReloadTimer = enable;
	updateTimers();
}

void AOTimerCollection::enableImportTimer(bool enable)
{
	mImportTimer = enable;
	updateTimers();
}

void AOTimerCollection::updateTimers()
{
	if (!mInventoryTimer && !mSettingsTimer && !mReloadTimer && !mImportTimer)
	{
		LL_DEBUGS("AOEngine") << "no timer needed, stopping internal timer." << LL_ENDL;
		mEventTimer.stop();
	}
	else
	{
		LL_DEBUGS("AOEngine") << "timer needed, starting internal timer." << LL_ENDL;
		mEventTimer.start();
	}
}
