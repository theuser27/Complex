/*
	==============================================================================

		OpenGlImageComponent.cpp
		Created: 14 Dec 2022 2:07:44am
		Author:  theuser27

	==============================================================================
*/

#include "Interface/LookAndFeel/Fonts.h"
#include "OpenGlImageComponent.h"
#include "../Sections/BaseSection.h"

namespace Interface
{
	using namespace juce::gl;

	static constexpr int kNumPositions = 16;
	static constexpr int kNumTriangleIndices = 6;

	OpenGlImageComponent::OpenGlImageComponent(String name) : OpenGlComponent(std::move(name))
	{
		positionVertices_ = std::make_unique<float[]>(kNumPositions);
		float position_vertices[kNumPositions] =
		{
			-1.0f,  1.0f, 0.0f, 1.0f,
			-1.0f, -1.0f, 0.0f, 0.0f,
			 1.0f, -1.0f, 1.0f, 0.0f,
			 1.0f,  1.0f, 1.0f, 1.0f
		};
		memcpy(positionVertices_.get(), position_vertices, kNumPositions * sizeof(float));

		positionTriangles_ = std::make_unique<int[]>(kNumTriangleIndices);
		int position_triangles[kNumTriangleIndices] =
		{
			0, 1, 2,
			2, 3, 0
		};
		memcpy(positionTriangles_.get(), position_triangles, kNumTriangleIndices * sizeof(int));

		setInterceptsMouseClicks(false, false);
	}

	void OpenGlImageComponent::redrawImage(bool forceRedraw)
	{
		if (!active_)
			return;

		BaseComponent *component = targetComponent_ ? targetComponent_ : this;
		Rectangle<int> customDrawBounds = customDrawBounds_.get();
		auto bounds = (!customDrawBounds.isEmpty()) ? customDrawBounds : component->getLocalBounds();
		int width = bounds.getWidth();
		int height = bounds.getHeight();
		if (width <= 0 || height <= 0)
			return;

		auto *drawImage = drawImage_.lock();
		bool newImage = drawImage == nullptr || drawImage->getWidth() != width || drawImage->getHeight() != height;
		if (!newImage && !forceRedraw)
			return;

		if (newImage)
		{
			drawImage_.unlock();
			drawImage_ = std::make_unique<Image>(Image::ARGB, width, height, false);
			drawImage = drawImage_.lock();
		}

		if (clearOnRedraw_)
			drawImage->clear(Rectangle{ 0, 0, width, height });

		Graphics g(*drawImage);
		if (paintFunction_)
			paintFunction_(g);
		else
			paintToImage(g, component);

		drawImage_.unlock();
		shouldReloadImage_ = true;
	}

	void OpenGlImageComponent::init(OpenGlWrapper &openGl)
	{
		glGenBuffers(1, &vertexBuffer_);
		glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer_);

		GLsizeiptr vertSize = static_cast<GLsizeiptr>(kNumPositions * sizeof(float));
		glBufferData(GL_ARRAY_BUFFER, vertSize, positionVertices_.get(), GL_STATIC_DRAW);

