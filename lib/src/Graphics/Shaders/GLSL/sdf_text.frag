#version 450 core

layout(location = 0) out vec4 fColor;
layout(set = 0, binding = 0) uniform sampler2D sTexture;
layout(location = 0) in struct {
    vec4 Color;
    vec2 TexCoord;
} In;

void main() {
    float dist = texture(sTexture, In.TexCoord).r;
    float threshold = 0.5;
    float alpha = smoothstep(threshold - 0.01, threshold + 0.01, dist);
    fColor = vec4(In.Color.rgb, In.Color.a * alpha);
}