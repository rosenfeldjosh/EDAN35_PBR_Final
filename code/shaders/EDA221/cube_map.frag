#version 410

uniform samplerCube cube_tex;
uniform vec3 light_position;

in VS_OUT {
    vec3 vertex;
} fs_in;

out vec4 frag_color;

void main()
{
    vec4 tex = texture(cube_tex, fs_in.vertex);
    frag_color = tex;
}
