#include "Math.h"

#include "Geometry.h"

using namespace DirectX;

namespace rendering {
	Sphere::Sphere(float radius, size_t n_theta, size_t n_phi, bool correct_orientation) :
		_n_vertices((n_theta - 2)* n_phi + 2), _n_indices(6 * (n_theta - 2) * n_phi)
	{
		assert(n_theta >= 3 && n_phi >= 2);

		_vertices.reserve(_n_vertices);
		int vert_count = calculateVertices(radius, n_theta, n_phi);
		assert(vert_count == _n_vertices);

		_indices.reserve(_n_indices);
		int ind_count = calculateIndices(n_theta, n_phi, correct_orientation);
		assert(ind_count == _n_indices);
	}


	const rendering::SimpleVertex* Sphere::getVertices() const
	{
		return _vertices.data();
	}

	const WORD* Sphere::getIndices() const
	{
		return _indices.data();
	}

	int Sphere::calculateVertices(float radius, size_t n_theta, size_t n_phi)
	{
		const float d_theta = XM_PI / (n_theta - 1), d_phi = XM_2PI / n_phi;
		float theta = d_theta, phi, sin_theta, cos_theta, sin_phi, cos_phi;
		float x, y, z;

		_vertices.push_back({ XMFLOAT3(0.0f, radius, 0.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) });
		for (int i = 0; i < n_theta - 2; i++)
		{
			sin_theta = sinf(theta);
			cos_theta = cosf(theta);
			phi = 0.0f;
			for (int j = 0; j < n_phi; j++)
			{
				sin_phi = sinf(phi);
				cos_phi = cosf(phi);
				x = sin_theta * sin_phi;
				z = -sin_theta * cos_phi;
				y = cos_theta;
				_vertices.push_back({ XMFLOAT3(x * radius, y * radius, z * radius), XMFLOAT3(x, y, z) });
				phi += d_phi;
			}
			theta += d_theta;
		}
		_vertices.push_back({ XMFLOAT3(0.0f, -radius, 0.0f), XMFLOAT3(0.0f, -1.0f, 0.0f) });
		return (int)_vertices.size();
	}

	int Sphere::calculateIndices(size_t n_theta, size_t n_phi, bool correct_orientation)
	{
		const size_t top_pole_index = 0, bottom_pole_index = _n_vertices - 1;
		const size_t top_ring_offset = 1, bottom_ring_offset = _n_vertices - n_phi - 1;

		for (size_t i = 0; i < n_phi; i++)
		{
			_indices.push_back(top_pole_index);
			_indices.push_back(top_ring_offset + (i + 1) % n_phi);
			_indices.push_back(top_ring_offset + i);
		}
		for (size_t i = 0; i < n_theta - 3; i++)
		{
			for (size_t j = 0; j < n_phi; j++)
			{
				_indices.push_back(top_ring_offset + i * n_phi + j);
				if (correct_orientation)
				{
					_indices.push_back(top_ring_offset + i * n_phi + (j + 1) % n_phi);
					_indices.push_back(top_ring_offset + (i + 1) * n_phi + j);
				}
				else
				{
					_indices.push_back(top_ring_offset + (i + 1) * n_phi + j);
					_indices.push_back(top_ring_offset + i * n_phi + (j + 1) % n_phi);
				}

				_indices.push_back(top_ring_offset + (i + 1) * n_phi + j);
				_indices.push_back(top_ring_offset + i * n_phi + (j + 1) % n_phi);
				_indices.push_back(top_ring_offset + (i + 1) * n_phi + (j + 1) % n_phi);
			}
		}
		for (size_t i = 0; i < n_phi; i++)
		{
			_indices.push_back(bottom_pole_index);
			if (correct_orientation)
			{
				_indices.push_back(bottom_ring_offset + i);
				_indices.push_back(bottom_ring_offset + (i + 1) % n_phi);
			}
			else
			{
				_indices.push_back(bottom_ring_offset + (i + 1) % n_phi);
				_indices.push_back(bottom_ring_offset + i);
			}

		}
		return (int)_indices.size();
	}
}
