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

float D_TR(float NdotH, float a)
{
	float NdotH2 = NdotH * NdotH;
	return a * a / (PI * pow(NdotH2 * (a * a - 1.0) + 1.0, 2));
}

float G_S(float roughness, float NdotV, float NdotL)
{
	float k = roughness * roughness * 0.5;
	float V = NdotV * (1.0 - k) + k; 
	float L = NdotL * (1.0 - k) + k; 
	return 0.25 / (V * L); 
}

float fresnel(float spec, vec3 l, vec3 h)
{
	return spec + (1 - spec) * pow(1 - dot(l, h), 5);
}

float fresnel2(float a)
{
	return pow(1 - a, 5);
}

vec3 fresnel_factor(vec3 col, float a)
{
	return col + (1 - col) * pow(1 - a, 5);	
}

vec3 cooktorrance_specular(float NdotL, float NdotV, float NdotH, 
	float rim_lightning, vec3 specfresnel, float roughness)
{
	float D = D_TR(NdotH, roughness);
	float G = G_S(roughness, NdotV, NdotL);
	float rim = 1 / mix(1.0 - roughness * rim_lightning * 0.9, 1.0, NdotV);

	return D * G * rim * specfresnel;
}

float G_Schlick(float dot, float k)
{
	return dot / (dot * (1 - k) + k);
}

float G_Smith(vec3 n, vec3 v, vec3 l, float k)
{
	float NV = max(dot(n, v), 0.0);
	float NL = max(dot(n, l), 0.0);
	return G_Schlick(NV, k) * G_Schlick(NL, k);
}

float smithG_GGX(float Ndotv, float alphaG)
{
    float a = alphaG*alphaG;
    float b = Ndotv*Ndotv;
    return 1/(Ndotv + sqrt(a + b - a*b));
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
	
	vec3 base_color = vec3(1, 0.86, 0.57);
	float roughness = 0.4;
	float metallic = 0.9;
	float spec = 0.5;
	float spec_tint = 0;
	float sheen_tint = 0;
	float sheen = 0;
	float clearcoat = 0;
	float rim_lightning = 1;

	vec3 specular = mix(vec3(0.04), base_color, metallic);

	vec3 specfresnel = fresnel_factor(specular, HdotV); 
    vec3 specref = cooktorrance_specular(NdotL, NdotV, NdotH, 
		rim_lightning, specfresnel, roughness); 
	
	specref *= vec3(NdotL);
	vec3 diffref = (vec3(1.0) - specfresnel) * base_color * NdotL / PI;

    vec3 reflected_light = vec3(0); 
    vec3 diffuse_light = vec3(0); // initial value == constant ambient light 
    vec3 light_color = vec3(1);

	reflected_light += specref * light_color; 
    diffuse_light += diffref * light_color; 

	//HOW DO I ADD THIS?
	//vec3 reflection = reflect(-v, n);
	//vec3 reflection_color = texture(cube_tex, reflection).xyz; 

	//diffuse_light += reflection_color * (1.0 / PI);
	//reflected_light += reflection_color;

	frag_color.xyz = diffuse_light * mix(base_color, vec3(0.0), metallic) + reflected_light; 

	/*
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

	float k = (roughness + 1) * (roughness + 1) / 8;

	vec3 Cdlin = mon2lin(base_color);
    float Cdlum = .3*Cdlin[0] + .6*Cdlin[1]  + .1*Cdlin[2]; // luminance approx.

    vec3 Ctint = Cdlum > 0 ? Cdlin/Cdlum : vec3(1);
    vec3 Cspec0 = mix(spec*.08*mix(vec3(1), Ctint, spec_tint), Cdlin, metalness);
    vec3 Csheen = mix(vec3(1), Ctint, sheen_tint);

	float F0 = 0.04;
	//F0 = mix(base_color, base_color, metalness);
	vec3 specular_col = mix(vec3(0.04), base_color, metalness);
	vec3 specfresnel = specular_col + (1 - specular_col) * pow(1 - dot(h,v), 5);
	float F = fresnel(F0, l, h);
	k = pow(roughness * 0.5 + 0.5, 2);
	float G = G_Smith(n, v, l, k);
	G = smithG_GGX(NdotL, k) * smithG_GGX(NdotV, k);
	float D = D_TR(NdotH, roughness);
	vec3 f = specfresnel * D * G / (4 * NdotL * NdotV);

	float FH = fresnel2(LdotH);
	float FL = fresnel2(NdotL);
	float FV = fresnel2(NdotV);
    float Fd90 = 0.5 + 2 * pow(LdotH,2) * roughness;
    float Fd = mix(1, Fd90, FL) * mix(1, Fd90, FV);
	vec3 Fsheen = FH * sheen * Csheen;

	vec3 Fs = mix(Cspec0, vec3(1), FH);
	float val = NdotH < 0 ? 0.0 : D * G * F;
    val = val / NdotL;

	vec3 light_color = vec3(1);
	vec3 diffuse = (vec3(1) - specfresnel) * base_color * NdotL / PI; 
	vec3 specular = light_color * val;

	float f0 = 0.04;
	float fres = f0 + (1.0 - f0) * pow(1.0 - LdotH, 5.0);

	frag_color.xyz = ((1/PI)*Fd*Cdlin+Fsheen)*(1-metalness)+G*Fs*D+0.25*Fs*G*D*clearcoat;
	diffuse += reflection_color / PI;
	//frag_color.xyz = diffuse * mix(base_color, vec3(0.0), metalness) + specular;

	if (NdotL < 0 || NdotV < 0)
		frag_color.xyz = vec3(0);
	frag_color.w = 1.0;*/
}

//CREATE NEW WITH : https://gist.github.com/galek/53557375251e1a942dfa
