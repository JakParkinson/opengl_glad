#version 330 core
out vec4 FragColor;
in vec3 fColor;  // Changed from vec4 to vec3, and in (not out)

void main()
{
    FragColor = vec4(fColor, 1.0);  // Changed from outColor to ourColor
}