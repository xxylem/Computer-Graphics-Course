#pragma once

#ifndef GLM_FORCE_RADIANS
#define GLM_FORCE_RADIANS
#endif
#include <glm/glm.hpp>

//Point struct

// We can bring all these small files together into a header file such as utilities but for now I'll keep them separate

/*
A Point has

x
y
z
*/

class Point
{
	public:
		Point(float x, float y, float z);
		Point(glm::vec3 p);
		Point(glm::vec4 p);
		~Point();

		float x() const;
		float y() const;
		float z() const;

		glm::vec3 toGlmVec3() const;
		glm::vec4 toGlmVec4() const; // homogeneous coordinates

	private:
		const float _x;
		const float _y;
		const float _z;

};

