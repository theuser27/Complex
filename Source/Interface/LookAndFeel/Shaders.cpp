/*
	==============================================================================

		Shaders.cpp
		Created: 16 Nov 2022 6:47:15am
		Author:  theuser27

	==============================================================================
*/

#include "Shaders.h"
#include "Framework/platform_definitions.h"

namespace
{
	#if JUCE_OPENGL_ES || OPENGL_ES
		#define HIGHP "highp"
		#define MEDIUMP "mediump"
		#define LOWP "lowp"
	#else
		#define HIGHP
		#define MEDIUMP
		#define LOWP
	#endif

	#define CONSTRAIN_AXIS_FUNCTION																								\
		"float constrainAxis(float normAxis, float constraint, float offset) {\n"		\
		"    return clamp(ceil(-abs(normAxis + offset) + constraint), 0.0, 1.0);\n" \
		"}\n"

	#define CONSTRAIN_AXIS_VEC2_FUNCTION																					\
		"vec2 constrainAxis(vec2 normAxis, vec2 constraint, vec2 offset) {\n"				\
		"    return clamp(ceil(-abs(normAxis + offset) + constraint), 0.0, 1.0);\n"	\
		"}\n"

	const char *kImageVertexShader =
		"attribute " MEDIUMP " vec4 position;\n"
		"attribute " MEDIUMP " vec2 tex_coord_in;\n"
		"\n"
		"varying " MEDIUMP " vec2 tex_coord_out;\n"
		"\n"
		"void main() {\n"
		"    tex_coord_out = tex_coord_in;\n"
		"    gl_Position = vec4(position.xy, 1.0, 1.0);\n"
		"}\n";

	const char *kImageFragmentShader =
		"varying " MEDIUMP " vec2 tex_coord_out;\n"
		"\n"
		"uniform sampler2D image;\n"
		"\n"
		"void main() {\n"
		"    gl_FragColor = texture2D(image, tex_coord_out);\n"
		"}\n";

	const char *kTintedImageFragmentShader =
		"varying " MEDIUMP " vec2 tex_coord_out;\n"
		"\n"
		"uniform sampler2D image;\n"
		"uniform " MEDIUMP " vec4 color;\n"
		"\n"
		"void main() {\n"
		"    " MEDIUMP " vec4 image_color = texture2D(image, tex_coord_out);\n"
		"    image_color.r *= color.r;\n"
		"    image_color.g *= color.g;\n"
		"    image_color.b *= color.b;\n"
		"    image_color.a *= color.a;\n"
		"    gl_FragColor = image_color;\n"
		"}\n";

	const char *kPassthroughVertexShader =
		"attribute " MEDIUMP " vec4 position;\n"
		"attribute " MEDIUMP " vec2 dimensions;\n"
		"attribute " MEDIUMP " vec2 coordinates;\n"
		"attribute " MEDIUMP " vec4 shader_values;\n"
		"\n"
		"varying " MEDIUMP " vec2 dimensions_out;\n"
		"varying " MEDIUMP " vec2 coordinates_out;\n"
		"varying " MEDIUMP " vec4 shader_values_out;\n"
		"\n"
		"void main() {\n"
		"    dimensions_out = dimensions;\n"
		"    coordinates_out = coordinates;\n"
		"    shader_values_out = shader_values;\n"
		"    gl_Position = position;\n"
		"}\n";

	const char *kScaleVertexShader =
		"attribute " MEDIUMP " vec4 position;\n"
		"uniform " MEDIUMP " vec2 scale;\n"
		"\n"
		"void main() {\n"
		"    gl_Position = position;\n"
		"    gl_Position.x = gl_Position.x * scale.x;\n"
		"    gl_Position.y = gl_Position.y * scale.y;\n"
		"    gl_Position.z = 0.0;\n"
		"    gl_Position.a = 1.0;\n"
		"}\n";

	const char *kRotaryModulationVertexShader =
		"attribute " MEDIUMP " vec4 position;\n"
		"attribute " MEDIUMP " vec2 coordinates;\n"
		"attribute " MEDIUMP " vec4 range;\n"
		"attribute " MEDIUMP " float meter_radius;\n"
		"\n"
		"varying " MEDIUMP " vec2 coordinates_out;\n"
		"varying " MEDIUMP " vec4 range_out;\n"
		"varying " MEDIUMP " float meter_radius_out;\n"
		"\n"
		"void main() {\n"
		"    coordinates_out = coordinates;\n"
		"    range_out = range;\n"
		"    meter_radius_out = meter_radius;\n"
		"    gl_Position = position;\n"
		"}\n";

	const char *kLinearModulationVertexShader =
		"attribute " MEDIUMP " vec4 position;\n"
		"attribute " MEDIUMP " vec2 coordinates;\n"
		"attribute " MEDIUMP " vec4 range;\n"
		"\n"
		"varying " MEDIUMP " vec2 coordinates_out;\n"
		"varying " MEDIUMP " vec4 range_out;\n"
		"\n"
		"void main() {\n"
		"    coordinates_out = coordinates;\n"
		"    range_out = range;\n"
		"    gl_Position = position;\n"
		"}\n";

	const char *kGainMeterVertexShader =
		"attribute " MEDIUMP " vec4 position;\n"
		"\n"
		"varying " MEDIUMP " vec2 position_out;\n"
		"\n"
		"void main() {\n"
		"    gl_Position = position;\n"
		"    position_out = position.xz;\n"
		"}\n";

