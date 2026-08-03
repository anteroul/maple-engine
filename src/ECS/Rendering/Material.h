#ifndef MAPLEENGINE_MATERIAL_H
#define MAPLEENGINE_MATERIAL_H

#include <string>
#include <glm/glm.hpp>
#include "../Components/Shader.h"

struct Material {
    std::string name;
    glm::vec3 colour;
    Shader* shader = nullptr;
    float shininess;
};

Material createMaterial(const std::string& name, const glm::vec3& colour, Shader* shader, float shininess);
Material colourMaterial(const std::string& name, const glm::vec3& colour);
void applyMaterial(const Material& mat);
void deleteMaterial(Material& mat);
void setMaterialShader(Material& mat, Shader* shader);
void changeMaterialColour(Material& mat, const glm::vec3& colour);
void changeMaterialShininess(Material& mat, float shininess);
Material* getMaterialByName(const std::string& name);
Material genUnnamedMaterial(const glm::vec3& colour, Shader* shader, float shininess);
std::vector<const std::string&> getAllMaterialNames();
void listAllMaterials();
void clearAllMaterials();
void deleteMaterialByName(const std::string& name);
void getMaterialsFromEntity(Entity& entity, Material& mat);
void setMaterialsForEntity(Entity& entity, const Material& mat);
Material* getMaterialFromEntity(const std::string& entityName, const std::string& entityTag);

#endif