#include "autowall.h"
#include "Math.h"
#include "Aimbot.h"



float Autowall::GetHitgroupDamageMultiplier(HitGroups iHitGroup) {
	switch (iHitGroup) {
	case HitGroups::HITGROUP_HEAD:
		return 4.0f;
	case HitGroups::HITGROUP_CHEST:
	case HitGroups::HITGROUP_LEFTARM:
	case HitGroups::HITGROUP_RIGHTARM:
		return 1.0f;
	case HitGroups::HITGROUP_STOMACH:
		return 1.25f;
	case HitGroups::HITGROUP_LEFTLEG:
	case HitGroups::HITGROUP_RIGHTLEG:
		return 0.75f;
	default:
		return 1.0f;
	}
}

static void Autowall::ScaleDamage(HitGroups hitgroup, C_BasePlayer* enemy, float weapon_armor_ratio, float& current_damage) {
	current_damage *= GetHitgroupDamageMultiplier(hitgroup);

	if (enemy->GetArmor() > 0) {
		if (hitgroup == HitGroups::HITGROUP_HEAD) {
			if (enemy->HasHelmet())
				current_damage *= weapon_armor_ratio * 0.5f;
		}
		else
			current_damage *= weapon_armor_ratio * 0.5f;
	}
}


static bool Autowall::TraceToExit(Vector& end, trace_t* enter_trace, Vector start, Vector dir, trace_t* exit_trace) {
	float distance = 0.0f;

	while (distance <= 90.0f) {
		distance += 4.0f;
		end = start + dir * distance;

		auto point_contents = pTrace->GetPointContents(end, MASK_SHOT_HULL | CONTENTS_HITBOX, NULL);

		if (point_contents & MASK_SHOT_HULL && !(point_contents & CONTENTS_HITBOX))
			continue;

		auto new_end = end - (dir * 4.0f);

		Ray_t ray;
		ray.Init(end, new_end);
		pTrace->TraceRay(ray, MASK_SHOT, 0, exit_trace);

		if (exit_trace->startsolid && exit_trace->surface.flags & SURF_HITBOX) {
			ray.Init(end, start);

			CTraceFilter filter;
			filter.pSkip = exit_trace->m_pEnt;

			pTrace->TraceRay(ray, 0x600400B, &filter, exit_trace);

			if ((exit_trace->fraction < 1.0f || exit_trace->allsolid) && !exit_trace->startsolid) {
				end = exit_trace->endpos;
				return true;
			}

			continue;
		}

		if (!(exit_trace->fraction < 1.0 || exit_trace->allsolid || exit_trace->startsolid) ||
			exit_trace->startsolid) {
			if (exit_trace->m_pEnt) {
				if (enter_trace->m_pEnt &&
					enter_trace->m_pEnt == pEntityList->GetClientEntity(Aimbot::targetAimbot))
					return true;
			}

			continue;
		}

		if (exit_trace->surface.flags >> 7 & 1 && !(enter_trace->surface.flags >> 7 & 1))
			continue;

		if (exit_trace->plane.normal.Dot(dir) <= 1.0f) {
			auto fraction = exit_trace->fraction * 4.0f;
			end = end - (dir * fraction);

			return true;
		}
	}

	return false;
}

//gay
//bool TraceToExit(Vector& end, trace_t& tr, float x, float y, float z, float x2, float y2, float z2, trace_t* trace)
//{
//	typedef bool(__fastcall* TraceToExitFn)(Vector&, trace_t&, float, float, float, float, float, float, trace_t*);
//	static TraceToExitFn TraceToExit = (TraceToExitFn)FindPatternV2("client.dll", "55 8B EC 83 EC 30 F3 0F 10 75");
//
//	if (!TraceToExit)
//		return false;
//
//	_asm
//	{
//		push trace
//		push z2
//		push y2
//		push x2
//		push z
//		push y
//		push x
//		mov edx, tr
//		mov ecx, end
//		call TraceToExit
//		add esp, 0x1C
//	}
//}

