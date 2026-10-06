#version 450

// 從 Vertex Shader 接收插值後的顏色
layout(location = 0) in vec3 fragColor;

// 輸出最終的像素顏色（Attachment 0 = Swapchain Image）
layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(fragColor, 1.0);  // RGB + Alpha=1.0（不透明）
}
