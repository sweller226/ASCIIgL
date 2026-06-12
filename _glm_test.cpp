#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
int main() {
    glm::vec2 center(960.f, 540.f);
    glm::vec2 half(88.f, 83.f);
    glm::mat4 model(1.f);
    model = glm::translate(model, glm::vec3(center.x, center.y, 0.f));
    model = glm::scale(model, glm::vec3(half.x, half.y, 1.f));
    glm::vec4 v(-1.f, -1.f, 0.f, 1.f);
    glm::vec4 r = model * v;
    std::cout << "ST: " << r.x << ", " << r.y << "\n";
    model = glm::mat4(1.f);
    model = glm::scale(model, glm::vec3(half.x, half.y, 1.f));
    model = glm::translate(model, glm::vec3(center.x, center.y, 0.f));
    r = model * v;
    std::cout << "TS: " << r.x << ", " << r.y << "\n";
    return 0;
}
