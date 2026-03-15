#version 330 core

layout (location = 0) in vec2 position;


out vec3 outputcolor;

void main(){
    gl_Position = vec4(position.x, position.y, 0.0, 1.0);
    outputcolor = vec3(1.0, 0.5, 0.2);
}