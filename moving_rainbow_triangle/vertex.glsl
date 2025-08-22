#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
uniform float xOffset;
uniform vec3 colorTint;  // Add this uniform

out vec3 ourColor;

void main()
{
    gl_Position = vec4(aPos.x + xOffset, aPos.y, aPos.z, 1.0);
    ourColor = aColor * colorTint;  // Multiply vertex color by tint
}