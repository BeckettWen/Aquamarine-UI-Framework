#version 330 core

layout (location = 0) in vec2 position;
layout (location = 1) in vec2 UVCoords;

out vec2 uvs;

uniform mat4 OrthographicProjection;

void main(){
    gl_Position = OrthographicProjection * vec4(position.x, position.y, 0.0, 1.0);
    uvs = UVCoords;
}