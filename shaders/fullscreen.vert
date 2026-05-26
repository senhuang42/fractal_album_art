#version 330 core
// Emits a single oversized triangle that covers the whole viewport, so no
// vertex buffer is needed. Bind an empty VAO and draw 3 vertices.
void main() {
    vec2 verts[3] = vec2[3](vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));
    gl_Position = vec4(verts[gl_VertexID], 0.0, 1.0);
}
