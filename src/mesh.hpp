#pragma once

#include <cstdint>
#include <string>
#include <memory>
#include <vector>
#include <glm/glm.hpp>

class Mesh
{
public:
    // Vertex definition
    struct Vertex
    {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec3 tangent;
        glm::vec3 bitangent;
        glm::vec2 texcoord;
    };
    static_assert(sizeof(Vertex) == 14 * sizeof(float), "Vertex size is not as expected");
    static const int NumAttributes = 5;

    // Face definition
    struct Face
    {
        uint32_t v1, v2, v3;
    };
    static_assert(sizeof(Face) == 3 * sizeof(uint32_t), "Face size is not as expected");

    // Static factory methods
    static std::shared_ptr<Mesh> fromFile(const std::string& filename);
    static std::shared_ptr<Mesh> fromString(const std::string& data);

    // Getter methods
    const std::vector<Vertex>& vertices() const { return m_vertices; }
    const std::vector<Face>& faces() const { return m_faces; }

	// Constructor
    explicit Mesh(const struct aiMesh* mesh);
private:

    // Member variables
    std::vector<Vertex> m_vertices;
    std::vector<Face> m_faces;
};
