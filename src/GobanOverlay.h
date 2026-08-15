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

/** \brief Text placed by board coordinate rather than by board point.
 *
 * The per-point overlays take their position from the point that holds them, so
 * they can only ever sit on an intersection. This carries its own coordinate in
 * the same space — board units from the centre — but as a float, and signed. The
 * grid occupies [0, N-1]; the wood extends 0.85 spacings beyond it on all four
 * sides, so anything in [-0.85, 0) is in the margin.
 *
 * That the space is shared with the point overlays is the point of it: no second
 * mental model, and a margin coordinate scales across board sizes for free,
 * since the 0.85 is in grid units on every board.
 *
 * Unlike a point overlay this touches no material — there is no grid out in the
 * margin to erase — and nothing clears it when a stone lands.
 */
struct FloatingLabel {
	glm::vec2 boardPos;   ///< Board coordinates; centred on this point.
	std::string text;
	float size;           ///< Em size in world units, as add_text takes it.
	glm::vec4 color;
	unsigned layer;       ///< Which pass and height. 0 is the board surface.
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

	/// Replaces the free-positioned labels drawn on top of the point overlays.
	/// Cheap to call every frame: it is a small vector and Update() rebuilds
	/// every buffer anyway.
	void setFloatingLabels(std::vector<FloatingLabel> labels) {
		floating = std::move(labels);
	}
private:
    FT_Library ft_library;
    FT_Face ft_face;
	std::shared_ptr<GlyphyFont> font;
    const GobanView& view;
    bool overlayReady;
	double font_size;
	static std::array<Layer, 3> layers;
	std::vector<FloatingLabel> floating;
	std::shared_ptr<GlyphyState> st;
};


#endif //GOBAN_GOBANOVERLAY_H
