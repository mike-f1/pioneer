// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "FixedGuns.h"

#include "Beam.h"
#include "Body.h"
#include "GameSaveError.h"
#include "Projectile.h"
#include "JsonUtils.h"
#include "libs/StringF.h"
#include "scenegraph/Model.h"

#include <stdexcept>

Json FixedGuns::SaveToJson() const
{
	Json gunArray = Json::array(); // Create JSON array to contain gun data.

	for (unsigned i = 0; i < m_guns.size(); i++) {
		Json gunArrayEl({});// = Json::object(); // Create JSON object to contain gun.
		//Json projectileObj({})
		gunArrayEl["state"] = m_guns[i].is_firing;
		gunArrayEl["active"] = m_guns[i].is_active;
		gunArrayEl["recharge"] = m_guns[i].recharge_stat;
		gunArrayEl["temperature"] = m_guns[i].temperature_stat;
		gunArrayEl["contemporary_barrels"] = m_guns[i].contemporary_barrels;
		gunArrayEl["next_firing"] = m_guns[i].next_firing_barrels;
		gunArrayEl["mount_name"] = m_mounts[m_guns[i].mount_id].name; // <- Save the name of hardpoint (mount)
		// Save "GunData":
		gunArrayEl["gun_data"] = m_guns[i].gun_data.SaveToJson();

		gunArray.push_back(gunArrayEl); // Append gun object to array.
	}
	return gunArray; // Add gun array to ship object.
}

void FixedGuns::LoadFromJson(const Json &jsonObj)
{
	try {
		if (!jsonObj.is_array()) throw SavedGameCorruptException();
		const Json &gunArray = jsonObj;

		m_guns.reserve(gunArray.size());

		for (unsigned int i = 0; i < gunArray.size(); i++) {
			Json gunArrayEl = gunArray[i];
			// Load status data:
			bool is_firing = gunArrayEl["state"];
			bool is_active = gunArrayEl["active"];
			float recharge_stat = gunArrayEl["recharge"];
			float temperature_stat = gunArrayEl["temperature"];
			int contemporary_barrels = gunArrayEl["contemporary_barrels"];
			int next_firing = gunArrayEl["next_firing"];
			std::string mount_name = gunArrayEl["mount_name"];
			int mount_id = -1;
			for (unsigned j = 0; j < m_mounts.size(); j++) {
				if (m_mounts[j].name == mount_name.substr(0,14)) {
					mount_id = j;
					break;
				}
			}
			if (mount_id < 0) throw SavedGameCorruptException();
			// Load "GunData" for this gun:
			GunData gd(gunArrayEl["gun_data"]);
			GunStatus gs(mount_id, gd);

			// Set the gun status
			gs.is_firing = is_firing;
			gs.is_active = is_active;
			gs.recharge_stat = recharge_stat;
			gs.temperature_stat = temperature_stat;
			gs.contemporary_barrels = contemporary_barrels;
			gs.next_firing_barrels = next_firing;

			gs.UpdateFireModes(m_mounts[mount_id]);

			m_guns.push_back(gs);
		}
	} catch (Json::type_error &) {
		Output("Loading error in '%s' in function '%s' \n", __FILE__, __func__);
		throw SavedGameCorruptException();
	}
}

void FixedGuns::GetGunsTags(SceneGraph::Model *m)
{
	if (m == nullptr) {
		Output("In FixedGuns::GetGunsTags:\nFATAL: No Model no Guns, sorry...\n");
		abort();
	}
	m_mounts = m->GetGunTags();
}