	const char *kGainMeterFragmentShader =
		"varying " MEDIUMP " vec2 position_out;\n"
		"uniform " MEDIUMP " vec4 color_from;\n"
		"uniform " MEDIUMP " vec4 color_to;\n"
		"void main() {\n"
		"    " MEDIUMP " float t = (position_out.x + 1.0) / 2.0;\n"
		"    gl_FragColor = color_to * t + color_from * (1.0 - t);\n"
		"}\n";

	const char *kColorFragmentShader =
		"uniform " MEDIUMP " vec4 color;\n"
		"void main() {\n"
		"    gl_FragColor = color;\n"
		"}\n";

	const char *kFadeSquareFragmentShader =
		"uniform " MEDIUMP " vec4 color;\n"
		"varying " MEDIUMP " vec2 dimensions_out;\n"
		"varying " MEDIUMP " vec2 coordinates_out;\n"
		"varying " MEDIUMP " vec4 shader_values_out;\n"
		"void main() {\n"
		"    float alpha1 = clamp((dimensions_out.x - abs(coordinates_out.x) * dimensions_out.x) * 0.5, 0.0, 1.0);\n"
		"    float alpha2 = clamp((dimensions_out.y - abs(coordinates_out.y) * dimensions_out.y) * 0.5, 0.0, 1.0);\n"
		"    gl_FragColor = color;\n"
		"    gl_FragColor.a = color.a * alpha1 * alpha2 * shader_values_out.x;\n"
		"}\n";

	const char *kCircleFragmentShader =
		"uniform " MEDIUMP " vec4 color;\n"
		"varying " MEDIUMP " vec2 dimensions_out;\n"
		"varying " MEDIUMP " vec2 coordinates_out;\n"
		"void main() {\n"
		"    float delta_center = length(coordinates_out) * 0.5 * dimensions_out.x;\n"
		"    float alpha = clamp(dimensions_out.x * 0.5 - delta_center, 0.0, 1.0);\n"
		"    gl_FragColor = color;\n"
		"    gl_FragColor.a = color.a * alpha;\n"
		"}\n";

	// ring around points when hovered over
	const char *kRingFragmentShader =
		"uniform " MEDIUMP " vec4 color;\n"
		"uniform " MEDIUMP " vec4 alt_color;\n"
		"varying " MEDIUMP " vec2 dimensions_out;\n"
		"uniform " MEDIUMP " float thickness;\n"
		"varying " MEDIUMP " vec2 coordinates_out;\n"
		"void main() {\n"
		"    float full_radius = 0.5 * dimensions_out.x;\n"
		"    float delta_center = length(coordinates_out) * full_radius;\n"
		"    float alpha_out = clamp(full_radius - delta_center, 0.0, 1.0);\n"
		"    float alpha_in = clamp(delta_center - full_radius + thickness + 1.0, 0.0, 1.0);\n"
		"    gl_FragColor = color * alpha_in + (1.0 - alpha_in) * alt_color;\n"
		"    gl_FragColor.a = gl_FragColor.a * alpha_out;\n"
		"}\n";

	// the diamond points inside the wavetable editor
	const char *kDiamondFragmentShader =
		"uniform " MEDIUMP " vec4 color;\n"
		"uniform " MEDIUMP " vec4 alt_color;\n"
		"varying " MEDIUMP " vec2 dimensions_out;\n"
		"uniform " MEDIUMP " float thickness;\n"
		"varying " MEDIUMP " vec2 coordinates_out;\n"
		"void main() {\n"
		"    float full_radius = 0.5 * dimensions_out.x;\n"
		"    float delta_center = (abs(coordinates_out.x) + abs(coordinates_out.y)) * full_radius;\n"
		"    float alpha_out = clamp(full_radius - delta_center, 0.0, 1.0);\n"
		"    float alpha_in = clamp(delta_center - full_radius + thickness + 1.0, 0.0, 1.0);\n"
		"    gl_FragColor = color * alpha_in + (1.0 - alpha_in) * alt_color;\n"
		"    gl_FragColor.a = gl_FragColor.a * alpha_out;\n"
		"}\n";

	// rounded corners on the inside of sections (i.e. corners of wavetable/lfo/envelope windows)
	const char *kRoundedCornerFragmentShader =
		"uniform " MEDIUMP " vec4 color;\n"
		"varying " MEDIUMP " vec2 dimensions_out;\n"
		"varying " MEDIUMP " vec2 coordinates_out;\n"
		"void main() {\n"
		"    float delta_center = length(coordinates_out * dimensions_out);\n"
		"    float alpha = clamp(delta_center - dimensions_out.x + 0.5, 0.0, 1.0);\n"
		"    gl_FragColor = color;\n"
		"    gl_FragColor.a = color.a * alpha;\n"
		"}\n";

	// rounded corners on the outside of sections
	const char *kRoundedRectangleFragmentShader =
		"uniform " MEDIUMP " vec4 color;\n"
		"varying " MEDIUMP " vec2 dimensions_out;\n"
		"varying " MEDIUMP " vec2 coordinates_out;\n"
		"uniform " MEDIUMP " float rounding;\n"
		"void main() {\n"
		"    vec2 center_offset = abs(coordinates_out) * dimensions_out - dimensions_out;\n"
		"    float delta_center = length(max(center_offset + vec2(rounding, rounding), vec2(0.0, 0.0)));\n"
		"    float alpha = clamp((rounding - delta_center) * 0.5 + 0.5, 0.0, 1.0);\n"
		"    gl_FragColor = color;\n"
		"    gl_FragColor.a = color.a * alpha;\n"
		"}\n";

