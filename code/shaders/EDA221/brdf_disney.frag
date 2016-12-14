#version 410

uniform vec3 light_position;
uniform vec3 camera_position;

uniform samplerCube cube_tex;

#define PI 3.1415926535897932

in VS_OUT {
	vec3 vertex;
	vec3 normal;
} fs_in;

out vec4 frag_color;

float fresnel(float a)
{
	return pow(1 - a, 5);
}

float smithG_GGX(float Ndotv, float alphaG)
{
    float a = alphaG*alphaG;
    float b = Ndotv*Ndotv;
    return 1/(Ndotv + sqrt(a + b - a*b));
}

float D_TR(float NdotH, float a)
{
	float NdotH2 = NdotH * NdotH;
	return a * a / (PI * pow(NdotH2 * (a * a - 1.0) + 1.0, 2));
}

vec3 mon2lin(vec3 x)
{
    return vec3(pow(x[0], 2.2), pow(x[1], 2.2), pow(x[2], 2.2));
}

void main()
{
	vec3 n = normalize(fs_in.normal);
	vec3 l = normalize(light_position - fs_in.vertex);
	vec3 v = normalize(camera_position - fs_in.vertex);
	vec3 h = normalize(l + v);

	float NdotL = max(0, dot(n,l));
    float NdotV = max(0, dot(n,v));
	float NdotH = max(0, dot(n,h));
    float LdotH = max(0, dot(l,h));
	float HdotV = max(0, dot(h,v));
	
	vec3 reflection = reflect(-v, n);
	vec3 reflection_color = texture(cube_tex, reflection).xyz; 

	vec3 base_color = vec3(1, 0.86, 0.57); //gold
	float roughness = 0.3;
	float metalness = 0.7;
	float spec = 0.5;
	float spec_tint = 0;
	float sheen_tint = 0;
	float sheen = 0;
	float clearcoat = 0;

	vec3 Cdlin = mon2lin(base_color);
    float Cdlum = .3*Cdlin[0] + .6*Cdlin[1]  + .1*Cdlin[2]; // luminance approx.

    vec3 Ctint = Cdlum > 0 ? Cdlin/Cdlum : vec3(1);
    vec3 Cspec0 = mix(spec*.08*mix(vec3(1), Ctint, spec_tint), Cdlin, metalness);
    vec3 Csheen = mix(vec3(1), Ctint, sheen_tint);

	float k = pow(roughness * 0.5 + 0.5, 2);

	float G = smithG_GGX(NdotL, k) * smithG_GGX(NdotV, k);
	float D = D_TR(NdotH, roughness);

	float FH = fresnel(LdotH);
	float FL = fresnel(NdotL);
	float FV = fresnel(NdotV);
    float Fd90 = 0.5 + 2 * pow(LdotH,2) * roughness;
    float Fd = mix(1, Fd90, FL) * mix(1, Fd90, FV);
	vec3 Fsheen = FH * sheen * Csheen;
	vec3 Fs = mix(Cspec0, vec3(1), FH);

	frag_color.xyz = ((1/PI)*Fd*Cdlin+Fsheen)*(1-metalness)+G*Fs*D+0.25*Fs*G*D*clearcoat;
	frag_color.xyz = G*Fs*D;
	frag_color.w = 1.0;

	if (NdotL < 0 || NdotV < 0)
		frag_color.xyz = vec3(0);
}
