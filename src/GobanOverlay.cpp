#include "GobanOverlay.h"
#include "GobanView.h"

#undef HAVE_CONFIG_H
#include "glyphy/GlyphyBuffer.h"
#include "glyphy/GlyphyState.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <memory>

const char *font_path = nullptr;

static std::array<std::shared_ptr<GlyphyBuffer>, 3> buffer;

std::array<Layer, 3> GobanOverlay::layers = {
	{ { 0.0f, glm::vec4(0.0,0.0,0.0, 1.0) },
	{ 1.0f, glm::vec4(0.9, 0.9, 0.9, 1.0) },
	{ 1.0f, glm::vec4(0.1, 0.1, 0.1, 1.0) } }
};

bool GobanOverlay::init() {
	st = std::make_shared<GlyphyState>();

    if(FT_Init_FreeType(&ft_library) != 0) {
        spdlog::warn("Failed to load freetype library.");
        return false;
    }
    ft_face = nullptr;
    if (font_path) {
        FT_New_Face(ft_library, font_path, 0/*face_index*/, &ft_face);
        if(ft_face) {
            spdlog::info("Default font file loaded from {}", font_path);
        }
    }
    else {
        using nlohmann::json;
        std::string overlay_font = config->data
                .value("fonts", json({}))
                .value("overlay", "./config/fonts/default-font.ttf");
        FT_New_Face(ft_library, overlay_font.c_str(), 0/*face_index*/, &ft_face);
        if(ft_face) {
            spdlog::info("Loading font file [{}]", overlay_font);
        }
    }
    if (!ft_face) {
		spdlog::error("Failed to open font file");
	}
    st->setup();

    font = std::make_shared<GlyphyFont>(ft_face, st->get_atlas());

	for (size_t i = 0; i < layers.size(); ++i) {
		spdlog::debug("Creating overlay buffer[{0}]", i);
	    auto b = std::make_shared<GlyphyBuffer>();
        glyphy_point_t p = {.0, .0};
        b->move_to(&p);
        spdlog::debug("Adding text glyphs[{0}]", i);
        // Warms the atlas; the geometry is cleared before anything is drawn, so
        // the colour is irrelevant here.
        static const GLfloat white[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        // Every character that can ever be drawn has to be in the atlas. The
        // punctuation is for the evaluation readout ("B+4.5", "62%"); the font
        // has all of it, but a glyph absent from this string simply does not
        // appear.
	    b->add_text("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz#^O+-.%",
	                font, 12.0, white);
		buffer[i] = b;
	}

    font->print_stats();
    glUseProgram(0);
    return true;
}
void GobanOverlay::use() { }
void GobanOverlay::unuse() { }

// The ink a label is actually built with. Under an anaglyph shader the two eyes
// go into separate colour channels, so a label keeps only its brightness —
// `partial/stereo/on.glsl` reduces the board to `(r+g+b)/3` per eye for exactly
// the same reason, and text tinted green would otherwise vanish from the red
// eye entirely. The alpha is left alone: it is the glyph's antialiasing.
//
// Baked in here rather than applied at draw time because glyphy carries the
// colour in the vertex buffer. GobanView::Update() therefore asks for an
// overlay rebuild whenever the selected shader changes.
glm::vec4 GobanOverlay::eyeInk(const glm::vec4& color) const {
	if (!view.gobanShader.isStereo()) return color;
	const float luma = (color.r + color.g + color.b) / 3.0f;
	return {luma, luma, luma, color.a};
}

void GobanOverlay::Update(const Board& board, const GobanModel& model) {
	font_size = 0.8 / model.getBoardSize();

    auto& points = board.get();
    int boardSize = board.getSize();
    float halfN = 0.5f * static_cast<float>(boardSize) - 0.5f;

	for (size_t layer = 0; layer < layers.size(); ++layer) {
		int cnt = 0;
		buffer[layer]->clear();
		int idx = 0;
		for (const auto & point : points) {
			if (!point.overlay.text.empty() && point.overlay.layer == layer) {
				float posX, posY;
				if (layer == 0) {
					// Board-level overlay: use exact grid position (no fuzzy offset)
					// Points are indexed as ord(p) = col * MAX_BOARD + row
					int col = idx / Board::MAX_BOARD;
					int row = idx % Board::MAX_BOARD;
					posX = static_cast<float>(col) - halfN;
					posY = static_cast<float>(row) - halfN;
				} else {
					// Stone-level overlay: use fuzzy position (matches stone placement)
					posX = point.x;
					posY = point.y;
				}
                glyphy_point_t pos = { model.metrics.squareSizeX * posX, -model.metrics.squareSizeY * posY };
				buffer[layer]->move_to(&pos);
				// The label's own colour when it has one, else the layer's. The
				// layer array is still the palette everything ordinary draws
				// from; an explicit colour is how the evaluation overlay tints a
				// single label — including one the navigation overlay wrote —
				// without needing a layer of its own.
				const glm::vec4 color = eyeInk(point.overlay.color
				                       ? *point.overlay.color
				                       : layers[layer].color);
				buffer[layer]->add_text(point.overlay.text.c_str(), font, font_size,
				                        glm::value_ptr(color));
				cnt += 1;
			}
			idx++;
		}
		// Free-positioned labels ride the same buffers. Same coordinate space
		// as the loop above — board units from the centre — but float and
		// signed, so they can sit out in the margin where there is no point to
		// hang them on. add_text centres on the cursor, so the coordinate given
		// is the middle of the text.
		for (const auto& label : floating) {
			if (label.layer != layer) continue;
			glyphy_point_t pos = {
				model.metrics.squareSizeX * (label.boardPos.x - halfN),
				-model.metrics.squareSizeY * (label.boardPos.y - halfN)
			};
			buffer[layer]->move_to(&pos);
			const glm::vec4 ink = eyeInk(label.color);
			buffer[layer]->add_text(label.text.c_str(), font, label.size,
			                        glm::value_ptr(ink), label.align);
			cnt += 1;
		}

		layers[layer].count = cnt;
		layers[layer].empty = cnt == 0;
	}

}
unsigned GobanOverlay::draw(const GobanModel& model, const DDG::Camera& cam, unsigned which,
                            int eye) const {
	if (!overlayReady
		|| std::all_of(layers.begin(), layers.end(), [](const Layer& x){return x.empty; }))
			return 0;

	unsigned drawn = 0;

    glPushAttrib(GL_ALL_ATTRIB_BITS);

	glm::mat4 mat(1.0);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadMatrixf(glm::value_ptr(mat));

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();

	int viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);
	GLint width = viewport[2];
	GLint height = viewport[3];

	st->set_depth(which < 1 ? 0.6 : 0.4);

	// This call draws one eye; the caller loops. The two terms that differ are
	// exactly the two the stereo vertex shader applies to the board — the eye
	// offset and the horizontal image shift — and the offset comes from
	// GobanView::stereoHalfBase(), the same value the shader is handed. Two
	// implementations would let the labels drift off the wood they are supposed
	// to be lying on.
	//
	// The loop used to be here, both eyes back to back after a single board
	// pass. It moved out to GobanView::Render() so that each eye's text is drawn
	// against a depth buffer holding that eye's own board: with one buffer for
	// both, whichever occlusion it held clipped the other eye's labels.
	const bool stereo = view.gobanShader.isStereo();
	const float halfBase = stereo ? view.stereoHalfBase() : 0.0f;
	const float hit = stereo ? view.gobanShader.getDof() : 0.0f;
	const int pass = stereo ? eye : 0;

	// The text writes no depth. It runs last within its eye's pass, over a board
	// that is already resolved, and both layers sit at fixed depths of their own
	// — so with writes on, the first layer drawn would reject the second.
	glDepthMask(GL_FALSE);

	for (size_t layer = which; layer < (which == 0 ? 1 : layers.size()); ++layer) {

		if (layers[layer].empty)
			continue;

		glm::mat4 m(cam.setView());

		// White: the colour now travels with each glyph, and u_color is a global
		// multiplier over it. Leaving the layer colour here would square it.
		static const float noTint[4] = {1.0f, 1.0f, 1.0f, 1.0f};
		st->set_color(const_cast<float*>(noTint));
		st->fast_setup();

		using namespace glm;
		// Board target with layer height offset, in world space
		vec3 boardTarget(view.cameraPan.x,
		                 -layers[layer].height * model.metrics.h * view.gobanShader.getStoneHeight(),
		                 view.cameraPan.y);
		// Transform to camera-local space (transpose(m) = m^-1 for rotation)
		vec3 ta_cam = vec3(transpose(m) * vec4(boardTarget, 0.0f));
		// Camera is cameraDistance away along +Z in camera space
		ta_cam.z += GobanView::FOCAL_LENGTH - view.cameraDistance;

		glyphy_extents_t extents;
		buffer[layer]->extents(nullptr, &extents);
		float content_scale = std::min(static_cast<float>(height) / 2.0f, 10000.0f);
		float text_scale = content_scale;

		{
		// The left eye sits at -halfBase along the camera's right axis, so the
		// target's camera-space X grows by that much; the image shift goes the
		// same way, the shader's `q0.x + dof`.
		const float side = (pass == 0) ? 1.0f : -1.0f;
		if (stereo) {
			// Exactly the channels this eye's image occupies — asked of the same
			// function the shader's composite follows, because the answer is not
			// a constant: green belongs to the right eye in red/cyan and to the
			// left in red/blue, and in Gray it belongs to neither.
			//
			// Following the board here is not tidiness. Text in a channel the
			// board is not using ghosts on its own, and text in the *other* eye's
			// channel is a second picture — the two failures this whole setting
			// exists to avoid, reproduced by the overlay alone.
			//
			// The text is masked even though the board it lies on is summed
			// rather than masked: a label carries no colour worth preserving,
			// eyeInk() having already reduced it to brightness.
			const auto own = Stereo::eyeChannels(view.anaglyph(), view.glasses(), pass);
			glColorMask(own.r ? GL_TRUE : GL_FALSE,
			            own.g ? GL_TRUE : GL_FALSE,
			            own.b ? GL_TRUE : GL_FALSE, GL_FALSE);
		}
		{
			float F = GobanView::FOCAL_LENGTH;
			float cs = content_scale;

			// Minus, because this frame's x runs opposite to the camera's: the
			// layer origin is placed at -cs*ta_cam.x. Getting it backwards adds
			// the eye offset to the window shift instead of subtracting it, and
			// the labels then separate by the sum of the two — measured 9.5% of
			// the image width where the stone under them separated by 3.5%,
			// which is what "the number floats miles off the stone" looks like.
			float x = -cs * (ta_cam.x - side * halfBase);
			float y = -cs * ta_cam.y;
			float z = cs * ta_cam.z;

			// Frustum matching ray-traced FOV: tan(halfFOV) = 1/F
			float nearPlane = 0.01f * height;
			float farPlane = 11.0f * height;
			float halfH = nearPlane / F;
			float halfW = halfH * static_cast<float>(width) / static_cast<float>(height);
			// An off-axis window is the horizontal image shift: moving it by s
			// slides the image by -s/halfH in q0 units, and the shader wants
			// -dof for the left eye. That is the stereoscopic window, and it
			// deliberately plays no part in the deviation — it moves the whole
			// depth range through the screen plane rather than stretching it.
			const float shift = side * halfH * hit;
			mat = frustum(-halfW + shift, halfW + shift, -halfH, halfH, nearPlane, farPlane);

			// Position overlay at focal plane: F/2 * height from near plane
			float baseZ = -(F / 2.0f) * static_cast<float>(height);
			mat = translate(mat, vec3(x, y, baseZ + z));
		}
		mat = scale(mat, vec3(1, 1, -1));
		mat4 rm(transpose(m)*rotate(mat4(1.0f), 3.141592656f / 2, vec3(1.0f, 0.0f, 0.0f)));

		mat = mat*rm;
		mat = scale(mat, vec3(1, -1, 1));

		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();

		glLoadMatrixf(value_ptr(mat));


		mat = scale(mat, vec3(text_scale));
		// Center buffer

		mat = translate(mat, vec3(
			-static_cast<float>(extents.max_x + extents.min_x) / 2.0f,
			-static_cast<float>(extents.max_y + extents.min_y) / 2.0f, 0.0f));

		st->set_matrix(value_ptr(mat));
		buffer[layer]->draw();
		drawn += static_cast<unsigned>(layers[layer].count);

		glPopMatrix();
		}
	}

    glUseProgram(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    glPopAttrib();

    glUseProgram(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    return drawn;
}

GobanOverlay::~GobanOverlay() {
    FT_Done_Face(ft_face);
    FT_Done_FreeType(ft_library);
}
