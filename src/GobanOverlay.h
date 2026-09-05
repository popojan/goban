#ifndef GOBAN_GOBANOVERLAY_H
#define GOBAN_GOBANOVERLAY_H

#include  "glyphy/GlyphyBuffer.h"
#include  "glyphy/GlyphyFont.h"
#include "GobanModel.h"
#include "Camera.h"
#include "WaitIndicator.h"
#include <GlyphyState.h>

class GobanView;

struct Layer {
	float height;
	glm::vec4 color;
	/// Nothing has been put in this layer yet; Update() sets it.
	bool empty = true;
	/// How many text items Update() put in this layer's buffer. `empty` is the
	/// same fact as a boolean; the count is what draw() reports back, so that a
	/// scenario can assert what was *drawn* rather than what was built.
	int count = 0;
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
	TextAlign align = TextAlign::Center;  ///< Where boardPos sits in the text.
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
	/// Draws one pass — layer 0, or layers 1 and up — and returns how many text
	/// items went out. The count is the only evidence there is that the glyph
	/// pass ran at all: every overlay in the program is built into these same
	/// buffers, so `eval_labels` or `coordinates_shown` can be perfectly true
	/// with nothing on screen. Same argument as `sounds_played`.
	/// Draw one layer group for one eye. `eye` is 0 under a mono shader and is
	/// driven by the caller under a stereo one, because the board has to be
	/// rendered per eye as well — each eye's text must be tested against that
	/// eye's own depth, which is the whole reason the passes are interleaved.
	/// See the eye loop in GobanView::Render().
	unsigned draw(const GobanModel&, const DDG::Camera&, unsigned, int eye = 0) const;
    ~GobanOverlay();
    void setReady() { overlayReady = true; }
	void Update(const Board& board, const GobanModel& model);

	/// The ink a label is built with: itself in mono, its own brightness under
	/// an anaglyph shader, where the two eyes live in separate colour channels.
	[[nodiscard]] glm::vec4 eyeInk(const glm::vec4& color) const;

	/// Replaces the free-positioned labels drawn on top of the point overlays.
	/// Cheap to call every frame: it is a small vector and Update() rebuilds
	/// every buffer anyway.
	void setFloatingLabels(std::vector<FloatingLabel> labels) {
		floating = std::move(labels);
	}

	/// Wait::BASE_ATLAS plus every character the configuration asks to be drawn,
	/// warning about any the font cannot supply. The composition itself is
	/// Wait::atlasWith() in WaitIndicator.h, where it tests without a font.
	static std::string atlasString(FT_Face face);
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
