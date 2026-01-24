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
	    b->add_text("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz#^O", font, 12.0);
		buffer[i] = b;
	}

    font->print_stats();
    glUseProgram(0);
    return true;
}
void GobanOverlay::use() { }
void GobanOverlay::unuse() { }

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
				buffer[layer]->add_text(point.overlay.text.c_str(), font, font_size);
				cnt += 1;
			}
			idx++;
		}
		layers[layer].empty = cnt == 0;
	}

}
void GobanOverlay::draw(const GobanModel& model, const DDG::Camera& cam, unsigned which) const {
	if (!overlayReady
		|| std::all_of(layers.begin(), layers.end(), [](const Layer& x){return x.empty; }))
			return;

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

	for (size_t layer = which; layer < (which == 0 ? 1 : layers.size()); ++layer) {

		if (layers[layer].empty)
			continue;

		glm::mat4 m(cam.setView());

		st->set_color(glm::value_ptr(layers[layer].color));
		st->fast_setup();

		using namespace glm;
		vec4 ta = vec4(.0f, -layers[layer].height*model.metrics.h*view.gobanShader.getStoneHeight(), .0f, 0.0f);//-0.5*stoneh
		vec4 tt = vec4(view.newTranslate[0], view.newTranslate[1], view.newTranslate[2], 0.0f);
		ta += tt;

		glyphy_extents_t extents;
		buffer[layer]->extents(nullptr, &extents);
		float content_scale = std::min(static_cast<float>(height) / 2.0f, 10000.0f);
		float text_scale = content_scale;
		{
			float x = -content_scale * (transpose(m) * ta).x;
			float y = -content_scale * (transpose(m)* ta).y;
			float z = content_scale * (transpose(m) * ta).z;

			auto d = static_cast<float>(height);
			float near = d;
			float factor = 0.01f * near / (2 * near + d);
			float far = near + 10.0f * d;
			mat = frustum(
                -static_cast<float>(width) * factor, static_cast<float>(width) * factor,
                -static_cast<float>(height) * factor, static_cast<float>(height) * factor,
                0.01f * near, far
            );
			mat = translate(mat, vec3(x, y, -0.5f*d - near + z));
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

		glPopMatrix();
	}

    glUseProgram(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    glPopAttrib();

    glUseProgram(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

}

GobanOverlay::~GobanOverlay() {
    FT_Done_Face(ft_face);
    FT_Done_FreeType(ft_library);
}
