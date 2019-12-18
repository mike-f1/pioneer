#ifndef RENDERERLOCATOR_H
#define RENDERERLOCATOR_H

namespace Graphics {
	class Renderer;
}

class RendererLocator
{
public:
	RendererLocator() = delete;

	static Graphics::Renderer *getRenderer() { return s_renderer; };
	static void provideRenderer(Graphics::Renderer *r);

private:
	static Graphics::Renderer *s_renderer;
};

#endif // RENDERERLOCATOR_H
