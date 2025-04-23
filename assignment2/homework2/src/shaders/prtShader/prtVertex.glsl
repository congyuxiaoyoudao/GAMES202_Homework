attribute vec3 aVertexPosition;
attribute vec3 aNormalPosition;
attribute mat3 aPrecomputeLT;

uniform mat4 uModelMatrix;
uniform mat4 uViewMatrix;
uniform mat4 uProjectionMatrix;
uniform mat3 uPrecomputeL[3];

varying highp vec3 vNormal;
varying highp vec3 vColor;

float LoT (mat3 L, mat3 LT) {
  vec3 L0 = L[0];
  vec3 L1 = L[1];
  vec3 L2 = L[2];
  vec3 LT0 = LT[0];
  vec3 LT1 = LT[1];
  vec3 LT2 = LT[2];
  return dot(L0, LT0) + dot(L1, LT1) + dot(L2, LT2);
}

void main(void) {

  vNormal = (uModelMatrix * vec4(aNormalPosition, 0.0)).xyz;

  gl_Position = uProjectionMatrix * uViewMatrix * uModelMatrix *
                vec4(aVertexPosition, 1.0);

  // rgb color from precomputed light transport
  float r = LoT(uPrecomputeL[0],aPrecomputeLT);
  float g = LoT(uPrecomputeL[1],aPrecomputeLT);
  float b = LoT(uPrecomputeL[2],aPrecomputeLT);
    vColor = vec3(r, g, b);
}