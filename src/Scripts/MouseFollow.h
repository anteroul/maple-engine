#ifndef MAPLEENGINE_MOUSEFOLLOW_H
#define MAPLEENGINE_MOUSEFOLLOW_H

#define SPEED (0.2)

static void mouseFollow(Component* owner, GLFWwindow *window, float deltaTime)
{
    double x, y;
    int width, height;
    float speed = SPEED * deltaTime;

    glfwGetWindowSize(window, &width, &height);
    glfwGetCursorPos(window, &x, &y);

    x = x - (width / 2);
    x /= (width / 2);

    y = y - (height / 2);
    y *= -1;
    y /= (height / 2);

    b2Body* body = owner->getBody();

    // Is gravity applied?
    if (!owner->getEntity().getComponent<RigidBody>()) {
        body->SetTransform(b2Vec2((float)x, (float)y), body->GetAngle());
    } else {
        body->SetTransform(b2Vec2(body->GetPosition().x + speed * x, body->GetPosition().y), body->GetAngle());
    }
}

#endif //MAPLEENGINE_MOUSEFOLLOW_H
