// Copyright � 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "graphics/VertexBuffer.h"

#include "libs/vector2.h"
#include "libs/vector3.h"
#include "Color.h"

#include <cassert>
#include <algorithm>

namespace Graphics {

	uint32_t VertexBufferDesc::GetAttribSize(VertexAttribFormat f)
	{
		switch (f) {
		case ATTRIB_FORMAT_FLOAT2:
			return 8;
		case ATTRIB_FORMAT_FLOAT3:
			return 12;
		case ATTRIB_FORMAT_FLOAT4:
			return 16;
		case ATTRIB_FORMAT_UBYTE4:
			return 4;
		default:
			return 0;
		}
	}

	VertexBufferDesc::VertexBufferDesc() :
		numVertices(0),
		stride(0),
		usage(BUFFER_USAGE_STATIC)
	{
		for (uint32_t i = 0; i < MAX_ATTRIBS; i++) {
			attrib[i].semantic = ATTRIB_NONE;
			attrib[i].format = ATTRIB_FORMAT_NONE;
			attrib[i].offset = 0;
		}

		assert(sizeof(vector2f) == 8);
		assert(sizeof(vector3f) == 12);
		assert(sizeof(Color4ub) == 4);
	}

	uint32_t VertexBufferDesc::GetOffset(VertexAttrib attr) const
	{
		for (uint32_t i = 0; i < MAX_ATTRIBS; i++) {
			if (attrib[i].semantic == attr)
				return attrib[i].offset;
		}

		//attrib not found
		assert(false);
		return 0;
	}

	uint32_t VertexBufferDesc::CalculateOffset(const VertexBufferDesc &desc, VertexAttrib attr)
	{
		uint32_t offs = 0;
		for (uint32_t i = 0; i < MAX_ATTRIBS; i++) {
			if (desc.attrib[i].semantic == attr)
				return offs;
			offs += GetAttribSize(desc.attrib[i].format);
		}

		//attrib not found
		assert(false);
		return 0;
	}

	VertexBuffer::~VertexBuffer()
	{
	}

	bool VertexBuffer::SetVertexCount(uint32_t v)
	{
		if (v <= m_desc.numVertices) {
			m_size = v;
			return true;
		}
		return false;
	}

	// ------------------------------------------------------------
	IndexBuffer::IndexBuffer(uint32_t size, BufferUsage usage) :
		Mappable(size),
		m_indexCount(size),
		m_usage(usage)
	{
	}

	IndexBuffer::~IndexBuffer()
	{
	}

	void IndexBuffer::SetIndexCount(uint32_t ic)
	{
		assert(ic <= GetSize());
		m_indexCount = std::min(ic, GetSize());
	}

	// ------------------------------------------------------------
	InstanceBuffer::InstanceBuffer(uint32_t size, BufferUsage usage) :
		Mappable(size),
		m_instanceCount(0),
		m_usage(usage)
	{
	}

	InstanceBuffer::~InstanceBuffer()
	{
	}

	void InstanceBuffer::SetInstanceCount(const uint32_t ic)
	{
		assert(ic <= GetSize());
		m_instanceCount = std::min(ic, GetSize());
	}

} // namespace Graphics
