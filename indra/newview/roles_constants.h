/// <license>
/// @file indra_constants.h
/// @brief some useful short term constants for Indra
///
/// $LicenseInfo:firstyear=2001&license=viewerlgpl$
/// Second Life Viewer Source Code
/// Copyright (C) 2010, Linden Research, Inc.
///
/// This library is free software; you can redistribute it and/or
/// modify it under the terms of the GNU Lesser General Public
/// License as published by the Free Software Foundation;
/// version 2.1 of the License only.
///
/// This library is distributed in the hope that it will be useful,
/// but WITHOUT ANY WARRANTY; without even the implied warranty of
/// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
/// Lesser General Public License for more details.
///
/// You should have received a copy of the GNU Lesser General Public
/// License along with this library; if not, write to the Free Software
/// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
///
/// Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
/// $/LicenseInfo$
/// </license>

#ifndef LL_ROLES_CONSTANTS_H
#define LL_ROLES_CONSTANTS_H

/// <summary>
/// Max Group Roles
/// 
/// This sets the maximum number of
/// roles in each group including the
/// roles of 'Everyone', 'Officer', and
/// 'Owner'.
/// 
/// TODO: This needs to be a configurable
/// obtained from the virtual world platform
/// due to the fact that different platforms
/// may want a different number here.  We
/// already are allowing the various virtual
/// world the right to maximum number of groups 
/// a user can belong to as a platform configurable
/// instead of it being in the viewer.
/// 
/// Historically Linden Labs has limited the
/// Maximum Group Roles in each group to 10 in
/// order to protect against heavy resource usage
/// when a viewer calls a group profile up at the
/// request of a user.
/// </summary>
const S32 MAX_ROLES = 10;

enum LLRoleMemberChangeType
{
	RMC_ADD,
	RMC_REMOVE,
	RMC_NONE
};

enum LLRoleChangeType
{
	RC_UPDATE_NONE,
	RC_UPDATE_DATA,
	RC_UPDATE_POWERS,
	RC_UPDATE_ALL,
	RC_CREATE,
	RC_DELETE
};

/// <summary>
/// Group Powers
/// 
/// KNOWN HOLES: Use these for any single
/// bit powers you need bit 0x1 << 52 and
/// above.
/// 
/// The following powers have been removed
/// to make group roles simpiler:
/// bit 0x1 << 41 (GP_ACCOUNTING_VIEW)
/// bit 0x1 << 46 (GP_PROPOSAL_VIEW)
/// </summary>
const U64 GP_NO_POWERS = 0x0;
const U64 GP_ALL_POWERS = 0xFFFFffffFFFFffffLL;

/// <summary>
/// Group Membership
/// </summary>

// Invite a new member
const U64 GP_MEMBER_INVITE = 0x1LL << 1;

// Eject a member from the group
const U64 GP_MEMBER_EJECT = 0x1LL << 2;

// Toggle "Open enrollment" and change 
// "Signup Fee"
const U64 GP_MEMBER_OPTIONS = 0x1LL << 3;

// Toggle member visibility in the group's
// member directory
const U64 GP_MEMBER_VISIBLE_IN_DIR = 0x1LL << 47;

/// <summary>
/// Roles
/// </summary>

// Create a new group role
const U64 GP_ROLE_CREATE = 0x1LL << 4;

// Delete a group role
const U64 GP_ROLE_DELETE = 0x1LL << 5;

// Change the Name, Title, and Descriptions of
// roles in the group that a user is in, only, 
// or any role in that group.
const U64 GP_ROLE_PROPERTIES = 0x1LL << 6;

// Assign a member to a role that the avatar
// that assigns the role is in.
const U64 GP_ROLE_ASSIGN_MEMBER_LIMITED = 0x1LL << 7;

// Assign member to a role in the group
const U64 GP_ROLE_ASSIGN_MEMBER = 0x1LL << 8;

// Remove member from a role in the group
const U64 GP_ROLE_REMOVE_MEMBER = 0x1LL << 9;

// Change actions and abilities that a role in
// the group can perform
const U64 GP_ROLE_CHANGE_ACTIONS = 0x1LL << 10;

/// <summary>
/// Group identity
/// 
/// This includes the ability to change the Charter,
/// insignia, 'Show in group List', 'Publish on the web',
/// 'Group Maturity Rating', all, 'Show member in group profile'
/// checkboxes
/// </summary>
const U64 GP_GROUP_CHANGE_IDENTITY = 0x1LL << 11;

