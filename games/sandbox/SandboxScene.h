#ifndef MAPLESANDBOX_SANDBOXSCENE_H
#define MAPLESANDBOX_SANDBOXSCENE_H

#include "World.h"

/// Build the sandbox: one entity carrying the controller that runs and draws
/// the whole simulation. Pass to World::setScene().
void createSandboxScene(World& game);

#endif //MAPLESANDBOX_SANDBOXSCENE_H
