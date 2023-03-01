/*
	==============================================================================

		OpenGlMultiQuad.cpp
		Created: 14 Dec 2022 2:05:11am
		Author:  theuser27

	==============================================================================
*/

#include "OpenGLMultiQuad.h"

namespace Interface
{
	using namespace juce::gl;

	OpenGlMultiQuad::OpenGlMultiQuad(int max_quads, Shaders::FragmentShader shader) :
		fragmentShader_(shader), maxQuads_(max_quads), numQuads_(max_quads)
	{
		static const int triangles[] = {
			0, 1, 2,
			2, 3, 0
		};

		data_ = std::make_unique<float[]>(maxQuads_ * kNumFloatsPerQuad);
		indices_ = std::make_unique<int[]>(maxQuads_ * kNumIndicesPerQuad);
		vertexBuffer_ = 0;
		indicesBuffer_ = 0;

		modColor_ = Colours::transparentBlack;

		for (u32 i = 0; i < maxQuads_; ++i)
		{
			setCoordinates(i, -1.0f, -1.0f, 2.0f, 2.0f);
			setShaderValue(i, 1.0f);

			for (u32 j = 0; j < kNumIndicesPerQuad; ++j)
				indices_[i * kNumIndicesPerQuad + j] = triangles[j] + i * kNumVertices;
		}

		setInterceptsMouseClicks(false, false);
	}

	void OpenGlMultiQuad::init(OpenGlWrapper &openGl)
	{
		glGenBuffers(1, &vertexBuffer_);
		glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer_);

		GLsizeiptr vert_size = static_cast<GLsizeiptr>(maxQuads_ * kNumFloatsPerQuad * sizeof(float));
		glBufferData(GL_ARRAY_BUFFER, vert_size, data_.get(), GL_STATIC_DRAW);

