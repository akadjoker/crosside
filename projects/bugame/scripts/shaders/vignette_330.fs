#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

out vec4 finalColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec2 u_resolution;
uniform float u_strength;

void main()
{
    vec2 res = max(u_resolution, vec2(1.0));
    vec4 texel = texture(texture0, fragTexCoord) * fragColor * colDiffuse;
    vec2 uv = gl_FragCoord.xy / res;
    vec2 p = uv - vec2(0.5);
    float dist2 = dot(p, p) * 2.0;
    float vignette = clamp(1.0 - dist2 * u_strength, 0.0, 1.0);
    finalColor = vec4(texel.rgb * vignette, texel.a);
}
