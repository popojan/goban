//
// Created by jan on 21.6.17.
//

#include "GobanOverlay.h"
#include "GobanView.h"

#undef HAVE_CONFIG_H
#include "glyphy/GlyphyBuffer.h"
#include "glyphy/GlyphyState.h"

#include <glm/vec3.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

//#define DEBUG_OVERLAY

const char *font_path = NULL;

static std::array<std::shared_ptr<GlyphyBuffer>, 3> buffer;

std::array<Layer, 3> GobanOverlay::layers = {
	{ { 0.0f, glm::vec4(0.0,0.0,0.0, 1.0) },
	{ 1.0f, glm::vec4(0.9, 0.9, 0.9, 1.0) },
	{ 1.0f, glm::vec4(0.1, 0.1, 0.1, 1.0) } }
};

bool GobanOverlay::init() {
	st = std::shared_ptr<GlyphyState>(new GlyphyState());

    FT_Init_FreeType(&ft_library);
    ft_face = NULL;
    if (font_path) {
        FT_New_Face(ft_library, font_path, 0/*face_index*/, &ft_face);
    }
    else {
        FT_New_Face(ft_library, "data/gui/default-font.ttf", 0/*face_index*/, &ft_face);
    }
    if (!ft_face) {
		console->error("Failed to open font file");
	}
    font = std::shared_ptr<GlyphyFont>(new GlyphyFont(ft_face, st->get_atlas()));

    st->setup();

	for (std::size_t i = 0; i < layers.size(); ++i) {
		console->debug("Creating overlay buffer[{0}]", i);
	    auto b = std::shared_ptr<GlyphyBuffer>(new GlyphyBuffer());
        console->debug("Adding text glyphs[{0}]", i);
	    b->add_text("0123456789", font, 12.0);
		buffer[i] = b;
	}

    font->print_stats();
    glUseProgram(0);
    return true;
}
void GobanOverlay::use() { }
void GobanOverlay::unuse() { }

void GobanOverlay::Update(const Board::Overlay& overlay, const GobanModel& model) {
	font_size = 0.8 / model.getBoardSize();
	
	for (std::size_t layer = 0; layer < layers.size(); ++layer) {
		int cnt = 0;
		buffer[layer]->clear();
		for (auto oit = overlay.begin(); oit != overlay.end(); ++oit) {
			if (!oit->text.empty() && oit->layer == layer) {
				glyphy_point_t pos = { model.metrics.squareSize * oit->x, -model.metrics.squareSize * oit->y };
				buffer[layer]->move_to(&pos);
				buffer[layer]->add_text(oit->text.c_str(), font, font_size);
				cnt += 1;
			}
		}
		layers[layer].empty = cnt == 0;
	}

}
void GobanOverlay::draw(const GobanModel& model, const DDG::Camera& cam, int updateFlag, float time, unsigned which) {
	if (!overlayReady
		|| std::all_of(layers.begin(), layers.end(), [](const Layer& x){return x.empty; }))
			return;

    glPushAttrib(GL_ALL_ATTRIB_BITS);

    //rewrite using glm
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
		
	for (std::size_t layer = which; layer < (which == 0 ? 1 : layers.size()); ++layer) {
		
		if (layers[layer].empty)
			continue;

		glm::mat4 m(cam.setView());
		
		mat = glm::mat4(1.0);

		st->set_color(glm::value_ptr(layers[layer].color));
		st->fast_setup();

		using namespace glm;
		vec4 ta = vec4(.0f, -layers[layer].height*model.metrics.h*view.gobanShader.getStoneHeight(), .0f, 0.0f);//-0.5*stoneh
		vec4 up = vec4(.0f, .1f, .0f, 0.0f);
		vec4 tt = vec4(view.newTranslate[0], view.newTranslate[1], view.newTranslate[2], 0.0f);
		ta += tt;
		up = normalize(m*up);

		glyphy_extents_t extents;
		buffer[layer]->extents(NULL, &extents);
		float content_scale = std::min(height / 2.0f, 10000.0f);
		float text_scale = content_scale;
		float x = -content_scale * (glm::transpose(m) * ta).x;//(extents.max_x + extents.min_x) / 2.;
		float y = -content_scale * (glm::transpose(m)* ta).y;//(extents.max_y + extents.min_y) / 2.;
		float z = content_scale * (glm::transpose(m) * ta).z;
		{

			float d = height;
			float near = d;
			float factor = 0.01f * near / (2 * near + d);
			float far = near + 10.0f * d;
			mat = glm::frustum(-width * factor, width * factor, -height * factor, height * factor, 0.01f * near, far);
			mat = glm::translate(mat, glm::vec3(x, y, -0.5f*d - near + z));
		}
		mat = glm::scale(mat, glm::vec3(1, 1, -1));
		glm::mat4 rm(glm::transpose(m)*glm::rotate(glm::mat4(1.0f), 3.141592656f / 2, glm::vec3(1.0f, 0.0f, 0.0f)));

		mat = mat*rm;
		mat = glm::scale(mat, glm::vec3(1, -1, 1));

		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();

		glLoadMatrixf(glm::value_ptr(mat));


		mat = glm::scale(mat, glm::vec3(text_scale));
		// Center buffer

		mat = glm::translate(mat, glm::vec3(
			-static_cast<float>(extents.max_x + extents.min_x) / 2.0f,
			-static_cast<float>(extents.max_y + extents.min_y) / 2.0f, 0.0f));

		st->set_matrix(glm::value_ptr(mat));
		buffer[layer]->draw();

		glPopMatrix();
	}

    glUseProgram(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

#ifdef DEBUG_OVERLAY
    glEnable(GL_BLEND);
    glColor4f(1.0f,0.0f,0.0f,0.2f);
    glBegin(GL_QUADS);
    double a = content_scale;
    x = 0.0;
    y = 0.0;

    glVertex3f(a*-1.0f+x, a*-1.0f+y, 0.0);
    glVertex3f(a*1.0f+x, a*-1.0f+y, 0.0);
    glVertex3f(a*1.0f+x, a*1.0f+y, 0.0);
    glVertex3f(a*-1.0f+x, a*1.0f+y, 0.0);
    glEnd();
#endif

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
