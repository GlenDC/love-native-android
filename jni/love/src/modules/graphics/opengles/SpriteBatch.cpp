/**
* Copyright (c) 2006-2012 LOVE Development Team
*
* This software is provided 'as-is', without any express or implied
* warranty.  In no event will the authors be held liable for any damages
* arising from the use of this software.
*
* Permission is granted to anyone to use this software for any purpose,
* including commercial applications, and to alter it and redistribute it
* freely, subject to the following restrictions:
*
* 1. The origin of this software must not be misrepresented; you must not
*    claim that you wrote the original software. If you use this software
*    in a product, an acknowledgment in the product documentation would be
*    appreciated but is not required.
* 2. Altered source versions must be plainly marked as such, and must not be
*    misrepresented as being the original software.
* 3. This notice may not be removed or altered from any source distribution.
**/

#include "SpriteBatch.h"

// STD
#include <iostream>

// LOVE
#include "Image.h"
#include "Quad.h"
#include "VertexBuffer.h"
#include "PixelEffect.h"

namespace love
{
namespace graphics
{
namespace opengles
{
	SpriteBatch::SpriteBatch(Image * image, int size, int usage, std::stack<love::Matrix*> &projMatrix, std::stack<love::Matrix*> &modelViewMatrix, float *curColor, PixelEffect *primitivesEffect)
		: image(image)
		, size(size)
		, next(0)
		, color(0)
		, array_buf(0)
		, element_buf(0)
		, projMatrix(projMatrix)
		, modelViewMatrix(modelViewMatrix)
		, curColor(curColor)
		, primitivesEffect(primitivesEffect)
	{
		image->retain();

		GLenum gl_usage;

		switch (usage)
		{
		default:
		case USAGE_DYNAMIC:
			gl_usage = GL_DYNAMIC_DRAW;
			break;
		case USAGE_STATIC:
			gl_usage = GL_STATIC_DRAW;
			break;
		case USAGE_STREAM:
			gl_usage = GL_STREAM_DRAW;
			break;
		}

		int vertex_size = sizeof(vertex) * 4 * size;
		int element_size = sizeof(GLushort) * 6 * size;

		array_buf = VertexBuffer::Create(vertex_size, GL_ARRAY_BUFFER, gl_usage);
		element_buf = VertexBuffer::Create(element_size, GL_ELEMENT_ARRAY_BUFFER, GL_STATIC_DRAW);

		// Fill element buffer.
		{
			VertexBuffer::Bind bind(*element_buf);
			VertexBuffer::Mapper mapper(*element_buf);

			GLushort *indices = static_cast<GLushort*>(mapper.get());

			if (indices)
			{
				for (int i = 0; i < size; ++i)
				{
					indices[i*6+0] = 0+(i*4);
					indices[i*6+1] = 1+(i*4);
					indices[i*6+2] = 2+(i*4);

					indices[i*6+3] = 0+(i*4);
					indices[i*6+4] = 2+(i*4);
					indices[i*6+5] = 3+(i*4);
				}
			}
		}
	}

	SpriteBatch::~SpriteBatch()
	{
		image->release();

		delete color;
		delete array_buf;
		delete element_buf;
	}

	int SpriteBatch::add(float x, float y, float a, float sx, float sy, float ox, float oy, float kx, float ky, int index /*= -1*/)
	{
		// Only do this if there's a free slot.
		if ((index == -1 && next >= size) || index < -1 || index >= size)
			return -1;

		// Needed for colors.
		memcpy(sprite, image->getVertices(), sizeof(vertex)*4);

		// Transform.
		Matrix t;
		t.setTransformation(x, y, a, sx, sy, ox, oy, kx, ky);
		t.transform(sprite, sprite, 4);

		if (color)
			setColorv(sprite, *color);

		addv(sprite, (index == -1 ? next : index));

		// Increment counter.
		if (index == -1)
			return next++;
		return index;
	}

	int SpriteBatch::addq(Quad * quad, float x, float y, float a, float sx, float sy, float ox, float oy, float kx, float ky, int index /*= -1*/)
	{
		// Only do this if there's a free slot.
		if ((index == -1 && next >= size) || index < -1 || index >= next)
			return -1;

		// Needed for colors.
		memcpy(sprite, quad->getVertices(), sizeof(vertex)*4);

		// Transform.
		Matrix t;
		t.setTransformation(x, y, a, sx, sy, ox, oy, kx, ky);
		t.transform(sprite, sprite, 4);

		if (color)
			setColorv(sprite, *color);

		addv(sprite, (index == -1 ? next : index));

		// Increment counter.
		if (index == -1)
			return next++;
		return index;
	}

