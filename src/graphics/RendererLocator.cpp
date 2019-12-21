#include "RendererLocator.h"

Graphics::Renderer *RendererLocator::s_renderer = nullptr;

void RendererLocator::provideRenderer(Graphics::Renderer *r)
{
	s_renderer = r;
}
