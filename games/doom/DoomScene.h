#ifndef MAPLEDOOM_DOOMSCENE_H
#define MAPLEDOOM_DOOMSCENE_H

#include "World.h"
#include <string>

/// Build the game: one entity carrying the controller that runs and draws
/// all of it. Pictures in `artDirectory`, if given, replace the built in
/// art. Wrap in a lambda for World::setScene().
void createDoomScene(World& game, const std::string& artDirectory);

#endif //MAPLEDOOM_DOOMSCENE_H