bool FixedGuns::MountGun(MountId num, const std::string &name, const std::string &sound, float recharge, float heatrate, float coolrate, int barrels, const ProjectileData &pd)
{
	// Check mount (num) is valid
	if (unsigned(num) >= m_mounts.size()) {
		Output("Attempt to mount a gun in %u, which is out of bounds\n", num);
		return false;
	}
	// Check ... well, there's a needs for explanations?
	if (barrels == 0) {
		Output("Attempt to mount a gun with zero barrels\n");
		return false;
	}

	// Check mount is free:
	for (unsigned i = 0; i < m_guns.size(); i++) {
		if (m_mounts[m_guns[i].mount_id].name == m_mounts[num].name.substr(0,14)) {
			Output("Attempt to mount gun %u on '%s', which is already used\n", num, m_mounts[num].name.c_str());
			return false;
		}
	}
	if (unsigned(barrels) > m_mounts[num].locs.size()) {
		Output("Gun with %i barrels mounted on '%s', which is for %i barrels\n", barrels, m_mounts[num].name.c_str(), int(m_mounts[num].locs.size()));
	}
	GunStatus gs(num, name, sound, recharge, heatrate, coolrate, barrels, pd);
	gs.UpdateFireModes(m_mounts[num]);

	m_guns.push_back(gs);

	return true;
}

bool FixedGuns::UnMountGun(MountId num)
{
	// Check mount (num) is valid
	if (m_mounts.empty() || (size_t(num) >= m_mounts.size())) {
		Output("Mount identifier (%i) is out of bounds (max is %lu) in 'UnMountGun'\n", num, m_mounts.size());
		return false;
	}
	// Check mount is used
	std::vector<GunStatus>::iterator found = std::find_if(begin(m_guns), end(m_guns), [&num](const GunStatus &gs)
	{
		return (num == gs.mount_id);
	});

	if (found == m_guns.end()) {
		Output("No gun found for the given identifier\n");
		return false;
	}
	//Output("Remove gun '%s' in %i, mounted on '%s'\n", found->gun_data.gun_name.c_str(), found->mount_id, m_mounts[m_guns[num].mount_id].name.c_str());
	// Mount 'num' is used and should be freed
	std::vector<GunStatus>::iterator last = m_guns.end() - 1;

	std::swap(*found, *last);

	m_guns.pop_back();

	return true;
}

bool FixedGuns::SwapGuns(MountId mount_a, MountId mount_b)
{
	if (unsigned(mount_a) >= m_mounts.size() || unsigned(mount_b) >= m_mounts.size()) {
		Output("A 'MountId' is out of bounds\n");
	}
	if (mount_a == mount_b) return true;

	std::vector<GunStatus>::iterator it_a = std::find_if(begin(m_guns), end(m_guns), [&mount_a](const GunStatus &gs)
	{
		return gs.mount_id == mount_a;
	});
	if (it_a == m_guns.end()) {
		// TODO: There's an implcit limit, which is
		// that  the first mount must be not empty...
		Output("No gun in mount %u\n", mount_a);
		return false;
	};
	std::vector<GunStatus>::iterator it_b = std::find_if(begin(m_guns), end(m_guns), [&mount_b](const GunStatus &gs)
	{
		return gs.mount_id == mount_b;
	});
	if (it_b == m_guns.end()) {
		// Second mount is empty:
		it_a->mount_id = mount_b;
		it_a->UpdateFireModes(m_mounts[mount_b]);
	} else {
		// Second mount have a gun:
		it_a->mount_id = mount_b;
		it_a->UpdateFireModes(m_mounts[mount_b]);
		it_b->mount_id = mount_a;
		it_b->UpdateFireModes(m_mounts[mount_a]);
	}

	return true;
}

void FixedGuns::SetGunsFiringState(GunDir dir, bool fire)
{
	std::for_each(begin(m_guns), end(m_guns), [&](GunStatus &gs) { if (m_mounts[gs.mount_id].dir == dir ) gs.is_firing = fire;});
}

