#include "movement.h"

// used: cheat variables
#include "../../core/variables.h"
// used: sv_autobunnyhopping, cl_sidespeed convars
#include "../../core/convar.h"
// used: getbindstate
#include "../../utilities/inputsystem.h"
// used: old player flags
#include "../prediction.h"

// used: interface handles
#include "../../core/interfaces.h"
// used: interface declarations
#include "../../sdk/interfaces/imovehelper.h"

// used: [stl] clamp, min
#include <algorithm>
// used: [crt] asinf, atanf
#include <cmath>

// flags of the local player before prediction began
static int nPreviousLocalFlags = 0;

using namespace F::MISC;

#pragma region movement_callbacks
void MOVEMENT::OnPreMove(CCSPlayer* pLocal, CUserCmd* pCmd, const bool* pbSendPacket)
{
	// save variables before prediction starts
	nPreviousLocalFlags = pLocal->GetFlags();

	// check if the player is alive and using the suitable movement type
	if (!pLocal->IsAlive() || pLocal->GetMoveType() == MOVETYPE_NOCLIP || pLocal->GetMoveType() == MOVETYPE_LADDER || pLocal->GetWaterLevel() >= WL_WAIST)
		return;

	BunnyHop(pLocal, pCmd);

	AutoStrafe(pLocal, pCmd);

	if (C::Get<bool>(Vars.bMiscNoCrouchCooldown)) // @todo: add safety check
		pCmd->nButtons |= IN_BULLRUSH;
}

void MOVEMENT::OnMove(CCSPlayer* pLocal, CUserCmd* pCmd, const bool* pbSendPacket)
{

}

void MOVEMENT::OnPostMove(CCSPlayer* pLocal, CUserCmd* pCmd, const bool* pbSendPacket)
{
	EdgeJump(pLocal, pCmd);
}
#pragma endregion

#pragma region movement_main
void MOVEMENT::BunnyHop(CCSPlayer* pLocal, CUserCmd* pCmd)
{
       static bool bLastJumped = false;
       static bool bShouldFake = false;

       if (!C::Get<bool>(Vars.bMiscBunnyHop) || CONVAR::sv_autobunnyhopping->GetBool())
               return;

       if (pLocal->GetMoveType() == MOVETYPE_LADDER || pLocal->GetMoveType() == MOVETYPE_NOCLIP)
               return;

       if (!bLastJumped && bShouldFake)
       {
               bShouldFake = false;
               pCmd->nButtons |= IN_JUMP;
       }
       else if (pCmd->nButtons & IN_JUMP)
       {
               if (pLocal->GetFlags() & FL_ONGROUND)
                       bShouldFake = bLastJumped = true;
               else
               {
                       pCmd->nButtons &= ~IN_JUMP;
                       bLastJumped = false;
               }
       }
       else
               bShouldFake = bLastJumped = false;
}

void MOVEMENT::AutoStrafe(CCSPlayer* pLocal, CUserCmd* pCmd)
{
       if (!C::Get<bool>(Vars.bMiscAutoStrafe))
               return;

       if (pLocal->GetMoveType() == MOVETYPE_LADDER || pLocal->GetMoveType() == MOVETYPE_NOCLIP)
               return;

       if (pLocal->GetFlags() & FL_ONGROUND)
               return;

       if (pCmd->nButtons & IN_SPEED)
               return;

       const bool bHoldingW = (pCmd->nButtons & IN_FORWARD);
       const bool bHoldingA = (pCmd->nButtons & IN_MOVELEFT);
       const bool bHoldingS = (pCmd->nButtons & IN_BACK);
       const bool bHoldingD = (pCmd->nButtons & IN_MOVERIGHT);

       const bool bPressingMove = bHoldingW || bHoldingA || bHoldingS || bHoldingD;
       if (!bPressingMove)
               return;

       Vector_t vecVelocity = pLocal->GetVelocity();
       vecVelocity.z = 0.0f;

       const float flSpeed = vecVelocity.Length();
       const float flStrafeSmooth = 0.0f;
       float flIdealStrafe = (flSpeed > 5.0f) ? M_RAD2DEG(std::asinf(15.0f / flSpeed)) : 90.0f;
       flIdealStrafe *= 1.0f - (flStrafeSmooth * 0.01f);

       if (flIdealStrafe > 90.0f)
               flIdealStrafe = 90.0f;

       static float flSwitchKey = 1.0f;
       flSwitchKey *= -1.0f;

       static QAngle_t angBaseView = { };
       float flWishDir = 0.0f;

       if (bPressingMove)
       {
               if (bHoldingW)
               {
                       if (bHoldingA)
                               flWishDir += (90.0f / 2.0f);
                       else if (bHoldingD)
                               flWishDir += (-90.0f / 2.0f);
                       else
                               flWishDir += 0.0f;
               }
               else if (bHoldingS)
               {
                       if (bHoldingA)
                               flWishDir += 135.0f;
                       else if (bHoldingD)
                               flWishDir += -135.0f;
                       else
                               flWishDir += 180.0f;

                       pCmd->flForwardMove = 0.0f;
               }
               else if (bHoldingA)
                       flWishDir += 90.0f;
               else if (bHoldingD)
                       flWishDir += -90.0f;

               angBaseView.y += std::remainderf(flWishDir, 360.0f);
       }

       const float flSmooth = 1.0f - (0.15f * (flStrafeSmooth * 0.01f));
       const float flForwardSpeed = CONVAR::cl_forwardspeed->GetFloat();
       const float flSideSpeed = CONVAR::cl_sidespeed->GetFloat();

       if (flSpeed <= 0.5f)
       {
               pCmd->flForwardMove = flForwardSpeed;
               return;
       }

       const float flDiff = std::remainderf(angBaseView.y - M_RAD2DEG(std::atan2f(vecVelocity.y, vecVelocity.x)), 360.0f);

       pCmd->flForwardMove = CRT::Clamp(5850.0f / flSpeed, -flForwardSpeed, flForwardSpeed);
       pCmd->flSideMove = (flDiff > 0.0f) ? -flSideSpeed : flSideSpeed;

       angBaseView.y = std::remainderf(angBaseView.y - flDiff * flSmooth, 360.0f);
}

void MOVEMENT::EdgeJump(CCSPlayer* pLocal, CUserCmd* pCmd)
{
	if (!IPT::GetBindState(C::Get<KeyBind_t>(Vars.keyMiscEdgeJump)))
		return;

	// check if the player is able to perform jump in the water and are going to fall in the next tick
	if (pLocal->GetWaterJumpTime() <= 0.0f && (nPreviousLocalFlags & FL_ONGROUND) && !(pLocal->GetFlags() & FL_ONGROUND))
		pCmd->nButtons |= IN_JUMP;
}

#pragma endregion
