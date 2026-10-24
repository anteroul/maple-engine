#ifndef MAPLESANDBOX_LEVEL_H
#define MAPLESANDBOX_LEVEL_H

#include "Simulation.h"

namespace sandbox {

/// Fill the simulation with something to play with: rolling stone ground, a
/// pool of water with oil floating on it, a wooden hut with a gunpowder
/// stash, a sand dune, a sand-laden ledge, and a pit of lava. Sized for
/// about 320 x 180, and scaled to fit others.
void buildStarterLevel(Simulation& simulation);

} // namespace sandbox

#endif //MAPLESANDBOX_LEVEL_H
