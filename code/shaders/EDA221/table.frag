#version 410

uniform sampler2D table_tex;

in VS_OUT {
	vec2 tex_coords;
} fs_in;

out vec4 frag_color;

void main()
{
	vec3 tex = texture(table_tex, fs_in.tex_coords).xyz;

	frag_color.xyz = tex;
	frag_color.w = 1.0;
}