/// <summary>
/// Parcel Management
/// </summary>
/// 
/// // Deed land and buy land for the Group
const U64 GP_LAND_DEED = 0x1LL << 12;

// Release land (i.e. Abandon land to Governor Linden)
const U64 GP_LAND_RELEASE = 0x1LL << 13;

// Set for sale info (toggle "For Sale", set price,
// Set Target, Toggle 'Sell objects with the land")
const U64 GP_LAND_SET_SALE_INFO = 0x1LL << 14;

// Divide and join parcels
const U64 GP_LAND_DIVIDE_JOIN = 0x1LL << 15;

/// <summary>
/// Parcel Identity
/// </summary>

// Toggle "Show in Find Places" and set Category
const U64 GP_LAND_FIND_PLACES = 0x1LL << 17;

// Change Parcel Identity: Parcel Name, Parcel Description,
// Snapshot, 'Publish on the web', and Parcel Maturity Rating 
// Checkboxes
const U64 GP_LAND_CHANGE_IDENTITY = 0x1LL << 18;

/// <summary>
/// Set Landing Point
/// 
/// NOTE:
/// 
/// If an avatar is GOD_LIKE 1 and a minimum of
/// GOD_LIAISON = 100 or above, the landing point
/// gets overridden to protect a staff account from
/// griefing, or attempts by scripts, objects, and
/// users at that landing point from hacking the 
/// staff account.  This means a grid god can land
/// anywhere and override the landing point set
/// at the parcel level or region telehub.
/// </summary>
const U64 GP_LAND_SET_LANDING_POINT = 0x1LL << 19;

/// <summary>
/// Parcel Settings
/// </summary>

// Change Media Settings on the parcel
const U64 GP_LAND_CHANGE_MEDIA = 0x1LL << 20;

// Toggle Edit Land
const U64 GP_LAND_EDIT = 0x1LL << 21;

// Toggle Set Home Point, Fly, Outside Scripts,
// Create/ Edit Objects, Landmark, and Damage 
// Checkboxes
const U64 GP_LAND_OPTIONS = 0x1LL << 22;

/// <summary>
/// Parcel Powers
/// </summary>

// Bypass Edit Land Restriction
const U64 GP_LAND_ALLOW_EDIT_LAND = 0x1LL << 23;

// Bypass Fly Restriction
const U64 GP_LAND_ALLOW_FLY = 0x1LL << 24;

// Bypass Create/ Edit Objects Restriction
const U64 GP_LAND_ALLOW_CREATE = 0x1LL << 25;

// Bypass Landmark Restriction
const U64 GP_LAND_ALLOW_LANDMARK = 0x1LL << 26;

// Bypass Set Home Point Restriction
const U64 GP_LAND_ALLOW_SET_HOME = 0x1LL << 28;

// Allowed to hold events on group-owned land
const U64 GP_LAND_ALLOW_HOLD_EVENT = 0x1LL << 41;

// Allowed to change the environment
const U64 GP_LAND_ALLOW_ENVIRONMENT = 0x1LL << 46;

/// <summary>
/// Parcel Access
/// </summary>

// Manage Allowed List
const U64 GP_LAND_MANAGE_ALLOWED = 0x1LL << 29;

// Manage Banned List
const U64 GP_LAND_MANAGE_BANNED = 0x1LL << 30;

// CHange Sell Pass Settings
const U64 GP_LAND_MANAGE_PASSES = 0x1LL << 31;

// Eject and Freeze Users on the land
const U64 GP_LAND_ADMIN = 0x1LL << 32;

/// <summary>
/// Parcel Content
/// </summary>

// Return objects on parcel that are set to group
const U64 GP_LAND_RETURN_GROUP_SET = 0x1LL << 33;

// Return objects on parcel that are not set to group
const U64 GP_LAND_RETURN_NON_GROUP = 0x1LL << 34;

// Return objects on parcel that are owned by the group
const U64 GP_LAND_RETURN_GROUP_OWNED = 0x1LL << 48;

/// <summary>
/// Select a power-bit based on an object's 
/// relationship to a parcel.
/// </summary>
const U64 GP_LAND_RETURN = GP_LAND_RETURN_GROUP_OWNED | GP_LAND_RETURN_GROUP_SET | GP_LAND_RETURN_NON_GROUP;

