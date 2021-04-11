#include "Sphere.h"

namespace rendering {
	namespace {
		std::vector<SimpleVertex> calculateVertices(float radius, size_t n_theta, size_t n_phi) {
			const float d_theta = DirectX::XM_PI / (n_theta - 1);
			const float d_phi = DirectX::XM_2PI / n_phi;
			float theta = d_theta;

			std::vector<SimpleVertex> vertices;
			vertices.push_back({ DirectX::XMFLOAT3(0.0f, radius, 0.0f), DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f) });
			for (size_t i = 0; i + 2 < n_theta; i++) {
				float sin_theta = sinf(theta);
				float cos_theta = cosf(theta);
				float phi = 0.0f;
				for (int j = 0; j < n_phi; j++) {
					float sin_phi = sinf(phi);
					float cos_phi = cosf(phi);
					float x = sin_theta * sin_phi;
					float z = -sin_theta * cos_phi;
					float y = cos_theta;
					vertices.push_back({ DirectX::XMFLOAT3(x * radius, y * radius, z * radius), DirectX::XMFLOAT3(x, y, z) });
					phi += d_phi;
				}
				theta += d_theta;
			}
			vertices.push_back({ DirectX::XMFLOAT3(0.0f, -radius, 0.0f), DirectX::XMFLOAT3(0.0f, -1.0f, 0.0f) });
			return vertices;
		}

		std::vector<unsigned> calculateIndices(unsigned n_vertices, unsigned n_theta, unsigned n_phi, bool outer_normals, bool correct_orientation) {
			const unsigned top_pole_index = 0;
			const unsigned bottom_pole_index = n_vertices - 1;
			const unsigned top_ring_offset = 1;
			const unsigned bottom_ring_offset = n_vertices - n_phi - 1;

			std::vector<unsigned> indices;
			for (unsigned i = 0; i < n_phi; i++) {
				indices.push_back(top_pole_index);
				indices.push_back(top_ring_offset + (i + 1) % n_phi);
				indices.push_back(top_ring_offset + i);
			}
			for (unsigned i = 0; i + 3 < n_theta; i++) {
				for (unsigned j = 0; j < n_phi; j++) {
					indices.push_back(top_ring_offset + i * n_phi + j);
					if (correct_orientation) {
						indices.push_back(top_ring_offset + i * n_phi + (j + 1) % n_phi);
						indices.push_back(top_ring_offset + (i + 1) * n_phi + j);
					}
					else {
						indices.push_back(top_ring_offset + (i + 1) * n_phi + j);
						indices.push_back(top_ring_offset + i * n_phi + (j + 1) % n_phi);
					}

					indices.push_back(top_ring_offset + (i + 1) * n_phi + j);
					indices.push_back(top_ring_offset + i * n_phi + (j + 1) % n_phi);
					indices.push_back(top_ring_offset + (i + 1) * n_phi + (j + 1) % n_phi);
				}
			}
			for (unsigned i = 0; i < n_phi; i++) {
				indices.push_back(bottom_pole_index);
				if (correct_orientation) {
					indices.push_back(bottom_ring_offset + i);
					indices.push_back(bottom_ring_offset + (i + 1) % n_phi);
				}
				else {
					indices.push_back(bottom_ring_offset + (i + 1) % n_phi);
					indices.push_back(bottom_ring_offset + i);
				}

			}
			if (!outer_normals) {
				std::reverse(indices.begin(), indices.end());
			}
			return indices;
		}
	}

	Sphere::Sphere(float radius, size_t n_theta, size_t n_phi, bool outer_normals, bool correct_orientation) 
		: _vertices(calculateVertices(radius, n_theta, n_phi)),
		  _indices(calculateIndices((unsigned)_vertices.size(), (unsigned)n_theta, (unsigned)n_phi, outer_normals, correct_orientation)) {}

	const std::vector<SimpleVertex>& Sphere::getVertices() const {
		return _vertices;
	}

	const std::vector<unsigned>& Sphere::getIndices() const {
		return _indices;
	}
}