bool FixedGuns::Fire(GunId num, Body *shooter)
{
	if (unsigned(num) >= m_guns.size()) return false;

	GunStatus &gun = m_guns[num];
	if (!gun.is_firing) return false;
	if (!gun.is_active) return false;
	if (gun.recharge_stat > 0) return false;
	if (gun.temperature_stat > 1.0) return false;

	gun.temperature_stat += gun.gun_data.temp_heat_rate * (float(gun.contemporary_barrels) / gun.gun_data.barrels);
	gun.recharge_stat = gun.gun_data.recharge * (float(gun.contemporary_barrels) / gun.gun_data.barrels);

	const Mount &mount = m_mounts[gun.mount_id];

	for (int iBarrel : gun.GetFiringBarrelsAndAdvance()) {
		// (0,0,-1) => Front ; (0,0,1) => Rear
		const vector3d front_rear = (mount.dir == GunDir::GUN_FRONT ? vector3d(0., 0., -1.) : vector3d(0., 0., 1.));
		const vector3d dir = (shooter->GetOrient() * front_rear);

		const vector3d pos = shooter->GetOrient() * vector3d(mount.locs[iBarrel]) + shooter->GetPosition();

		if (gun.gun_data.projData.beam) {
			Beam::Add(shooter, gun.gun_data.projData, pos, shooter->GetVelocity(), dir);
		} else {
			const vector3d dirVel = gun.gun_data.projData.speed * dir;
			Projectile::Add(shooter, gun.gun_data.projData, pos, shooter->GetVelocity(), dirVel);
		}
	}

	return true;
}

bool FixedGuns::UpdateGuns(float timeStep, Body *shooter)
{
	float coolingRate = m_cooler_boost * timeStep;
	std::for_each(begin(m_guns), end(m_guns), [&timeStep, &coolingRate](GunStatus &gun) {
		gun.temperature_stat -= gun.gun_data.temp_cool_rate * coolingRate;

		if (gun.temperature_stat < 0.0f)
			gun.temperature_stat = 0;
		else if (gun.temperature_stat > 1.0f)
			gun.is_firing = false;

		gun.recharge_stat -= timeStep;
		if (gun.recharge_stat < 0.0f)
			gun.recharge_stat = 0;
	});

	bool fire = false;

	for (GunId i = 0; i < m_guns.size(); i++) {
		bool res = Fire(i, shooter);
		// Skip sound management if 'sound' is not defined
		if (m_guns[i].gun_data.sound.empty()) continue;
		if (res) {
			fire = true;
			if (IsBeam(i)) {
				float vl, vr;
				Sound::CalculateStereo(shooter, 1.0f, &vl, &vr);
				if (!m_guns[i].sound.IsPlaying()) {
					m_guns[i].sound.Play(m_guns[i].gun_data.sound.c_str(), vl, vr, Sound::OP_REPEAT);
				} else {
					// update volume
					m_guns[i].sound.SetVolume(vl, vr);
				}
			} else {
				Sound::BodyMakeNoise(shooter, m_guns[i].gun_data.sound.c_str(), 1.0f);
			}
		} else if (!IsFiring(i) && m_guns[i].sound.IsPlaying()) {
			m_guns[i].sound.Stop();
		}
	}
	return fire;
}

MountId FixedGuns::FindFirstEmptyMount() const
{
	std::vector<MountId> free = FindEmptyMounts();
	if (free.empty()) return -1;
	else return free[0];
}

std::vector<MountId> FixedGuns::FindEmptyMounts() const
{
	std::vector<MountId> occupied;

	if (GetFreeMountsSize() == 0) return occupied;

	occupied.reserve(m_guns.size());

	std::for_each(begin(m_guns), end(m_guns), [&occupied](const GunStatus &gs) // <- Sure there's a better alghorithm
	{
		occupied.emplace_back(gs.mount_id);
	});

	std::sort(begin(occupied), end(occupied));

	std::vector<MountId> free;
	free.reserve(m_mounts.size() - occupied.size());

	for (unsigned mount = 0; mount < m_mounts.size(); mount++) {
		if (!std::binary_search(begin(occupied), end(occupied), mount)) {
			free.push_back(mount);
		}
	}
	return free;
}

