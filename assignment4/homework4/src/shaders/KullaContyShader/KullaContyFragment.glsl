#ifdef GL_ES
precision mediump float;
#endif

uniform vec3 uLightPos;
uniform vec3 uCameraPos;
uniform vec3 uLightRadiance;
uniform vec3 uLightDir;

uniform sampler2D uAlbedoMap;
uniform float uMetallic;
uniform float uRoughness;
uniform sampler2D uBRDFLut;
uniform sampler2D uEavgLut;
uniform samplerCube uCubeTexture;

varying highp vec2 vTextureCoord;
varying highp vec3 vFragPos;
varying highp vec3 vNormal;

const float PI = 3.14159265359;

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / max(denom, 0.0001);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float a = roughness;
    float k = (a * a) / 2.0;

    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NoV = max(dot(N,V),0.0);
    float NoL = max(dot(N,L),0.0);

    float ggx2 = GeometrySchlickGGX(NoV, roughness);
    float ggx1 = GeometrySchlickGGX(NoL, roughness);

    return ggx1 * ggx2;
}

vec3 fresnelSchlick(vec3 F0, vec3 V, vec3 H)
{
    float cosTheta = dot(V,H);
    vec3 F = F0 + (vec3(1.0)-F0)*pow(1.0-cosTheta,5.0);
    return F;
}


//https://blog.selfshadow.com/publications/s2017-shading-course/imageworks/s2017_pbs_imageworks_slides_v2.pdf
vec3 AverageFresnel(vec3 r, vec3 g)
{
    return vec3(0.087237) + 0.0230685*g - 0.0864902*g*g + 0.0774594*g*g*g
           + 0.782654*r - 0.136432*r*r + 0.278708*r*r*r
           + 0.19744*g*r + 0.0360605*g*g*r - 0.2586*g*r*r;
}

vec3 MultiScatterBRDF(float NdotL, float NdotV)
{
  vec3 albedo = pow(texture2D(uAlbedoMap, vTextureCoord).rgb, vec3(2.2));

  vec3 E_o = texture2D(uBRDFLut, vec2(NdotL, uRoughness)).xxx;
  vec3 E_i = texture2D(uBRDFLut, vec2(NdotV, uRoughness)).xxx;

  vec3 E_avg = texture2D(uEavgLut, vec2(0, uRoughness)).xxx;
  // copper
  vec3 edgetint = vec3(0.827, 0.792, 0.678);
  vec3 F_avg = AverageFresnel(albedo, edgetint);
  
  // calculate fms and missing energy here
  vec3 f_add = F_avg * E_avg / (1.0 - F_avg * (1.0 - E_avg));
  vec3 f_ms = (1.0 - E_o) * (1.0 - E_i) / (PI * (1.0 - E_avg));
  return f_add*f_ms;
  
}

// Add parameter F0
vec3 MultiScatterBRDF(float NdotL, float NdotV, vec3 F0)
{
  vec3 albedo = pow(texture2D(uAlbedoMap, vTextureCoord).rgb, vec3(2.2));

  vec3 BRDFLutVal_o = texture2D(uBRDFLut, vec2(NdotL, uRoughness)).xyz;
  vec3 BRDFLutVal_i = texture2D(uBRDFLut, vec2(NdotV, uRoughness)).xyz;

  vec3 E_o = F0 * BRDFLutVal_o.y + vec3(BRDFLutVal_o.z);
  vec3 E_i = F0 * BRDFLutVal_i.y + vec3(BRDFLutVal_i.z);

  vec3 EavgLutVal = texture2D(uEavgLut, vec2(0, uRoughness)).xyz;
  vec3 E_avg = F0 * EavgLutVal.y + vec3(EavgLutVal.z);
  // copper
  vec3 edgetint = vec3(0.827, 0.792, 0.678);
  vec3 F_avg = AverageFresnel(albedo, edgetint);
  
  // calculate fms and missing energy here
  vec3 f_add = F_avg * E_avg / (1.0 - F_avg * (1.0 - E_avg));
  vec3 f_ms = (1.0 - E_o) * (1.0 - E_i) / (PI * (1.0 - E_avg));
  return f_add*f_ms;
  
}

void main(void) {
  vec3 albedo = pow(texture2D(uAlbedoMap, vTextureCoord).rgb, vec3(2.2));

  vec3 N = normalize(vNormal);
  vec3 V = normalize(uCameraPos - vFragPos);
  float NdotV = max(dot(N, V), 0.0);

  vec3 F0 = vec3(0.04); 
  F0 = mix(F0, albedo, uMetallic);

  vec3 Lo = vec3(0.0);

  // calculate per-light radiance
  vec3 L = normalize(uLightDir);
  vec3 H = normalize(V + L);
  float distance = length(uLightPos - vFragPos);
  float attenuation = 1.0 / (distance * distance);
  vec3 radiance = uLightRadiance;

  float NDF = DistributionGGX(N, H, uRoughness);   
  float G   = GeometrySmith(N, V, L, uRoughness);
  vec3 F = fresnelSchlick(F0, V, H);
      
  vec3 numerator    = NDF * G * F; 
  float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0);
  vec3 Fmicro = numerator / max(denominator, 0.001); 
  
  float NdotL = max(dot(N, L), 0.0);        

  // vec3 Fms = MultiScatterBRDF(NdotL, NdotV);
  vec3 Fms = MultiScatterBRDF(NdotL, NdotV, F0);
  vec3 BRDF = Fmicro + Fms;
  
  Lo += BRDF * radiance * NdotL;
  vec3 color = Lo;
  
  color = color / (color + vec3(1.0));
  color = pow(color, vec3(1.0/2.2)); 
  gl_FragColor = vec4(color, 1.0);

}