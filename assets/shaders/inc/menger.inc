// ---------------------------------------------------------------------------
//  Menger Sponge Distance Estimator
// ---------------------------------------------------------------------------

float mengerDE(vec3 pos, int iters) {
    vec3 q = abs(pos);
    float d = max(q.x, max(q.y, q.z)) - 1.0;
    float s = 1.0;
    for (int i = 0; i < iters; ++i) {
        vec3 a = vec3(posMod(pos.x * s, 2.0) - 1.0,
                      posMod(pos.y * s, 2.0) - 1.0,
                      posMod(pos.z * s, 2.0) - 1.0);
        s *= 3.0;
        vec3 r = abs(1.0 - 3.0 * abs(a));
        float da = max(r.x, r.y);
        float db = max(r.y, r.z);
        float dc = max(r.z, r.x);
        float c = (min(da, min(db, dc)) - 1.0) / s;
        d = max(d, c);
    }
    return d;
}