	void SpriteBatch::clear()
	{
		// Reset the position of the next index.
		next = 0;
	}

	void * SpriteBatch::lock()
	{
		VertexBuffer::Bind bind(*array_buf);

		return array_buf->map();
	}

	void SpriteBatch::unlock()
	{
		VertexBuffer::Bind bind(*array_buf);

		array_buf->unmap();
	}

	void SpriteBatch::setImage(Image * newimage)
	{
		image->release();
		image = newimage;
		image->retain();
	}

	Image *SpriteBatch::getImage()
	{
		return image;
	}

	void SpriteBatch::setColor(const Color & color)
	{
		if (!this->color)
			this->color = new Color(color);
		else
			*(this->color) = color;
	}

	void SpriteBatch::setColor()
	{
		delete color;
		color = 0;
	}

	void SpriteBatch::draw(float x, float y, float angle, float sx, float sy, float ox, float oy, float kx, float ky) const
	{
		const int color_offset = 0;
		const int vertex_offset = sizeof(unsigned char) * 4;
		const int texel_offset = sizeof(unsigned char) * 4 + sizeof(float) * 2;

		static Matrix t;

		modelViewMatrix.push(new love::Matrix(*modelViewMatrix.top()));
		t.setTransformation(x, y, angle, sx, sy, ox, oy, kx, ky);
		*modelViewMatrix.top() *= t;

		image->bind();

		VertexBuffer::Bind array_bind(*array_buf);
		VertexBuffer::Bind element_bind(*element_buf);

		bool useStdShader = false;
		if(PixelEffect::current == NULL)
		{
			useStdShader = true;
			primitivesEffect->attach();
		}

		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), array_buf->getPointer(vertex_offset));
		if(color)
		{
			glVertexAttribPointer(0, 4, GL_UNSIGNED_BYTE, GL_FALSE, sizeof(vertex), array_buf->getPointer(color_offset));
			glEnableVertexAttribArray(1);
		}
		else
		{
			glVertexAttrib4fv(1, curColor);
		}
		
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), array_buf->getPointer(texel_offset));

		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(2);

		PixelEffect::current->bindAttribLocation("position", 0);
		PixelEffect::current->bindAttribLocation("colour", 1);
		PixelEffect::current->bindAttribLocation("texCoord", 2);
		
		Matrix mvp = *modelViewMatrix.top() * *projMatrix.top();
		PixelEffect::current->sendMatrix("mvp", 4, mvp.getElements(), 1);

		glDrawElements(GL_TRIANGLES, next*6, GL_UNSIGNED_SHORT, element_buf->getPointer(0));

		glDisableVertexAttribArray(0);
		glDisableVertexAttribArray(1);
		glDisableVertexAttribArray(2);

		if(useStdShader == true)
		  primitivesEffect->detach();

		delete modelViewMatrix.top();
		modelViewMatrix.pop();
	}

	void SpriteBatch::addv(const vertex * v, int index)
	{
		int sprite_size = sizeof(vertex) * 4;

		VertexBuffer::Bind bind(*array_buf);

		array_buf->fill(index * sprite_size, sprite_size, v);
	}

	void SpriteBatch::setColorv(vertex * v, const Color & color)
	{
		v[0].r = color.r; v[0].g = color.g; v[0].b = color.b; v[0].a = color.a;
		v[1].r = color.r; v[1].g = color.g; v[1].b = color.b; v[1].a = color.a;
		v[2].r = color.r; v[2].g = color.g; v[2].b = color.b; v[2].a = color.a;
		v[3].r = color.r; v[3].g = color.g; v[3].b = color.b; v[3].a = color.a;
	}

	bool SpriteBatch::getConstant(const char * in, UsageHint & out)
	{
		return usageHints.find(in, out);
	}

	bool SpriteBatch::getConstant(UsageHint in, const char *& out)
	{
		return usageHints.find(in, out);
	}

	StringMap<SpriteBatch::UsageHint, SpriteBatch::USAGE_MAX_ENUM>::Entry SpriteBatch::usageHintEntries[] =
	{
		{"dynamic", SpriteBatch::USAGE_DYNAMIC},
		{"static", SpriteBatch::USAGE_STATIC},
		{"stream", SpriteBatch::USAGE_STREAM},
	};

	StringMap<SpriteBatch::UsageHint, SpriteBatch::USAGE_MAX_ENUM> SpriteBatch::usageHints(usageHintEntries, sizeof(usageHintEntries));
} // opengl
} // graphics
} // love