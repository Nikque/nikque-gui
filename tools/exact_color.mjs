// Usage: download theme.css of Tailwind CSS v4 (packages/tailwindcss/theme.css) next to this script, then
// "node exact_color.mjs".
// Candidates for the color of exact values, from the Tailwind v4 palette (theme.css, OKLCH), checked against
// the reasons of the colors turned down before: low contrast with the board, pale or dull, green (emerald was
// turned down), orange, brown, the hue of the board (blue / pink), too near a stage color (dE < 15).
import fs from 'fs';
const css = fs.readFileSync('theme.css', 'utf8');
const re = /--color-([a-z]+)-(\d+):\s*oklch\(([\d.]+)%\s+([\d.]+)\s+([\d.]+)\)/g;
const toLin = (L, C, H) => {
  const a = C * Math.cos(H * Math.PI / 180), b = C * Math.sin(H * Math.PI / 180);
  const l = (L + 0.3963377774 * a + 0.2158037573 * b) ** 3, m = (L - 0.1055613458 * a - 0.0638541728 * b) ** 3,
        s = (L - 0.0894841775 * a - 1.291485548 * b) ** 3;
  return [4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s, -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s,
          -0.0041960863 * l - 0.7034186147 * m + 1.707614701 * s].map(v => Math.min(1, Math.max(0, v)));
};
const enc = v => Math.round(255 * (v <= 0.0031308 ? 12.92 * v : 1.055 * v ** (1 / 2.4) - 0.055));
const hex = lin => '#' + lin.map(enc).map(x => x.toString(16).padStart(2, '0')).join('');
const dec = c => { c /= 255; return c <= 0.04045 ? c / 12.92 : ((c + 0.055) / 1.055) ** 2.4; };
const linOf = h => [1, 3, 5].map(i => dec(parseInt(h.slice(i, i + 2), 16)));
const lab = h => { const [r, g, b] = linOf(h);
  const l = Math.cbrt(0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b), m = Math.cbrt(0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b),
        s = Math.cbrt(0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b);
  return [0.2104542553 * l + 0.793617785 * m - 0.0040720468 * s, 1.9779984951 * l - 2.428592205 * m + 0.4505937099 * s, 0.0259040371 * l + 0.7827717662 * m - 0.808675766 * s]; };
const Y = h => { const [r, g, b] = linOf(h); return 0.2126 * r + 0.7152 * g + 0.0722 * b; };
const cr = (a, b) => { const x = Y(a), y = Y(b); return (Math.max(x, y) + 0.05) / (Math.min(x, y) + 0.05); };
const dE = (a, b) => { const p = lab(a), q = lab(b); return 100 * Math.hypot(p[0] - q[0], p[1] - q[1], p[2] - q[2]); };
const stages = { white: '#ffffff', 'cyan-300': '#53eafd', 'lime-400': '#9de504', 'rose quartz': '#fda5d5',
  amethyst: '#aa4eff', ruby: '#fe2657', gold: '#ffd230', black: '#18181b' };
const out = [];
for (const m of css.matchAll(re)) {
  const [, name, step, L, C, H] = m;
  if (!['200', '300', '400', '500'].includes(step)) continue;
  const h = hex(toLin(L / 100, +C, +H));
  const why = [];
  if (+C < 0.17) why.push('pale');
  if (['green', 'emerald', 'teal'].includes(name)) why.push('green');
  if (name === 'orange') why.push('orange');
  if ((+H >= 30 && +H <= 115) && L / 100 < 0.65) why.push('brown');
  if (['sky', 'blue', 'indigo'].includes(name)) why.push('board hue (blue)');
  if (['pink'].includes(name)) why.push('board hue (pink)');
  let near = 1e9, who = '';
  for (const [s, c] of Object.entries(stages)) { const d = dE(h, c); if (d < near) { near = d; who = s; } }
  if (near < 15) why.push(`near ${who} ${near.toFixed(1)}`);
  const bs = cr(h, '#0684c7'), bp = cr(h, '#fdf3d9');
  if (bs < 2.3) why.push(`std board ${bs.toFixed(2)}`);
  if (cr(h, '#09090b') < 4.5) why.push('outline');
  out.push({ n: `${name}-${step}`, h, near: near.toFixed(1), who, bs: bs.toFixed(2), bp: bp.toFixed(2), why });
}
out.sort((a, b) => a.why.length - b.why.length || b.near - a.near);
for (const r of out) console.log(`${r.n.padEnd(11)} ${r.h} nearest ${r.near.padStart(5)} (${r.who.padEnd(11)}) std ${r.bs} paw ${r.bp}  ${r.why.length ? 'NG: ' + r.why.join(', ') : 'OK'}`);
