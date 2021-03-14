// Initial implementation of 3D Classic Sonic spindash in Generations.
//
// CREDITS:
// Chimera      - Mod development.
// Sajid        - Co-developer, overall help.
// Skyth        - DLLMods tools and reference material,
//                additional assistance with Gens API and ASM.

#include <iostream>
#include <xmmintrin.h>
#include <gmath/src/Vector3.hpp>
#include "Types.h"

#pragma region Global Variables
// HACK: Rotation for the spindash charge is injected in assembly.
// To do this cleanly, rotation is saved as an __m128
// so it can be fed directly into the xmm0 register.
__m128 keyRotation = { 0, 0, 0, 0 };
bool rotationOverride = false;

// HACK: Using global variables for detecting input.
// Skyth doesn't use these so I'm wary--APPARENTLY this works with both controller and keyboard, but WHO KNOWS
auto* controllerButtons = reinterpret_cast<BYTE*>(0x01E77B54);
BYTE crouchButton = 0x02;

// TODO: Make the rotation rate configurable.
const float spinChargeRotationRate = 10.0;

bool wasGrounded = false;

#pragma endregion 

// These are just helpful to have.
#pragma region Helpers
#define EXPORT extern "C" void __declspec(dllexport)
constexpr float RAD2DEG = static_cast<float>(57.2958);
constexpr float DEG2RAD = static_cast<float>(0.0174533);

CSonicStateFlags* GetStateFlagsFromContext(CSonicContext* sonic_context)
{
	auto* const context = reinterpret_cast<int*>(sonic_context);
	return reinterpret_cast<CSonicStateFlags*>(*reinterpret_cast<int*>(context[0x14D] + 4));
}

// Lets us cleanly take a quat and turn it into an __m128,
// which can then easily be passed into the xmm0 register via assembly.
void SetRotation(Quaternion q)
{
	keyRotation = { q.X, q.Y, q.Z, q.W };
}

// We might need more flags to reset in the future,
// but for such a simple mod we only need one. For now.
void ResetFlags()
{
	rotationOverride = false;
}
#pragma endregion 

// These are for small, but necessary, overrides, for functions like rotation and input handling.
#pragma region Assembly Hooks
// Credit to Sajid for finding this out in his double jump mod.
void* changeStateOriginal = (void*)0xE4FF30;
void __cdecl ChangeState(GensString* state, int* context)
{
	__asm
		{
		mov eax, state
		mov ecx, context
		call[changeStateOriginal]
		}
}

// This is so we can apply sonic's rotation while charging spin.
uint32_t TP_Return = 0xE35389;
void __declspec(naked) TransformPlayer_Hook()
{
	__asm
		{
		mov ebx, [ebx + 0x10]
		lea esi, [ebx + 0x60]

		cmp rotationOverride, 0
		jz jump

		push eax
		lea eax, keyRotation
		movaps xmm0, xmmword ptr [eax]
		pop eax

		jump:
		movaps xmmword ptr[esi], xmm0
		jmp[TP_Return]
		}
}

// Here we override an odd quirk about Sonic Generations.
// The game has a "world input" direction vector, based on both your controller input and the camera/path you're on.
// However, for SOME REASON, there are actions that DISABLE this, forcing this input vector to be zero.
// Rather than dig around for what's causing this in a clean way, I decided to disable this check when spindash charging,
// because realistically (at time of writing), this is the only action affected by this that matters.
// Plus, we only want to override this in 3D; it makes sense to not change your orientation in 2D.
// (Granted, 2D spin charge is an entirely different class, but that aside...)
// TODO: Would be a good idea to find out how to remove SpindashCharge from this comparison in memory anyway, rather than this janky method.

// The following has to be done twice, because 3D gameplay is *technically speaking* two different types of play
// - Free roam     / "Standard"
// - Path-relative / "Forward View"

// 3D view standard
uint32_t WICO3D_ReturnAddr = 0x00E303DD;
uint32_t WICO3D_JumpAddr = 0x00E303E6;

