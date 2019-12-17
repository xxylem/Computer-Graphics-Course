#include "Scene.h"
#include "Sphere.h" // TODO remove after implementing intersect
#include "Triangle.h"
#include <memory>
#include "ProgressBar.hpp"  // https://github.com/prakhar1989/progress-cpp

const Color AMBIENT_GLOBAL_DEFAULT = Color(0.0f, 0.0f, 0.0f);

Scene::Scene(int width, int height, Camera camera, std::vector<Triangle> triangles, std::vector<Sphere> spheres, 
	std::vector<DirectionalLight> directional_lights, std::vector<PointLight> point_lights) :
	_width(width), _height(height), _camera(camera), _ambient_global(AMBIENT_GLOBAL_DEFAULT),
	_triangles(triangles), _spheres(spheres), _directional_lights(directional_lights), _point_lights(point_lights)
{
	// Precompute these values now since they will be needed repeatedly and won't change.
	_aspect_ratio = (float)_width / (float)_height;
	_width_over_two = _width / 2.0f;
	_height_over_two = _height / 2.0f;
	float tan_fovy_over_two = tan(_camera.fovy_radians() / 2);
	float tan_fovx_over_two = tan_fovy_over_two * _aspect_ratio;
	_alpha_multiplicand = tan_fovx_over_two / _width_over_two;
	_beta_multiplicand = tan_fovy_over_two / _height_over_two;

	// Set up basis vectors for camera space.
	glm::vec3 center = _camera.lookat().toGlmVec3();	// center is what we are looking at
	glm::vec3 eye = _camera.lookfrom().toGlmVec3();		// eye is the camera location
	glm::vec3 a = eye - center;
	glm::vec3 b = _camera.up().toGlmVec3();
	_w = glm::normalize(a);
	_u = glm::cross(b, _w);
	_u = glm::normalize(_u);
	_v = glm::cross(_w, _u);
}

Scene::~Scene()
{
}


int Scene::width() const { return _width; }

int Scene::height() const {	return _height; }

Ray Scene::rayThroughPixel(int i, int j) const
{
	// Adjustment to hit center of pixel, rather than corner.
	float i_float = i + 0.5f;
	float j_float = j + 0.5f;

	float alpha = (j_float - _width_over_two) * _alpha_multiplicand;
	float beta = (_height_over_two - i_float) * _beta_multiplicand;

	glm::vec3 direction = alpha * _u + beta * _v - _w;
	direction = glm::normalize(direction);

	// The ray has origin at the camera location and looks in the given direction
	return Ray(_camera.lookfrom(), Direction(direction)); 
}



std::optional<Scene::Intersection> Scene::intersect(const Ray& ray) const
{
	float mindist = INFINITY;
	std::optional<Shape*> hit_object;
	glm::vec3 normal;

	// Ideally these two loops would be merged but I found it problematic storing shapes:
		// Shape class is abstract and C++ requires pointers to store derived objects.
	for (Triangle triangle : _triangles) {
		std::optional<DistanceAndNormal> result = triangle.intersect(ray);
		if (result.has_value() && result.value().distance < mindist) {
			mindist = result.value().distance;
			// TODO: a bad hack. This works but now we're creating new objects in each loop.
				// Is there a way to get a meaningful pointer to the object in the Scene and return that pointer
				// such that the pointer does not end up pointing to garbage?
			hit_object.reset(); 
			hit_object = new Triangle(triangle);
			normal = result.value().normal;
		}
	}

	for (Sphere sphere : _spheres) {
		std::optional<DistanceAndNormal> result = sphere.intersect(ray);
		if (result.has_value() && result.value().distance < mindist) {
			mindist = result.value().distance;
			hit_object.reset();
			hit_object = new Sphere(sphere);
			normal = result.value().normal;
		}
	}

	if (!hit_object.has_value()) {
		return {};
	}

	glm::vec3 intersection_location = ray.origin().toGlmVec3() + mindist * ray.direction().toGlmVec3();
	Point p = Point(intersection_location);

	Intersection i = { hit_object.value(), p, normal, ray, mindist };
	return { i };
}

