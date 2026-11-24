// ---------------------------------------------------------------------------
//  2D Mandelbrot
// ---------------------------------------------------------------------------

vec3 mandelbrot2D() {
    vec2 center = uCamPos.xy;
    vec2 uv = (gl_FragCoord.xy - 0.5 * uResolution) / (0.5 * uResolution.y);
    vec2 c = center + uv * (2.0 / max(ubo.uM2dZoom, 0.0001));

    vec2 z = vec2(0.0);
    int iter = ubo.uIterations;
    for (int i = 0; i < ubo.uIterations; ++i) {
        z = vec2(z.x * z.x - z.y * z.y, 2.0 * z.x * z.y) + c;
        if (dot(z, z) > ubo.uBailout * ubo.uBailout) {
            iter = i;
            break;
        }
    }

    if (iter == ubo.uIterations) return vec3(0.0);

    float mu = float(iter) + 1.0 - log2(log(dot(z, z)) * 0.5);
    float t = mu / float(ubo.uIterations);
    if (ubo.uColorMode == 1) return vec3(t);
    return palette(t * ubo.uColorScale + ubo.uHueShift);
}