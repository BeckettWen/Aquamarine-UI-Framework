#version 330 core

in vec2 uvs;
in float textureIndex;

uniform sampler2DArray buttonTextureSampler;

out vec4 color;

void main(){
    color = texture(buttonTextureSampler, vec3(uvs, textureIndex));
}