void __declspec(naked) WorldInputComparisonOverride_3D()
{
	__asm
	{
		// custom comparison
		// compare to ONE for some reason??
		cmp rotationOverride, 1
		jz jump

		// original comparison
		cmp byte ptr[ecx + 0x98], 0
		jz jump

		// Zero out otherwise
		jmp[WICO3D_ReturnAddr]

		jump:
		jmp[WICO3D_JumpAddr]
	}
}

// Forward View
uint32_t WICOFV_ReturnAddr = 0x00E2E820;
uint32_t WICOFV_JumpAddr = 0x00E2E829;

void __declspec(naked) WorldInputComparisonOverride_Forward()
{
	__asm
	{
		// custom comparison
		// compare to ONE for some reason??
		cmp rotationOverride, 1
		jz jump

		// original comparison
		cmp byte ptr[ecx + 0x98], 0
		jz jump

		// Zero out otherwise
		jmp[WICOFV_ReturnAddr]

		jump:
		jmp[WICOFV_JumpAddr]
	}
}

#pragma endregion 

// Here's where the magic happens, it's very complicated are you ready
#pragma region 3D Roll

// Lmfao this just works
FUNCTION_PTR(int, __fastcall, Classic3DSlideMovement, 0x011D6140, int a1);
HOOK(void, __fastcall, Spin3DMovement, 0x01115DB0, _DWORD* a1)
{
	Classic3DSlideMovement(reinterpret_cast<int>(a1));
}

#pragma endregion 

// All this here controls 
#pragma region Spindash Charge Rotation
// This gets used in multiple places because Gens is silly I guess.
void RotatePlayerWhileSpinning(int a1)
{
	rotationOverride = true;

	const int playerContextOffset = *reinterpret_cast<_DWORD*>(a1 + 8);

	// We want to do a custom rotation routine here because Generations doesn't actually let you rotate while spinning by default.
	// There's some bugs with this method, probably involving how we get his up vector, so investigate why.
	// TODO: Consider getting the ground up normal via Generations' MsgGetGroundInfo
	// ... when you learn how to do that lol
	const auto rotation = static_cast<Quaternion>(keyRotation.m128_f32);
	const auto inputVec = *reinterpret_cast<Vector3*>(playerContextOffset + 0x130);

	Quaternion targetRotation = rotation;
	if (Vector3::SqrMagnitude(inputVec) >= 0.05f * 0.05f)
	{
		targetRotation = Quaternion::LookRotation(inputVec, Quaternion::Up(rotation));
		targetRotation = Quaternion::RotateTowards(rotation, targetRotation, spinChargeRotationRate * DEG2RAD);
	}

	SetRotation(targetRotation);
}

HOOK(int*, __fastcall, SonicSpinChargeMovement, 0x1250AA0, int This, void* edx)
{
	auto* const result = originalSonicSpinChargeMovement(This, edx);

	RotatePlayerWhileSpinning(This);

	return result;
}

HOOK(int*, __fastcall, SonicSpinChargeSlideMovement, 0x11D3BE0, int This, void* edx)
{
	auto* const result = originalSonicSpinChargeSlideMovement(This, edx);

	RotatePlayerWhileSpinning(This);

	return result;
}

// Was intended to let you change your orientation when doing B+A spindash.
// UNDONE: This is so bugged and I don't even know where to BEGIN fixing it LOL YOU TRY IT
//HOOK(int*, __fastcall, SonicSpinChargeSquatMovement, 0x1250600, int This, void* edx)
//{
//	auto* const result = originalSonicSpinChargeSquatMovement(This, edx);
//
//	RotatePlayerWhileSpinning(This);
//
//	// HACK: do this cuz apparently squat charge lets you move LOL?
//	// BUG: IT DOESN'T WORK AHAHAHA
//	const auto playerContextOffset = *reinterpret_cast<_DWORD*>((int)This + 8);
//	auto* const sonicContext = reinterpret_cast<CSonicContext*>(playerContextOffset);
//
//	sonicContext->Sonic->Velocity = Vector3::Zero();
//
//	return result;
//}

