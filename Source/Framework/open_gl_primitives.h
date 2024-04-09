/*
	==============================================================================

		open_gl_primitives.h
		Created: 9 Apr 2024 5:39:44pm
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include <atomic>
#include <type_traits>
#include "AppConfig.h"
#include "juce_opengl/opengl/juce_gl.h"
#include "platform_definitions.h"

namespace juce
{
	class String;
	class PixelARGB;
	class Image;
	class OpenGLContext;
	class OpenGLShaderProgram;
}

namespace Interface
{
	class OpenGlShaderProgram;
	class OpenGlComponent;
	struct OpenGlWrapper;

	struct OpenGlUniform
	{
		OpenGlUniform() = default;
		OpenGlUniform(const OpenGlShaderProgram &program, const char *uniformName);

		/** Sets a float uniform. */
		void set(GLfloat n1) const noexcept;
		/** Sets an int uniform. */
		void set(GLint n1) const noexcept;
		/** Sets a vec2 uniform. */
		void set(GLfloat n1, GLfloat n2) const noexcept;
		/** Sets a vec3 uniform. */
		void set(GLfloat n1, GLfloat n2, GLfloat n3) const noexcept;
		/** Sets a vec4 uniform. */
		void set(GLfloat n1, GLfloat n2, GLfloat n3, GLfloat n4) const noexcept;
		/** Sets an ivec4 uniform. */
		void set(GLint n1, GLint n2, GLint n3, GLint n4) const noexcept;
		/** Sets a vector float uniform. */
		void set(const GLfloat *values, int numValues) const noexcept;
		/** Sets a 2x2 matrix float uniform. */
		void setMatrix2(const GLfloat *values, GLint count, GLboolean transpose) const noexcept;
		/** Sets a 3x3 matrix float uniform. */
		void setMatrix3(const GLfloat *values, GLint count, GLboolean transpose) const noexcept;
		/** Sets a 4x4 matrix float uniform. */
		void setMatrix4(const GLfloat *values, GLint count, GLboolean transpose) const noexcept;

		/** The uniform's ID number.
				If the uniform couldn't be found, this value will be < 0.
		*/
		GLint uniformID = (GLuint)-1;
	};

	struct OpenGlAttribute
	{
		OpenGlAttribute() = default;
		OpenGlAttribute(const OpenGlShaderProgram &program, const char *attributeName);

		/** The attribute's ID number.
				If the uniform couldn't be found, this value will be < 0.
		*/
		GLuint attributeID = (GLuint)-1;
	};

	class OpenGlTexture
	{
	public:
		OpenGlTexture() = default;
		~OpenGlTexture();

		enum TextureMagnificationFilter : u8
		{
			nearest,
			linear
		};

		/** Creates a texture from the given image.

				Note that if the image's dimensions aren't a power-of-two, the texture may
				be created with a larger size.

				The image will be arranged so that its top-left corner is at texture
				coordinate (0, 1).
		*/
		void loadImage(const juce::Image &image);

		/** Creates a texture from a raw array of pixels.
				If width and height are not powers-of-two, the texture will be created with a
				larger size, and only the subsection (0, 0, width, height) will be initialised.
				The data is sent directly to the OpenGL driver without being flipped vertically,
				so the first pixel will be mapped onto texture coordinate (0, 0).
		*/
		void loadARGB(const juce::PixelARGB *pixels, int width, int height);

		/** Creates a texture from a raw array of pixels.
				This is like loadARGB, but will vertically flip the data so that the first
				pixel ends up at texture coordinate (0, 1), and if the width and height are
				not powers-of-two, it will compensate by using a larger texture size.
		*/
		void loadARGBFlipped(const juce::PixelARGB *pixels, int width, int height);

		/** Creates an alpha-channel texture from an array of alpha values.
				If width and height are not powers-of-two, the texture will be created with a
				larger size, and only the subsection (0, 0, width, height) will be initialised.
				The data is sent directly to the OpenGL driver without being flipped vertically,
				so the first pixel will be mapped onto texture coordinate (0, 0).
		*/
		void loadAlpha(const u8 *pixels, int width, int height);

		/** Frees the texture, if there is one. */
		void release();

		/** Binds the texture to the currently active openGL context. */
		void bind() const;

		/** Unbinds the texture to the currently active openGL context. */
		void unbind() const;

		/** Returns the GL texture ID number. */
		GLuint getTextureID() const noexcept { return textureID; }

		int getWidth() const noexcept { return width; }
		int getHeight() const noexcept { return height; }

		/** Returns true if a texture can be created with the given size.
				Some systems may require that the sizes are powers-of-two.
		*/
		static bool isValidSize(int width, int height);

		void setTextureMagnificationFilter(TextureMagnificationFilter magFilterMode) noexcept
		{ texMagFilter = magFilterMode; }

	private:
		juce::OpenGLContext *ownerContext = nullptr;
		GLuint textureID = 0;
		int width = 0, height = 0;
		TextureMagnificationFilter texMagFilter = linear;

		void create(int w, int h, const void *, GLenum, bool topLeft);
	};

	class OpenGlShaderProgram
	{
	public:
		/** Creates a shader for use in a particular GL context. */
		OpenGlShaderProgram(const juce::OpenGLContext &) noexcept;

		/** Destructor. */
		~OpenGlShaderProgram() noexcept;

		/** Returns the version of GLSL that the current context supports.
				E.g.
				@code
				if (OpenGLShaderProgram::getLanguageVersion() > 1.199)
				{
						// ..do something that requires GLSL 1.2 or above..
				}
				@endcode
		*/
		static double getLanguageVersion();

		/** Compiles and adds a shader to this program.

				After adding all your shaders, remember to call link() to link them into
				a usable program.

				If your app is built in debug mode, this method will assert if the program
				fails to compile correctly.

				The shaderType parameter could be GL_VERTEX_SHADER, GL_FRAGMENT_SHADER, etc.

				@returns  true if the shader compiled successfully. If not, you can call
									getLastError() to find out what happened.
		*/
		bool addShader(const juce::String &shaderSourceCode, GLenum shaderType);

		/** Compiles and adds a fragment shader to this program.
				This is equivalent to calling addShader() with a type of GL_VERTEX_SHADER.
		*/
		bool addVertexShader(const juce::String &shaderSourceCode);

		/** Compiles and adds a fragment shader to this program.
				This is equivalent to calling addShader() with a type of GL_FRAGMENT_SHADER.
		*/
		bool addFragmentShader(const juce::String &shaderSourceCode);

		/** Links all the compiled shaders into a usable program.
				If your app is built in debug mode, this method will assert if the program
				fails to link correctly.
				@returns  true if the program linked successfully. If not, you can call
									getLastError() to find out what happened.
		*/
		bool link() noexcept;

		/** Selects this program into the current context. */
		void use() const noexcept;

		/** Deletes the program. */
		void release() noexcept;

		/** The ID number of the compiled program. */
		GLuint getProgramID() const noexcept;

	private:
		const juce::OpenGLContext &context;
		mutable GLuint programID = 0;
	};

	template<typename T>
	class gl_ptr
	{
	public:
		gl_ptr() noexcept = default;
		gl_ptr(nullptr_t) noexcept { }

		explicit gl_ptr(T *component) requires std::is_base_of_v<OpenGlComponent, T> :
			component_(component), controlBlock_(new control_block{}) { }

		gl_ptr(const gl_ptr &other) noexcept : component_(other.component_),
			controlBlock_(other.controlBlock_)
		{
			if (controlBlock_)
				controlBlock_->refCount.fetch_add(1, std::memory_order_relaxed);
		}
		gl_ptr &operator=(const gl_ptr &other) noexcept
		{
			if (this != &other)
				gl_ptr{ other }.swap(*this);
			return *this;
		}

		gl_ptr(gl_ptr &&other) noexcept : component_(other.component_),
			controlBlock_(other.controlBlock_)
		{
			other.component_ = nullptr;
			other.controlBlock_ = nullptr;
		}
		gl_ptr &operator=(gl_ptr &&other) noexcept
		{
			gl_ptr{ std::move(other) }.swap(*this);
			return *this;
		}

		// aliasing constructors through upcasting
		template<typename U> requires std::is_base_of_v<T, U>
		gl_ptr(const gl_ptr<U> &other) noexcept : component_(other.component_),
			controlBlock_(reinterpret_cast<control_block *>(other.controlBlock_))
		{
			if (controlBlock_)
				controlBlock_->refCount.fetch_add(1, std::memory_order_relaxed);
		}

		template<typename U> requires std::is_base_of_v<T, U>
		gl_ptr(gl_ptr<U> &&other) noexcept : component_(other.component_),
			controlBlock_(reinterpret_cast<control_block *>(other.controlBlock_))
		{
			other.component_ = nullptr;
			other.controlBlock_ = nullptr;
		}

		~gl_ptr()
		{
			if (!controlBlock_)
				return;

			if (controlBlock_->refCount.fetch_sub(1, std::memory_order_acq_rel) > 1)
				return;

			COMPLEX_ASSERT(component_ && "How do we have a control block to a non-existing object??");
			COMPLEX_ASSERT(controlBlock_->refCount.load() == 0 && "We have somehow underflowed");

			// we're already synchronised through refCount.fetch_sub, so we only care about atomicity here
			if (!controlBlock_->isInitialised.load(std::memory_order_relaxed))
				delete component_;
			else
				component_->pushForDeletion();

			delete controlBlock_;
		}

		// explicit upcasting
		template<typename U>
		gl_ptr<U> upcast() noexcept requires std::is_base_of_v<U, T>
		{
			using control_block_u = typename gl_ptr<U>::control_block;

			controlBlock_->refCount.fetch_add(1, std::memory_order_relaxed);

			gl_ptr<U> obj{};
			obj.component_ = component_;
			obj.controlBlock_ = reinterpret_cast<control_block_u *>(controlBlock_);
			return obj;
		}

		void doWorkOnComponent(OpenGlWrapper &openGl, bool animate = false)
		{
			if (!controlBlock_->isInitialised.load(std::memory_order_acquire))
			{
				controlBlock_->isInitialised.store(true, std::memory_order_release);
				component_->init(openGl);
			}

			auto customRenderFunction = component_->renderFunction_.get();
			if (customRenderFunction)
				customRenderFunction(openGl, animate);
			else
				component_->render(openGl, animate);
		}

		void deinitialise()
		{
			if (controlBlock_->isInitialised.load(std::memory_order_acquire))
			{
				controlBlock_->isInitialised.store(false, std::memory_order_release);
				component_->destroy();
			}
		}

		T *get() const noexcept { return component_; }
		T *operator->() const noexcept { return get(); }
		T &operator*() const noexcept { return *get(); }

		void swap(gl_ptr &other) noexcept
		{
			std::swap(component_, other.component_);
			std::swap(controlBlock_, other.controlBlock_);
		}

		explicit operator bool() const noexcept { return get() != nullptr; }

	private:
		struct control_block
		{
			std::atomic<size_t> refCount = 1;
			std::atomic<bool> isInitialised = false;
		};

		T *component_ = nullptr;
		control_block *controlBlock_ = nullptr;

		template <typename U>
		friend class gl_ptr;
	};

	template <typename T, typename U>
	inline bool operator==(const gl_ptr<T> &left, const gl_ptr<U> &right) noexcept
	{ return left.get() == right.get(); }

	// TODO: maybe optimise it to allocate space for both the object and control block at once

	template<typename T, typename ... Args> requires std::is_base_of_v<OpenGlComponent, T>
	inline gl_ptr<T> makeOpenGlComponent(Args&& ... args) { return gl_ptr<T>{ new T(std::forward<Args>(args)...) }; }
}