// Parcel Gardening - Plant and move Linden Trees
const U64 GP_LAND_GARDENING = 0x1LL << 35;

/// <summary>
/// Object Management
/// </summary>

// Deed Object
const U64 GP_OBJECT_DEED = 0x1LL << 36;

// Manipulate Group Owned Objects (Move, Copy, Modify)
const U64 GP_OBJECT_MANIPULATE = 0x1LL << 38;

// Set Group Owned Object for Sale
const U64 GP_OBJECT_SET_SALE = 0x1LL << 39;

/// <summary>
/// Accounting
/// </summary>

// Pay Group Liabilities and Receive Group Dividends
const U64 GP_ACCOUNTING_ACCOUNTABLE = 0x1LL << 40;

/// <summary>
/// Group Notices
/// </summary>

// Send Notices
const U64 GP_NOTICES_SEND = 0x1LL << 42;

// Receive and view notices and notice history
const U64 GP_NOTICES_RECEIVE = 0x1LL << 43;

/// <summary>
/// Proposals
/// </summary>

// TODO: DEPRECATED suffix as part of vote removal - DEV-24856:
// Start Proposal
const U64 GP_PROPOSAL_START = 0x1LL << 44;

// TODO: DEPRECATED suffix as part of vote removal - DEV-24856:
// Vote on Proposal
const U64 GP_PROPOSAL_VOTE = 0x1LL << 45;

/// <summary>
/// Group chat moderation related
/// </summary>

// Can join Group Chat Session
const U64 GP_SESSION_JOIN = 0x1LL << 16;

// Can hear and talk on Group Voice Chat
const U64 GP_SESSION_VOICE = 0x1LL << 27;

// Can mute people's Group Chat Sessions
const U64 GP_SESSION_MODERATOR = 0x1LL << 37;

// Has Admin Rights to any experiences that are
// owned by the group
const U64 GP_EXPERIENCE_ADMIN = 0x1LL << 49;

// Can Sign Scripts for Experiences owned by
// the group
const U64 GP_EXPERIENCE_CREATOR = 0x1LL << 50;

/// <summary>
/// Group Banning
/// </summary>

// Allows access to ban/ un-ban agents from
// a group
const U64 GP_GROUP_BAN_ACCESS = 0x1LL << 51;

const U64 GP_DEFAULT_MEMBER = GP_ACCOUNTING_ACCOUNTABLE
| GP_LAND_ALLOW_SET_HOME
| GP_NOTICES_RECEIVE
| GP_SESSION_JOIN
| GP_SESSION_VOICE;

// Superset of GP_DEFAULT_MEMBER
const U64 GP_DEFAULT_OFFICER = GP_DEFAULT_MEMBER
| GP_GROUP_CHANGE_IDENTITY
| GP_LAND_ADMIN
| GP_LAND_ALLOW_EDIT_LAND
| GP_LAND_ALLOW_FLY
| GP_LAND_ALLOW_CREATE
| GP_LAND_ALLOW_ENVIRONMENT
| GP_LAND_ALLOW_LANDMARK
| GP_LAND_CHANGE_IDENTITY
| GP_LAND_CHANGE_MEDIA
| GP_LAND_DEED
| GP_LAND_DIVIDE_JOIN
| GP_LAND_EDIT
| GP_LAND_FIND_PLACES
| GP_LAND_GARDENING
| GP_LAND_MANAGE_ALLOWED
| GP_LAND_MANAGE_BANNED
| GP_LAND_MANAGE_PASSES
| GP_LAND_OPTIONS
| GP_LAND_RELEASE
| GP_LAND_RETURN_GROUP_OWNED
| GP_LAND_RETURN_GROUP_SET
| GP_LAND_RETURN_NON_GROUP
| GP_LAND_SET_LANDING_POINT
| GP_LAND_SET_SALE_INFO
| GP_MEMBER_EJECT
| GP_MEMBER_INVITE
| GP_MEMBER_OPTIONS
| GP_MEMBER_VISIBLE_IN_DIR
| GP_NOTICES_SEND
| GP_OBJECT_DEED
| GP_OBJECT_MANIPULATE
| GP_OBJECT_SET_SALE
| GP_ROLE_ASSIGN_MEMBER_LIMITED
| GP_ROLE_PROPERTIES
| GP_SESSION_MODERATOR;

#endif
