#version 410

layout (location = 0) in vec3 vertex;
layout (location = 2) in vec3 tex_coords;

uniform mat4 vertex_model_to_world;
uniform mat4 vertex_world_to_clip;

out VS_OUT {
	vec2 tex_coords;
} vs_out;


void main()
{
	vs_out.tex_coords = tex_coords.xz;
	gl_Position = vertex_world_to_clip * vertex_model_to_world * vec4(vertex, 1.0);
}



