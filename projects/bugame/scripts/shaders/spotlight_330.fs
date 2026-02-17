#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

out vec4 finalColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

uniform vec2 u_resolution;
uniform vec2 u_light;
uniform float u_radius;
uniform float u_softness;
uniform float u_ambient;

void main()
{
    vec2 res = max(u_resolution, vec2(1.0));
    vec4 texel = texture(texture0, fragTexCoord) * fragColor * colDiffuse;

    float d = distance(gl_FragCoord.xy, u_light);
    float r = max(u_radius, 1.0);
    float soft = max(u_softness, 0.001);

    float falloff = smoothstep(r, r * (1.0 + soft), d);
    float lit = mix(1.0, clamp(u_ambient, 0.0, 1.0), falloff);

    finalColor = vec4(texel.rgb * lit, texel.a);
}
