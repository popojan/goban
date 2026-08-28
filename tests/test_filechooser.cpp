// Tests for FileChooserDataSource — the logic behind the Load dialog.
//
// This file exists because the class was untestable for a structural reason
// rather than a technical one: it includes no RmlUi at all (filesystem,
// GameRecord, SGF and spdlog only), but it was listed in the `goban` app target,
// so `goban_tests` could not link it. Moving it into `goban_core` was a one-line
// CMake change and made all of it reachable.
//
// The dialog is where a lot of this project's bugs have lived, and the parts
// most likely to hold them are here rather than in the RmlUi wiring: directory
// enumeration, SGF preview parsing, the tsumego heuristic, and selection state
// that has to survive navigating between directories.
#include <doctest/doctest.h>

#include "FileChooserDataSource.h"

#include <algorithm>
#include <filesystem>

#ifndef GOBAN_TEST_DATA_DIR
#define GOBAN_TEST_DATA_DIR "tests/data"
#endif

namespace {

// Absolute, via the compile definition the suite already sets, so these cases do
// not depend on the working directory the binary was launched from.
const std::string kData = GOBAN_TEST_DATA_DIR;

int indexOfFile(const FileChooserDataSource& ds, const std::string& name) {
    const auto& files = ds.GetFiles();
    for (size_t i = 0; i < files.size(); ++i) {
        if (files[i].name == name) return static_cast<int>(i);
    }
    return -1;
}

}  // namespace

TEST_CASE("the file list enumerates the directory and offers a way up") {
    FileChooserDataSource ds(kData);
    ds.RefreshFiles();

    const auto& files = ds.GetFiles();
    REQUIRE_FALSE(files.empty());

    // Every fixture we rely on elsewhere is visible.
    CHECK(indexOfFile(ds, "simple.sgf") >= 0);
    CHECK(indexOfFile(ds, "two_games.sgf") >= 0);
    CHECK(indexOfFile(ds, "tsumego.sgf") >= 0);

    // Nothing outside the directory leaks in.
    CHECK(indexOfFile(ds, "run_scenarios.sh") == -1);

    CHECK(ds.GetCurrentPath() == std::filesystem::path(kData));
}

TEST_CASE("selecting an SGF loads its games and reports their headers") {
    FileChooserDataSource ds(kData);
    ds.RefreshFiles();

    const int idx = indexOfFile(ds, "two_games.sgf");
    REQUIRE(idx >= 0);
    ds.SelectFile(idx);

    // two_games.sgf holds a 9x9 of two moves and a 13x13 of three.
    CHECK(ds.GetNumRows("games") == 2);

    ds.SelectGame(0);
    const SGFGameInfo* first = ds.GetSelectedGame();
    REQUIRE(first != nullptr);
    CHECK(first->boardSize == 9);
    CHECK(first->moveCount == 2);
    CHECK(first->blackPlayer == "FirstBlack");
    CHECK(first->whitePlayer == "FirstWhite");

    ds.SelectGame(1);
    const SGFGameInfo* second = ds.GetSelectedGame();
    REQUIRE(second != nullptr);
    CHECK(second->boardSize == 13);
    CHECK(second->moveCount == 3);
    CHECK(second->blackPlayer == "SecondBlack");

    CHECK(ds.GetSelectedGameIndex() == 1);
    CHECK(ds.GetSelectedFilePath().find("two_games.sgf") != std::string::npos);
}

TEST_CASE("a game index outside the file is refused rather than accepted") {
    FileChooserDataSource ds(kData);
    ds.RefreshFiles();
    const int idx = indexOfFile(ds, "two_games.sgf");
    REQUIRE(idx >= 0);
    ds.SelectFile(idx);
    ds.SelectGame(1);

    ds.SelectGame(7);
    // The out-of-range selection must not stick, or the dialog would hand a
    // bad index to loadSGF and the load would fail with no visible cause.
    CHECK(ds.GetSelectedGameIndex() == 1);

    ds.SelectGame(-1);
    CHECK(ds.GetSelectedGameIndex() == 1);
}

TEST_CASE("selecting a file index outside the list changes nothing") {
    FileChooserDataSource ds(kData);
    ds.RefreshFiles();
    const int idx = indexOfFile(ds, "simple.sgf");
    REQUIRE(idx >= 0);
    ds.SelectFile(idx);

    const int before = ds.GetSelectedFileIndex();
    ds.SelectFile(9999);
    ds.SelectFile(-3);
    CHECK(ds.GetSelectedFileIndex() == before);
}

