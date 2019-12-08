#pragma once

#include <FreeImage.h>

// Struct for an RGB color
// We can bring all these small files together into a header file such as utilities but for now I'll keep them separate


/* A color has:
r
g
b
*/

class Color
{
	public:
		Color(); // Initialise Color with (0, 0, 0)
		Color(float red, float green, float blue);
		~Color();

		float red() const;
		float green() const;
		float blue() const;

		BYTE color_as_byte() const;

	private:
		const float _red;
		const float _green;
		const float _blue;
};

