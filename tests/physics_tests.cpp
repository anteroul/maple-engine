// Headless check of the Box2D-driven simulation. Builds against the real
// engine sources; never opens a window, so it runs in a container.
#include "../src/Physics.h"
#include "../src/ECS/Entity.h"
#include "../src/ECS/Components/RigidBody.h"
#include <cstdio>
#include <cmath>

static int failures = 0;

static void check(bool condition, const char* what)
{
    printf("%s  %s\n", condition ? "[ ok ]" : "[FAIL]", what);
    if (!condition) failures++;
}

int main()
{
    Physics physics;
    b2WorldId world = physics.getWorld();

    // Ground: 4 wide, 0.4 tall, top surface at y = -0.4.
    Entity ground(world, b2Vec2{-2.f, -0.4f}, b2Vec2{2.f, -0.8f}, BodyType::Static);

    // A 0.4 x 0.4 dynamic box dropped from y = 1.0.
    Entity box(world, b2Vec2{-0.2f, 1.2f}, b2Vec2{0.2f, 0.8f}, BodyType::Dynamic);
    RigidBody rigidBody(box, 1.5f);

    check(b2Body_GetType(ground.body) == b2_staticBody, "ground is a static body");
    check(b2Body_GetType(box.body) == b2_dynamicBody, "box is a dynamic body");
    check(std::fabs(b2Body_GetMass(box.body) - 1.5f) < 1e-4f,
          "RigidBody mass reaches the solver (1.5 kg)");
    check(rigidBody.onFreefall(), "box starts in freefall");

    const float startY = b2Body_GetPosition(box.body).y;

    // One frame's worth of stepping must advance the world exactly once per
    // frame, not once per entity.
    physics.step(Physics::getStepSize());
    check(b2Body_GetLinearVelocity(box.body).y < 0.f, "gravity pulls the box down");

    // Two seconds of frames at 60 Hz.
    for (int frame = 0; frame < 120; ++frame)
    {
        rigidBody.update(nullptr, Physics::getStepSize());
        physics.step(Physics::getStepSize());
        box.syncTransform();
        ground.syncTransform();
    }

    const b2Vec2 restPosition = b2Body_GetPosition(box.body);
    const b2Vec2 restVelocity = b2Body_GetLinearVelocity(box.body);

    printf("       box came to rest at y = %.4f, vy = %.4f\n", restPosition.y, restVelocity.y);

    check(restPosition.y < startY, "box fell");
    // Ground top is -0.4, box half-height is 0.2, so it rests at -0.2.
    check(std::fabs(restPosition.y - (-0.2f)) < 0.02f, "box rests on the ground surface");
    check(std::fabs(restVelocity.y) < 0.05f, "box has come to rest, not tunnelled through");
    check(std::fabs(restPosition.x) < 0.05f, "box did not drift sideways");

    rigidBody.update(nullptr, Physics::getStepSize());
    check(rigidBody.isGrounded(), "RigidBody reports grounded from the contact normal");
    check(!rigidBody.onFreefall(), "onFreefall() is false while grounded");

    check(std::fabs(box.transform.y - restPosition.y) < 1e-5f,
          "syncTransform published the body position to the transform");
    check(std::fabs(box.transform.x - restPosition.x) < 1e-5f,
          "syncTransform published x too");

    // Restitution: dropped from higher, it should bounce at least once.
    Entity ball(world, b2Vec2{0.8f, 2.2f}, b2Vec2{1.2f, 1.8f}, BodyType::Dynamic);
    RigidBody ballBody(ball, 1.f);
    bool sawUpwardVelocity = false;
    for (int frame = 0; frame < 240; ++frame)
    {
        physics.step(Physics::getStepSize());
        if (b2Body_GetLinearVelocity(ball.body).y > 0.3f)
            sawUpwardVelocity = true;
    }
    check(sawUpwardVelocity, "restitution bounces the ball back up off the ground");

    // The accumulator must not run the world for a frame shorter than a step.
    Physics idle;
    Entity faller(idle.getWorld(), b2Vec2{-0.1f, 1.1f}, b2Vec2{0.1f, 0.9f}, BodyType::Dynamic);
    const float before = b2Body_GetPosition(faller.body).y;
    idle.step(Physics::getStepSize() / 4.f);
    check(std::fabs(b2Body_GetPosition(faller.body).y - before) < 1e-6f,
          "a sub-step frame accumulates instead of stepping");
    idle.step(Physics::getStepSize());
    check(b2Body_GetPosition(faller.body).y < before,
          "the accumulated remainder is spent on the next step");

    // A long stall must not queue up an unbounded number of steps.
    Physics stalled;
    Entity stallFaller(stalled.getWorld(), b2Vec2{-0.1f, 1.1f}, b2Vec2{0.1f, 0.9f}, BodyType::Dynamic);
    stalled.step(10.f);
    const float fallenAfterStall = 1.f - b2Body_GetPosition(stallFaller.body).y;
    printf("       a 10 s stall advanced the world by %.3f units of fall\n", fallenAfterStall);
    check(fallenAfterStall < 1.f, "a 10 s stall is clamped, not simulated in full");

    printf("\n%s (%d failure(s))\n", failures == 0 ? "ALL CHECKS PASSED" : "CHECKS FAILED", failures);
    return failures == 0 ? 0 : 1;
}
