#ifndef __GEOMETRY__
#define __GEOMETRY__
#pragma once

#include <memory>
#include <vector>

#include <DirectXMath.h>
#include <windows.h>

#include "SimpleVertex.h"

namespace rendering {
	class Sphere
	{
	public:
		Sphere(float radius = 1.0f, size_t n_theta = 30, size_t n_phi = 30, bool correct_orientation = true);
		const rendering::SimpleVertex* getVertices() const;
		const WORD* getIndices() const;

		const size_t _n_vertices, _n_indices;

	private:
		int calculateVertices(float radius, size_t n_theta, size_t n_phi);
		int calculateIndices(size_t n_theta, size_t n_phi, bool correct_orientation);

		std::vector<rendering::SimpleVertex> _vertices;
		std::vector<WORD> _indices;
	};
}



#endif // !__GEOMETRY__


