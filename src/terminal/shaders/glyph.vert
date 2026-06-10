#version 440

// Vertex-Eingaben des Glyph-Geometrie-Knotens: Position (Item-Koordinaten),
// Atlas-Texturkoordinate und Per-Vertex-Vordergrundfarbe (normalisiertes ubyte4).
layout(location = 0) in vec2 pos;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec4 inColor;

layout(location = 0) out vec2 texCoord;
layout(location = 1) out vec4 color;

// Von der Scene-Graph-Pipeline gefülltes Uniform (Standardlayout, binding 0).
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
};

void main() {
    gl_Position = qt_Matrix * vec4(pos, 0.0, 1.0);
    texCoord = inTexCoord;
    color = inColor;
}
