#ifndef GOBAN_GOBANOVERLAY_H
#define GOBAN_GOBANOVERLAY_H

#include  "glyphy/GlyphyFont.h"
#include "GobanModel.h"
#include "Camera.h"
#include <GlyphyState.h>

class GobanView;

struct Layer {
	float height;
	glm::vec4 color;
	bool empty;
};

class GobanOverlay {
public:
	explicit GobanOverlay(const GobanView& view): ft_library(nullptr), ft_face(nullptr),
	                                              view(view), overlayReady(false), font_size(.0)
    {
        init();
    }
    bool init();

	static void use();

	static void unuse();
    void draw(const GobanModel&, const DDG::Camera&, unsigned) const;
    ~GobanOverlay();
    void setReady() { overlayReady = true; }
	void Update(const Board& board, const GobanModel& model);
private:
    FT_Library ft_library;
    FT_Face ft_face;
	std::shared_ptr<GlyphyFont> font;
    const GobanView& view;
    bool overlayReady;
	double font_size;
	static std::array<Layer, 3> layers;
	std::shared_ptr<GlyphyState> st;
};


#endif //GOBAN_GOBANOVERLAY_H
