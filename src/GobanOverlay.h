//
// Created by jan on 21.6.17.
//

#ifndef GOBAN_GOBANOVERLAY_H
#define GOBAN_GOBANOVERLAY_H

#include <ft2build.h>
//#include  "glyphy-freetype.h"
#include  "glyphy/demo-font.h"
#include "Metrics.h"
#include "GobanModel.h"
#include "Camera.h"
#include <spdlog/spdlog.h>
#include <GlyphyGLState.h>

class GobanView;

struct Layer {
	float height;
	glm::vec4 color;
	bool empty;
};

class GobanOverlay {
public:
    GobanOverlay(const GobanView& view): view(view), overlayReady(false) {
		console = spdlog::get("console");
    }
    bool init();
    void use();
    void unuse();
    void draw(const GobanModel&, const DDG::Camera&, int updateFlag, float time, unsigned);
    ~GobanOverlay();
    void setReady() { overlayReady = true; }
	void Update(const Board::Overlay& overlay, const GobanModel& model);
private:
    FT_Library ft_library;
    FT_Face ft_face;
    demo_font_t *font;
    const GobanView& view;
    bool overlayReady;
	double font_size;
	static std::array<Layer, 3> layers;
	std::shared_ptr<spdlog::logger> console;
	std::shared_ptr<GlyphyGLState> st;
};


#endif //GOBAN_GOBANOVERLAY_H
