#version 440

layout(location = 0) in vec2 texCoord;
layout(location = 1) in vec4 color;

layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
};

// Glyph-Atlas: Mono-Glyphen liegen weiß (Deckung im Alpha) vor; Farb-Glyphen
// (Emoji) als echte vormultiplizierte RGBA-Pixel.
layout(binding = 1) uniform sampler2D atlasTex;

void main() {
    vec4 tex = texture(atlasTex, texCoord);
    // color.a ist der Glyph-Typ-Selektor (NICHT Deckung): 1 = Mono-Glyphe (mit der
    // Per-Vertex-Vordergrundfarbe einfärben), 0 = Farb-Glyphe (Atlas-RGB direkt,
    // bereits vormultipliziert). color.rgb kommt unmultipliziert (a-frei) an.
    vec4 mono = vec4(color.rgb * tex.a, tex.a);   // fg × Deckung, vormultipliziert
    fragColor = mix(tex, mono, color.a) * qt_Opacity;
}
