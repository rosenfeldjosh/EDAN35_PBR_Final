#version 410

uniform vec3 light_position;
uniform vec3 camera_position;


in VS_OUT {
	vec3 vertex;
} fs_in;

out vec4 frag_color;


void main()
{
	frag_color = vec4(1);
}