TEST_CASE("the tsumego heuristic needs white setup stones and no result") {
    FileChooserDataSource ds(kData);
    ds.RefreshFiles();

    SUBCASE("a problem with AB and AW and few moves is detected") {
        const int idx = indexOfFile(ds, "tsumego.sgf");
        REQUIRE(idx >= 0);
        ds.SelectFile(idx);
        CHECK(ds.isTsumegoDetected());
    }

    SUBCASE("a handicap game has AB but no AW, so it is not a problem") {
        // The distinction the heuristic exists to make: handicap stones are
        // setup stones too, and calling a handicap game a tsumego would start
        // the user in a mode they cannot leave without a new game.
        const int idx = indexOfFile(ds, "handicap.sgf");
        REQUIRE(idx >= 0);
        ds.SelectFile(idx);
        CHECK_FALSE(ds.isTsumegoDetected());
    }

    SUBCASE("an ordinary game with no setup stones is not a problem") {
        const int idx = indexOfFile(ds, "simple.sgf");
        REQUIRE(idx >= 0);
        ds.SelectFile(idx);
        CHECK_FALSE(ds.isTsumegoDetected());
    }

    SUBCASE("a finished game is never a problem, whatever its setup") {
        // resign.sgf carries RE, and a recorded result means a game that was
        // played rather than a puzzle to solve.
        const int idx = indexOfFile(ds, "resign.sgf");
        REQUIRE(idx >= 0);
        ds.SelectFile(idx);
        CHECK_FALSE(ds.isTsumegoDetected());
    }

    SUBCASE("nothing selected is not a problem either") {
        FileChooserDataSource fresh(kData);
        fresh.RefreshFiles();
        CHECK_FALSE(fresh.isTsumegoDetected());
    }
}

TEST_CASE("a malformed SGF yields no games instead of throwing") {
    FileChooserDataSource ds(kData);
    ds.RefreshFiles();

    const int idx = indexOfFile(ds, "malformed.sgf");
    REQUIRE(idx >= 0);
    ds.SelectFile(idx);

    // The dialog previews every file the user clicks, so a junk file in the
    // games folder must not take the application down with it.
    CHECK(ds.GetSelectedGame() == nullptr);
    CHECK_FALSE(ds.isTsumegoDetected());
}

TEST_CASE("changing directory clears the previous file's games") {
    FileChooserDataSource ds(kData);
    ds.RefreshFiles();
    const int idx = indexOfFile(ds, "two_games.sgf");
    REQUIRE(idx >= 0);
    ds.SelectFile(idx);
    REQUIRE(ds.GetNumRows("games") == 2);

    // Stale games from the previous directory would be offered against the new
    // file, and the index handed to loadSGF would refer to the wrong document.
    ds.NavigateUp();
    CHECK(ds.GetNumRows("games") == 0);
    CHECK(ds.GetSelectedGame() == nullptr);
    CHECK(ds.GetSelectedFileIndex() == -1);
}

TEST_CASE("FindFileByPath locates a file the caller already knows about") {
    FileChooserDataSource ds(kData);
    ds.RefreshFiles();

    // This is how ShowDialog re-selects the game already on the board.
    const std::string path = (std::filesystem::path(kData) / "simple.sgf").string();
    const int found = ds.FindFileByPath(path);
    CHECK(found >= 0);
    CHECK(ds.GetFiles()[static_cast<size_t>(found)].name == "simple.sgf");

    CHECK(ds.FindFileByPath((std::filesystem::path(kData) / "does_not_exist.sgf").string()) == -1);
}

TEST_CASE("pagination reports one page when everything fits") {
    FileChooserDataSource ds(kData);
    ds.RefreshFiles();

    // The page sizes are effectively unbounded today (10000), so the fixtures
    // occupy a single page. Pinned so that lowering them is a deliberate change
    // with a visible test failure rather than a silent truncation of the list.
    CHECK(ds.GetFilesTotalPages() == 1);
    CHECK(ds.GetFilesCurrentPage() == 1);
    CHECK_FALSE(ds.CanGoToFilesPrevPage());
    CHECK_FALSE(ds.CanGoToFilesNextPage());

    ds.SetFilesPage(5);
    CHECK(ds.GetFilesCurrentPage() == 1);   // clamped, not accepted
}

TEST_CASE("a nonexistent directory leaves an empty list rather than throwing") {
    FileChooserDataSource ds((std::filesystem::path(kData) / "no_such_directory").string());
    ds.RefreshFiles();
    CHECK(ds.GetFiles().empty());
    CHECK(ds.GetSelectedFile() == nullptr);
}
