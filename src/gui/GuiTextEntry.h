// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _GUITEXTENTRY_H
#define _GUITEXTENTRY_H

#include "GuiWidget.h"
#include <string>

#include "graphics/Drawables.h"
#include "utils.h"

namespace Text {
	class TextureFont;
}

namespace Gui {
	class TextEntry : public Widget {
	public:
		enum NewlineMode {
			IgnoreNewline,
			AcceptNewline,
			AcceptCtrlNewline
		};

		TextEntry();
		virtual ~TextEntry();
		virtual void GetSizeRequested(float size[2]);
		virtual void Draw();
		virtual bool OnMouseDown(MouseButtonEvent *e);
		void SetText(const std::string &text);
		std::string GetText() const { return m_text; }
		void SetCursorPos(int pos) { m_cursPos = Clamp(pos, 0, signed(m_text.size())); }
		int GetCursorPos() const { return m_cursPos; };
		virtual bool OnKeyDown(const SDL_Keysym *);
		virtual void OnTextInput(Uint32 unicode);
		virtual void Show()
		{
			GrabFocus();
			Widget::Show();
		}
		virtual void GrabFocus();
		void Unfocus();
		NewlineMode GetNewlineMode() const { return m_newlineMode; }
		void SetNewlineMode(NewlineMode mode) { m_newlineMode = mode; }

		sigc::signal<void, const SDL_Keysym *> onKeyPress;
		sigc::signal<void> onValueChanged;

	private:
		void OnRawMouseDown(MouseButtonEvent *e);

		std::string m_text;
		int m_cursPos;
		int m_scroll;
		RefCountedPtr<Text::TextureFont> m_font;
		NewlineMode m_newlineMode;
		int m_newlineCount;

		bool m_justFocused;
		sigc::connection m_clickout;

		Graphics::Drawables::Lines m_cursorLines;
		Graphics::Drawables::Lines m_outlines;
		std::unique_ptr<Graphics::Drawables::Rect> m_background;
		RefCountedPtr<Graphics::VertexBuffer> m_vb;
	};
} // namespace Gui

#endif /* _GUITEXTENTRY_H */
