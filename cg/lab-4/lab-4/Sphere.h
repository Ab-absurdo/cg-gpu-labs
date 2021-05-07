#pragma once

#include <vector>

#include "SimpleVertex.h"

namespace rendering {
	class Sphere {
	public:
		Sphere(float radius, size_t n_theta, size_t n_phi, bool outer_normals, bool correct_orientation);

		const std::vector<SimpleVertex>& getVertices() const;
		const std::vector<unsigned>& getIndices() const;

	private:
		std::vector<SimpleVertex> _vertices;
		std::vector<unsigned> _indices;
	};
}
