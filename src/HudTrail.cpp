// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "HudTrail.h"

#include "Body.h"
#include "Frame.h"
#include "graphics/RenderState.h"
#include "graphics/Renderer.h"
#include "graphics/RendererLocator.h"

const float UPDATE_INTERVAL = 0.1f;
const uint16_t MAX_POINTS = 100;

HudTrail::HudTrail(Body *b, const Color &c) :
	m_body(b),
	m_currentFrame(b->GetFrame()),
	m_updateTime(0.f),
	m_color(c)
{
	Graphics::RenderStateDesc rsd;
	rsd.blendMode = Graphics::BLEND_ALPHA_ONE;
	rsd.depthWrite = false;
	m_renderState = RendererLocator::getRenderer()->CreateRenderState(rsd);
}

void HudTrail::Update(float time)
{
	PROFILE_SCOPED();
	//record position
	m_updateTime += time;
	if (m_updateTime > UPDATE_INTERVAL) {
		m_updateTime = 0.f;
		FrameId bodyFrameId = m_body->GetFrame();

		if (!m_currentFrame) {
			m_currentFrame = bodyFrameId;
			m_trailPoints.clear();
		}

		if (bodyFrameId == m_currentFrame)
			m_trailPoints.push_back(m_body->GetInterpPosition());
	}

	while (m_trailPoints.size() > MAX_POINTS)
		m_trailPoints.pop_front();
}

void HudTrail::Render()
{
	PROFILE_SCOPED();
	//render trail
	if (m_trailPoints.size() > 1) {
		const vector3d vpos = m_transform * m_body->GetInterpPosition();
		m_transform[12] = vpos.x;
		m_transform[13] = vpos.y;
		m_transform[14] = vpos.z;
		m_transform[15] = 1.0;

		static std::vector<vector3f> tvts;
		static std::vector<Color> colors;
		tvts.clear();
		colors.clear();
		const vector3d curpos = m_body->GetInterpPosition();
		tvts.reserve(MAX_POINTS);
		colors.reserve(MAX_POINTS);
		tvts.push_back(vector3f(0.f));
		colors.push_back(Color::BLANK);
		float alpha = 1.f;
		const float decrement = 1.f / m_trailPoints.size();
		const Color tcolor = m_color;
		for (size_t i = m_trailPoints.size() - 1; i > 0; i--) {
			tvts.push_back(-vector3f(curpos - m_trailPoints[i]));
			alpha -= decrement;
			colors.push_back(tcolor);
			colors.back().a = uint8_t(alpha * 255);
		}

		RendererLocator::getRenderer()->SetTransform(m_transform);
		m_lines.SetData(tvts.size(), &tvts[0], &colors[0]);
		m_lines.Draw(RendererLocator::getRenderer(), m_renderState, Graphics::LINE_STRIP);
	}
}

void HudTrail::Reset(FrameId newFrame)
{
	m_currentFrame = newFrame;
	m_trailPoints.clear();
}
