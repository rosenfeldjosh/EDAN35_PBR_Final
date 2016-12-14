#pragma once
#include <vector>
#include "glm\glm.hpp"
#include "node.hpp"

class InputHandler;
class Window;

namespace eda221
{
	class Assignment5
	{
	public:
		Assignment5();
		~Assignment5();
		void run();
		float get_y(float x, float z, int width, int height, float radius);
		glm::vec3 move(float delta, Node* ball);

	private:
		InputHandler *inputHandler;
		Window       *window;
		glm::vec2 mMousePosition;
		float mMouseSensitivity;
		glm::vec2 mRotation;
		glm::vec3 front;
		glm::vec3 right;
		int width;
		int height;
		float radius;
		glm::mat4 rotation_forward;
		bool check_collision(glm::vec3 p1, float r1, glm::vec3 p2, float r2);
		float angle;
		float distance;
		int sign;
		float gravity;
		float y_vel;
		float y_pos;
		bool jumping;
		bool space_released;
	};
	std::vector<glm::vec3> vertices;
}