#pragma endregion 

// Lastly, this logs our rotation in standard play, AND lets us roll by pressing B! 

// RND shows that this is the general routine for moving Sonic around given a Vector3 for his velocity.
// It might do some other things I'm not sure about, hence the "maybe,"
// but for now this is a pretty good place to handle B-button rolling and setting his game-state rotation.
HOOK(void, __stdcall, SonicMovementMaybe, 0x00E32180, int a1, Vector3* a2)
{
	originalSonicMovementMaybe(a1, a2);

	// This method affects both Sonics so we wanna make sure we're not Modern sonic.
	// HACK: This is checking if Classic *exists* first, but what if there's a situation in the future where both are on screen via mods?
	// Looking at how this game's structured, that doesn't seem to be possible, but it's still worth considering. Probably not my problem tho.
	// TODO: Future-proof this by finding a way to check if a1 == Classic Sonic context, INSTEAD of comparing CSonicClassicContext to nullptr.

	const auto classicContextPtr = *reinterpret_cast<int*>(0x01E5E304);
	if (classicContextPtr == 0)
	{
		return;
	}


	const auto playerContextOffset = *reinterpret_cast<_DWORD*>((int)a1 + 8);
	auto* const sonicContext = reinterpret_cast<CSonicContext*>(playerContextOffset);

	const auto isGrounded = *reinterpret_cast<bool*>(playerContextOffset + 0x360);

	auto spinString = GensString("Spin");
	auto squatString = GensString("Squat");

	auto* const stateFlags = GetStateFlagsFromContext(sonicContext);

	const bool canRoll = !stateFlags->eStateFlag_OutOfControl
		&& !stateFlags->eStateFlag_Squat
		&& !stateFlags->eStateFlag_SpinDash
		&& !stateFlags->eStateFlag_SpinChargeSliding
		&& !stateFlags->eStateFlag_SpikeSpin
		&& isGrounded
		&& wasGrounded;

	if (canRoll && (*controllerButtons & crouchButton))
	{
		if (Vector3::SqrMagnitude(*a2) > static_cast<float>(16.0))	// 4*4, seems reasonable. TODO: make configurable
		{
			ChangeState(&spinString, reinterpret_cast<int*>(sonicContext));
		}
		else
		{
			// Here I wanted to have Sonic crouch more consistently; in Gens, classic can only do that from a complete standstill.
			// However, there's some issues with that... Un-comment this code to find out!
			// BUG: This just doesn't work; if you crouch he just snaps to the ground.
			// TODO: Investigate how Gens handles sending sonic to crouch state.
			//ChangeState(&squatString, reinterpret_cast<int*>(sonicContext));
		}
	}

	// BUG: Gens seems to get upset if you roll the exact same frame you land.
	// HACK: Therefore we implement a heuristic to see if he was grounded on the last frame.
	// Without this, he's kind of... permanently in crouch?

	wasGrounded = isGrounded;

	// This lets us store the rotation whenever, since its always executing.
	// TODO: this COULD be bug prone, possibly consider doing this OnFrame before we reset our flag?

	if (!rotationOverride)
	{
		SetRotation(sonicContext->Transform->Rotation);
	}
}

EXPORT Init()
{
	WRITE_JUMP(0x00E35380, TransformPlayer_Hook)

	WRITE_JUMP(0x00E303D4, WorldInputComparisonOverride_3D)
	WRITE_JUMP(0x00E2E817, WorldInputComparisonOverride_Forward)

	INSTALL_HOOK(Spin3DMovement)
	INSTALL_HOOK(SonicMovementMaybe)
	INSTALL_HOOK(SonicSpinChargeMovement)
	INSTALL_HOOK(SonicSpinChargeSlideMovement)
}

EXPORT OnFrame()
{
	ResetFlags();
}
