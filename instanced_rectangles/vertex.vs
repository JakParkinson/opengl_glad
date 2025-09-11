#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
layout (location = 2) in vec2 aOffset;

out vec3 ourColor;


void main()
{    gl_Position = vec4(aPos.x + aOffset.x, aPos.y + aOffset.y, 0.0, 1.0);
    ourColor = aColor;  // Multiply vertex color by tint
}