	// the border around the popup menus and currently selected modulator
	const char *kRoundedRectangleBorderFragmentShader =
		"uniform " MEDIUMP " vec4 color;\n"
		"varying " MEDIUMP " vec2 dimensions_out;\n"
		"varying " MEDIUMP " vec2 coordinates_out;\n"
		"uniform " MEDIUMP " float rounding;\n"
		"uniform " MEDIUMP " float thickness;\n"
		"uniform " MEDIUMP " float overall_alpha;\n"
		"void main() {\n"
		"    vec2 center_offset = abs(coordinates_out) * dimensions_out - dimensions_out;\n"
		"    float delta_center = length(max(center_offset + vec2(rounding, rounding), vec2(0.0, 0.0)));\n"
		"    float inside_rounding = rounding + 2.0 * thickness;\n"
		"    float delta_center_inside = length(max(center_offset + vec2(inside_rounding, inside_rounding), vec2(0.0, 0.0)));\n"
		"    float border_delta = (rounding - delta_center) * 0.5;\n"
		"    float inside_border_delta = (rounding - delta_center_inside) * 0.5;\n"
		"    float alpha = clamp(border_delta + 0.5, 0.0, 1.0) * clamp(-inside_border_delta + 0.5, 0.0, 1.0);\n"
		"    gl_FragColor = color;\n"
		"    gl_FragColor.a = color.a * overall_alpha * alpha;\n"
		"}\n";

	// overall knob design
	const char *kRotarySliderFragmentShader =
		"uniform " MEDIUMP " vec4 color;\n"
		"uniform " MEDIUMP " vec4 alt_color;\n"
		"uniform " MEDIUMP " vec4 thumb_color;\n"
		"varying " MEDIUMP " vec2 dimensions_out;\n"
		"uniform " MEDIUMP " float thickness;\n"
		"uniform " MEDIUMP " float thumb_amount;\n"
		"uniform " MEDIUMP " float start_pos;\n"
		"uniform " MEDIUMP " float max_arc;\n"
		"varying " MEDIUMP " vec4 shader_values_out;\n"
		"varying " MEDIUMP " vec2 coordinates_out;\n"
		"void main() {\n"
		"    " MEDIUMP " float rads = atan(coordinates_out.x, coordinates_out.y);\n"
		"    float full_radius = 0.5 * dimensions_out.x;\n"
		"    float delta_center = length(coordinates_out) * full_radius;\n"
		"    float center_arc = full_radius - thickness * 0.5 - 0.5;\n"
		"    float delta_arc = delta_center - center_arc;\n"
		"    float distance_arc = abs(delta_arc);\n"
		"    float dist_curve_left = max(center_arc * (rads - max_arc), 0.0);\n"
		"    float dist_curve = max(center_arc * (-rads - max_arc), dist_curve_left);\n"
		"    float alpha = clamp(thickness * 0.5 - length(vec2(distance_arc, dist_curve)) + 0.5, 0.0, 1.0);\n"
		"    float delta_rads = rads - shader_values_out.x;\n"
		"    float color_step1 = step(0.0, delta_rads);\n"
		"    float color_step2 = step(0.0, start_pos - rads);\n"
		"    float color_step = abs(color_step2 - color_step1);\n"
		"    gl_FragColor = alt_color * color_step + color * (1.0 - color_step);\n"
		"    gl_FragColor.a = gl_FragColor.a * alpha;\n"
		"    float thumb_length = full_radius * thumb_amount;\n"
		"    float thumb_x = sin(delta_rads) * delta_center;\n"
		"    float thumb_y = cos(delta_rads) * delta_center - (0.5 * center_arc);\n"
		"    float adjusted_thumb_y = min(thumb_y + thumb_length, 0.0);\n"
		"    float outside_arc_step = step(0.0, thumb_y);\n"
		"    float thumb_y_distance = thumb_y * outside_arc_step + adjusted_thumb_y * (1.0 - outside_arc_step);\n"
		"    float thumb_distance = length(vec2(thumb_x, thumb_y_distance));\n"
		"    float thumb_alpha = clamp(thickness * 0.5 - thumb_distance + 0.5, 0.0, 1.0);\n"
		"    gl_FragColor = gl_FragColor * (1.0 - thumb_alpha) + thumb_color * thumb_alpha;\n"
		"}\n";