bool FixedGuns::GetMountIsFront(MountId num) const
{
	if (unsigned(num) < m_mounts.size())
		if (m_mounts[num].dir == GunDir::GUN_FRONT) return true;
		else return false;
	else {
		Output("Given mount identifier (%u) is out of bounds (max is %lu)\n", num, m_mounts.size());
		return true;
	}
}

int FixedGuns::GetMountBarrels(MountId num) const
{
	if (unsigned(num) < m_mounts.size())
		return int(m_mounts[num].locs.size());
	else {
		Output("Given mount identifier (%u) is out of bounds (max is %lu)\n", num, m_mounts.size());
		return true;
	}
}

MountId FixedGuns::FindMountOfGun(const std::string &name) const
{
	std::vector<GunStatus>::const_iterator found = std::find_if(begin(m_guns), end(m_guns), [&name](const GunStatus &gs)
	{
		if (gs.gun_data.gun_name == name) {
			return true;
		} else return false;
	});
	if (found != m_guns.end()) {
		return (*found).mount_id;
	}
	return -1;
}

MountId FixedGuns::FindMountOfGun(GunId num) const
{
	std::vector<GunStatus>::const_iterator found = std::find_if(begin(m_guns), end(m_guns), [&num](const GunStatus &gs)
	{
		if (gs.mount_id == num) {
			return true;
		} else return false;
	});
	if (found != m_guns.end()) {
		return (*found).mount_id;
	}
	return -1;
}

GunId FixedGuns::FindGunOnMount(MountId num) const
{
	for (unsigned mount = 0; mount < m_guns.size(); mount ++) {
		if (m_guns[mount].mount_id == num ) return mount;
	}
	// no guns on given mount
	return -1;
}

void FixedGuns::SetActivationStateOfGun(GunId num, bool active)
{
	if (num < m_guns.size())
		m_guns[num].is_active = active;
	else {
		Output("Given gun identifier (%u) is out of bounds (max is %lu)\n", num, m_guns.size());
	}
}

bool FixedGuns::GetActivationStateOfGun(GunId num) const
{
	if (num < m_guns.size())
		return m_guns[num].is_active;
	else {
		Output("Given gun identifier (%u) is out of bounds (max is %lu)\n", num, m_guns.size());
		return false;
	}
}

int FixedGuns::GetNumAvailableBarrels(GunId num)
{
	if (num < m_guns.size())
		return std::min(m_guns[num].gun_data.barrels, unsigned(m_mounts[m_guns[num].mount_id].locs.size()));
	else {
		Output("Given gun identifier (%u) is out of bounds (max is %lu)\n", num, m_guns.size());
 		return false;
 	}
}

int FixedGuns::GetNumBarrels(GunId num)
{
	if (unsigned(num) < m_guns.size())
		return m_guns[num].gun_data.barrels;
	else {
		Output("Given gun identifier (%i) is out of bounds (max is %lu)\n", num, m_guns.size());
		return false;
	}
}

int FixedGuns::GetNumActiveBarrels(GunId num)
{
	if (unsigned(num) < m_guns.size())
		return m_guns[num].contemporary_barrels;
	else {
		Output("Given gun identifier (%i) is out of bounds (max is %lu)\n", num, m_guns.size());
		return false;
	}
}

void FixedGuns::CycleFireModeForGun(GunId num)
{
	if (unsigned(num) < m_guns.size()) {
		std::vector<int> &fire_modes = m_guns[num].fire_modes;
		std::vector<int>::iterator it = std::find(begin(fire_modes), end(fire_modes), m_guns[num].contemporary_barrels);

		if (it == fire_modes.end()) throw std::runtime_error {"What?! In FixedGuns::CycleFireModeForGun seems actual fire mode don't exist...\n"};

		++it;
		if (it == fire_modes.end())
			m_guns[num].contemporary_barrels = 1;
		else m_guns[num].contemporary_barrels = (*it);
	} else {
		Output("Given gun identifier (%i) is out of bounds (max is %lu)\n", num, m_guns.size());
		return;
	}
}