		glGenBuffers(1, &triangleBuffer_);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, triangleBuffer_);

		GLsizeiptr triSize = static_cast<GLsizeiptr>(kNumTriangleIndices * sizeof(float));
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, triSize, positionTriangles_.get(), GL_STATIC_DRAW);

		imageShader_ = openGl.shaders->getShaderProgram(Shaders::kImageVertex, Shaders::kTintedImageFragment);

		imageShader_->use();
		imageColor_ = getUniform(*imageShader_, "color").value();
		imagePosition_ = getAttribute(*imageShader_, "position").value();
		textureCoordinates_ = getAttribute(*imageShader_, "tex_coord_in").value();
	}

	void OpenGlImageComponent::render(OpenGlWrapper &openGl, bool)
	{
		BaseComponent *component = targetComponent_ ? targetComponent_ : this;
		Rectangle<int> customDrawBounds = customDrawBounds_.get();
		auto bounds = (!customDrawBounds.isEmpty()) ? customDrawBounds : component->getLocalBoundsSafe();
		if (!active_ || !component->isVisibleSafe() || !setViewPort(component, bounds, openGl))
			return;

		if (shouldReloadImage_)
		{
			auto *drawImage = drawImage_.lock();
			texture_.loadImage(*drawImage);
			shouldReloadImage_ = false;
			drawImage_.unlock();
		}

		glEnable(GL_BLEND);
		if (scissor_)
			glEnable(GL_SCISSOR_TEST);
		else
			glDisable(GL_SCISSOR_TEST);

		if (additive_)
			glBlendFunc(GL_ONE, GL_ONE);
		else if (useAlpha_)
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		else
			glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer_);

		if (hasNewVertices_)
		{
			GLsizeiptr vertSize = static_cast<GLsizeiptr>(kNumPositions * sizeof(float));
			glBufferData(GL_ARRAY_BUFFER, vertSize, positionVertices_.get(), GL_STATIC_DRAW);
			hasNewVertices_ = false;
		}

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, triangleBuffer_);
		texture_.bind();
		glActiveTexture(GL_TEXTURE0);

		imageShader_->use();

		Colour colour = color_.get();
		imageColor_.set(colour.getFloatRed(), colour.getFloatGreen(), colour.getFloatBlue(), colour.getFloatAlpha());

		glVertexAttribPointer(imagePosition_.attributeID, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
		glEnableVertexAttribArray(imagePosition_.attributeID);

		glVertexAttribPointer(textureCoordinates_.attributeID, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (GLvoid *)(2 * sizeof(float)));
		glEnableVertexAttribArray(textureCoordinates_.attributeID);

		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);

		glDisableVertexAttribArray(imagePosition_.attributeID);
		glDisableVertexAttribArray(textureCoordinates_.attributeID);
		texture_.unbind();

		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

		glDisable(GL_BLEND);
		glDisable(GL_SCISSOR_TEST);
	}

	void OpenGlImageComponent::destroy()
	{
		texture_.release();

		// preparing the image for next time if openGl reinitialises this object
		shouldReloadImage_ = true;

		imageShader_ = nullptr;
		imageColor_.uniformID = (GLuint)-1;
		imagePosition_.attributeID = (GLuint)-1;
		textureCoordinates_.attributeID = (GLuint)-1;

		glDeleteBuffers(1, &vertexBuffer_);
		glDeleteBuffers(1, &triangleBuffer_);
	}

	void OpenGlBackground::paintToImage(Graphics &g, [[maybe_unused]] BaseComponent *target)
	{
		Rectangle<int> bounds = targetComponent_.get()->getLocalArea(
			componentToRedraw_, componentToRedraw_->getLocalBounds());
		g.reduceClipRegion(bounds);
		g.setOrigin(bounds.getTopLeft());

		auto &internalContext = g.getInternalContext();
		internalContext.setFill(Colours::transparentBlack);
		internalContext.fillRect(bounds, true);

		componentToRedraw_->paintBackground(g);
	}

	PlainTextComponent::PlainTextComponent(String name, String text):
		OpenGlImageComponent(std::move(name)), text_(std::move(text)), 
		font_(Fonts::instance()->getInterVFont()) { }

	void PlainTextComponent::resized()
	{
		OpenGlImageComponent::resized();
		redrawImage();
	}

	void PlainTextComponent::paintToImage(Graphics &g, BaseComponent *target)
	{
		updateState();

		g.setFont(font_);
		g.setColour(textColour_);

		g.drawText(text_, 0, 0, target->getWidth(), target->getHeight(), justification_, true);
	}

	void PlainTextComponent::updateState()
	{
		Font font;
		if (fontType_ == kTitle)
		{
			textColour_ = getColour(Skin::kHeadingText);
			font = Fonts::instance()->getInterVFont().boldened();
		}
		else if (fontType_ == kText)
		{
			textColour_ = getColour(Skin::kNormalText);
			font = Fonts::instance()->getInterVFont();
		}
		else
		{
			textColour_ = getColour(Skin::kWidgetPrimary1);
			font = Fonts::instance()->getDDinFont();
		}

		Fonts::instance()->setFontFromHeight(font, container_.get()->scaleValue(textSize_));
		font_ = std::move(font);
	}

	void PlainShapeComponent::paintToImage(Graphics &g, BaseComponent *target)
	{
		Rectangle<float> bounds = target->getLocalBounds().toFloat();

		g.setColour(Colours::white);
		auto transform = strokeShape_.getTransformToScaleToFit(bounds, true, justification_);
		if (!fillShape_.isEmpty())
			g.fillPath(fillShape_, transform);
		if (!strokeShape_.isEmpty())
			g.strokePath(strokeShape_, PathStrokeType{ 1.0f, PathStrokeType::JointStyle::beveled, PathStrokeType::EndCapStyle::butt },
				transform);
	}

}
