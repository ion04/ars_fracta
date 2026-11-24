#version 330 core

// ---------------------------------------------------------------------------
//  Fractal Shader — Main Entry Point
//  Includes modular components from inc/
// ---------------------------------------------------------------------------

#include "inc/common.inc"
#include "inc/mandelbulb.inc"
#include "inc/menger.inc"
#include "inc/mandelbrot2d.inc"
#include "inc/terrain.inc"
#include "inc/coast.inc"

void main() {
    float tanHalf = tan(radians(uFov * 0.5));
    vec2 p = (gl_FragCoord.xy - 0.5 * uResolution) / uResolution.y;
    vec3 dir = normalize(uCamForward +
                         uCamRight * p.x * tanHalf +
                         uCamUp * p.y * tanHalf);

    // 2D Mandelbrot (no ray marching)
    if (ubo.uFractalType == 0) {
        FragColor = vec4(mandelbrot2D(), 1.0);
        return;
    }

    // Procedural Terrain
    if (ubo.uFractalType == 4) {
        FragColor = vec4(renderTerrain(uCamPos, dir), 1.0);
        return;
    }

    // Coast3D (Mandelbrot boundary)
    if (ubo.uFractalType == 5) {
        FragColor = vec4(renderCoast(uCamPos, dir), 1.0);
        return;
    }

    // 3D Fractals: Mandelbulb, Menger, Julia
    float progress;
    vec3 pos = march(uCamPos, dir, progress);

    vec3 col;
    float dist = length(pos - uCamPos);
    if (dist > 39.5) {
        // Far background
        col = palette(0.08 + 0.20 * dir.y);
        col *= 0.5;
    } else {
        vec3 n = calcNormal(pos);
        col = shade(pos, n, progress);
    }

    FragColor = vec4(col, 1.0);
}