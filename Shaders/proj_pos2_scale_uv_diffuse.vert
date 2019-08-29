#version 420
#extension GL_ARB_explicit_uniform_location : enable
layout(location=0) uniform mat4 ProjMtx;
layout(location=1) uniform mat4 WorldScale = mat4(1.0);
layout(location=0) attribute vec2 Position;
layout(location=1) attribute vec2 UV;
layout(location=2) attribute vec4 Color;
varying vec2 Frag_UV;
varying vec4 Frag_Color;
void main() {
  Frag_UV = UV;
  Frag_Color = Color;
  gl_Position = ProjMtx * WorldScale * vec4(Position.xy, 0, 1);
}