	// modulation ring around the knob
	const char *kRotaryModulationFragmentShader =
		"varying " MEDIUMP " vec2 coordinates_out;\n"
		"varying " MEDIUMP " vec2 dimensions_out;\n"
		"varying " MEDIUMP " vec4 shader_values_out;\n"
		"uniform " MEDIUMP " float thickness;\n"
		"uniform " MEDIUMP " vec4 color;\n"
		"uniform " MEDIUMP " vec4 alt_color;\n"
		"uniform " MEDIUMP " vec4 mod_color;\n"
		"uniform " MEDIUMP " float overall_alpha;\n"
		"uniform " MEDIUMP " float start_pos;\n"
		"const " MEDIUMP " float kPi = 3.14159265359;\n"
		"\n"
		"void main() {\n"
		"    " MEDIUMP " float full_radius = dimensions_out.x * 0.5;\n"
		"    " MEDIUMP " float dist = length(coordinates_out) * full_radius;\n"
		"    " MEDIUMP " float inner_radius = full_radius - thickness;\n"
		"    " MEDIUMP " float dist_outer_amp = clamp((full_radius - dist) * 0.5 + 0.5, 0.0, 1.0);\n"
		"    " MEDIUMP " float dist_amp = dist_outer_amp * clamp((dist - inner_radius) * 0.5 + 0.5, 0.0, 1.0);\n"
		"    " MEDIUMP " float rads = mod(atan(coordinates_out.x, coordinates_out.y) + kPi + start_pos, 2.0 * kPi) - kPi;\n"
		"    " MEDIUMP " float rads_amp_low = clamp(full_radius * 0.5 * (rads - shader_values_out.x) + 1.0, 0.0, 1.0);\n"
		"    " MEDIUMP " float rads_amp_high = clamp(full_radius * 0.5 * (shader_values_out.y - rads) + 1.0, 0.0, 1.0);\n"
		"    " MEDIUMP " float rads_amp_low_stereo = clamp(full_radius * 0.5 * (rads - shader_values_out.z) + 0.5, 0.0, 1.0);\n"
		"    " MEDIUMP " float rads_amp_high_stereo = clamp(full_radius * 0.5 * (shader_values_out.a - rads) + 0.5, 0.0, 1.0);\n"
		"    " MEDIUMP " float alpha = rads_amp_low * rads_amp_high;\n"
		"    " MEDIUMP " float alpha_stereo = rads_amp_low_stereo * rads_amp_high_stereo;\n"
		"    " MEDIUMP " float alpha_center = min(alpha, alpha_stereo);\n"
		"    " MEDIUMP " vec4 color_left = (alpha - alpha_center) * color;\n"
		"    " MEDIUMP " vec4 color_right = (alpha_stereo - alpha_center) * alt_color;\n"
		"    " MEDIUMP " vec4 color_center = alpha_center * mod_color;\n"
		"    " MEDIUMP " vec4 out_color = color * (1.0 - alpha_stereo) + alt_color * alpha_stereo;\n"
		"    out_color = out_color * (1.0 - alpha_center) + color_center * alpha_center;\n"
		"    out_color.a = max(alpha, alpha_stereo) * overall_alpha * dist_amp;\n"
		"    gl_FragColor = out_color;\n"
		"}\n";

	// horizontal slider 
	const char *kHorizontalSliderFragmentShader =
		"uniform " MEDIUMP " vec4 color;\n"
		"uniform " MEDIUMP " vec4 alt_color;\n"
		"uniform " MEDIUMP " vec4 thumb_color;\n"
		"varying " MEDIUMP " vec2 dimensions_out;\n"
		"uniform " MEDIUMP " float thickness;\n"
		"uniform " MEDIUMP " float thumb_amount;\n"
		"uniform " MEDIUMP " float start_pos;\n"
		"uniform " MEDIUMP " float rounding;\n"
		"varying " MEDIUMP " vec4 shader_values_out;\n"
		"varying " MEDIUMP " vec2 coordinates_out;\n"
		"void main() {\n"
		"    vec2 position = coordinates_out * dimensions_out;\n"
		"    vec2 center_offset = abs(position) - vec2(dimensions_out.x, thickness);\n"
		"    float delta_center = length(max(center_offset + vec2(rounding, rounding), vec2(0.0, 0.0)));\n"
		"    float alpha = clamp((rounding - delta_center) * 0.5 + 0.5, 0.0, 1.0);\n"
		"    float adjusted_value = shader_values_out.x * 2.0 - 1.0;\n"
		"    float delta_pos = coordinates_out.x - adjusted_value;\n"
		"    float color_step1 = step(0.001, delta_pos);\n"
		"    float color_step2 = step(0.001, start_pos - coordinates_out.x);\n"
		"    float color_step = abs(color_step2 - color_step1);\n"
		"    gl_FragColor = alt_color * color_step + color * (1.0 - color_step);\n"
		"    gl_FragColor.a = gl_FragColor.a * alpha;\n"
		"}\n";

	// vertical slider
	const char *kVerticalSliderFragmentShader =
		"uniform " MEDIUMP " vec4 color;\n"
		"uniform " MEDIUMP " vec4 alt_color;\n"
		"uniform " MEDIUMP " vec4 thumb_color;\n"
		"varying " MEDIUMP " vec2 dimensions_out;\n"
		"uniform " MEDIUMP " float thickness;\n"
		"uniform " MEDIUMP " float thumb_amount;\n"
		"uniform " MEDIUMP " float start_pos;\n"
		"uniform " MEDIUMP " float rounding;\n"
		"varying " MEDIUMP " vec4 shader_values_out;\n"
		"varying " MEDIUMP " vec2 coordinates_out;\n"
		"void main() {\n"
		"    vec2 position = coordinates_out * dimensions_out;\n"
		"    vec2 center_offset = abs(position) - vec2(thickness, dimensions_out.y);\n"
		"    float delta_center = length(max(center_offset + vec2(rounding, rounding), vec2(0.0, 0.0)));\n"
		"    float alpha = clamp((rounding - delta_center) * 0.5 + 0.5, 0.0, 1.0);\n"
		"    float adjusted_value = shader_values_out.x * 2.0 - 1.0;\n"
		"    float delta_pos = coordinates_out.y - adjusted_value;\n"
		"    float color_step1 = step(0.001, delta_pos);\n"
		"    float color_step2 = step(0.001, start_pos - coordinates_out.y);\n"
		"    float color_step = abs(color_step2 - color_step1);\n"
		"    gl_FragColor = color * color_step + alt_color * (1.0 - color_step);\n"
		"    gl_FragColor.a = gl_FragColor.a * alpha;\n"
		"    vec2 thumb_center_offset = abs(position - vec2(0.0, adjusted_value * dimensions_out.y)) - vec2(thickness, thumb_amount);\n"
		"    float thumb_delta_center = length(max(thumb_center_offset + vec2(rounding, rounding), vec2(0.0, 0.0)));\n"
		"    float thumb_alpha = clamp((rounding - thumb_delta_center) * 0.5 + 0.5, 0.0, 1.0) * alpha;\n"
		"    gl_FragColor = gl_FragColor * (1.0 - thumb_alpha) + thumb_color * thumb_alpha;\n"
		"}\n";

