#ifndef GOBAN_WAITINDICATOR_H
#define GOBAN_WAITINDICATOR_H

/** \file
 *  \brief The board's own report that it is waiting, and the glyph atlas it needs.
 *
 * Two waits used to be a line of text in `#lblStatus` — a wide black-backed
 * banner laid over a board that took some trouble to render. The text was right
 * about the problem it solved and wrong about where to say it: a genmove was
 * completely silent (30.9 s measured for one kibitz from the stock 9x9 KataGo on
 * a CPU backend, reported as "nothing happens"), so *something* had to be shown,
 * but the board is the product and a panel over it reads as chrome.
 *
 * They are drawn in the wood margin instead, through the overlay's glyph pass —
 * the one layer every shader has. `#lblStatus` keeps the two tenants that are
 * about the *application* rather than the game: which engine is still loading,
 * and the message badge.
 *
 * What lives here is the part with a right answer: what the indicator shows at a
 * given moment, and the atlas composition. Both are pure over plain data so they
 * test without a GL context, a font or a thread — the same reason
 * `availableActions()` takes plain data rather than a `GobanModel`.
 */

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

/// Which wait the board is reporting, if any.
enum class WaitKind { None, Thinking, Syncing };

inline const char* waitKindName(WaitKind kind) {
    switch (kind) {
        case WaitKind::Thinking: return "thinking";
        case WaitKind::Syncing:  return "syncing";
        case WaitKind::None:     break;
    }
    return "none";
}

namespace Wait {

/// Nothing is drawn yet — still inside the grace period.
constexpr int NOT_SHOWN = -1;

/// What the indicator shows for a given elapsed time: NOT_SHOWN while it is not
/// worth mentioning yet, otherwise whole seconds.
///
/// This is the whole animation, and it is deliberately the only one. The mark
/// does not pulse: a board annotation is *carved*, or it is not there — an
/// opacity that breathes reads as a screen effect laid over the scene rather
/// than as part of it, which is exactly the quality the diegetic version exists
/// to have. The count ticking over once a second is the liveness signal, and it
/// is the physical kind of motion, the kind a clock beside a board has.
///
/// It doubles as the repaint gate. A frame is worth drawing when this value
/// changes and not otherwise, so a wait costs one frame per second rather than
/// the twenty `getIdleTimeout()` makes available.
///
/// The grace period is not decoration either: GNU Go answers a genmove in 13 ms,
/// so without it every move of a bot-versus-bot match puts the mark on screen
/// for a single frame — which is precisely the "something is broken" reading the
/// indicator exists to prevent. The clock still runs from the true start, so the
/// first count shown is honest.
inline int displayedSecond(float elapsedSeconds, float graceSeconds) {
    if (!(graceSeconds >= 0.0f)) graceSeconds = 0.0f;      // NaN-safe
    if (!(elapsedSeconds >= graceSeconds)) return NOT_SHOWN;
    return static_cast<int>(elapsedSeconds);
}

/// The characters every build can draw. Digits and letters for move numbers,
/// variation labels and the evaluation's A/B/C; the punctuation for the
/// evaluation readout ("B+4.5", "62%") and this indicator's count ("12s").
constexpr const char* BASE_ATLAS =
    "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz#^O+-.%";

/// Splits a UTF-8 string into whole characters, the same way
/// GlyphyBuffer::add_text decodes it — which is what decides which codepoints
/// are actually asked for at draw time, so a second rule here would let the
/// atlas and the draw disagree about what a byte sequence means.
inline std::vector<std::string> utf8Chars(const std::string& s) {
    std::vector<std::string> out;
    for (size_t i = 0; i < s.size();) {
        const auto c = static_cast<unsigned char>(s[i]);
        size_t len = 1;
        if (c >= 0xF0)      len = 4;
        else if (c >= 0xE0) len = 3;
        else if (c >= 0xC0) len = 2;
        len = std::min(len, s.size() - i);
        out.push_back(s.substr(i, len));
        i += len;
    }
    return out;
}

/// The codepoint of a whole UTF-8 character, for asking a font whether it has one.
inline unsigned long codepoint(const std::string& ch) {
    if (ch.empty()) return 0;
    const auto c = static_cast<unsigned char>(ch[0]);
    if (c < 0x80) return c;
    unsigned long cp;
    size_t len;
    if (c >= 0xF0)      { len = 4; cp = c & 0x07u; }
    else if (c >= 0xE0) { len = 3; cp = c & 0x0Fu; }
    else                { len = 2; cp = c & 0x1Fu; }
    for (size_t j = 1; j < len && j < ch.size(); ++j)
        cp = (cp << 6) | (static_cast<unsigned char>(ch[j]) & 0x3Fu);
    return cp;
}

/// BASE_ATLAS plus every character the configuration asks to be drawn, in order,
/// without duplicates.
///
/// The atlas used to be a string literal, which made it a *silent* gate: a glyph
/// absent from it simply does not appear, with no error anywhere. That was
/// survivable while every drawable string was written in C++. It stops being so
/// the moment `annotations.wait_glyph` lets someone name a character we have
/// never seen — so the configured strings are folded in here, and the caller
/// asks the font whether it actually has each one.
inline std::string atlasWith(const std::vector<std::string>& extras) {
    std::string atlas(BASE_ATLAS);
    for (const auto& s : extras) {
        for (const auto& ch : utf8Chars(s)) {
            if (atlas.find(ch) == std::string::npos) atlas += ch;
        }
    }
    return atlas;
}

}  // namespace Wait

#endif  // GOBAN_WAITINDICATOR_H
