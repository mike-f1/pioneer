// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef DUMMY_VERTEXBUFFER_H
#define DUMMY_VERTEXBUFFER_H

#include "graphics/VertexBuffer.h"

namespace Graphics {

	namespace Dummy {

		class VertexBuffer : public Graphics::VertexBuffer {
		public:
			VertexBuffer(const VertexBufferDesc &d) :
				Graphics::VertexBuffer(d),
				m_buffer(new uint8_t[m_desc.numVertices * m_desc.stride])
			{}

			// copies the contents of the VertexArray into the buffer
			virtual bool Populate(const VertexArray &) override final { return true; }

			// change the buffer data without mapping
			virtual void BufferData(const size_t, void *) override final {}

			virtual void Bind() override final {}
			virtual void Release() override final {}

			virtual void Unmap() override final {}

		protected:
			virtual uint8_t *MapInternal(BufferMapMode) override final { return m_buffer.get(); }

		private:
			std::unique_ptr<uint8_t[]> m_buffer;
		};

		class IndexBuffer : public Graphics::IndexBuffer {
		public:
			IndexBuffer(uint32_t size, BufferUsage bu) :
				Graphics::IndexBuffer(size, bu),
				m_buffer(new uint32_t[size]){};

			virtual uint32_t *Map(BufferMapMode) override final { return m_buffer.get(); }
			virtual void Unmap() override final {}

			virtual void BufferData(const size_t, void *) override final {}

			virtual void Bind() override final {}
			virtual void Release() override final {}

		private:
			std::unique_ptr<uint32_t[]> m_buffer;
		};

		// Instance buffer
		class InstanceBuffer : public Graphics::InstanceBuffer {
		public:
			InstanceBuffer(uint32_t size, BufferUsage hint) :
				Graphics::InstanceBuffer(size, hint),
				m_data(new matrix4x4f[size])
			{}
			virtual ~InstanceBuffer(){};
			virtual matrix4x4f *Map(BufferMapMode) override final { return m_data.get(); }
			virtual void Unmap() override final {}

			uint32_t GetSize() const { return m_size; }
			BufferUsage GetUsage() const { return m_usage; }

			virtual void Bind() override final {}
			virtual void Release() override final {}

		protected:
			std::unique_ptr<matrix4x4f> m_data;
		};

	} // namespace Dummy
} // namespace Graphics

#endif
