// accel.mjs -- Super Blit Kart's accelerators for c4's libjs (its
// libjs/accel.js, docs/accel.md): the track's mode 7 spans, the blits and
// their blends, and -demo's frame hash, run by the host when the machine
// has them and the main menu's Accelerated is on. c4/c4accel.h is the
// game's side, with each operation's number (SA_*); track.c, blit.c and
// game.c call them, and keep their own C for every other machine. Each
// does exactly what that C does: the same pixels in the same order, 32-bit
// arithmetic wrapping, divisions toward zero.
export default function sbk({ reader, reader32, writer32, probe, args }) {
  const div = (a, b) => (a / b) | 0;      // C's int division, the operands words
  // writes of one operation: straight to the screen, or, where it reads the
  // screen, held until every page they go to is made sure of (probe), so
  // that a run again after a fault under the MMU does not blend twice
  function sink(m, held) {
    const w32 = writer32(m), r32 = reader32(m);
    if (!held) return { get: r32, put: w32, done() {} };
    const pend = new Map();
    return {
      get: a => (pend.has(a) ? pend.get(a) : r32(a)),
      put(a, v) { pend.set(a, v | 0); },
      done() {
        let page = -1;
        for (const a of pend.keys()) if ((a >>> 12) !== page) { probe(m, a, 4, true); page = a >>> 12; }
        for (const [a, v] of pend) w32(a, v);
      },
    };
  }
  // RGBA_RGB's blend of p over v at alpha a, as blit.c has it
  const blend = (p, v, a) => {
    const dr = (v >> 16) & 255, dg = (v >> 8) & 255, db = v & 255;
    return ((dr + ((a * (((p >> 16) & 255) - dr) + 127) >> 8)) << 16) |
           ((dg + ((a * (((p >> 8) & 255) - dg) + 127) >> 8)) << 8) |
           (db + ((a * ((p & 255) - db) + 127) >> 8));
  };
  // pen_span's work from its blend on: a its alpha with the screen's
  // already, over c words from d
  function penspan(out, pen, d, c, a) {
    if (a >= 255) {
      const v = pen & 0xFFFFFF;
      for (; c > 0; c--, d = (d + 4) | 0) out.put(d, v);
      return;
    }
    for (; c > 0; c--, d = (d + 4) | 0) out.put(d, blend(pen, out.get(d), a));
  }

  // SA_HASH (p, n): -demo's frame_hash, FNV-1a over the frame's n words
  function hash(m) {
    const [p, n] = args(m, 2);
    const r32 = reader32(m);
    let h = 2166136261 | 0;
    for (let i = 0; i < n; i++) h = Math.imul(h ^ r32((p + 4 * i) | 0), 16777619);
    return h;
  }

  // SA_PIXELS (world, wshift, row, d, c, wx, wy, dx, dy): span_pixels, every
  // texel on the map, row[] its colour
  function pixels(m) {
    let [world, wshift, row, d, c, wx, wy, dx, dy] = args(m, 9);
    const rw = reader(m), rr = reader32(m), w32 = writer32(m);
    for (; c > 0; c--, d = (d + 4) | 0) {
      w32(d, rr((row + 4 * rw((world + (wx >> 16) + ((wy >> 16) << wshift)) | 0)) | 0));
      wx = (wx + dx) | 0; wy = (wy + dy) | 0;
    }
    return 0;
  }

  // TileMap::offset's clamp at the map's edge, then the texel: world_clamp
  const clamp = (rw, world, wshift, w, x, y) => {
    let cx = x >> 3, cy = y >> 3;
    if (cx !== (cx & (w - 1))) cx = cx < 0 ? 0 : w - 1;
    if (cy !== (cy & (w - 1))) cy = cy < 0 ? 0 : w - 1;
    return rw((world + (cx << 3) + (x & 7) + ((((cy << 3) + (y & 7)) | 0) << wshift)) | 0);
  };

  // SA_RUNS (world, wshift, w, row, d, c, wx, wy, dx, dy): span_runs, the
  // pixels until the next texel worked out at once
  function runs(m) {
    let [world, wshift, w, row, d, c, wx, wy, dx, dy] = args(m, 10);
    const rw = reader(m), rr = reader32(m), w32 = writer32(m);
    const notmask = ~((w << 3) - 1);
    let x = wx >> 16, y = wy >> 16, kx, ky, n, i = 0, v, col;
    if (dx > 0) kx = div((((x + 1) << 16) - wx + dx - 1) | 0, dx);
    else if (dx < 0) kx = (div((wx - (x << 16)) | 0, -dx) + 1) | 0;
    else kx = 0x7FFFFFFF;
    if (dy > 0) ky = div((((y + 1) << 16) - wy + dy - 1) | 0, dy);
    else if (dy < 0) ky = (div((wy - (y << 16)) | 0, -dy) + 1) | 0;
    else ky = 0x7FFFFFFF;
    for (;;) {
      if ((x | y) & notmask) v = clamp(rw, world, wshift, w, x, y);
      else v = rw((world + x + (y << wshift)) | 0);
      n = kx < ky ? kx : ky;
      if (n > c) n = c;
      c = (c - n) | 0; i = (i + n) | 0; kx = (kx - n) | 0; ky = (ky - n) | 0;
      col = rr((row + 4 * v) | 0);
      if (v !== 255) for (; n > 0; n--, d = (d + 4) | 0) w32(d, col);
      else d = (d + 4 * n) | 0;
      if (!c) return 0;
      if (!kx) {
        n = (wx + Math.imul(i, dx)) | 0;
        x = n >> 16;
        if (dx > 0) kx = div((((x + 1) << 16) - n + dx - 1) | 0, dx);
        else kx = (div((n - (x << 16)) | 0, -dx) + 1) | 0;
      }
      if (!ky) {
        n = (wy + Math.imul(i, dy)) | 0;
        y = n >> 16;
        if (dy > 0) ky = div((((y + 1) << 16) - n + dy - 1) | 0, dy);
        else ky = (div((n - (y << 16)) | 0, -dy) + 1) | 0;
      }
    }
  }

  // SA_EDGE (world, wshift, w, empty, row, d, c, wx, wy, dx, dy): span_edge,
  // some of it off the map, whose edge goes on for ever unless empty
  function edge(m) {
    let [world, wshift, w, empty, row, d, c, wx, wy, dx, dy] = args(m, 11);
    const rw = reader(m), rr = reader32(m), w32 = writer32(m);
    const notmask = ~((w << 3) - 1);
    do {
      const x = wx >> 16, y = wy >> 16;
      if ((x | y) & notmask) {
        if (!empty) w32(d, rr((row + 4 * clamp(rw, world, wshift, w, x, y)) | 0));
      } else w32(d, rr((row + 4 * rw((world + x + (y << wshift)) | 0)) | 0));
      d = (d + 4) | 0; wx = (wx + dx) | 0; wy = (wy + dy) | 0;
    } while (--c);
    return 0;
  }

  // SA_PEN (pen, d, c, a): pen_span from its blend on, a the pen's alpha
  // with the screen's already
  function pen(m) {
    const [p, d, c, a] = args(m, 4);
    const out = sink(m, a < 255);
    penspan(out, p, d, c, a);
    out.done();
    return 0;
  }

  // SA_ROW (s, pal, d, cnt, alpha1): row_blit, a row of 8-bit pixels through
  // their palette, alpha1 the screen's alpha + 1
  function row(m) {
    let [s, pal, d, cnt, alpha1] = args(m, 5);
    const rs = reader(m), rp = reader32(m), out = sink(m, true);
    do {
      const p = rp((pal + 4 * rs(s)) | 0);
      s = (s + 1) | 0;
      const a = Math.imul(((p >> 24) & 255) + 1, alpha1) >> 8;
      if (a >= 255) out.put(d, p & 0xFFFFFF);
      else if (a > 1) out.put(d, blend(p, out.get(d), a));
      d = (d + 4) | 0;
    } while (--cnt > 0);
    out.done();
    return 0;
  }

  // SA_TINT (src, w, h, d0, stride, rgb, a): scr_blit_tinted, the minimap,
  // index 0 nothing, the others rgb[] at alpha a
  function tint(m) {
    let [src, w, h, d0, stride, rgb, a] = args(m, 7);
    const rs = reader(m), rp = reader32(m), out = sink(m, true);
    for (let y = 0; y < h; y++) {
      let d = (d0 + Math.imul(y, stride)) | 0, x = w;
      do {
        const c = rs(src); src = (src + 1) | 0;
        if (c) out.put(d, blend(rp((rgb + 4 * c) | 0), out.get(d), a));
        d = (d + 4) | 0;
      } while (--x > 0);
    }
    out.done();
    return 0;
  }

  // SA_ANY (tiles, trans, pal, sheet, w, d, c, wx, wy, dx, dy, alpha):
  // span_any, TileMap::texture_span as it is, each texel's run a pen_span
  // at the screen's alpha
  function any(m) {
    let [tiles, trans, pal, sheet, w, d, c, wx, wy, dx, dy, alpha] = args(m, 12);
    const rt = reader(m), rx = reader(m), rh = reader(m), rp = reader32(m), out = sink(m, true);
    do {
      const wcx = wx >> 16, wcy = wy >> 16;
      let cx = (wcx >> 3) & (w - 1), cy = (wcy >> 3) & (w - 1);
      if ((wcx >> 3) !== cx) cx = (wcx >> 3) < 0 ? 0 : w - 1;
      if ((wcy >> 3) !== cy) cy = (wcy >> 3) < 0 ? 0 : w - 1;
      const toff = (cx + Math.imul(cy, w)) | 0;
      const tile = rt((tiles + toff) | 0);
      if (tile !== 0) {
        const tr = trans ? rx((trans + toff) | 0) : 0;
        let u = wcx & 7, v = wcy & 7;
        if (tr) {
          v = (tr & 2) ? 7 - v : v;
          u = (tr & 4) ? 7 - u : u;
          if (tr & 1) { const t = u; u = v; v = t; }
        }
        u += (tile & 15) * 8;
        v += (tile >> 4) * 8;
        let count = 0;
        do {
          wx = (wx + dx) | 0; wy = (wy + dy) | 0; c--; count++;
        } while (c && (wx >> 16) === wcx && (wy >> 16) === wcy);
        const p = rp((pal + 4 * rh((sheet + u + v * 128) | 0)) | 0);
        let a = (p >> 24) & 255;
        if (a) penspan(out, p, d, count, ((a + 1) * (alpha + 1)) >> 8);
        d = (d + 4 * count) | 0;
        continue;
      }
      do {
        wx = (wx + dx) | 0; wy = (wy + dy) | 0; d = (d + 4) | 0; c--;
      } while (c && (wx >> 19) === (wcx >> 3) && (wy >> 19) === (wcy >> 3));
    } while (c);
    out.done();
    return 0;
  }

  // SA_STRETCH1 (row0, sy, sw2, pal, y, ystep, left, xstep, d, dw, dh,
  // alpha1): scr_stretch where it is no wider than the source, each pixel
  // the source's under it, row0 the source's data + sx
  function stretch1(m) {
    let [row0, sy, sw2, pal, y, ystep, left, xstep, d, dw, dh, alpha1] = args(m, 12);
    const rs = reader(m), rp = reader32(m), out = sink(m, true);
    do {
      const row = (row0 + Math.imul((sy + (y >> 16)) | 0, sw2)) | 0;
      let x = left, dd = d, n = dw;
      do {
        const p = rp((pal + 4 * rs((row + (x >> 16)) | 0)) | 0);
        x = (x + xstep) | 0;
        if (p >> 24) {
          const a = Math.imul(((p >> 24) & 255) + 1, alpha1) >> 8;
          out.put(dd, a >= 255 ? p & 0xFFFFFF : blend(p, out.get(dd), a));
        }
        dd = (dd + 4) | 0;
      } while (--n > 0);
      d = (d + 4 * 320) | 0;
      y = (y + ystep) | 0;
    } while (--dh > 0);
    out.done();
    return 0;
  }

  // SA_STRETCHW (row0, sy, sw2, pal, y, ystep, left, xstep, xpx, d, dw, dh,
  // alpha1): scr_stretch wider than the source (xpx, x_scale_px, above 1):
  // upstream's stepping across worked out once into runs, each row of the
  // source's visible runs when it changes, opaque ones filled and the rest
  // through pen_span
  function stretchw(m) {
    let [row0, sy, sw2, pal, y, ystep, left, xstep, xpx, d, dw, dh, alpha1] = args(m, 13);
    const rs = reader(m), rp = reader32(m), out = sink(m, true);
    const runX = [], runN = [];
    let xc = dw, x = left, num = (x & 65535) === 0 ? xpx : 1;
    do {
      let nx = (x + Math.imul(xstep, num)) | 0;
      if ((x >> 16) === (nx >> 16) && xc > num) { num++; nx = (nx + xstep) | 0; }
      runX.push(x >> 16); runN.push(num);
      x = nx;
      xc -= num;
      num = xc < xpx ? xc : xpx;
    } while (xc);
    let last = -1, vis = [];
    do {
      const srcY = (sy + (y >> 16)) | 0;
      if (srcY !== last) {
        last = srcY;
        const row = (row0 + Math.imul(srcY, sw2)) | 0;
        vis = [];
        let off = 0;
        for (let j = 0; j < runX.length; j++) {
          const p = rp((pal + 4 * rs((row + runX[j]) | 0)) | 0);
          if (p >> 24) {
            const a = Math.imul(((p >> 24) & 255) + 1, alpha1) >> 8;
            vis.push([a >= 255 ? p & 0xFFFFFF : p, a >= 255, off, runN[j]]);
          }
          off += runN[j];
        }
      }
      for (const [p, opaque, off, n] of vis) {
        const at = (d + 4 * off) | 0;
        if (opaque) penspan(out, p, at, n, 255);
        else {
          const a = (p >> 24) & 255;
          if (a) penspan(out, p, at, n, Math.imul(a + 1, alpha1) >> 8);
        }
      }
      d = (d + 4 * 320) | 0;
      y = (y + ystep) | 0;
    } while (--dh > 0);
    out.done();
    return 0;
  }

  // SA_TEXT (s, data, widths, cw, ch, cx, cy, clipx, clipy, clipw, cliph,
  // plain, ink, pen, alpha, fb): scr_text from where its first character
  // goes: each glyph's columns of bits, ink where it is plain, else
  // pen_span's blend of pen at the screen's alpha
  function text(m) {
    let [s, data, widths, cw, ch, cx, cy, clx, cly, clw, clh, plain, ink, pen, alpha, fb] = args(m, 16);
    const rc = reader(m), rg = reader(m), rw = reader(m), out = sink(m, !plain);
    const hb = ((ch + 7) / 8) | 0, size = Math.imul(cw, hb);
    const pa = (pen >> 24) & 255, a = Math.imul(pa + 1, alpha + 1) >> 8;
    for (let c; (c = rc(s)) !== 0; ) {
      s = (s + 1) | 0;
      c &= 0x7F;
      c = c < 32 ? 0 : c - 32;
      let g = (data + Math.imul(c, size)) | 0;
      for (let x = 0; x < cw; x++) {
        const xx = (cx + x) | 0;
        if (xx < clx || xx >= clx + clw) { g = (g + hb) | 0; continue; }
        for (let y = 0; y < ch; y += 8) {
          const bit = rg(g); g = (g + 1) | 0;
          if (!bit) continue;
          for (let po = 0; po < 8; po++) {
            if (!(bit & (1 << po))) continue;
            const yy = (cy + y + po) | 0;
            if (yy >= 0 && yy >= cly && yy < cly + clh) {
              const at = (fb + 4 * ((xx + Math.imul(yy, 320)) | 0)) | 0;
              if (plain) out.put(at, ink);
              else if (pa) penspan(out, pen, at, 1, a);
            }
          }
        }
      }
      cx = (cx + rw((widths + c) | 0)) | 0;
    }
    out.done();
    return 0;
  }

  return [hash, pixels, runs, edge, pen, row, tint, any, stretch1, stretchw, text];
}