	// modulation line next to a slider 
	const char *kLinearModulationFragmentShader =
		"varying " MEDIUMP " vec2 coordinates_out;\n"
		"varying " MEDIUMP " vec4 shader_values_out;\n"
		"uniform " MEDIUMP " vec4 color;\n"
		"uniform " MEDIUMP " vec4 alt_color;\n"
		"uniform " MEDIUMP " vec4 mod_color;\n"
		"\n"
		"void main() {\n"
		"    " MEDIUMP " float position = coordinates_out.x * 0.5 + 0.5;\n"
		"    " MEDIUMP " float dist1 = clamp(200.0 * (position - shader_values_out.x), 0.0, 1.0);\n"
		"    " MEDIUMP " float dist2 = clamp(200.0 * (shader_values_out.y - position), 0.0, 1.0);\n"
		"    " MEDIUMP " float stereo_dist1 = clamp(200.0 * (position - shader_values_out.z), 0.0, 1.0);\n"
		"    " MEDIUMP " float stereo_dist2 = clamp(200.0 * (shader_values_out.a - position), 0.0, 1.0);\n"
		"    " MEDIUMP " float alpha = dist1 * dist2;\n"
		"    " MEDIUMP " float alpha_stereo = stereo_dist1 * stereo_dist2;\n"
		"    " MEDIUMP " float alpha_center = min(alpha, alpha_stereo);\n"
		"    " MEDIUMP " vec4 color_left = (alpha - alpha_center) * color;\n"
		"    " MEDIUMP " vec4 color_right = (alpha_stereo - alpha_center) * alt_color;\n"
		"    " MEDIUMP " vec4 color_center = alpha_center * mod_color;\n"
		"    " MEDIUMP " vec4 color = color_left + color_right + color_center;\n"
		"    color.a = max(alpha, alpha_stereo);\n"
		"    gl_FragColor = color;\n"
		"}\n";

	// coordinates_out are ndc (the same values as the position varying
	//		except when OpenGlCorners when they are coordinates inside the quad itself)
	// dimensions_out are the absolute dimensions of the object 
	//		(almost always act as a uniform but why it isn't one idk)
	const char *kPinSliderFragmentShader =
		"uniform " MEDIUMP " vec4 color;\n"
		"varying " MEDIUMP " vec2 dimensions_out;\n"
		"varying " MEDIUMP " vec2 coordinates_out;\n"
		CONSTRAIN_AXIS_FUNCTION
		"\n"
		"void main() {\n"
		"    float pinXAlpha = constrainAxis(coordinates_out.x, 0.2, 0.0);\n"
		"    float pinYAlpha = clamp((coordinates_out.y + 1.0) * 0.75, 0.05, 1.0);\n"
		"    float alpha = pinXAlpha * pinYAlpha;\n"
		"    gl_FragColor = color;\n"
		"    gl_FragColor.a = color.a * alpha;\n"
		"}\n";

	// plus thickness is (width / dimensions)
	const char *kPlusFragmentShader =
		"uniform " MEDIUMP " vec4 color;\n"
		"uniform " MEDIUMP " float thickness;\n"
		"varying " MEDIUMP " vec2 coordinates_out;\n"
		CONSTRAIN_AXIS_FUNCTION
		"\n"
		"void main() {\n"
		"    vec2 coordinates_out_norm = (coordinates_out * 0.5) + 0.5;\n"
		"    float normBound = (1.0 - thickness) * 0.5;\n"
		"    float xAlpha1 = constrainAxis(coordinates_out_norm.x, normBound, 0.0);\n"
		"    float xAlpha2 = constrainAxis(-coordinates_out_norm.x, normBound, 1.0);\n"
		"    float yAlpha1 = constrainAxis(coordinates_out_norm.y, normBound, 0.0);\n"
		"    float yAlpha2 = constrainAxis(-coordinates_out_norm.y, normBound, 1.0);\n"
		"    float alpha = (1.0 - xAlpha1 - xAlpha2) + (1.0 - yAlpha1 - yAlpha2);\n"
		"    gl_FragColor = color;\n"
		"    gl_FragColor.a = color.a * alpha;\n"
		"}\n";

