#ifndef MAPLETETRIS_TETRISSCENE_H
#define MAPLETETRIS_TETRISSCENE_H

#include "World.h"

/// Build the Tetris scene: the well, the side panel and the controller that
/// plays the game on them. Pass to World::setScene().
void createTetrisScene(World& game);

#endif //MAPLETETRIS_TETRISSCENE_H
