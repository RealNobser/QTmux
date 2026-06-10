#version 440

layout(location = 0) in vec2 texCoord;
layout(location = 1) in vec4 color;

layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
};

// Glyph-Atlas: speichert die Deckung (Coverage) des Glyphs im Alpha-Kanal.
layout(binding = 1) uniform sampler2D atlasTex;

void main() {
    float coverage = texture(atlasTex, texCoord).a;
    // color kommt unmultipliziert (a = 1) an; Ausgabe vormultipliziert mit der
    // Glyph-Deckung, wie es der Scene-Graph erwartet.
    fragColor = vec4(color.rgb * coverage, coverage) * qt_Opacity;
}