static bool Autowall::HandleBulletPenetration(CCSWeaponInfo* weaponInfo, FireBulletData& data) {
	surfacedata_t* enter_surface_data = pPhysics->GetSurfaceData(data.enter_trace.surface.surfaceProps);
	int enter_material = enter_surface_data->game.material;
	float enter_surf_penetration_mod = enter_surface_data->game.flPenetrationModifier;

	data.trace_length += data.enter_trace.fraction * data.trace_length_remaining;
	data.current_damage *= powf(weaponInfo->GetRangeModifier(), data.trace_length * 0.002f);

	if (data.trace_length > 3000.f || enter_surf_penetration_mod < 0.1f)
		data.penetrate_count = 0;

	if (data.penetrate_count <= 0)
		return false;

	Vector dummy;
	trace_t trace_exit;

	if (!TraceToExit(dummy, &data.enter_trace, data.enter_trace.endpos, data.direction, &trace_exit))
		return false;

	//if (!TraceToExit(dummy, data.enter_trace, data.enter_trace.endpos.x, data.enter_trace.endpos.y, data.enter_trace.endpos.z, data.direction.x, data.direction.y, data.direction.z, &trace_exit))
	//	return false;

	surfacedata_t* exit_surface_data = pPhysics->GetSurfaceData(trace_exit.surface.surfaceProps);
	int exit_material = exit_surface_data->game.material;

	float exit_surf_penetration_mod = exit_surface_data->game.flPenetrationModifier;

	float final_damage_modifier = 0.16f;
	float combined_penetration_modifier = 0.0f;

	if ((data.enter_trace.contents & CONTENTS_GRATE) != 0 || enter_material == 89 || enter_material == 71) { //  Enter at 96 /\ 88 / 92              OLD 89 71
		combined_penetration_modifier = 3.0f;
		final_damage_modifier = 0.05f;
	}
	else
		combined_penetration_modifier = (enter_surf_penetration_mod + exit_surf_penetration_mod) * 0.5f;

	if (enter_material == exit_material) {
		if (exit_material == 87 || exit_material == 85) // OLD 87   //    85
			combined_penetration_modifier = 3.0f;
		else if (exit_material == 76)
			combined_penetration_modifier = 2.0f;
	}

	float v34 = fmaxf(0.f, 1.0f / combined_penetration_modifier);
	float v35 = (data.current_damage * final_damage_modifier) +
		v34 * 3.0f * fmaxf(0.0f, (3.0f / weaponInfo->GetPenetration()) * 1.25f);
	float thickness = (trace_exit.endpos - data.enter_trace.endpos).Length();

	thickness *= thickness;
	thickness *= v34;
	thickness /= 24.0f;

	float lost_damage = fmaxf(0.0f, v35 + thickness);

	if (lost_damage > data.current_damage)
		return false;

	if (lost_damage >= 0.0f)
		data.current_damage -= lost_damage;

	if (data.current_damage < 1.0f)
		return false;

	data.src = trace_exit.endpos;
	data.penetrate_count--;

	return true;
}


static void TraceLine(Vector vecAbsStart, Vector vecAbsEnd, unsigned int mask, C_BasePlayer* ignore, trace_t* ptr) {
	Ray_t ray;
	ray.Init(vecAbsStart, vecAbsEnd);
	CTraceFilter filter;
	filter.pSkip = ignore;

	pTrace->TraceRay(ray, mask, &filter, ptr);
}

static bool Autowall::SimulateFireBullet(C_BaseCombatWeapon* pWeapon, bool teamCheck, FireBulletData& data) {
	C_BasePlayer* localplayer = (C_BasePlayer*)pEntityList->GetClientEntity(pEngine->GetLocalPlayer());
	CCSWeaponInfo* weaponInfo = pWeapon->GetCSWpnData();

	data.penetrate_count = 4;
	data.trace_length = 0.0f;
	data.current_damage = (float)weaponInfo->GetDamage();

	while (data.penetrate_count > 0 && data.current_damage >= 1.0f) {
		data.trace_length_remaining = weaponInfo->GetRange() - data.trace_length;

		Vector end = data.src + data.direction * data.trace_length_remaining;

		// data.enter_trace
		TraceLine(data.src, end, MASK_SHOT, localplayer, &data.enter_trace);

		Ray_t ray;
		ray.Init(data.src, end + data.direction * 40.f);

		pTrace->TraceRay(ray, MASK_SHOT, &data.filter, &data.enter_trace);

		TraceLine(data.src, end + data.direction * 40.f, MASK_SHOT, localplayer, &data.enter_trace);

		if (data.enter_trace.fraction == 1.0f)
			break;

		if (data.enter_trace.hitgroup <= HitGroups::HITGROUP_RIGHTLEG &&
			data.enter_trace.hitgroup > HitGroups::HITGROUP_GENERIC) {
			data.trace_length += data.enter_trace.fraction * data.trace_length_remaining;
			data.current_damage *= powf(weaponInfo->GetRangeModifier(), data.trace_length * 0.002f);

			C_BasePlayer* player = (C_BasePlayer*)data.enter_trace.m_pEnt;
			if (teamCheck && player->GetTeam() == localplayer->GetTeam())
				return false;

			ScaleDamage(data.enter_trace.hitgroup, player, weaponInfo->GetWeaponArmorRatio(), data.current_damage);

			return true;
		}

		if (!HandleBulletPenetration(weaponInfo, data))
			break;
	}

	return false;
}

float Autowall::GetDamage(const Vector& point, bool teamCheck, FireBulletData& fData) {
	float damage = 0.f;
	Vector dst = point;
	C_BasePlayer* localplayer = (C_BasePlayer*)pEntityList->GetClientEntity(pEngine->GetLocalPlayer());
	FireBulletData data;
	data.src = localplayer->GetEyePosition();
	data.filter.pSkip = localplayer;

	Vector angles = Math::CalcAngle(data.src, dst);
	Math::AngleVectors(angles, data.direction);

	data.direction.NormalizeInPlace();

	C_BaseCombatWeapon* activeWeapon = (C_BaseCombatWeapon*)pEntityList->GetClientEntityFromHandle(
		localplayer->GetActiveWeapon());
	if (!activeWeapon)
		return -1.0f;

	if (SimulateFireBullet(activeWeapon, teamCheck, data))
		damage = data.current_damage;

	fData = data;

	return damage;
}