	const char *kHighlightFragmentShader =
		"uniform " MEDIUMP " vec4 color;\n"
		"uniform " MEDIUMP " vec4 alt_color;\n"
		"uniform " MEDIUMP " vec4 mod_color;\n"
		"uniform " MEDIUMP " vec4 static_values;\n"
		"varying " MEDIUMP " vec2 coordinates_out;\n"
		"\n"
		"void main() {\n"
		"    vec2 coordinates_out_norm = (coordinates_out * 0.5) + 0.5;\n"
		"    float normLeftBound = static_values.x;\n"
		"    float normRightBound = static_values.z;\n"
		"    float areBoundsSwitched = sign(normLeftBound - normRightBound) * 0.5 + 0.5;\n"
		"    float pinXAlpha1 = clamp(ceil(-abs(coordinates_out_norm.x) + normLeftBound), 0.0, 1.0);\n"
		"    float pinXAlpha2 = clamp(ceil(-abs(-coordinates_out_norm.x + 1.0) + 1.0 - normRightBound), 0.0, 1.0);\n"
		"    float alpha = (areBoundsSwitched + 1.0 - pinXAlpha1 - pinXAlpha2);\n"
		"    gl_FragColor = color;\n"
		"    gl_FragColor.a *= alpha;\n"
		"}\n";

	// modulation knob when hovered over a control
	const char *kModulationKnobFragmentShader =
		"uniform " MEDIUMP " vec4 color;\n"
		"uniform " MEDIUMP " vec4 alt_color;\n"
		"uniform " MEDIUMP " vec4 mod_color;\n"
		"uniform " MEDIUMP " vec4 background_color;\n"
		"uniform " MEDIUMP " vec4 thumb_color;\n"
		"varying " MEDIUMP " vec2 dimensions_out;\n"
		"uniform " MEDIUMP " float thickness;\n"
		"uniform " MEDIUMP " float overall_alpha;\n"
		"varying " MEDIUMP " vec4 shader_values_out;\n"
		"varying " MEDIUMP " vec2 coordinates_out;\n"
		"void main() {\n"
		"    float rads = atan(coordinates_out.x, -coordinates_out.y);\n"
		"    float full_radius = 0.5 * dimensions_out.x;\n"
		"    float delta_center = length(coordinates_out) * full_radius;\n"
		"    float circle_alpha = clamp(full_radius - delta_center, 0.0, 1.0);\n"
		"    float delta_rads = rads - shader_values_out.x;\n"
		"    float color_amount = clamp(delta_rads * max(delta_center, 1.0) * 1.6, 0.0, 1.0);\n"
		"    gl_FragColor = alt_color * color_amount + color * (1.0 - color_amount);\n"
		"    gl_FragColor.a = gl_FragColor.a * circle_alpha;\n"
		"    float center_arc = full_radius - thickness * 0.5 - 0.5;\n"
		"    float delta_arc = delta_center - center_arc;\n"
		"    float distance_arc = abs(delta_arc);\n"
		"    float thumb_alpha = clamp(thickness * 0.5 - distance_arc + 0.5, 0.0, 1.0);\n"
		"    gl_FragColor = gl_FragColor * (1.0 - thumb_alpha) + thumb_color * thumb_alpha;\n"
		"    float mod_alpha1 = clamp(full_radius * 0.48 - delta_center, 0.0, 1.0) * mod_color.a;\n"
		"    float mod_alpha2 = clamp(full_radius * 0.35 - delta_center, 0.0, 1.0) * mod_color.a;\n"
		"    gl_FragColor = gl_FragColor * (1.0 - mod_alpha1) + background_color * mod_alpha1;\n"
		"    gl_FragColor = gl_FragColor * (1.0 - mod_alpha2) + mod_color * mod_alpha2;\n"
		"    gl_FragColor.a = gl_FragColor.a * overall_alpha;\n"
		"}\n";

	// coordinates_out are ndc (the same values as the position varying
	//		except when OpenGlCorners when they are coordinates inside the quad itself)
	// dimensions_out are the absolute dimensions of the object 
	//		(almost always act as a uniform but why it isn't one idk)
	const char *kDotSliderFragmentShader =
		"uniform " MEDIUMP " vec4 color;\n"
		"uniform " MEDIUMP " float thumb_amount;\n"
		"varying " MEDIUMP " vec2 dimensions_out;\n"
		"varying " MEDIUMP " vec2 coordinates_out;\n"
		"varying " MEDIUMP " vec4 shader_values_out;\n"
		CONSTRAIN_AXIS_FUNCTION
		"\n"
		"void main() {\n"
		"    vec2 position = coordinates_out * dimensions_out;\n"
		"    float adjusted_value = shader_values_out.x * 2.0 - 1.0;\n"
		"    vec2 thumb_center_offset = abs(position - vec2(adjusted_value * dimensions_out.x, 0.0)) - vec2(thumb_amount);\n"
		"    float rounding = thumb_amount * 0.5;\n"
		"    float thumb_delta_center = length(max(thumb_center_offset + vec2(rounding), vec2(0.0)));\n"
		"    float thumb_alpha = clamp((rounding - thumb_delta_center) * 0.5 + 0.5, 0.0, 1.0);\n"
		"    gl_FragColor = gl_FragColor * (1.0 - thumb_alpha) + thumb_color * thumb_alpha;\n"
		"}\n";

	const char *kLineFragmentShader =
		"uniform " MEDIUMP " vec4 color;\n"
		"uniform " MEDIUMP " float line_width;\n"
		"uniform " MEDIUMP " float boost;\n"
		"varying " MEDIUMP " float depth_out;\n"
		"void main() {\n"
		"    " MEDIUMP " float dist_from_edge = min(depth_out, 1.0 - depth_out);\n"
		"    " MEDIUMP " float mult = 1.0 + boost * max(dist_from_edge - 2.0 / line_width, 0.0);\n"
		"    " MEDIUMP " vec4 result = mult * color;\n"
		"    " MEDIUMP " float scale = line_width * dist_from_edge;\n"
		"    result.a = result.a * scale / 2.0;\n"
		"    gl_FragColor = result;\n"
		"}\n";

