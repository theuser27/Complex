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

	OpenGlMultiQuad::OpenGlMultiQuad(int maxQuads, Shaders::FragmentShader shader, String name) :
		OpenGlComponent(std::move(name)), fragmentShader_(shader), maxQuads_(maxQuads), numQuads_(maxQuads)
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

		for (int i = 0; i < (int)maxQuads_; i++)
		{
			setCoordinates(i, -1.0f, -1.0f, 2.0f, 2.0f);
			setShaderValue(i, 1.0f);

			for (int j = 0; j < (int)kNumIndicesPerQuad; j++)
				indices_[i * kNumIndicesPerQuad + j] = triangles[j] + i * (int)kNumVertices;
		}

		setInterceptsMouseClicks(false, false);
	}

	void OpenGlMultiQuad::init(OpenGlWrapper &openGl)
	{
		glGenBuffers(1, &vertexBuffer_);
		glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer_);

		GLsizeiptr vertSize = (GLsizeiptr)(maxQuads_ * kNumFloatsPerQuad * sizeof(float));
		glBufferData(GL_ARRAY_BUFFER, vertSize, data_.get(), GL_STATIC_DRAW);

		glGenBuffers(1, &indicesBuffer_);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indicesBuffer_);

		GLsizeiptr barSize = (GLsizeiptr)(maxQuads_ * kNumIndicesPerQuad * sizeof(int));
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, barSize, indices_.get(), GL_STATIC_DRAW);

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
		valuesUniform_ = getUniform(*shader_, "static_values");
	}

	void OpenGlMultiQuad::destroy()
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
		valuesUniform_ = nullptr;
		glDeleteBuffers(1, &vertexBuffer_);
		glDeleteBuffers(1, &indicesBuffer_);

		vertexBuffer_ = 0;
		indicesBuffer_ = 0;
	}

	void OpenGlMultiQuad::render(OpenGlWrapper &openGl, bool)
	{
		Component *component = targetComponent_ ? targetComponent_ : this;
		auto bounds = (!customDrawBounds_.isEmpty()) ? customDrawBounds_ : component->getLocalBounds();
		if (!active_ || (!drawWhenNotVisible_ && !component->isVisible()) || !setViewPort(component, bounds, openGl))
			return;

		if (scissorComponent_)
			setScissor(scissorComponent_, openGl);

		if (currentAlphaMult_ == 0.0f && alphaMult_ == 0.0f)
			return;

		if (shader_ == nullptr)
			init(openGl);

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

			for (u32 i = 0; i < numQuads_; ++i)
				setDimensions(i, getQuadWidth(i), getQuadHeight(i), (float)bounds.getWidth(), (float)bounds.getHeight());

			glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer_);

			GLsizeiptr vertSize = static_cast<GLsizeiptr>(kNumFloatsPerQuad * maxQuads_ * sizeof(float));
			glBufferData(GL_ARRAY_BUFFER, vertSize, data_.get(), GL_STATIC_DRAW);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
		}

		shader_->use();

		if (alphaMult_ > currentAlphaMult_)
			currentAlphaMult_ = std::min(alphaMult_, currentAlphaMult_ + kAlphaInc);
		else
			currentAlphaMult_ = std::max(alphaMult_, currentAlphaMult_ - kAlphaInc);

		float alphaColorMult = 1.0f;
		if (alphaMultUniform_)
			alphaMultUniform_->set(currentAlphaMult_);
		else
			alphaColorMult = currentAlphaMult_;

		colorUniform_->set(color_.getFloatRed(), color_.getFloatGreen(),
			color_.getFloatBlue(), alphaColorMult * color_.getFloatAlpha());

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

		if (valuesUniform_)
			valuesUniform_->set(values_[0], values_[1], values_[2], values_[3]);

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
		glDrawElements(GL_TRIANGLES, (GLsizei)(numQuads_ * kNumIndicesPerQuad), GL_UNSIGNED_INT, nullptr);

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
	
	void OpenGlScrollQuad::render(OpenGlWrapper &openGl, bool animate)
	{
		float lastHover = hoverAmount_;
		animator_.tick();
		hoverAmount_ = animator_.getValue(Animator::Hover);

		if (lastHover != hoverAmount_)
		{
			if (shrinkLeft_)
				setQuadHorizontal(0, -1.0f, 1.0f + hoverAmount_);
			else
				setQuadHorizontal(0, 0.0f - hoverAmount_, 1.0f + hoverAmount_);
		}

		Range<double> range = scrollBar_->getCurrentRange();
		Range<double> totalRange = scrollBar_->getRangeLimit();
		double startRatio = (range.getStart() - totalRange.getStart()) / totalRange.getLength();
		double endRatio = (range.getEnd() - totalRange.getStart()) / totalRange.getLength();
		setQuadVertical(0, 1.0f - 2.0f * (float)endRatio, 2.0f * (float)(endRatio - startRatio));

		OpenGlMultiQuad::render(openGl, animate);
	}
}