GunDir FixedGuns::IsFront(GunId num) const
{
	if (unsigned(num) < m_guns.size())
		return m_mounts[m_guns[num].mount_id].dir;
	else
		return GunDir::GUNMOUNT_MAX;
}

bool FixedGuns::IsFiring() const
{
	for (unsigned j = 0; j < m_guns.size(); j++)
		if (m_guns[j].is_firing) return true;
	return false;
}

bool FixedGuns::IsFiring(GunId num) const
{
	return m_guns[num].is_firing;
}

bool FixedGuns::IsBeam(GunId num) const
{
	return m_guns[num].gun_data.projData.beam;
}

float FixedGuns::GetGunTemperature(GunId idx) const
{
	if (unsigned(idx) < m_guns.size())
		return m_guns[idx].temperature_stat;
	else
		return 0.0f;
}

const std::string &FixedGuns::GetGunName(GunId idx) const
{
	static std::string empty_str("");
	if (unsigned(idx) < m_guns.size())
		return m_guns[idx].gun_data.gun_name;
	else
		return empty_str;
}

FixedGuns::GunData::GunData(const Json &jsonObj)
{
	try {
		gun_name = jsonObj["gd_name"];
		sound = jsonObj["gd_sound"];
		barrels = jsonObj["gd_barrels"];
		recharge = jsonObj["gd_recharge"];
		temp_cool_rate = jsonObj["gd_cool_rate"];
		temp_heat_rate = jsonObj["gd_heat_rate"];
		// Load "ProjectileData" for this gun:
		projData = ProjectileData(jsonObj["proj_data"]);
	} catch (Json::type_error &) {
		Output("Loading error in '%s' in function '%s' \n", __FILE__, __func__);
		throw SavedGameCorruptException();
	}
}

Json FixedGuns::GunData::SaveToJson() const
{
	Json jsonObj;
	jsonObj["gd_name"] = gun_name;
	jsonObj["gd_sound"] = sound;
	jsonObj["gd_barrels"] = barrels;
	jsonObj["gd_recharge"] = recharge;
	jsonObj["gd_cool_rate"] = temp_cool_rate;
	jsonObj["gd_heat_rate"] = temp_heat_rate;
	// Save "ProjectileData" for this gun:
	jsonObj["proj_data"] = projData.SaveToJson();
	return jsonObj;
}

/* Given a number will return their factors,
 / which in turn are the number of barrels
 / which can be fired simoultaneusly
*/
std::vector<int> calculateFireModes(int b)
{
	std::vector<int> fire_modes;
	fire_modes.reserve(b / 2);
	for (int factor = 1; factor <= b / 2; factor++)
		if (b % factor == 0) fire_modes.push_back(factor);
	fire_modes.push_back(b);
	fire_modes.shrink_to_fit();

	return fire_modes;
}

void FixedGuns::GunStatus::UpdateFireModes(const Mount &mount)
{
	int max_barrels = std::min(gun_data.barrels, unsigned(mount.locs.size()));
	fire_modes = calculateFireModes(max_barrels);
	contemporary_barrels = 1;
	next_firing_barrels = 1;
}

const std::vector<int> &FixedGuns::GunStatus::GetFiringBarrelsAndAdvance()
{
	m_cont.reserve(contemporary_barrels);
	m_cont.clear();

	for (int i = next_firing_barrels; i < contemporary_barrels + next_firing_barrels; i++)
		m_cont.push_back(i);

	next_firing_barrels += contemporary_barrels;
	if (next_firing_barrels >= gun_data.barrels) next_firing_barrels = 0;
	return m_cont;
}