	const char *kFillFragmentShader =
		"uniform " MEDIUMP " vec4 color_from;\n"
		"uniform " MEDIUMP " vec4 color_to;\n"
		"varying " MEDIUMP " float boost;\n"
		"varying " MEDIUMP " float distance;\n"
		"void main() {\n"
		"    " MEDIUMP " float delta = abs(distance);\n"
		"    " MEDIUMP " vec4 base_color = color_to * delta + color_from * (1.0 - delta);\n"
		"    gl_FragColor = base_color;\n"
		"    gl_FragColor.a = (boost + 1.0) * base_color.a;\n"
		"}\n";

	const char *kLineVertexShader =
		"attribute " MEDIUMP " vec4 position;\n"
		"uniform " MEDIUMP " vec2 scale;\n"
		"out " MEDIUMP " float depth_out;\n"
		"\n"
		"void main() {\n"
		"    depth_out = position.z;\n"
		"    gl_Position = position;\n"
		"    gl_Position.x = position.x * scale.x;\n"
		"    gl_Position.y = position.y * scale.y;\n"
		"    gl_Position.z = 0.0;\n"
		"    gl_Position.w = 1.0;\n"
		"}\n";

	const char *kFillVertexShader =
		"attribute " MEDIUMP " vec4 position;\n"
		"uniform " MEDIUMP " vec2 scale;\n"
		"uniform " MEDIUMP " float center_position;\n"
		"uniform " MEDIUMP " float boost_amount;\n"
		"out " MEDIUMP " float distance;\n"
		"out " MEDIUMP " float boost;\n"
		"\n"
		"void main() {\n"
		"    distance = (position.y - center_position) / (1.0 - center_position);\n"
		"    boost = boost_amount * position.z;\n"
		"    gl_Position = position;\n"
		"    gl_Position.x = gl_Position.x * scale.x;\n"
		"    gl_Position.y = gl_Position.y * scale.y;\n"
		"    gl_Position.z = 0.0;\n"
		"    gl_Position.a = 1.0;\n"
		"}\n";

	const char *kBarFragmentShader =
		"uniform " MEDIUMP " vec4 color;\n"
		"varying " MEDIUMP " vec2 corner_out;\n"
		"varying " MEDIUMP " vec2 size;\n"
		"void main() {\n"
		"    " MEDIUMP " float alpha_x = min(corner_out.x * size.x, (1.0 - corner_out.x) * size.x);\n"
		"    " MEDIUMP " float alpha_y = min(corner_out.y * size.y, (1.0 - corner_out.y) * size.y);\n"
		"    gl_FragColor = color;\n"
		"    gl_FragColor.a = gl_FragColor.a * min(1.0, min(alpha_x, alpha_y));\n"
		"}\n";

	const char *kBarHorizontalVertexShader =
		"attribute " MEDIUMP " vec4 position;\n"
		"attribute " MEDIUMP " vec2 corner;\n"
		"uniform " MEDIUMP " float offset;\n"
		"uniform " MEDIUMP " float scale;\n"
		"uniform " MEDIUMP " float width_percent;\n"
		"uniform " MEDIUMP " vec2 dimensions;\n"
		"out " MEDIUMP " vec2 corner_out;\n"
		"out " MEDIUMP " vec2 size;\n"
		"void main()\n"
		"{\n"
		"    gl_Position = position;\n"
		"    size.x = position.z * dimensions.x / 2.0;\n"
		"    size.y = width_percent * dimensions.y / 2.0;\n"
		"    gl_Position.x = scale * (position.x + 1.0) - 1.0;\n"
		"    corner_out = corner;\n"
		"    gl_Position = gl_Position + vec4(0.0, offset - width_percent * corner.y, 0.0, 0.0);\n"
		"    gl_Position.z = 0.0;\n"
		"    gl_Position.w = 1.0;\n"
		"}\n";

	const char *kBarVerticalVertexShader =
		"attribute " MEDIUMP " vec4 position;\n"
		"attribute " MEDIUMP " vec2 corner;\n"
		"uniform " MEDIUMP " float offset;\n"
		"uniform " MEDIUMP " float scale;\n"
		"uniform " MEDIUMP " float width_percent;\n"
		"uniform " MEDIUMP " vec2 dimensions;\n"
		"out " MEDIUMP " vec2 corner_out;\n"
		"out " MEDIUMP " vec2 size;\n"
		"void main()\n"
		"{\n"
		"    gl_Position = position;\n"
		"    size.x = width_percent * dimensions.x / 2.0;\n"
		"    size.y = position.z * dimensions.y / 2.0;\n"
		"    gl_Position.x = scale * (position.x + 1.0) - 1.0;\n"
		"    corner_out = corner;\n"
		"    gl_Position = gl_Position + vec4(offset + width_percent * corner.x, 0.0, 0.0, 0.0);\n"
		"    gl_Position.z = 0.0;\n"
		"    gl_Position.w = 1.0;\n"
		"}\n";

	inline juce::String translateFragmentShader(const juce::String &code)
	{
	#if OPENGL_ES
		return String("#version 300 es\n") + "out mediump vec4 fragColor;\n" +
			code.replace("varying", "in").replace("texture2D", "texture").replace("gl_FragColor", "fragColor");
	#else
		return juce::OpenGLHelpers::translateFragmentShaderToV3(code);
	#endif
	}

