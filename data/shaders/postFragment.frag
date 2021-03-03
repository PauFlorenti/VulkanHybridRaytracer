#version 450
layout(location = 0) in vec2 outUV;
layout(location = 0) out vec4 fragColor;

layout(set = 0, binding = 0) uniform sampler2D finalTexture;

void main()
{
  vec2  uv    = outUV;
  float gamma = 1. / 2.2;

  ivec2 texturePosition = ivec2(gl_FragCoord.x, gl_FragCoord.y);

  vec4 color1 = texelFetch(finalTexture, texturePosition, 0);
  vec4 color2 = texelFetch(finalTexture, ivec2(texturePosition.x - 1, texturePosition.y), 0);
  vec4 color3 = texelFetch(finalTexture, ivec2(texturePosition.x + 1, texturePosition.y), 0);
  vec4 color4 = texelFetch(finalTexture, ivec2(texturePosition.x, texturePosition.y - 1), 0);
  vec4 color5 = texelFetch(finalTexture, ivec2(texturePosition.x, texturePosition.y + 1), 0);

  vec4 color = color1;// + color2 + color3 + color4 + color5;
  //color /= 5;

  fragColor = color;//texture(finalTexture, uv);
}
