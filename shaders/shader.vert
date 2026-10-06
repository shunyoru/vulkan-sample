#version 450

// 三個頂點的座標直接寫在 Shader 裡（不用 Vertex Buffer）
// Vulkan NDC 座標系：X [-1,1] 左到右，Y [-1,1] 上到下，Z [0,1] 近到遠
vec2 positions[3] = vec2[](
    vec2( 0.0, -0.5),   // 頂點（上方）
    vec2( 0.5,  0.5),   // 右下
    vec2(-0.5,  0.5)    // 左下
);

vec3 colors[3] = vec3[](
    vec3(1.0, 0.0, 0.0),  // 紅
    vec3(0.0, 1.0, 0.0),  // 綠
    vec3(0.0, 0.0, 1.0)   // 藍
);

layout(location = 0) out vec3 fragColor;

void main() {
    // gl_VertexIndex: 目前是第幾個頂點（0, 1, 2）
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    fragColor = colors[gl_VertexIndex];
}