	inline juce::String translateVertexShader(const juce::String &code)
	{
	#if OPENGL_ES
		return String("#version 300 es\n") + code.replace("attribute", "in").replace("varying", "out");
	#else
		return juce::OpenGLHelpers::translateVertexShaderToV3(code);
	#endif
	}
}

namespace Interface
{
	using namespace juce::gl;

	juce::OpenGLShaderProgram *Shaders::getShaderProgram(VertexShader vertex_shader,
		FragmentShader fragment_shader, const GLchar **varyings)
	{
		int shaderProgramIndex = vertex_shader * (int)kFragmentShaderCount + fragment_shader;
		if (shaderPrograms_.contains(shaderProgramIndex))
			return shaderPrograms_.at(shaderProgramIndex).get();

		shaderPrograms_[shaderProgramIndex] = std::make_unique<juce::OpenGLShaderProgram>(openGlContext_);
		juce::OpenGLShaderProgram *result = shaderPrograms_[shaderProgramIndex].get();
		GLuint program_id = result->getProgramID();
		glAttachShader(program_id, getVertexShaderId(vertex_shader));
		glAttachShader(program_id, getFragmentShaderId(fragment_shader));
		if (varyings)
			glTransformFeedbackVaryings(program_id, 1, varyings, GL_INTERLEAVED_ATTRIBS);

		result->link();
		return result;
	}

	const char *Shaders::getVertexShader(VertexShader shader)
	{
		switch (shader) {
		case kImageVertex:
			return kImageVertexShader;
		case kPassthroughVertex:
			return kPassthroughVertexShader;
		case kScaleVertex:
			return kScaleVertexShader;
		case kRotaryModulationVertex:
			return kRotaryModulationVertexShader;
		case kLinearModulationVertex:
			return kLinearModulationVertexShader;
		case kGainMeterVertex:
			return kGainMeterVertexShader;
		case kLineVertex:
			return kLineVertexShader;
		case kFillVertex:
			return kFillVertexShader;
		case kBarHorizontalVertex:
			return kBarHorizontalVertexShader;
		case kBarVerticalVertex:
			return kBarVerticalVertexShader;
		default:
			COMPLEX_ASSERT(false);
			return nullptr;
		}
	}

	const char *Shaders::getFragmentShader(FragmentShader shader)
	{
		switch (shader) {
		case kImageFragment:
			return kImageFragmentShader;
		case kTintedImageFragment:
			return kTintedImageFragmentShader;
		case kGainMeterFragment:
			return kGainMeterFragmentShader;
		case kLineFragment:
			return kLineFragmentShader;
		case kFillFragment:
			return kFillFragmentShader;
		case kBarFragment:
			return kBarFragmentShader;
		case kColorFragment:
			return kColorFragmentShader;
		case kFadeSquareFragment:
			return kFadeSquareFragmentShader;
		case kCircleFragment:
			return kCircleFragmentShader;
		case kRingFragment:
			return kRingFragmentShader;
		case kDiamondFragment:
			return kDiamondFragmentShader;
		case kRoundedCornerFragment:
			return kRoundedCornerFragmentShader;
		case kRoundedRectangleFragment:
			return kRoundedRectangleFragmentShader;
		case kRoundedRectangleBorderFragment:
			return kRoundedRectangleBorderFragmentShader;
		case kRotarySliderFragment:
			return kRotarySliderFragmentShader;
		case kRotaryModulationFragment:
			return kRotaryModulationFragmentShader;
		case kHorizontalSliderFragment:
			return kHorizontalSliderFragmentShader;
		case kVerticalSliderFragment:
			return kVerticalSliderFragmentShader;
		case kPinSliderFragment:
			return kPinSliderFragmentShader;
		case kPlusFragment:
			return kPlusFragmentShader;
		case kHighlightFragment:
			return kHighlightFragmentShader;
		case kDotSliderFragment:
			return kDotSliderFragmentShader;
		case kLinearModulationFragment:
			return kLinearModulationFragmentShader;
		case kModulationKnobFragment:
			return kModulationKnobFragmentShader;
		default:
			COMPLEX_ASSERT(false);
			return nullptr;
		}
	}

	bool Shaders::checkShaderCorrect(GLuint shaderId) const
	{
		GLint status = GL_FALSE;
		glGetShaderiv(shaderId, GL_COMPILE_STATUS, &status);

		if (status != GL_FALSE)
			return true;

		GLchar info[16384];
		GLsizei info_length = 0;
		glGetShaderInfoLog(shaderId, sizeof(info), &info_length, info);
		DBG(juce::String(info, (size_t)info_length));
		return false;
	}

	GLuint Shaders::createVertexShader(VertexShader shader) const
	{
		GLuint shader_id = glCreateShader(GL_VERTEX_SHADER);
		juce::String code_string = translateVertexShader(getVertexShader(shader));
		const GLchar *code = code_string.toRawUTF8();
		glShaderSource(shader_id, 1, &code, nullptr);
		glCompileShader(shader_id);

		COMPLEX_ASSERT(checkShaderCorrect(shader_id));
		return shader_id;
	}

	GLuint Shaders::createFragmentShader(FragmentShader shader) const
	{
		GLuint shader_id = glCreateShader(GL_FRAGMENT_SHADER);
		juce::String code_string = translateFragmentShader(getFragmentShader(shader));
		const GLchar *code = code_string.toRawUTF8();
		glShaderSource(shader_id, 1, &code, nullptr);
		glCompileShader(shader_id);

		COMPLEX_ASSERT(checkShaderCorrect(shader_id));
		return shader_id;
	}

}
