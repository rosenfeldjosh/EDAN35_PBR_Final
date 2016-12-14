#include "assignment5.hpp"
#include "node.hpp"
#include "parametric_shapes.hpp"

#include "config.hpp"
#include "external/glad/glad.h"
#include "core/Bonobo.h"
#include "core/FPSCamera.h"
#include "core/InputHandler.h"
#include "core/Log.h"
#include "core/LogView.h"
#include "core/Misc.h"
#include "core/utils.h"
#include "core/Window.h"

#include <imgui.h>
#include "external/imgui_impl_glfw_gl3.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "external/glad/glad.h"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cstdlib>
#include <stdexcept>

enum class polygon_mode_t : unsigned int {
	fill = 0u,
	line,
	point
};

static polygon_mode_t get_next_mode(polygon_mode_t mode)
{
	return static_cast<polygon_mode_t>((static_cast<unsigned int>(mode) + 1u) % 3u);
}

eda221::Assignment5::Assignment5()
{
	Log::View::Init();
	window = Window::Create("EDA221: Assignment 5", config::resolution_x,
		config::resolution_y, config::msaa_rate, false);
	//glfwSetInputMode(window->GetGLFW_Window(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	inputHandler = new InputHandler();
	window->SetInputHandler(inputHandler);
}

eda221::Assignment5::~Assignment5()
{
	delete inputHandler;
	inputHandler = nullptr;

	//Window::Destroy(window);
	window = nullptr;
	Log::View::Destroy();
}

