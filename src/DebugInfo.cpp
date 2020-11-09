#include "DebugInfo.h"

#include <stddef.h>

#include <imgui/imgui.h>
#include <SDL_timer.h>

#include "Game.h"
#include "GameLocator.h"
#include "Frame.h"
#include "Player.h"
#include "graphics/Renderer.h"
#include "graphics/RendererLocator.h"
#include "graphics/Graphics.h"
#include "graphics/Light.h"
#include "graphics/Stats.h"
#include "galaxy/SystemPath.h"
#include "galaxy/SystemBody.h"
#include "libs/StringF.h"
#include "libs/utils.h"
#include "text/TextureFont.h"

void DebugInfo::NewCycle()
{
	m_last_stats = SDL_GetTicks();
	m_frame_stat = 0;
	m_phys_stat = 0;
}

void DebugInfo::Update()
{
	if (SDL_GetTicks() - m_last_stats < 1000) return;

	std::ostringstream ss;

	size_t lua_mem = Lua::manager->GetMemoryUsage();
	int lua_memB = int(lua_mem & ((1u << 10) - 1));
	int lua_memKB = int(lua_mem >> 10) % 1024;
	int lua_memMB = int(lua_mem >> 20);
	const Graphics::Stats::TFrameData &stats = RendererLocator::getRenderer()->GetStats().FrameStatsPrevious();
	const uint32_t numDrawCalls = stats.m_stats[Graphics::Stats::STAT_DRAWCALL];
	const uint32_t numBuffersCreated = stats.m_stats[Graphics::Stats::STAT_CREATE_BUFFER];
	const uint32_t numDrawTris = stats.m_stats[Graphics::Stats::STAT_DRAWTRIS];
	const uint32_t numDrawPointSprites = stats.m_stats[Graphics::Stats::STAT_DRAWPOINTSPRITES];
	const uint32_t numDrawBuildings = stats.m_stats[Graphics::Stats::STAT_BUILDINGS];
	const uint32_t numDrawCities = stats.m_stats[Graphics::Stats::STAT_CITIES];
	const uint32_t numDrawGroundStations = stats.m_stats[Graphics::Stats::STAT_GROUNDSTATIONS];
	const uint32_t numDrawSpaceStations = stats.m_stats[Graphics::Stats::STAT_SPACESTATIONS];
	const uint32_t numDrawAtmospheres = stats.m_stats[Graphics::Stats::STAT_ATMOSPHERES];
	const uint32_t numDrawPatches = stats.m_stats[Graphics::Stats::STAT_PATCHES];
	const uint32_t numDrawPlanets = stats.m_stats[Graphics::Stats::STAT_PLANETS];
	const uint32_t numDrawGasGiants = stats.m_stats[Graphics::Stats::STAT_GASGIANTS];
	const uint32_t numDrawStars = stats.m_stats[Graphics::Stats::STAT_STARS];
	const uint32_t numDrawShips = stats.m_stats[Graphics::Stats::STAT_SHIPS];
	const uint32_t numDrawBillBoards = stats.m_stats[Graphics::Stats::STAT_BILLBOARD];
	const uint32_t numDrawPatchesTris = stats.m_stats[Graphics::Stats::STAT_PATCHES_TRIS];
	ss << m_frame_stat << " fps (" << (1000.0 / m_frame_stat) << " ms/f) " << m_phys_stat << " phys updates\n" ;
	ss << numDrawPatchesTris << " triangles, " << numDrawPatchesTris * m_frame_stat * 1e-6 << "M tris/sec," << Text::TextureFont::GetGlyphCount() << " glyphs/sec, " << numDrawPatches << " patches/frame\n";
	ss << "Lua mem usage: " << lua_memMB << "MB + " << lua_memKB << " KB + " << lua_memB << " bytes (stack top: " << lua_gettop(Lua::manager->GetLuaState()) << ")\n";
	ss << "Draw Calls (" << numDrawCalls << "), of which were:\n Tris (" << numDrawTris << "), Point Sprites (" << numDrawPointSprites << "), Billboards (" << numDrawBillBoards << ")\n";
	ss << "Buildings (" << numDrawBuildings << "), Cities (" << numDrawCities << "), GroundStations (" << numDrawGroundStations << "), SpaceStations (" << numDrawSpaceStations << "), Atmospheres (" << numDrawAtmospheres << ")\n";
	ss << "Patches (" << numDrawPatches << "), Planets (" << numDrawPlanets << "), GasGiants (" << numDrawGasGiants << "), Stars (" << numDrawStars << "), Ships (" << numDrawShips << ")\n";
	ss << "Buffers Created(" << numBuffersCreated << ")\n";

	if (GameLocator::getGame() && GameLocator::getGame()->GetPlayer()->GetFlightState() != Ship::HYPERSPACE) {
		vector3d pos = GameLocator::getGame()->GetPlayer()->GetPosition();
		vector3d abs_pos = GameLocator::getGame()->GetPlayer()->GetPositionRelTo(Frame::GetRootFrameId());

		const Frame *playerFrame = Frame::GetFrame(GameLocator::getGame()->GetPlayer()->GetFrame());

		ss << "\nPlayer:\n";
		ss << stringf("Pos: %0{f.2}, %1{f.2}, %2{f.2}\n", pos.x, pos.y, pos.z);
		ss << stringf("AbsPos: %0{f.2}, %1{f.2}, %2{f.2}\n", abs_pos.x, abs_pos.y, abs_pos.z);

		const SystemPath &path(playerFrame->GetSystemBody()->GetPath());
		ss << stringf("Rel-to: %0 [%1{d},%2{d},%3{d},%4{u},%5{u}] ",
			playerFrame->GetLabel(),
			path.sectorX, path.sectorY, path.sectorZ, path.systemIndex, path.bodyIndex);
		ss << stringf("(%0{f.2} km), rotating: %1, has rotation: %2\n",
			pos.Length() / 1000, (playerFrame->IsRotFrame() ? "yes" : "no"), (playerFrame->HasRotFrame() ? "yes" : "no"));

		//Calculate lat/lon for ship position
		const vector3d dir = pos.NormalizedSafe();
		const float lat = RAD2DEG(asin(dir.y));
		const float lon = RAD2DEG(atan2(dir.x, dir.z));

		ss << stringf("Lat / Lon: %0{f.8} / %1{f.8}\n", lat, lon);

		char aibuf[256];
		GameLocator::getGame()->GetPlayer()->AIGetStatusText(aibuf);
		aibuf[255] = 0;
		ss << aibuf << std::endl;
	}

	m_dbg_text = ss.str();

	m_frame_stat = 0;
	m_phys_stat = 0;
	Text::TextureFont::ClearGlyphCount();
	if (SDL_GetTicks() - m_last_stats > 1200)
		m_last_stats = SDL_GetTicks();
	else
		m_last_stats += 1000;
}

