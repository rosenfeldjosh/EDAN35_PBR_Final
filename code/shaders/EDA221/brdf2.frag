#version 410

uniform vec3 light_position;
uniform vec3 camera_position;

uniform samplerCube cube_tex2;
uniform samplerCube diffuse_cubemap2;
uniform sampler2D albedo;
uniform sampler2D roughness;
uniform sampler2D metallic;


#define PI 3.1415926535897932

in VS_OUT {
	vec3 vertex;
	vec3 normal;
	vec2 texcoords;
	vec3 tangent;
	vec3 binormal;
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
	return (NdotV / V) * (NdotL / L);
}

vec3 fresnel_factor(vec3 col, float a)
{
	return col + (1 - col) * pow(1 - a, 5);	
}

float radical_inverse(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);

    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

vec2 Hammersley(uint i, uint N)
{
	return vec2(float(i) / float(N), radical_inverse(i));
}


vec3 importance_sample(vec2 Xi, float Roughness, vec3 N)
{
	float a = Roughness * Roughness;
	float Phi = 2 * PI * Xi.x;
	float CosTheta = sqrt( (1 - Xi.y) / ( 1 + (a*a - 1) * Xi.y ) );
	float SinTheta = sqrt( 1 - CosTheta * CosTheta );
	vec3 H;
	H.x = SinTheta * cos( Phi );
	H.y = SinTheta * sin( Phi );
	H.z = CosTheta;
	vec3 UpVector = abs(N.z) < 0.999 ? vec3(0,0,1) : vec3(1,0,0);
	vec3 TangentX = normalize( cross( UpVector, N ) );
	vec3 TangentY = cross( N, TangentX );

	// Tangent to world space
	return TangentX * H.x + TangentY * H.y + N * H.z;
}

vec3 get_specular(vec3 n, vec3 v, vec3 l_d[3], float roughness, vec3 F0, out vec3 ks)
{
	vec3 radiance;

	int steps = 100; //should be > 10
	for (int i = 0; i < steps; ++i)
	{
		vec2 xi = Hammersley(i, steps);
		vec3 h = importance_sample(xi, roughness, n);
		vec3 l = normalize(-reflect(v, h));

		float NoL = dot(n, l);

		if (NoL > 0)
		{
			NoL = clamp(NoL, 0.01, 1.0);
			float NoH = clamp(dot(n, h), 0.01, 1.0);
			float VoH = clamp(dot(v, h), 0.01, 1.0);
			float NoV = clamp(dot(n, v), 0.01, 1.0);
			float HoL = clamp(dot(h, l), 0.01, 1.0);

			vec3 F = fresnel_factor(F0, VoH);
			float G = G_S(roughness, NoV, NoL);
			float D = D_TR(NoH, roughness);
			vec3 f_r = G * F / (4 * NoH * NoV);
			vec3 L = textureLod(cube_tex2, l, roughness * 11).rgb;
			ks += F;
			radiance += f_r * L * VoH;
		}
	}
vec3 direct;
	for (int i = 0; i < 3; i++)
	{
		vec3 h = normalize(l_d[i] + v);
		float NoH = clamp(dot(n, h), 0.01, 1.0);
		float NoV = clamp(dot(n, v), 0.01, 1.0);
		float VoH = clamp(dot(v, h), 0.01, 1.0);
		float NoL = clamp(dot(n, l_d[i]), 0.01, 1.0);
		vec3 F = fresnel_factor(F0, VoH);
		float G = G_S(roughness, NoV, NoL);
		float D = D_TR(NoH, roughness);
		vec3 f_r = F * G * D / (4 * NoH * NoV);
		direct += f_r * NoL; 
	}
	ks = clamp(ks / steps, 0.0, 1.0);
	float direct_intensity = .04;
	if(roughness == 0.11)
		direct_intensity = .1;
	return radiance / steps + direct_intensity * direct;
}

void main()
{
	vec3 n = normalize(fs_in.normal);
	const vec3 lights_pos[3]=vec3[3](
		vec3(400, 150, 500),
		vec3(-20, 180, 500),
		vec3(-2500, 500, 500)
	);
	vec3 l[3];
	for (int i = 0; i < 3; i++)
		l[i] = normalize(lights_pos[i] - fs_in.vertex);
	vec3 v = normalize(camera_position - fs_in.vertex);

	vec3 albedo = texture(albedo, fs_in.texcoords).rgb;
	float roughness = texture(roughness, fs_in.texcoords).r;
	float metallic = texture(metallic, fs_in.texcoords).r;
		if(albedo.r > 0.990 && albedo.g < .01 && albedo.b < .01)
	{
		metallic = 0.720;
		roughness = 0.011;
	}


    float ior = 1.5;
    vec3 F0 = vec3(abs((1.0 - ior) / (1.0 + ior)));
    F0 = F0 * F0;
    F0 = mix(F0, albedo, metallic);
        
    vec3 ks = vec3(0);
    vec3 specular = get_specular(n, v, l, roughness, F0, ks);
    vec3 kd = (1 - ks) * (1 - metallic);

	vec3 irradiance = texture(diffuse_cubemap2, n).rgb;
	vec3 diffuse = albedo * irradiance / PI;

	float intensity = 5.0;
	frag_color.xyz = intensity * (ks * specular + kd * diffuse);
	frag_color.w = 1.0;
	
}









