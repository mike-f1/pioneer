// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef GRAPHICS_VERTEXBUFFER_H
#define GRAPHICS_VERTEXBUFFER_H
/**
 * A Vertex Buffer is created by filling out a
 * description struct with desired vertex attributes
 * and calling renderer->CreateVertexBuffer.
 * Can be used in combination with IndexBuffer,
 * for optimal rendering of complex geometry.
 * Call Map to write/read from the buffer, and Unmap to
 * commit the changes.
 * Buffers come in two usage flavors, static and dynamic.
 * Use Static buffer, when the geometry never changes.
 * Avoid mapping a buffer for reading, as it may be slow,
 * especially with static buffers.
 *
 * Expansion possibilities: range-based Map
 */
#include "Types.h"

#include <cstdint>
#include "matrix4x4.h"
#include "RefCounted.h"

namespace Graphics {

	// fwd declaration
	class VertexArray;

	const uint32_t MAX_ATTRIBS = 8;

	struct VertexAttribDesc {
		//position, texcoord, normal etc.
		VertexAttrib semantic;
		//float3, float2 etc.
		VertexAttribFormat format;
		//byte offset of the attribute, if zero this
		//is automatically filled for created buffers
		uint32_t offset;
	};

	struct VertexBufferDesc {
		VertexBufferDesc();
		//byte offset of an existing attribute
		uint32_t GetOffset(VertexAttrib) const;

		//used internally for calculating offsets
		static uint32_t CalculateOffset(const VertexBufferDesc &, VertexAttrib);
		static uint32_t GetAttribSize(VertexAttribFormat);

		//semantic ATTRIB_NONE ends description (when not using all attribs)
		VertexAttribDesc attrib[MAX_ATTRIBS];
		uint32_t numVertices;
		//byte size of one vertex, if zero this is
		//automatically calculated for created buffers
		uint32_t stride;
		BufferUsage usage;
	};

	class Mappable : public RefCounted {
	public:
		virtual ~Mappable() {}
		virtual void Unmap() = 0;

		inline uint32_t GetSize() const { return m_size; }
		inline uint32_t GetCapacity() const { return m_capacity; }

	protected:
		explicit Mappable(const uint32_t size) :
			m_mapMode(BUFFER_MAP_NONE),
			m_size(size),
			m_capacity(size) {}
		BufferMapMode m_mapMode; //tracking map state

		// size is the current number of elements in the buffer
		uint32_t m_size;
		// capacity is the maximum number of elements that can be put in the buffer
		uint32_t m_capacity;
	};

	class VertexBuffer : public Mappable {
	public:
		VertexBuffer(const VertexBufferDesc &desc) :
			Mappable(desc.numVertices),
			m_desc(desc) {}
		virtual ~VertexBuffer();
		const VertexBufferDesc &GetDesc() const { return m_desc; }

		template <typename T>
		T *Map(BufferMapMode mode)
		{
			return reinterpret_cast<T *>(MapInternal(mode));
		}

		//Vertex count used for rendering.
		//By default the maximum set in description, but
		//you may set a smaller count for partial rendering
		bool SetVertexCount(uint32_t);

		// copies the contents of the VertexArray into the buffer
		virtual bool Populate(const VertexArray &) = 0;

		// change the buffer data without mapping
		virtual void BufferData(const size_t, void *) = 0;

		virtual void Bind() = 0;
		virtual void Release() = 0;

	protected:
		virtual uint8_t *MapInternal(BufferMapMode) = 0;
		VertexBufferDesc m_desc;
	};

	// Index buffer
	class IndexBuffer : public Mappable {
	public:
		IndexBuffer(uint32_t size, BufferUsage);
		virtual ~IndexBuffer();
		virtual uint32_t *Map(BufferMapMode) = 0;

		// change the buffer data without mapping
		virtual void BufferData(const size_t, void *) = 0;

		uint32_t GetIndexCount() const { return m_indexCount; }
		void SetIndexCount(uint32_t);
		BufferUsage GetUsage() const { return m_usage; }

		virtual void Bind() = 0;
		virtual void Release() = 0;

	protected:
		uint32_t m_indexCount;
		BufferUsage m_usage;
	};

	// Instance buffer
	class InstanceBuffer : public Mappable {
	public:
		InstanceBuffer(uint32_t size, BufferUsage);
		virtual ~InstanceBuffer();
		virtual matrix4x4f *Map(BufferMapMode) = 0;

		uint32_t GetInstanceCount() const { return m_instanceCount; }
		void SetInstanceCount(const uint32_t);
		BufferUsage GetUsage() const { return m_usage; }

		virtual void Bind() = 0;
		virtual void Release() = 0;

	protected:
		uint32_t m_instanceCount;
		BufferUsage m_usage;
	};

} // namespace Graphics
#endif // GRAPHICS_VERTEXBUFFER_H
