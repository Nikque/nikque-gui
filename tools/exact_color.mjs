// Picks the color of the values of a search to the end: far from the 8 stage colors (OKLab x100),
// readable on the black outline (contrast >= 4.5), not brown (hue 30..115 with L < 0.65).
const lin = c => { c /= 255; return c <= 0.04045 ? c / 12.92 : ((c + 0.055) / 1.055) ** 2.4; };
const rgb = h => [parseInt(h.slice(1, 3), 16), parseInt(h.slice(3, 5), 16), parseInt(h.slice(5, 7), 16)];
function oklab(h) {
  const [r, g, b] = rgb(h).map(lin);
  const l = Math.cbrt(0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b);
  const m = Math.cbrt(0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b);
  const s = Math.cbrt(0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b);
  return [0.2104542553 * l + 0.793617785 * m - 0.0040720468 * s, 1.9779984951 * l - 2.428592205 * m + 0.4505937099 * s,
          0.0259040371 * l + 0.7827717662 * m - 0.808675766 * s];
}
const lum = h => { const [r, g, b] = rgb(h).map(lin); return 0.2126 * r + 0.7152 * g + 0.0722 * b; };
const contrast = (a, b) => { const x = lum(a), y = lum(b); return (Math.max(x, y) + 0.05) / (Math.min(x, y) + 0.05); };
const dE = (a, b) => { const p = oklab(a), q = oklab(b); return 100 * Math.hypot(p[0] - q[0], p[1] - q[1], p[2] - q[2]); };
const stages = { '0 white': '#ffffff', '1 cyan-300': '#53eafd', '2 lime-400': '#9de504', '3 rose quartz': '#fda5d5',
  '4 amethyst': '#aa4eff', '5 ruby': '#fe2657', '6 gold': '#ffd230', '7 black': '#18181b' };
const boards = { standard: '#0684c7', paw: '#fdf3d9' };
const cand = { 'emerald-300': '#5ee9b5', 'emerald-400': '#00d492', 'teal-300': '#46ecd5', 'teal-400': '#00d5be',
  'green-300': '#7bf1a8', 'green-400': '#05df72', 'violet-300': '#c4b4ff', 'violet-400': '#a684ff',
  'indigo-300': '#a3b3ff', 'indigo-400': '#7c86ff', 'fuchsia-300': '#f4a8ff', 'fuchsia-400': '#ed6aff',
  'purple-300': '#dab2ff', 'purple-400': '#c27aff', 'orange-300': '#ffb86a', 'orange-400': '#ff8904',
  'red-300': '#ffa2a2', 'red-400': '#ff6467', 'lime-300': '#bbf451', 'sky-300': '#77d4ff', 'blue-300': '#8ec5ff' };
const rows = [];
for (const [n, h] of Object.entries(cand)) {
  const [L, a, b] = oklab(h);
  const hue = (Math.atan2(b, a) * 180 / Math.PI + 360) % 360;
  const brown = hue >= 30 && hue <= 115 && L < 0.65;
  let near = 1e9, who = '';
  for (const [s, c] of Object.entries(stages)) { const d = dE(h, c); if (d < near) { near = d; who = s; } }
  const db = Math.min(dE(h, boards.standard), dE(h, boards.paw));
  rows.push({ n, h, near: near.toFixed(1), who, outline: contrast(h, '#09090b').toFixed(2), board: db.toFixed(1), hue: hue.toFixed(0), brown });
}
rows.sort((x, y) => y.near - x.near);
for (const r of rows) console.log(`${r.n.padEnd(12)} ${r.h}  nearest stage ${r.near.padStart(5)} (${r.who})  outline ${r.outline}  boards ${r.board}  hue ${r.hue}${r.brown ? ' BROWN' : ''}`);