Color Scene::findColor(Intersection intersection, int recursive_depth_permitted) const
{
	// Base case: no more reflections permitted
	if (recursive_depth_permitted < 1) {
		return Color(0.0f, 0.0f, 0.0f); // Colors are additive so we can just return black.
	}

	// TODO: for now, using glm::vec3 to handle the vector operations. Later on, could add operations to the custom Vector class.

	Shape* intersected_shape = intersection.intersected_shape;

	// Ambient term, per object
	glm::vec3 final_color = intersected_shape->ambient().toGlmVec3();

	// Emission term, per object
	final_color += intersected_shape->emission().toGlmVec3();

	for (PointLight point_light : _point_lights) {

		bool isVisible = point_light.isVisibleFrom(intersection.intersection_location, this);
		if (isVisible) { 
			// TODO this is duplicated code form isVisibleFrom
			glm::vec3 light_direction = point_light.point().toGlmVec3() - intersection.intersection_location.toGlmVec3();
			light_direction = glm::normalize(light_direction);
			float l_dot_n = glm::dot(light_direction, intersection.normal);

			// diffuse
			final_color += point_light.color().toGlmVec3() * intersected_shape->diffuse().toGlmVec3() * glm::max(l_dot_n, 0.0f);

			// specular
			glm::vec3 half_angle = (-intersection.ray.direction().toGlmVec3()) + light_direction;
			half_angle = glm::normalize(half_angle);
			float h_dot_n = glm::dot(half_angle, intersection.normal);

			final_color += point_light.color().toGlmVec3() * intersected_shape->specular().toGlmVec3() * (pow(glm::max(h_dot_n, 0.0f), intersected_shape->shininess()));
		}


		// TODO: Attenuation:
			// Light contribution / (const + lin * dist + quad * dist^2)
	}

	for (DirectionalLight directional_light : _directional_lights) {
		bool isVisible = directional_light.isVisibleFrom(intersection.intersection_location, this);
		if (isVisible) { 
			// No need to compute the direction
			glm::vec3 light_direction = directional_light.direction().toGlmVec3();
			float l_dot_n = glm::dot(light_direction, intersection.normal);

			// diffuse
			final_color += directional_light.color().toGlmVec3() * intersected_shape->diffuse().toGlmVec3() * glm::max(l_dot_n, 0.0f);

			// specular
			glm::vec3 half_angle = (-intersection.ray.direction().toGlmVec3()) + light_direction;
			half_angle = glm::normalize(half_angle);
			float h_dot_n = glm::dot(half_angle, intersection.normal);

			final_color += directional_light.color().toGlmVec3() * intersected_shape->specular().toGlmVec3() * (pow(glm::max(h_dot_n, 0.0f), intersected_shape->shininess()));
		}
		// No attenuation?
		// TODO: implement directional_light contribution to color
	}

	// It's expensive to compute the reflected ray and intersection, so let's not do it unless we need to.
	// TODO Check that there is a significant specular component to the intersected object.
	if (recursive_depth_permitted > 1) {
		glm::vec3 original_ray_direction = intersection.ray.direction().toGlmVec3();
		glm::vec3 reflected_ray_origin = intersection.intersection_location.toGlmVec3();
		glm::vec3 reflected_ray_direction = original_ray_direction + 2.0f * (glm::dot(-original_ray_direction, intersection.normal)) * intersection.normal;
		reflected_ray_direction = glm::normalize(reflected_ray_direction);

		// Push reflected ray slightly out to avoid self-intersection due to floating point inaccuracy.
		reflected_ray_origin += 0.0001f * reflected_ray_direction;


		Ray reflected_ray = Ray(Point(reflected_ray_origin), Direction(reflected_ray_direction));
		std::optional<Intersection> reflected_intersection = intersect(reflected_ray);

		if (reflected_intersection.has_value()) {

			glm::vec3 recursive_specular_component = intersected_shape->specular().toGlmVec3() * findColor(reflected_intersection.value(), recursive_depth_permitted - 1).toGlmVec3();
			final_color += recursive_specular_component;
		}
	}

	return Color(final_color.x, final_color.y, final_color.z);
}

// TODO return smart pointer
BYTE* Scene::raytrace(int max_recursion_depth) const
{
	int pix = _width * _height;
	BYTE* pixels = new BYTE[3 * pix];
	ProgressBar progressBar(_height, 70);

	for (int i = 0; i < _height; i++) {
		for (int j = 0; j < _width; j++) {
			Ray ray = rayThroughPixel(i, j);
			std::optional<Intersection> intersection = intersect(ray);
			if (intersection.has_value()) {
				Color pixel_color = findColor(intersection.value(), max_recursion_depth);
				//Color pixel_color = Color(0.0f, 0.0f, 1.0f); // TODO: debug value: objects in view are coloured pure blue
				RGBTRIPLE pixel_color_triple = pixel_color.to_freeimage_rgbtriple();
				int pixel_start_index = (i * _width * 3) + (j * 3);

				// BlueGreenRed. TODO: consider copying memory as 3 bytes together
				pixels[pixel_start_index] = pixel_color_triple.rgbtBlue;   // TODO check indexes
				pixels[pixel_start_index + 1] = pixel_color_triple.rgbtGreen;
				pixels[pixel_start_index + 2] = pixel_color_triple.rgbtRed;
			}
			else { // No intersection so use global ambient value.
				Color pixel_color = _ambient_global;
				//Color pixel_color = Color(0.5f, 0.0f, 0.0f); // TODO: debug value: background is pure dark red.
				RGBTRIPLE pixel_color_triple = pixel_color.to_freeimage_rgbtriple();
				int pixel_start_index = (i * _width * 3) + (j * 3);

				// BlueGreenRed. TODO: consider copying memory as 3 bytes together
				pixels[pixel_start_index] = pixel_color_triple.rgbtBlue;   // TODO check indexes
				pixels[pixel_start_index + 1] = pixel_color_triple.rgbtGreen;
				pixels[pixel_start_index + 2] = pixel_color_triple.rgbtRed;

				// TODO reduce duplication : smart pointer to pixel_color
			}
		}
		++progressBar;
		progressBar.display();
	}
	  
	progressBar.done();
	return pixels;
}
