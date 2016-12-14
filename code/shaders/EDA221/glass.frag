#version 410

uniform sampler2D reflection_texture;
uniform sampler2D refraction_texture;
uniform sampler2D dudv_map;

//REMOVE LATER or not?
uniform vec3 light_position;
uniform vec3 camera_position;

in VS_OUT {
	vec3 vertex;
	vec3 normal;
	vec2 tex_coords;
} fs_in;


out vec4 frag_color;

void main()
{
	vec2 res = vec2(1600, 900);
	vec2 screen_coords = gl_FragCoord.xy / res; //range 0, 1

	vec2 distortion = (texture(dudv_map, fs_in.tex_coords).xy * 2.0 - 1.0) * 0.01;
	//screen_coords += distortion;

    vec3 refraction_tex = texture(refraction_texture, screen_coords).xyz;
	vec3 reflection_tex = texture(reflection_texture, screen_coords).xyz;

	/* REMOVE LATER*/
	vec3 n = normalize(fs_in.normal);
	vec3 l = normalize(light_position - fs_in.vertex);
	vec3 v = normalize(camera_position - fs_in.vertex);

	float R0 = 0.02037;
	float fresnel = R0 + (1 - R0) * (1 - pow(dot(v, n), 5));

	frag_color.xyz = mix(refraction_tex, reflection_tex,1);
	frag_color.w = 1.0;
}