void DebugInfo::Print()
{
	int32_t viewport[4];
	RendererLocator::getRenderer()->GetCurrentViewport(&viewport[0]);
	ImVec2 pos(0.0, 0.7);
	pos.x = pos.x * viewport[2] + viewport[0];
	pos.y = pos.y * viewport[3] + viewport[1];
	pos.y = RendererLocator::getRenderer()->GetWindowHeight() - pos.y;

	ImVec2 size = ImGui::CalcTextSize(m_dbg_text.c_str());
	ImGuiStyle& style = ImGui::GetStyle();
	size.x += style.WindowPadding.x * 2;
	size.y += style.WindowPadding.y * 2;

	pos.y -= size.y / 2.0;
	ImGui::SetNextWindowBgAlpha(0.7f);
	ImGui::Begin("dbg", nullptr, ImGuiWindowFlags_NoTitleBar
				| ImGuiWindowFlags_NoResize
				| ImGuiWindowFlags_NoMove
				| ImGuiWindowFlags_NoScrollbar
				| ImGuiWindowFlags_NoCollapse
				| ImGuiWindowFlags_NoSavedSettings
				| ImGuiWindowFlags_NoFocusOnAppearing
				| ImGuiWindowFlags_NoBringToFrontOnFocus
				);
	ImGui::SetWindowPos(pos);
	ImGui::SetWindowSize(size);
	ImVec4 color(1.0f, 1.0f, 1.0f, 1.0);
	ImGui::PushStyleColor(ImGuiCol_Text, color);
	ImGui::TextUnformatted(m_dbg_text.c_str());
	ImGui::PopStyleColor(1);
	ImGui::End();
}
