#version 330 core

in vec3 outputcolor;

out vec4 color;

void main(){
    color = vec4(outputcolor, 1.0);
}
