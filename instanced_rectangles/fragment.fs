#version 330 core
out vec4 FragColor;
in vec3 ourColor;  // Changed from vec4 to vec3, and in (not out)

void main()
{
    FragColor = vec4(ourColor, 1.0);  // Changed from outColor to ourColor
}