#version 330 core

in vec2 uvs;

uniform sampler2D buttonTextureSampler;

out vec4 color;

void main(){
    color = texture(buttonTextureSampler, uvs);
}