void eda221::Assignment5::run()
{
	int sphere_radius = 2;
	auto sphere_shape = parametric_shapes::createSphere(98, 98, sphere_radius);
	int width = 30;
	int height = 30;
	auto plane_shape = parametric_shapes::createQuad(width, height);

	auto brdf_shader = eda221::createProgram("brdf.vert", "brdf.frag");

	auto fallback_shader = eda221::createProgram("fallback.vert", "fallback.frag");
	auto glass_shader = eda221::createProgram("glass.vert", "glass.frag");
	auto cubemap_shader = eda221::createProgram("cube_map.vert", "cube_map.frag");

	FPSCameraf mCamera(bonobo::pi / 4.0f,
		static_cast<float>(config::resolution_x) / static_cast<float>(config::resolution_y),
		0.01f, 1000.0f);
	mCamera.mWorld.SetTranslate(glm::vec3(15.0f, 10.0f, 50.0f));
	mCamera.mMouseSensitivity = 0.003f;
	mCamera.mMovementSpeed = 0.01;

	auto const window_size = window->GetDimensions();
	window->SetCamera(&mCamera);

	glm::vec3 light_position = glm::vec3(15, 1000, 15);
	auto camera_position = mCamera.mWorld.GetTranslation();
	auto const set_uniforms = [&light_position, &camera_position](GLuint program) {
		glUniform3fv(glGetUniformLocation(program, "light_position"), 1, glm::value_ptr(light_position));
		glUniform3fv(glGetUniformLocation(program, "camera_position"), 1, glm::value_ptr(camera_position));
	};

	auto const reload_shader = [fallback_shader](std::string const& vertex_path, std::string const& fragment_path, GLuint& program) {
		if (program != 0u && program != fallback_shader)
			glDeleteProgram(program);
		program = eda221::createProgram("../EDA221/" + vertex_path, "../EDA221/" + fragment_path);
		if (program == 0u) {
			LogError("Failed to load \"%s\" and \"%s\"", vertex_path.c_str(), fragment_path.c_str());
			program = fallback_shader;
		}
	};

	auto const reflection_texture = eda221::createTexture(window_size.x, window_size.y);
	auto const depth_texture_1 = eda221::createTexture(window_size.x, window_size.y, GL_TEXTURE_2D, GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT);
	auto const reflection_fbo = eda221::createFBO({ reflection_texture }, depth_texture_1);
	
	auto const refraction_texture = eda221::createTexture(window_size.x, window_size.y);
	auto const depth_texture_2 = eda221::createTexture(window_size.x, window_size.y, GL_TEXTURE_2D, GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT);
	auto const refraction_fbo = eda221::createFBO({ refraction_texture }, depth_texture_2);

	auto const default_sampler = eda221::createSampler([](GLuint sampler) {
		glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	});
	auto const bind_texture_with_sampler = [](GLenum target, unsigned int slot, GLuint program, std::string const& name, GLuint texture, GLuint sampler) {
		glActiveTexture(GL_TEXTURE0 + slot);
		glBindTexture(target, texture);
		glUniform1i(glGetUniformLocation(program, name.c_str()), static_cast<GLint>(slot));
		glBindSampler(slot, sampler);
	};

	//load brdf textures
	auto brdf_rust_albedo = loadTexture2D("brdf_rust_albedo.png");
	auto brdf_rust_roughness = loadTexture2D("brdf_rust_roughness.png");
	auto brdf_rust_metallic = loadTexture2D("brdf_rust_metallic.png");

	auto brdf_granite_albedo = loadTexture2D("brdf_plastic_albedo.png");
	auto brdf_granite_normal = loadTexture2D("brdf_plastic_normal.png");
	auto brdf_granite_roughness = loadTexture2D("brdf_plastic_roughness.png");
	auto brdf_granite_metallic = loadTexture2D("brdf_plastic_metallic.png");

	auto brdf_gold_albedo = loadTexture2D("brdf_gold_albedo.png");
	auto brdf_gold_roughness = loadTexture2D("brdf_gold_roughness.png");
	auto brdf_gold_metallic = loadTexture2D("brdf_gold_metallic.png");

	auto brdf_forest_albedo = loadTexture2D("brdf_worn_albedo.png");
	auto brdf_forest_roughness = loadTexture2D("brdf_worn_roughness.png");
	auto brdf_forest_metallic = loadTexture2D("brdf_worn_metallic.png");



	auto big_sphere = Node();
	big_sphere.set_geometry(sphere_shape);
	auto big_sphere_tex = loadTextureCubeMap("nvlobby_new/posx.png", "nvlobby_new/negx.png", "nvlobby_new/posy.png", "nvlobby_new/negy.png", "nvlobby_new/negz.png", "nvlobby_new/posz.png", true);
	big_sphere.add_texture("cube_tex", big_sphere_tex, GL_TEXTURE_CUBE_MAP);
	big_sphere.scale(glm::vec3(1, 1, 1));

	auto big_sphere_tex2 = loadTextureCubeMap("arch/posx.png", "arch/negx.png", "arch/posy.png", "arch/negy.png", "arch/negz.png", "arch/posz.png", true);;
	auto diffuse_cube = loadTextureCubeMap("nvlobby_new_diff/posx.png", "nvlobby_new_diff/negx.png", "nvlobby_new_diff/posy.png", "nvlobby_new_diff/negy.png", "nvlobby_new_diff/negz.png", "nvlobby_new_diff/posz.png", true);
	auto diffuse_cube2 = loadTextureCubeMap("arch_diff/posx.png", "arch_diff/negx.png", "arch_diff/posy.png", "arch_diff/negy.png", "arch_diff/negz.png", "arch_diff/posz.png", true);
	big_sphere.add_texture("cube_tex2", big_sphere_tex2, GL_TEXTURE_CUBE_MAP);

	auto sphere1 = Node();
	sphere1.set_geometry(sphere_shape);
	sphere1.add_texture("albedo", brdf_rust_albedo);
	sphere1.add_texture("roughness", brdf_rust_roughness);
	sphere1.add_texture("metallic", brdf_rust_metallic);
	sphere1.add_texture("cube_tex", big_sphere_tex, GL_TEXTURE_CUBE_MAP);
	sphere1.add_texture("diffuse_cubemap", diffuse_cube, GL_TEXTURE_CUBE_MAP);
	sphere1.add_texture("cube_tex2", big_sphere_tex2, GL_TEXTURE_CUBE_MAP);
	sphere1.add_texture("diffuse_cubemap2", diffuse_cube2, GL_TEXTURE_CUBE_MAP);

	auto sphere2 = Node();
	sphere2.set_geometry(sphere_shape);
	sphere2.add_texture("albedo", brdf_granite_albedo);
	sphere2.add_texture("normal", brdf_granite_normal);
	sphere2.add_texture("roughness", brdf_granite_roughness);
	sphere2.add_texture("metallic", brdf_granite_metallic);
	sphere2.add_texture("cube_tex", big_sphere_tex, GL_TEXTURE_CUBE_MAP);
	sphere2.add_texture("diffuse_cubemap", diffuse_cube, GL_TEXTURE_CUBE_MAP);
	sphere2.add_texture("cube_tex2", big_sphere_tex2, GL_TEXTURE_CUBE_MAP);
	sphere2.add_texture("diffuse_cubemap2", diffuse_cube2, GL_TEXTURE_CUBE_MAP);

	auto sphere3 = Node();
	sphere3.set_geometry(sphere_shape);
	sphere3.add_texture("albedo", brdf_gold_albedo);
	sphere3.add_texture("roughness", brdf_gold_roughness);
	sphere3.add_texture("metallic", brdf_gold_metallic);
	sphere3.add_texture("cube_tex", big_sphere_tex, GL_TEXTURE_CUBE_MAP);
	sphere3.add_texture("diffuse_cubemap", diffuse_cube, GL_TEXTURE_CUBE_MAP);
	sphere3.add_texture("cube_tex2", big_sphere_tex2, GL_TEXTURE_CUBE_MAP);
	sphere3.add_texture("diffuse_cubemap2", diffuse_cube2, GL_TEXTURE_CUBE_MAP);

	auto sphere4 = Node();
	sphere4.set_geometry(sphere_shape);
	sphere4.add_texture("albedo", brdf_forest_albedo);
	sphere4.add_texture("roughness", brdf_forest_roughness);
	sphere4.add_texture("metallic", brdf_forest_metallic);
	sphere4.add_texture("cube_tex", big_sphere_tex, GL_TEXTURE_CUBE_MAP);
	sphere4.add_texture("diffuse_cubemap", diffuse_cube, GL_TEXTURE_CUBE_MAP);
	sphere4.add_texture("cube_tex2", big_sphere_tex2, GL_TEXTURE_CUBE_MAP);
	sphere4.add_texture("diffuse_cubemap2", diffuse_cube2, GL_TEXTURE_CUBE_MAP);


	sphere1.translate(glm::vec3(.25*width, sphere_radius,.25*height));
	sphere2.translate(glm::vec3(.25*width, sphere_radius, .75*height));
	sphere3.translate(glm::vec3(.75*width, sphere_radius, .75*height));
	sphere4.translate(glm::vec3(.75*width, sphere_radius, .25*height));

	auto plane = Node();
	plane.set_geometry(plane_shape);
	auto glass_normal_tex = loadTexture2D("dudv_map.png");
	plane.add_texture("dudv_map", glass_normal_tex);
	plane.scale(glm::vec3(100));

	auto polygon_mode = polygon_mode_t::fill; //what does this one do?

	glEnable(GL_DEPTH_TEST);
	//glEnable(GL_CULL_FACE);
	//glCullFace(GL_FRONT);
	//glCullFace(GL_BACK);

	f64 ddeltatime;
	size_t fpsSamples = 0;
	double nowTime, lastTime = GetTimeMilliseconds();
	double fpsNextTick = lastTime + 1000.0;

	while (!glfwWindowShouldClose(window->GetGLFW_Window()))
	{
		nowTime = GetTimeMilliseconds();
		ddeltatime = nowTime - lastTime;
		if (nowTime > fpsNextTick)
		{
			fpsNextTick += 1000.0;
			fpsSamples = 0;
		}
		fpsSamples++;

		glfwPollEvents();
		inputHandler->Advance();

		mCamera.Update(ddeltatime, *inputHandler);

		ImGui_ImplGlfwGL3_NewFrame();

		if (inputHandler->GetKeycodeState(GLFW_KEY_Z) & JUST_PRESSED) {
			polygon_mode = get_next_mode(polygon_mode);
		}
		switch (polygon_mode) {
		case polygon_mode_t::fill:
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
			break;
		case polygon_mode_t::line:
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
			break;
		case polygon_mode_t::point:
			glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
			break;
		}

		if (inputHandler->GetKeycodeState(GLFW_KEY_R) & JUST_PRESSED)
		{
			reload_shader("cube_map.vert", "cube_map.frag", cubemap_shader);
			reload_shader("brdf.vert", "brdf.frag", brdf_shader);
			reload_shader("glass.vert", "glass.frag", glass_shader);
		}
		if (inputHandler->GetKeycodeState(GLFW_KEY_T) & JUST_PRESSED)
		{
			reload_shader("cube_map.vert", "cube_map2.frag", cubemap_shader);
			reload_shader("brdf.vert", "brdf2.frag", brdf_shader);
		}

		camera_position = mCamera.mWorld.GetTranslation();

		//Fix order of matrix mult in node.cpp when necessary!
		sphere1.rotate_y(0.01);
		sphere2.rotate_y(-0.01);
		sphere3.rotate_y(0.01);
		sphere4.rotate_y(-0.01);

		glClearDepthf(1.0f);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		//glEnable(GL_BLEND);
		//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		//Pass 1 (reflection pass)
		glBindFramebuffer(GL_FRAMEBUFFER, reflection_fbo);
		GLenum reflection_buffer[1] = { GL_COLOR_ATTACHMENT0 };
		glDrawBuffers(1, reflection_buffer);
		auto const status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (status != GL_FRAMEBUFFER_COMPLETE)
			LogError("Something went wrong with framebuffer %u", reflection_fbo);
		glViewport(0, 0, window_size.x, window_size.y);
		glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

		camera_position.y *= -1; //makes sure camera_vector is changed
		glm::mat4 view = glm::scale(mCamera.GetWorldToViewMatrix(), glm::vec3(1, -1, 1)); //makes sure geometry is changed

		glm::mat4 VP = mCamera.GetViewToClipMatrix() * glm::scale(glm::mat4(glm::mat3(mCamera.GetWorldToViewMatrix())), glm::vec3(1, -1, 1));
		glDepthMask(GL_FALSE); //makes sure skybox is drawn first, using no depth
		big_sphere.render(VP, glm::mat4(), cubemap_shader, set_uniforms); //makes skybox follow camera
		glDepthMask(GL_TRUE); //following objects will be drawn on top of skybox

		sphere1.render(mCamera.GetViewToClipMatrix() * view, sphere1.get_transform(), brdf_shader, set_uniforms);
		sphere2.render(mCamera.GetViewToClipMatrix() * view, sphere2.get_transform(), brdf_shader, set_uniforms);
		sphere3.render(mCamera.GetViewToClipMatrix() * view, sphere3.get_transform(), brdf_shader, set_uniforms);
		sphere4.render(mCamera.GetViewToClipMatrix() * view, sphere4.get_transform(), brdf_shader, set_uniforms);

		camera_position.y *= -1; //transform camera_vector back to original

		//Pass 2
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, window_size.x, window_size.y);
		glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

		glUseProgram(glass_shader);
		bind_texture_with_sampler(GL_TEXTURE_2D, 1, glass_shader, "reflection_texture", reflection_texture, default_sampler);
		bind_texture_with_sampler(GL_TEXTURE_2D, 2, glass_shader, "refraction_texture", refraction_texture, default_sampler);

		VP = mCamera.GetViewToClipMatrix() * glm::mat4(glm::mat3(mCamera.GetWorldToViewMatrix()));
		glDepthMask(GL_FALSE); //makes sure skybox is drawn first, using no depth
		big_sphere.render(VP, glm::mat4(), cubemap_shader, set_uniforms); //makes skybox follow camera
		glDepthMask(GL_TRUE); //following objects will be drawn on top of skybox


		plane.render(mCamera.GetWorldToClipMatrix(), plane.get_transform(), glass_shader, set_uniforms);
		sphere1.render(mCamera.GetWorldToClipMatrix(), sphere1.get_transform(), brdf_shader, set_uniforms);
		sphere2.render(mCamera.GetWorldToClipMatrix(), sphere2.get_transform(), brdf_shader, set_uniforms);
		sphere3.render(mCamera.GetWorldToClipMatrix(), sphere3.get_transform(), brdf_shader, set_uniforms);
		sphere4.render(mCamera.GetWorldToClipMatrix(), sphere4.get_transform(), brdf_shader, set_uniforms);



		window->Swap();
		lastTime = nowTime;
	}
}

int main()
{
	Bonobo::Init();
	eda221::Assignment5 Assignment5;
	Assignment5.run();
	Bonobo::Destroy();
	return 0;
}