		glGenBuffers(1, &indicesBuffer_);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indicesBuffer_);

		GLsizeiptr bar_size = static_cast<GLsizeiptr>(maxQuads_ * kNumIndicesPerQuad * sizeof(int));
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, bar_size, indices_.get(), GL_STATIC_DRAW);

		shader_ = openGl.shaders->getShaderProgram(Shaders::kPassthroughVertex, fragmentShader_);
		shader_->use();
		colorUniform_ = getUniform(*shader_, "color");
		altColorUniform_ = getUniform(*shader_, "alt_color");
		modColorUniform_ = getUniform(*shader_, "mod_color");
		backgroundColorUniform_ = getUniform(*shader_, "background_color");
		thumbColorUniform_ = getUniform(*shader_, "thumb_color");
		position_ = getAttribute(*shader_, "position");
		dimensions_ = getAttribute(*shader_, "dimensions");
		coordinates_ = getAttribute(*shader_, "coordinates");
		shader_values_ = getAttribute(*shader_, "shader_values");
		thicknessUniform_ = getUniform(*shader_, "thickness");
		roundingUniform_ = getUniform(*shader_, "rounding");
		maxArcUniform_ = getUniform(*shader_, "max_arc");
		thumbAmountUniform_ = getUniform(*shader_, "thumb_amount");
		startPositionUniform_ = getUniform(*shader_, "start_pos");
		alphaMultUniform_ = getUniform(*shader_, "alpha_mult");
	}

	void OpenGlMultiQuad::destroy(OpenGlWrapper &open_gl)
	{
		shader_ = nullptr;
		position_ = nullptr;
		dimensions_ = nullptr;
		coordinates_ = nullptr;
		shader_values_ = nullptr;
		colorUniform_ = nullptr;
		altColorUniform_ = nullptr;
		modColorUniform_ = nullptr;
		thumbColorUniform_ = nullptr;
		thicknessUniform_ = nullptr;
		roundingUniform_ = nullptr;
		maxArcUniform_ = nullptr;
		thumbAmountUniform_ = nullptr;
		startPositionUniform_ = nullptr;
		alphaMultUniform_ = nullptr;
		glDeleteBuffers(1, &vertexBuffer_);
		glDeleteBuffers(1, &indicesBuffer_);

		vertexBuffer_ = 0;
		indicesBuffer_ = 0;
	}

	void OpenGlMultiQuad::render(OpenGlWrapper &open_gl, bool animate)
	{
		Component *component = targetComponent_ ? targetComponent_ : this;
		if (!active_ || (!drawWhenNotVisible_ && !component->isVisible()) || !setViewPort(component, open_gl))
			return;

		if (scissorComponent_)
			setScissor(scissorComponent_, open_gl);

		if (currentAlphaMult_ == 0.0f && alphaMult_ == 0.0f)
			return;

		if (shader_ == nullptr)
			init(open_gl);

		// setup
		glEnable(GL_BLEND);
		glEnable(GL_SCISSOR_TEST);
		if (additiveBlending_)
			glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		else
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		if (dirty_)
		{
			dirty_ = false;

			for (int i = 0; i < numQuads_; ++i)
				setDimensions(i, getQuadWidth(i), getQuadHeight(i), component->getWidth(), component->getHeight());

			glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer_);

			GLsizeiptr vert_size = static_cast<GLsizeiptr>(kNumFloatsPerQuad * maxQuads_ * sizeof(float));
			glBufferData(GL_ARRAY_BUFFER, vert_size, data_.get(), GL_STATIC_DRAW);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
		}

		shader_->use();

		if (alphaMult_ > currentAlphaMult_)
			currentAlphaMult_ = std::min(alphaMult_, currentAlphaMult_ + kAlphaInc);
		else
			currentAlphaMult_ = std::max(alphaMult_, currentAlphaMult_ - kAlphaInc);

		float alpha_color_mult = 1.0f;
		if (alphaMultUniform_)
			alphaMultUniform_->set(currentAlphaMult_);
		else
			alpha_color_mult = currentAlphaMult_;

		colorUniform_->set(color_.getFloatRed(), color_.getFloatGreen(),
			color_.getFloatBlue(), alpha_color_mult * color_.getFloatAlpha());

		if (altColorUniform_)
			altColorUniform_->set(altColor_.getFloatRed(), altColor_.getFloatGreen(),
				altColor_.getFloatBlue(), altColor_.getFloatAlpha());

		if (modColorUniform_)
			modColorUniform_->set(modColor_.getFloatRed(), modColor_.getFloatGreen(),
				modColor_.getFloatBlue(), modColor_.getFloatAlpha());

		if (backgroundColorUniform_)
			backgroundColorUniform_->set(backgroundColor_.getFloatRed(), backgroundColor_.getFloatGreen(),
				backgroundColor_.getFloatBlue(), backgroundColor_.getFloatAlpha());

		if (thumbColorUniform_)
			thumbColorUniform_->set(thumbColor_.getFloatRed(), thumbColor_.getFloatGreen(),
				thumbColor_.getFloatBlue(), thumbColor_.getFloatAlpha());

		if (thumbAmountUniform_)
			thumbAmountUniform_->set(thumbAmount_);

		if (startPositionUniform_)
			startPositionUniform_->set(startPosition_);

		if (thicknessUniform_)
		{
			currentThickness_ = currentThickness_ + kThicknessDecay * (thickness_ - currentThickness_);
			thicknessUniform_->set(currentThickness_);
		}
		if (roundingUniform_)
			roundingUniform_->set(rounding_);
		if (maxArcUniform_)
			maxArcUniform_->set(maxArc_);

		glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer_);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indicesBuffer_);

		glVertexAttribPointer(position_->attributeID, 2, GL_FLOAT, GL_FALSE, kNumFloatsPerVertex * sizeof(float), nullptr);
		glEnableVertexAttribArray(position_->attributeID);
		if (dimensions_)
		{
			glVertexAttribPointer(dimensions_->attributeID, 2, GL_FLOAT, GL_FALSE, 
				kNumFloatsPerVertex * sizeof(float), (GLvoid *)(2 * sizeof(float)));
			glEnableVertexAttribArray(dimensions_->attributeID);
		}
		if (coordinates_)
		{
			glVertexAttribPointer(coordinates_->attributeID, 2, GL_FLOAT, GL_FALSE,
				kNumFloatsPerVertex * sizeof(float), (GLvoid *)(4 * sizeof(float)));
			glEnableVertexAttribArray(coordinates_->attributeID);
		}
		if (shader_values_)
		{
			glVertexAttribPointer(shader_values_->attributeID, 4, GL_FLOAT, GL_FALSE, 
				kNumFloatsPerVertex * sizeof(float), (GLvoid *)(6 * sizeof(float)));
			glEnableVertexAttribArray(shader_values_->attributeID);
		}

		// render
		glDrawElements(GL_TRIANGLES, numQuads_ * kNumIndicesPerQuad, GL_UNSIGNED_INT, nullptr);

		// clean-up
		glDisableVertexAttribArray(position_->attributeID);
		if (dimensions_)
			glDisableVertexAttribArray(dimensions_->attributeID);
		if (coordinates_)
			glDisableVertexAttribArray(coordinates_->attributeID);
		if (shader_values_)
			glDisableVertexAttribArray(shader_values_->attributeID);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		glDisable(GL_BLEND);
		glDisable(GL_SCISSOR_TEST);
	}
}
