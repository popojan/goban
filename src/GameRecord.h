#ifndef GOBAN_GAMERECORD_H
#define GOBAN_GAMERECORD_H

#include <ISgfcGame.h>

class GameRecord {
public:
    std::shared_ptr<LibSgfcPlusPlus::ISgfcGame> game;

};

#endif //GOBAN_GAMERECORD_H
