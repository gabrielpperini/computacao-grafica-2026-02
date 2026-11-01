#!/usr/bin/env python3
"""Gera as malhas procedurais do Rail Rush e o atlas de textura que elas usam.

Uso (na raiz do repositório):   python tools/gen_models.py
Saída: data/procedural/*.obj e data/procedural/atlas.png (sobrescreve).
(Os modelos do túnel usam texturas do Poly Haven em vez do atlas: a abóbada,
data/castle_wall_slates_diff_1k.jpg, e o portal, data/red_brick_diff_1k.jpg; as
paredes dos prédios de tijolo, data/red_brick_diff_1k.jpg.)

Só biblioteca padrão (math, struct, zlib). Tudo é determinístico: rodar de novo
gera exatamente os mesmos arquivos.

Convenções dos modelos (as mesmas que main.cpp espera):
  - metros, +Y para cima, base apoiada em y = 0, centrados em x = z = 0;
  - "frente" voltada para +Z (o lado que o jogador vê ao se aproximar);
  - todo objeto tem nome ("o <nome>"), normais (vn) e coordenadas de textura (vt);
  - triângulos em sentido anti-horário vistos de fora (há backface culling).
Os obstáculos são modelados exatamente do tamanho da caixa de colisão de
include/game.h (OBSTACLE_SIZE), então a escala aplicada no jogo é ~1.

As coordenadas de textura apontam para regiões de data/procedural/atlas.png:
faixas listradas, o rosto da moeda, o emblema "x2", a placa de "proibido
passar" e quadrados de cor lisa (swatches).

Licença dos arquivos gerados: CC0 (criados para este trabalho).
"""

import math
import os
import struct
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_DIR = os.path.join(ROOT, "data", "procedural")

# ---------------------------------------------------------------------------
# Atlas de textura (512 x 512, RGB)
# ---------------------------------------------------------------------------

ATLAS = 512
PPM = 64.0  # pixels do atlas por metro nas faces com padrão (listras não esticam)

# Regiões (x0, y0, x1, y1) em pixels, origem no canto superior esquerdo da imagem.
REGION_STRIPES = (0, 0, 256, 256)      # listras vermelho/branco a 45 graus
REGION_HAZARD = (256, 0, 512, 128)     # listras amarelo/preto a 45 graus
REGION_COIN = (256, 128, 384, 256)     # face da moeda (disco dourado com estrela)
REGION_X2 = (384, 128, 512, 256)       # emblema do multiplicador "x2"
REGION_SIGN = (0, 256, 160, 448)       # placa "proibido passar" (2.0 x 2.4 m)
REGION_CONCRETE = (0, 448, 160, 512)   # concreto com ruído
SWATCH_X0, SWATCH_Y0, SWATCH = 160, 256, 32  # quadrados de cor lisa, 11 por linha

SWATCH_COLORS = [
    ("white", (240, 240, 236)), ("red", (214, 44, 40)), ("dark_red", (150, 28, 26)),
    ("orange", (246, 142, 32)), ("yellow", (252, 206, 44)), ("gold", (244, 184, 44)),
    ("dark_gold", (196, 132, 26)), ("silver", (205, 210, 220)), ("steel", (118, 124, 136)),
    ("dark", (46, 46, 52)), ("black", (18, 18, 20)), ("pink", (246, 158, 176)),
    ("dark_pink", (214, 100, 128)), ("light_pink", (252, 204, 214)), ("blue", (48, 110, 214)),
    ("light_blue", (150, 196, 244)), ("green", (60, 172, 84)), ("purple", (132, 78, 204)),
    ("light_purple", (196, 160, 240)), ("brown", (122, 80, 44)), ("concrete", (168, 168, 162)),
    ("glow", (255, 232, 150)), ("navy", (36, 52, 110)),
]
SWATCH_INDEX = {name: i for i, (name, _) in enumerate(SWATCH_COLORS)}


def swatch_uv(name):
    """UV do centro do quadrado de cor 'name' (v cresce para cima, como no OBJ)."""
    i = SWATCH_INDEX[name]
    col, row = i % 11, i // 11
    px = SWATCH_X0 + col * SWATCH + SWATCH * 0.5
    py = SWATCH_Y0 + row * SWATCH + SWATCH * 0.5
    return (px / ATLAS, 1.0 - py / ATLAS)


def region_uv(region, fx, fy):
    """UV de um ponto da região: fx em [0,1] da esquerda para a direita, fy em
    [0,1] de baixo para cima."""
    x0, y0, x1, y1 = region
    px = x0 + fx * (x1 - x0)
    py = y1 - fy * (y1 - y0)
    return (px / ATLAS, 1.0 - py / ATLAS)


def region_uv_metric(region, x_m, y_m):
    """UV de um ponto (em metros a partir do canto inferior esquerdo da região)
    com densidade fixa PPM: o padrão tem o mesmo tamanho em qualquer face."""
    x0, y0, x1, y1 = region
    return region_uv(region, x_m * PPM / (x1 - x0), y_m * PPM / (y1 - y0))


def lerp_color(a, b, t):
    return tuple(a[i] + (b[i] - a[i]) * t for i in range(3))


# Glifos 5x7 para o emblema "x2" ('#' = pixel aceso).
GLYPHS = {
    "x": ["     ", "     ", "#   #", " # # ", "  #  ", " # # ", "#   #"],
    "2": [" ### ", "#   #", "    #", "   # ", "  #  ", " #   ", "#####"],
}


def star_inside(x, y, r_outer, r_inner, points=5):
    """Ponto (x, y) dentro de uma estrela centrada na origem, ponta para cima."""
    ang = math.atan2(x, y)  # 0 = para cima
    r = math.hypot(x, y)
    sector = 2.0 * math.pi / points
    a = (ang % sector) / sector  # 0..1 dentro do setor
    t = abs(a - 0.5) * 2.0       # 1 nas pontas, 0 entre pontas
    # Raio da borda interpolado linearmente entre ponta e vale.
    edge = r_inner + (r_outer - r_inner) * t
    return r <= edge


def atlas_color(px, py):
    """Cor (r, g, b) em [0,255] do ponto (px, py) do atlas (floats, subpixel)."""
    def inside(region):
        return region[0] <= px < region[2] and region[1] <= py < region[3]

    if inside(REGION_STRIPES):
        return (214, 44, 40) if ((px + py) % 64.0) < 32.0 else (244, 244, 240)
    if inside(REGION_HAZARD):
        return (252, 200, 30) if ((px - py) % 48.0) < 24.0 else (34, 34, 38)
    if inside(REGION_COIN):
        cx, cy, R = 320.0, 192.0, 64.0
        x, y = px - cx, -(py - cy)
        r = math.hypot(x, y) / R
        if r > 0.88:
            return (214, 150, 30)
        if r > 0.80:
            return (178, 118, 22)                # sulco do aro interno
        if star_inside(x / R, y / R, 0.55, 0.24):
            return lerp_color((255, 238, 140), (250, 210, 70), r / 0.55)
        return lerp_color((250, 200, 60), (226, 160, 34), r / 0.8)
    if inside(REGION_X2):
        cx, cy, R = 448.0, 192.0, 64.0
        x, y = px - cx, py - cy
        r = math.hypot(x, y) / R
        if r > 0.9:
            return (90, 50, 160)
        if r > 0.82:
            return (196, 160, 240)
        base = lerp_color((150, 96, 226), (110, 62, 186), r / 0.82)
        # Texto "x2" centralizado: 2 glifos 5x7 com 1 coluna de espaço, célula de 8 px.
        cell = 8.0
        text_w, text_h = 11 * cell, 7 * cell
        gx = (x + text_w / 2) / cell
        gy = (y + text_h / 2) / cell
        if 0 <= gx < 11 and 0 <= gy < 7:
            col, row = int(gx), int(gy)
            if col < 5 and GLYPHS["x"][row][col] == "#":
                return (255, 255, 255)
            if col >= 6 and GLYPHS["2"][row][col - 6] == "#":
                return (255, 255, 255)
        return base
    if inside(REGION_SIGN):
        x0, y0, x1, y1 = REGION_SIGN
        lx, ly = px - x0, py - y0
        w, h = x1 - x0, y1 - y0
        if lx < 10 or ly < 10 or lx > w - 10 or ly > h - 10:
            return (214, 44, 40)                  # moldura vermelha
        if lx < 14 or ly < 14 or lx > w - 14 or ly > h - 14:
            return (244, 244, 240)
        cx, cy = w / 2.0, h / 2.0
        r = math.hypot(lx - cx, ly - cy)
        if r < 62:
            if abs(ly - cy) < 12 and abs(lx - cx) < 44:
                return (250, 250, 250)            # barra branca do "proibido"
            return (214, 44, 40) if r < 58 else (120, 20, 20)
        return (244, 244, 240)
    if inside(REGION_CONCRETE):
        # Ruído determinístico (hash inteiro) em blocos de 2 px.
        ix, iy = int(px) // 2, int(py) // 2
        h = (ix * 73856093) ^ (iy * 19349663)
        h = (h ^ (h >> 13)) * 1274126177 & 0xFFFFFFFF
        n = (h & 0xFF) / 255.0
        g = 150 + 30 * n
        return (g, g, g * 0.97)
    # Swatches de cor lisa.
    if px >= SWATCH_X0 and py >= SWATCH_Y0:
        col = int((px - SWATCH_X0) // SWATCH)
        row = int((py - SWATCH_Y0) // SWATCH)
        i = row * 11 + col
        if col < 11 and i < len(SWATCH_COLORS):
            return SWATCH_COLORS[i][1]
    return (128, 128, 128)


def build_atlas():
    rows = []
    sub = [(0.25, 0.25), (0.75, 0.25), (0.25, 0.75), (0.75, 0.75)]  # 4x supersampling
    for y in range(ATLAS):
        row = bytearray([0])  # filtro PNG "None"
        for x in range(ATLAS):
            acc = [0.0, 0.0, 0.0]
            for sx, sy in sub:
                c = atlas_color(x + sx, y + sy)
                acc[0] += c[0]
                acc[1] += c[1]
                acc[2] += c[2]
            row.extend(int(max(0, min(255, round(a / 4.0)))) for a in acc)
        rows.append(bytes(row))
    return b"".join(rows)


def write_png(path, width, height, raw_rgb_rows):
    def chunk(tag, data):
        body = tag + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw_rgb_rows, 9))
    png += chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)


# ---------------------------------------------------------------------------
# Malhas
# ---------------------------------------------------------------------------

def v_add(a, b):
    return (a[0] + b[0], a[1] + b[1], a[2] + b[2])


def v_sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def v_scale(a, s):
    return (a[0] * s, a[1] * s, a[2] * s)


def v_dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def v_cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def v_norm(a):
    length = math.sqrt(v_dot(a, a))
    if length < 1e-12:
        return (0.0, 1.0, 0.0)
    return (a[0] / length, a[1] / length, a[2] / length)


def rot_x(p, ang):
    c, s = math.cos(ang), math.sin(ang)
    return (p[0], p[1] * c - p[2] * s, p[1] * s + p[2] * c)


def rot_y(p, ang):
    c, s = math.cos(ang), math.sin(ang)
    return (p[0] * c + p[2] * s, p[1], -p[0] * s + p[2] * c)


def rot_z(p, ang):
    c, s = math.cos(ang), math.sin(ang)
    return (p[0] * c - p[1] * s, p[0] * s + p[1] * c, p[2])


class Mesh:
    """Um objeto nomeado do OBJ: vértices com posição, normal e UV próprios."""

    def __init__(self, name):
        self.name = name
        self.pos = []
        self.nrm = []
        self.uv = []
        self.tris = []

    def vertex(self, p, n, uv):
        self.pos.append(p)
        self.nrm.append(v_norm(n))
        self.uv.append(uv)
        return len(self.pos) - 1

    def tri(self, a, b, c):
        # Garante sentido anti-horário visto de fora: a normal geométrica tem
        # de concordar com a média das normais dos vértices.
        pa, pb, pc = self.pos[a], self.pos[b], self.pos[c]
        geo = v_cross(v_sub(pb, pa), v_sub(pc, pa))
        avg = v_add(v_add(self.nrm[a], self.nrm[b]), self.nrm[c])
        if v_dot(geo, geo) < 1e-16:
            return  # triângulo degenerado (polos)
        if v_dot(geo, avg) < 0.0:
            b, c = c, b
        self.tris.append((a, b, c))

    def quad(self, a, b, c, d):
        self.tri(a, b, c)
        self.tri(a, c, d)

    def transform(self, fn_point, fn_normal=None):
        """Aplica fn_point às posições (e fn_normal, ou a parte linear, às normais)."""
        fn_normal = fn_normal or fn_point
        self.pos = [fn_point(p) for p in self.pos]
        self.nrm = [v_norm(fn_normal(n)) for n in self.nrm]
        return self

    def append(self, other):
        base = len(self.pos)
        self.pos += other.pos
        self.nrm += other.nrm
        self.uv += other.uv
        self.tris += [(a + base, b + base, c + base) for (a, b, c) in other.tris]
        return self

    def bbox(self):
        mn = tuple(min(p[i] for p in self.pos) for i in range(3))
        mx = tuple(max(p[i] for p in self.pos) for i in range(3))
        return mn, mx


def box(name, mn, mx, uv_face=None, color="white"):
    """Caixa alinhada aos eixos com normais planas. uv_face(face, u_m, v_m, w_m,
    h_m) -> uv permite mapear um padrão por face (face em 'px','nx','py','ny',
    'pz','nz'; u_m/v_m em metros a partir do canto inferior esquerdo da face
    vista de fora); por padrão, cor lisa 'color'."""
    m = Mesh(name)
    x0, y0, z0 = mn
    x1, y1, z1 = mx
    flat = swatch_uv(color)
    # (normal, origem, eixo u, eixo v, largura, altura) vistos de fora.
    faces = [
        ("pz", (0, 0, 1), (x0, y0, z1), (1, 0, 0), (0, 1, 0), x1 - x0, y1 - y0),
        ("nz", (0, 0, -1), (x1, y0, z0), (-1, 0, 0), (0, 1, 0), x1 - x0, y1 - y0),
        ("px", (1, 0, 0), (x1, y0, z1), (0, 0, -1), (0, 1, 0), z1 - z0, y1 - y0),
        ("nx", (-1, 0, 0), (x0, y0, z0), (0, 0, 1), (0, 1, 0), z1 - z0, y1 - y0),
        ("py", (0, 1, 0), (x0, y1, z1), (1, 0, 0), (0, 0, -1), x1 - x0, z1 - z0),
        ("ny", (0, -1, 0), (x0, y0, z0), (1, 0, 0), (0, 0, 1), x1 - x0, z1 - z0),
    ]
    for face, n, o, eu, ev, w, h in faces:
        ids = []
        for (fu, fv) in ((0, 0), (1, 0), (1, 1), (0, 1)):
            p = v_add(o, v_add(v_scale(eu, fu * w), v_scale(ev, fv * h)))
            uv = uv_face(face, fu * w, fv * h, w, h) if uv_face else None
            ids.append(m.vertex(p, n, uv if uv is not None else flat))
        m.quad(*ids)
    return m


def parametric(name, fn, nu, nv, color, normal_fn=None, closed_u=True, uv_fn=None):
    """Superfície paramétrica fn(u, v) com u em [0,1) (volta) e v em [0,1]
    (polo a polo). Normais pela derivada numérica (ou normal_fn)."""
    m = Mesh(name)
    flat = swatch_uv(color)
    eps = 1e-4
    grid = []
    for j in range(nv + 1):
        v = j / nv
        row = []
        for i in range(nu + (0 if closed_u else 1)):
            u = i / nu
            p = fn(u, v)
            if normal_fn:
                n = normal_fn(u, v, p)
            else:
                du = v_sub(fn(u + eps, v), fn(u - eps, v))
                dv = v_sub(fn(u, min(1.0, v + eps)), fn(u, max(0.0, v - eps)))
                n = v_cross(du, dv)
                if v_dot(n, n) < 1e-14:
                    n = v_sub(p, fn(u, 0.5))  # polo: aponta para fora
            uv = uv_fn(u, v) if uv_fn else flat
            row.append(m.vertex(p, n, uv))
        grid.append(row)
    cols = nu if closed_u else nu + 1
    for j in range(nv):
        for i in range(nu if closed_u else nu):
            i1 = (i + 1) % cols
            m.quad(grid[j][i], grid[j][i1], grid[j + 1][i1], grid[j + 1][i])
    return m


def ellipsoid(name, center, radii, color, nu=28, nv=16):
    cx, cy, cz = center
    rx, ry, rz = radii

    def fn(u, v):
        th = 2 * math.pi * u
        ph = math.pi * v - math.pi / 2
        return (cx + rx * math.cos(ph) * math.sin(th), cy + ry * math.sin(ph), cz + rz * math.cos(ph) * math.cos(th))

    def nf(u, v, p):
        return ((p[0] - cx) / (rx * rx), (p[1] - cy) / (ry * ry), (p[2] - cz) / (rz * rz))

    return parametric(name, fn, nu, nv, color, normal_fn=nf)


def superellipsoid(name, center, radii, exponent, color, nu=32, nv=18):
    """Caixa arredondada: |x/a|^e + |y/b|^e + |z/c|^e = 1 (e > 2 = mais quadrada)."""
    cx, cy, cz = center
    rx, ry, rz = radii
    p_exp = 2.0 / exponent

    def spow(x, e):
        return math.copysign(abs(x) ** e, x)

    def fn(u, v):
        th = 2 * math.pi * u
        ph = math.pi * min(1.0, max(0.0, v)) - math.pi / 2
        cp, sp = math.cos(ph), math.sin(ph)
        return (cx + rx * spow(cp, p_exp) * spow(math.sin(th), p_exp),
                cy + ry * spow(sp, p_exp),
                cz + rz * spow(cp, p_exp) * spow(math.cos(th), p_exp))

    def nf(u, v, p):
        # Gradiente da função implícita.
        x, y, z = (p[0] - cx) / rx, (p[1] - cy) / ry, (p[2] - cz) / rz
        e = exponent
        g = (math.copysign(abs(x) ** (e - 1), x) / rx, math.copysign(abs(y) ** (e - 1), y) / ry,
             math.copysign(abs(z) ** (e - 1), z) / rz)
        if v_dot(g, g) < 1e-12:
            return (0.0, 1.0 if v > 0.5 else -1.0, 0.0)
        return g

    return parametric(name, fn, nu, nv, color, normal_fn=nf)


def cylinder(name, base, radius, height, color, segments=24, axis="y", caps=True,
             cap_uv=None, side_color=None):
    """Cilindro ao longo de +axis a partir de 'base'. cap_uv(x, y) -> uv mapeia
    as tampas (x, y em [-1,1] no plano da tampa); senão cor lisa."""
    m = Mesh(name)
    flat = swatch_uv(color)
    side_flat = swatch_uv(side_color or color)

    def place(lx, ly, lz):
        # Sistema local: (lx, lz) no plano da tampa, ly ao longo do eixo.
        if axis == "y":
            return v_add(base, (lx, ly, lz))
        if axis == "z":
            return v_add(base, (lx, lz, ly))
        return v_add(base, (ly, lx, lz))  # axis == "x"

    def dir_(lx, ly, lz):
        if axis == "y":
            return (lx, ly, lz)
        if axis == "z":
            return (lx, lz, ly)
        return (ly, lx, lz)

    ring0, ring1 = [], []
    for i in range(segments):
        a = 2 * math.pi * i / segments
        c, s = math.cos(a), math.sin(a)
        n = dir_(c, 0.0, s)
        ring0.append(m.vertex(place(radius * c, 0.0, radius * s), n, side_flat))
        ring1.append(m.vertex(place(radius * c, height, radius * s), n, side_flat))
    for i in range(segments):
        j = (i + 1) % segments
        m.quad(ring0[i], ring0[j], ring1[j], ring1[i])
    if caps:
        for (h, sign) in ((0.0, -1.0), (height, 1.0)):
            n = dir_(0.0, sign, 0.0)
            center = m.vertex(place(0.0, h, 0.0), n, cap_uv(0.0, 0.0) if cap_uv else flat)
            ids = []
            for i in range(segments):
                a = 2 * math.pi * i / segments
                c, s = math.cos(a), math.sin(a)
                uv = cap_uv(c, s * (1 if sign > 0 else -1)) if cap_uv else flat
                ids.append(m.vertex(place(radius * c, h, radius * s), n, uv))
            for i in range(segments):
                m.tri(center, ids[i], ids[(i + 1) % segments])
    return m


def disc_token(name, radius, thickness, rim, recess, face_region, rim_color, segments=48):
    """Ficha/moeda em pé (eixo Z), com aro saliente e face rebaixada que recebe
    'face_region' do atlas (mapeamento planar, círculo inscrito na região).
    Base em y = 0."""
    m = Mesh(name)
    rim_uv = swatch_uv(rim_color)
    ri = radius - rim
    zt = thickness / 2.0          # aro: z em [-zt, zt]
    zf = zt - recess              # face rebaixada
    cy = radius                   # centro do disco (base em y = 0)

    def ring(r, z, n_fn, uv_fn):
        ids = []
        for i in range(segments):
            a = 2 * math.pi * i / segments
            c, s = math.cos(a), math.sin(a)
            ids.append(m.vertex((r * c, cy + r * s, z), n_fn(c, s), uv_fn(c, s, r)))
        return ids

    def strip(r0, r1):
        for i in range(segments):
            j = (i + 1) % segments
            m.quad(r0[i], r0[j], r1[j], r1[i])

    def face_uv(c, s, r):
        return region_uv(face_region, 0.5 + 0.5 * c * r / ri, 0.5 + 0.5 * s * r / ri)

    for sign in (1.0, -1.0):
        z_rim, z_face = sign * zt, sign * zf
        nz = (0.0, 0.0, sign)
        # Topo do aro (anel plano entre ri e radius).
        a = ring(radius, z_rim, lambda c, s: nz, lambda c, s, r: rim_uv)
        b = ring(ri, z_rim, lambda c, s: nz, lambda c, s, r: rim_uv)
        strip(a, b)
        # Parede interna do aro (desce até a face), normal para dentro.
        c1 = ring(ri, z_rim, lambda c, s: (-c, -s, 0.0), lambda c, s, r: rim_uv)
        c2 = ring(ri, z_face, lambda c, s: (-c, -s, 0.0), lambda c, s, r: rim_uv)
        strip(c1, c2)
        # Face (leque) com a imagem.
        if sign > 0:
            fuv = face_uv
        else:
            # Verso visto de trás: espelha U para a imagem não ficar invertida.
            def fuv(c, s, r):
                return region_uv(face_region, 0.5 - 0.5 * c * r / ri, 0.5 + 0.5 * s * r / ri)
        f = ring(ri, z_face, lambda c, s: nz, fuv)
        center = m.vertex((0.0, cy, z_face), nz, region_uv(face_region, 0.5, 0.5))
        for i in range(segments):
            m.tri(center, f[i], f[(i + 1) % segments])
    # Borda externa (normais suaves em volta: realce contínuo no Gouraud).
    e0 = ring(radius, zt, lambda c, s: (c, s, 0.0), lambda c, s, r: rim_uv)
    e1 = ring(radius, -zt, lambda c, s: (c, s, 0.0), lambda c, s, r: rim_uv)
    strip(e0, e1)
    return m


def torus_arc(name, center, major, minor, a0, a1, color, nu=24, nv=14, plane="xy"):
    """Arco de toro de a0 a a1 (rad) no plano 'xy' em volta de 'center'."""
    cx, cy, cz = center

    def fn(u, v):
        a = a0 + (a1 - a0) * u
        b = 2 * math.pi * v
        r = major + minor * math.cos(b)
        return (cx + r * math.cos(a), cy + r * math.sin(a), cz + minor * math.sin(b))

    def nf(u, v, p):
        a = a0 + (a1 - a0) * u
        b = 2 * math.pi * v
        return (math.cos(b) * math.cos(a), math.cos(b) * math.sin(a), math.sin(b))

    return parametric(name, fn, nu, nv, color, normal_fn=nf, closed_u=False)


def merged(name, parts):
    m = Mesh(name)
    for p in parts:
        m.append(p)
    return m


# ---------------------------------------------------------------------------
# Modelos
# ---------------------------------------------------------------------------

def stripes_uv(region):
    """uv_face para box(): padrão da região nas faces ±Z, com densidade fixa."""
    def fn(face, u_m, v_m, w, h):
        if face in ("pz", "nz"):
            return region_uv_metric(region, u_m, v_m)
        return None
    return fn


def model_barrier_low():
    """Barreira baixa (2.0 x 0.9 x 0.3 m): tábua listrada sobre dois cavaletes,
    com pisca-alertas laranja no topo. Pula-se por cima."""
    parts = []
    for sx in (-1.0, 1.0):
        x = sx * 0.82
        parts.append(box("p", (x - 0.05, 0.0, -0.15), (x + 0.05, 0.06, 0.15), color="steel"))   # pé
        parts.append(box("p", (x - 0.04, 0.06, -0.04), (x + 0.04, 0.80, 0.04), color="silver"))  # poste
        parts.append(cylinder("p", (x, 0.80, 0.0), 0.05, 0.04, "dark", segments=16))
        parts.append(ellipsoid("p", (x, 0.86, 0.0), (0.05, 0.04, 0.05), "orange", nu=16, nv=8))
    parts.append(box("p", (-1.0, 0.40, -0.05), (1.0, 0.76, 0.05), stripes_uv(REGION_STRIPES)))
    parts.append(box("p", (-0.9, 0.16, -0.035), (0.9, 0.26, 0.035), stripes_uv(REGION_STRIPES)))
    return merged("barrier_low", parts)


def model_barrier_high():
    """Barreira alta (2.0 x 3.0 x 0.3 m): dois postes nas bordas (do chão ao
    topo) e painéis de y = 1.1 a 3.0. Vão livre 0-1.1 m: passa-se rolando.
    Os postes ocupam |x| em [0.88, 1.0] (BARRIER_POST_HALF_WIDTH = 0.06)."""
    parts = []
    for sx in (-1.0, 1.0):
        x = sx * 0.94
        parts.append(box("p", (x - 0.06, 0.0, -0.15), (x + 0.06, 0.06, 0.15), color="steel"))
        parts.append(box("p", (x - 0.06, 0.06, -0.06), (x + 0.06, 2.92, 0.06), color="silver"))
        parts.append(ellipsoid("p", (x, 2.96, 0.0), (0.06, 0.04, 0.06), "orange", nu=16, nv=8))
    # Painel listrado (embaixo) e placa de perigo (em cima).
    parts.append(box("p", (-0.88, 1.10, -0.05), (0.88, 1.85, 0.05), stripes_uv(REGION_STRIPES)))
    parts.append(box("p", (-0.88, 1.85, -0.03), (0.88, 1.97, 0.03), color="dark"))
    parts.append(box("p", (-0.88, 1.97, -0.05), (0.88, 2.85, 0.05), stripes_uv(REGION_HAZARD)))
    parts.append(box("p", (-0.88, 2.85, -0.06), (0.88, 2.92, 0.06), color="dark"))
    return merged("barrier_high", parts)


def model_sign():
    """Placa de bloqueio (2.0 x 3.0 x 0.3 m): placa de "proibido passar" de
    2.0 x 2.4 m sobre dois postes com sapatas de concreto. Só desviando."""
    parts = []

    def concrete_uv(face, u_m, v_m, w, h):
        return region_uv_metric(REGION_CONCRETE, u_m * 0.5, v_m * 0.5)

    for sx in (-1.0, 1.0):
        x = sx * 0.7
        parts.append(box("p", (x - 0.22, 0.0, -0.15), (x + 0.22, 0.35, 0.15), concrete_uv))
        parts.append(box("p", (x - 0.07, 0.35, -0.07), (x + 0.07, 0.62, 0.07), color="steel"))

    def sign_uv(face, u_m, v_m, w, h):
        if face == "pz":
            return region_uv(REGION_SIGN, u_m / w, v_m / h)
        if face == "nz":
            return swatch_uv("steel")
        return swatch_uv("dark")

    parts.append(box("p", (-1.0, 0.6, -0.06), (1.0, 3.0, 0.06), sign_uv))
    # Travessa de reforço atrás da placa.
    parts.append(box("p", (-0.9, 1.6, -0.14), (0.9, 1.75, -0.06), color="steel"))
    return merged("sign_board", parts)


def model_coin():
    """Moeda (diâmetro 0.8 m, espessura 0.12 m) em pé, eixo Z, aro saliente."""
    return disc_token("coin", 0.4, 0.12, 0.07, 0.025, REGION_COIN, "gold")


def model_multiplier():
    """Ficha "x2" (diâmetro 0.7 m)."""
    return disc_token("powerup_multiplier", 0.35, 0.14, 0.05, 0.02, REGION_X2, "purple")


def model_magnet():
    """Ímã em ferradura: arco vermelho, braços vermelhos e pontas prateadas."""
    major, minor = 0.24, 0.09
    cy = 0.50
    parts = [torus_arc("p", (0.0, cy, 0.0), major, minor, 0.0, math.pi, "red", nu=28, nv=16)]
    for sx in (-1.0, 1.0):
        x = sx * major
        parts.append(cylinder("p", (x, 0.17, 0.0), minor, cy - 0.17, "red", segments=16, caps=False))
        parts.append(cylinder("p", (x, 0.0, 0.0), minor, 0.17, "silver", segments=16))
    return merged("powerup_magnet", parts)


def model_sneaker():
    """Tênis (comprimento 0.75 m, bico em +Z): solado branco, cabedal vermelho
    com biqueira arredondada, cadarços brancos e gola escura."""
    parts = [
        superellipsoid("p", (0.0, 0.05, 0.0), (0.15, 0.05, 0.375), 4.0, "white"),          # solado
        superellipsoid("p", (0.0, 0.20, -0.09), (0.135, 0.15, 0.25), 3.0, "red"),          # cabedal
        ellipsoid("p", (0.0, 0.11, 0.17), (0.13, 0.10, 0.19), "red"),                      # biqueira
        ellipsoid("p", (0.0, 0.07, 0.30), (0.12, 0.05, 0.07), "white", nu=20, nv=10),      # ponteira
        cylinder("p", (0.0, 0.33, -0.17), 0.10, 0.04, "dark", segments=20),               # gola
    ]
    for k in range(3):
        z = 0.02 + 0.07 * k
        y = 0.27 - 0.045 * k
        parts.append(box("p", (-0.09, y, z - 0.012), (0.09, y + 0.03, z + 0.012), color="white"))
    # Faixa lateral azul.
    parts.append(box("p", (-0.142, 0.12, -0.22), (0.142, 0.17, 0.02), color="blue"))
    return merged("powerup_sneaker", parts)


def pig_parts(with_slot):
    """Partes do porco (corpo, focinho, orelhas, olhos, patas, rabo), cabeça em +Z."""
    parts = [
        ellipsoid("p", (0.0, 0.34, 0.0), (0.28, 0.25, 0.36), "pink", nu=32, nv=18),       # corpo
        cylinder("p", (0.0, 0.36, 0.33), 0.10, 0.09, "dark_pink", segments=20, axis="z"),  # focinho
        ellipsoid("p", (-0.035, 0.37, 0.425), (0.018, 0.028, 0.01), "dark", nu=10, nv=6),  # narinas
        ellipsoid("p", (0.035, 0.37, 0.425), (0.018, 0.028, 0.01), "dark", nu=10, nv=6),
        ellipsoid("p", (-0.11, 0.45, 0.28), (0.035, 0.04, 0.025), "black", nu=12, nv=8),   # olhos
        ellipsoid("p", (0.11, 0.45, 0.28), (0.035, 0.04, 0.025), "black", nu=12, nv=8),
        ellipsoid("p", (0.0, 0.40, -0.36), (0.04, 0.04, 0.04), "dark_pink", nu=12, nv=8),  # rabo
    ]
    for sx in (-1.0, 1.0):
        # Orelhas: elipsoides achatados inclinados para a frente.
        ear = ellipsoid("p", (0.0, 0.0, 0.0), (0.07, 0.09, 0.025), "dark_pink", nu=14, nv=8)
        ear.transform(lambda p, sx=sx: v_add(rot_z(rot_x(p, 0.5), -0.35 * sx), (sx * 0.15, 0.56, 0.16)),
                      lambda n, sx=sx: rot_z(rot_x(n, 0.5), -0.35 * sx))
        parts.append(ear)
        for sz in (-1.0, 1.0):
            parts.append(cylinder("p", (sx * 0.15, 0.0, sz * 0.18), 0.065, 0.16, "dark_pink", segments=14))
    if with_slot:
        parts.append(box("p", (-0.10, 0.575, -0.03), (0.10, 0.595, 0.03), color="black"))
    return parts


def model_piggy_bank():
    """Cofrinho (power-up): porco rosa com fenda de moeda no topo."""
    return merged("piggy_bank", pig_parts(True))


def model_flying_pig():
    """Porco voador: corpo e duas asas como objetos separados (para bater as
    asas em torno da raiz, em x = ±0.2, numa tarefa futura)."""
    body = merged("flying_pig_body", pig_parts(False))
    wings = []
    for sx, name in ((-1.0, "flying_pig_wing_left"), (1.0, "flying_pig_wing_right")):
        # Asa com a raiz em x = 0 e a ponta em x = sx*0.6 (sem espelhar a malha,
        # o que inverteria o sentido dos triângulos).
        w = merged(name, [
            ellipsoid("p", (sx * 0.30, 0.0, 0.0), (0.30, 0.025, 0.14), "white", nu=24, nv=10),
            ellipsoid("p", (sx * 0.38, 0.012, -0.06), (0.20, 0.03, 0.08), "light_blue", nu=20, nv=8),
        ])
        # Raiz nas costas, ponta para fora e um pouco para cima.
        w.transform(lambda p, sx=sx: v_add(rot_z(p, 0.35 * sx), (sx * 0.16, 0.50, 0.02)),
                    lambda n, sx=sx: rot_z(n, 0.35 * sx))
        wings.append(w)
    return [body] + wings


# ---------------------------------------------------------------------------
# Túnel (Tarefa 4.2): módulo de abóbada e portal. Medidas IGUAIS às de
# include/scenery.h (TUNNEL_*): alterar lá e aqui juntos. Estes modelos usam a
# textura de tijolos (red_brick_diff_1k.jpg, Poly Haven), não o atlas: as UV
# estão em metros / TUNNEL_TILE (o padrão nunca estica e se repete sem emenda
# entre módulos vizinhos).
# ---------------------------------------------------------------------------

TUNNEL_HALF_WIDTH = 5.0         # TUNNEL_INNER_HALF_WIDTH (raio da abóbada)
TUNNEL_SPRING = 2.0             # TUNNEL_SPRING_HEIGHT (altura das paredes retas)
TUNNEL_THICK = 0.6              # TUNNEL_WALL_THICKNESS
TUNNEL_SECTION = 4.0            # TUNNEL_SECTION_LENGTH (= TRACK_TUNNEL_SECTION)
TUNNEL_PORTAL_HALF_WIDTH = 7.5  # TUNNEL_PORTAL_HALF_WIDTH
TUNNEL_PORTAL_HEIGHT = 9.0      # TUNNEL_PORTAL_HEIGHT
TUNNEL_PORTAL_DEPTH = 1.0       # TUNNEL_PORTAL_DEPTH
TUNNEL_TILE = 2.0               # m por repetição da textura (pedra na abóbada, tijolo no portal)
TUNNEL_ARC_SEGMENTS = 24        # segmentos da abóbada (meia-volta)
ARCHIVOLT_WIDTH = 0.7           # m: moldura em volta da abertura do portal
ARCHIVOLT_DEPTH = 0.15          # m: quanto a moldura avança à frente da fachada


def tunnel_profile(r, extra_angles=()):
    """Perfil da seção (de baixo à direita, subindo, passando pelo topo e
    descendo à esquerda) com raio r: paredes retas em x = ±r de y = 0 a
    TUNNEL_SPRING e o semicírculo de centro (0, TUNNEL_SPRING). Devolve
    [(x, y, nx, ny, comprimento acumulado, ângulo)], normal para FORA do eixo.
    O ângulo é None nos pés das paredes."""
    angles = [math.pi * i / TUNNEL_ARC_SEGMENTS for i in range(TUNNEL_ARC_SEGMENTS + 1)]
    angles = sorted(set(angles) | set(extra_angles))
    pts = [(r, 0.0, 1.0, 0.0, None)]
    for a in angles:
        c, s = math.cos(a), math.sin(a)
        pts.append((r * c, TUNNEL_SPRING + r * s, c, s, a))
    pts.append((-r, 0.0, -1.0, 0.0, None))
    out = []
    length = 0.0
    for i, (x, y, nx, ny, a) in enumerate(pts):
        if i > 0:
            length += math.hypot(x - pts[i - 1][0], y - pts[i - 1][1])
        out.append((x, y, nx, ny, length, a))
    return out


def tunnel_shell(m, r, z0, z1, inward):
    """Superfície do perfil de raio r entre z0 e z1. inward: normais para o
    eixo (vista de dentro). UV: U ao longo do túnel (as fiadas de tijolo
    acompanham o comprimento), V ao longo do perfil, em metros/TUNNEL_TILE."""
    prof = tunnel_profile(r)
    sign = -1.0 if inward else 1.0
    rows = []
    for z in (z0, z1):
        row = []
        for (x, y, nx, ny, length, _) in prof:
            row.append(m.vertex((x, y, z), (sign * nx, sign * ny, 0.0),
                                ((z - z0) / TUNNEL_TILE, length / TUNNEL_TILE)))
        rows.append(row)
    for i in range(len(prof) - 1):
        m.quad(rows[0][i], rows[0][i + 1], rows[1][i + 1], rows[1][i])


def model_tunnel_section():
    """Módulo do túnel: TUNNEL_SECTION m ao longo de z (centrado em z = 0),
    face interna (normais para dentro, vista de dentro do túnel) e externa.
    Não tem piso nem tampas: o chão é o leito da pista e as pontas ficam
    encostadas no módulo vizinho ou atrás de um portal."""
    m = Mesh("tunnel_section")
    z0, z1 = -0.5 * TUNNEL_SECTION, 0.5 * TUNNEL_SECTION
    tunnel_shell(m, TUNNEL_HALF_WIDTH, z0, z1, inward=True)
    tunnel_shell(m, TUNNEL_HALF_WIDTH + TUNNEL_THICK, z0, z1, inward=False)
    return m


def ray_to_rect(a, hw, h):
    """Ponto em que o raio que sai de (0, TUNNEL_SPRING) com ângulo a encontra
    a borda do retângulo |x| <= hw, y <= h (lados ou topo)."""
    c, s = math.cos(a), math.sin(a)
    if abs(c) > 1e-9:
        t = hw / abs(c)
        y = TUNNEL_SPRING + t * s
        if y <= h + 1e-9:
            return (math.copysign(hw, c), y)
    t = (h - TUNNEL_SPRING) / s
    return (t * c, h)


def flat_ring(m, r, z, normal_z, hw, h, outer_r=None):
    """Face plana em z (normal (0, 0, normal_z)) entre o perfil de raio r
    (borda interna) e o contorno externo: o retângulo |x| <= hw, 0 <= y <= h,
    ou o perfil de raio outer_r. Cada ponto do perfil é ligado ao ponto do
    contorno externo no mesmo ângulo (quadriláteros convexos); os cantos do
    retângulo entram como ângulos extras. UV planar em metros."""
    corner = math.atan2(h - TUNNEL_SPRING, hw)
    extra = (corner, math.pi - corner) if outer_r is None else ()
    inner = tunnel_profile(r, extra)
    outer = tunnel_profile(outer_r, extra) if outer_r is not None else None
    n = (0.0, 0.0, normal_z)

    def uv(x, y):
        return ((x if normal_z > 0 else -x) / TUNNEL_TILE, y / TUNNEL_TILE)

    a_ids, b_ids = [], []
    for i, (x, y, nx, ny, _, a) in enumerate(inner):
        if outer is not None:
            ox, oy = outer[i][0], outer[i][1]
        elif a is None:
            ox, oy = math.copysign(hw, x), 0.0   # pés das paredes
        else:
            ox, oy = ray_to_rect(a, hw, h)
        a_ids.append(m.vertex((x, y, z), n, uv(x, y)))
        b_ids.append(m.vertex((ox, oy, z), n, uv(ox, oy)))
    for i in range(len(inner) - 1):
        m.quad(a_ids[i], a_ids[i + 1], b_ids[i + 1], b_ids[i])


def model_tunnel_portal():
    """Portal (fachada) de 2*TUNNEL_PORTAL_HALF_WIDTH x TUNNEL_PORTAL_HEIGHT x
    TUNNEL_PORTAL_DEPTH com a abertura do perfil interno do túnel e uma
    moldura (arquivolta) saliente em volta da abertura, na frente (+Z, voltada
    para quem chega). Corpo em z de -DEPTH/2 a DEPTH/2; a moldura avança até
    DEPTH/2 + ARCHIVOLT_DEPTH. Base em y = 0, centrado em x = 0 no corpo (o
    jogo posiciona o portal pelas coordenadas do modelo, sem recentrar)."""
    m = Mesh("tunnel_portal")
    hw, h, d = TUNNEL_PORTAL_HALF_WIDTH, TUNNEL_PORTAL_HEIGHT, 0.5 * TUNNEL_PORTAL_DEPTH
    r, ra = TUNNEL_HALF_WIDTH, TUNNEL_HALF_WIDTH + ARCHIVOLT_WIDTH
    zf = d + ARCHIVOLT_DEPTH
    # Fachada da frente (fora da moldura) e de trás (até a abertura).
    flat_ring(m, ra, d, 1.0, hw, h)
    flat_ring(m, r, -d, -1.0, hw, h)
    # Moldura: face da frente, borda externa e o vão (intradorso), contínuo
    # com o do túnel, da face de trás até a frente da moldura.
    flat_ring(m, r, zf, 1.0, hw, h, outer_r=ra)
    tunnel_shell(m, ra, d, zf, inward=False)
    tunnel_shell(m, r, -d, zf, inward=True)

    # Laterais e topo do bloco da fachada (as faces ±Z têm a abertura, feitas
    # acima, e a de baixo nunca aparece).
    def side_uv(face, u_m, v_m, w, hh):
        return (u_m / TUNNEL_TILE, v_m / TUNNEL_TILE)

    blk = box("p", (-hw, 0.0, -d), (hw, h, d), side_uv)
    # box() faz 2 triângulos por face, na ordem pz, nz, px, nx, py, ny.
    blk.tris = blk.tris[4:10]
    m.append(blk)
    return m


# ---------------------------------------------------------------------------
# Rampa (Tarefa 4.4)
# ---------------------------------------------------------------------------
# Mesmas medidas de OBSTACLE_SIZE[RAMP] em include/game.h (= largura e altura
# do trem): alterar lá e aqui juntos.
RAMP_HALF_WIDTH = 1.1    # m
RAMP_HEIGHT = 3.2        # m (teto do trem)
RAMP_LENGTH = 8.0        # m ao longo da pista
RAMP_CURB = 0.15         # m: largura das guias metálicas nas bordas do piso
RAMP_LIP = 0.4           # m: chapa metálica da ponta baixa (antes das tábuas)
RAMP_PLANK = 0.5         # m (em z) de cada tábua
RAMP_GAP = 0.06          # m (em z) entre tábuas: mostra a chapa de baixo
RAMP_PLANK_DEPTH = 0.03  # m: tábuas acima da chapa de baixo
RAMP_METAL_TILE = 2.0    # m por repetição de metal_plate_diff_1k.jpg
RAMP_WOOD_TILE = 1.5     # m por repetição de rough_wood_diff_1k.jpg


def ramp_surface_y(z):
    """Altura do piso (caminho do jogador) em z: 0 na ponta baixa (+Z, de onde
    o jogador vem) até RAMP_HEIGHT na alta (-Z, colada à frente do trem). É a
    mesma reta de RampHeightAt (src/collisions.cpp)."""
    h = 0.5 * RAMP_LENGTH
    return RAMP_HEIGHT * (h - z) / RAMP_LENGTH


def ramp_quad(m, pts, n, uvs):
    ids = [m.vertex(p, n, uv) for p, uv in zip(pts, uvs)]
    m.quad(*ids)


def model_ramp():
    """Rampa (2.2 x 3.2 x 8 m): cunha que sobe do chão (z = +4) ao teto do trem
    (z = -4). Dois objetos, cada um com a sua textura (UV em metros, sem
    esticar): 'ramp_frame' (metal_plate: laterais, guias das bordas, chapa da
    ponta baixa, chapa sob as tábuas e o fundo) e 'ramp_deck' (rough_wood:
    tábuas transversais). O topo das tábuas, das guias e da chapa da ponta é
    exatamente o piso de colisão; a chapa de baixo fica RAMP_PLANK_DEPTH
    abaixo (aparece nas frestas, sem faces coplanares sobrepostas)."""
    hw, hl = RAMP_HALF_WIDTH, 0.5 * RAMP_LENGTH
    iw = hw - RAMP_CURB
    slope = RAMP_HEIGHT / RAMP_LENGTH
    n_up = v_norm((0.0, 1.0, slope))           # normal do piso (para cima e para +Z)
    k = math.sqrt(1.0 + slope * slope)          # m de piso por m em z
    mt, wt = RAMP_METAL_TILE, RAMP_WOOD_TILE
    y = ramp_surface_y

    frame = Mesh("ramp_frame")
    # Laterais (triângulos), UV planar em (z, y).
    for sx in (-1.0, 1.0):
        x = sx * hw
        pts = [(x, 0.0, hl), (x, 0.0, -hl), (x, RAMP_HEIGHT, -hl)]
        ids = [frame.vertex(p, (sx, 0.0, 0.0), (p[2] / mt, p[1] / mt)) for p in pts]
        frame.tri(*ids)
    # Fundo (encostado na frente do trem).
    ramp_quad(frame, [(-hw, 0.0, -hl), (hw, 0.0, -hl), (hw, RAMP_HEIGHT, -hl), (-hw, RAMP_HEIGHT, -hl)],
              (0.0, 0.0, -1.0), [(-hw / mt, 0), (hw / mt, 0), (hw / mt, RAMP_HEIGHT / mt),
                                 (-hw / mt, RAMP_HEIGHT / mt)])

    def slope_uv(x, z, tile):
        return (x / tile, (hl - z) * k / tile)

    def slope_quad(m, x0, x1, z0, z1, dy, tile):
        """Faixa do piso entre x0..x1 e z0..z1 (z0 > z1), deslocada dy em y."""
        pts = [(x0, y(z0) + dy, z0), (x1, y(z0) + dy, z0), (x1, y(z1) + dy, z1), (x0, y(z1) + dy, z1)]
        ramp_quad(m, pts, n_up, [slope_uv(p[0], p[2], tile) for p in pts])

    z_lip = hl - RAMP_LIP
    slope_quad(frame, -hw, hw, hl, z_lip, 0.0, mt)                 # chapa da ponta baixa
    for x0, x1 in ((-hw, -iw), (iw, hw)):                          # guias das bordas
        slope_quad(frame, x0, x1, z_lip, -hl, 0.0, mt)
    slope_quad(frame, -iw, iw, z_lip, -hl, -RAMP_PLANK_DEPTH, mt)  # chapa sob as tábuas
    # Faces internas das guias (descem do piso até a chapa de baixo).
    for sx in (-1.0, 1.0):
        x = sx * iw
        pts = [(x, y(z_lip), z_lip), (x, y(-hl), -hl),
               (x, y(-hl) - RAMP_PLANK_DEPTH, -hl), (x, y(z_lip) - RAMP_PLANK_DEPTH, z_lip)]
        ramp_quad(frame, pts, (-sx, 0.0, 0.0), [(p[2] / mt, p[1] / mt) for p in pts])
    # Degrau entre a chapa da ponta e a chapa de baixo (voltado para -Z).
    pts = [(-iw, y(z_lip), z_lip), (iw, y(z_lip), z_lip),
           (iw, y(z_lip) - RAMP_PLANK_DEPTH, z_lip), (-iw, y(z_lip) - RAMP_PLANK_DEPTH, z_lip)]
    ramp_quad(frame, pts, (0.0, 0.0, -1.0), [(p[0] / mt, p[1] / mt) for p in pts])

    deck = Mesh("ramp_deck")
    z = z_lip - RAMP_GAP
    i = 0
    while z > -hl + 1e-6:
        z1 = max(z - RAMP_PLANK, -hl)
        # Tábua: topo no piso; U ao longo da pista, V ao longo da tábua (veio
        # como nos dormentes); cada tábua com um trecho diferente da textura.
        off = (i * 0.37) % 1.0
        pts = [(-iw, y(z), z), (iw, y(z), z), (iw, y(z1), z1), (-iw, y(z1), z1)]
        ramp_quad(deck, pts, n_up, [((hl - p[2]) * k / wt + off, p[0] / wt) for p in pts])
        # Bordas da tábua (frente, para +Z, e trás, para -Z), de RAMP_PLANK_DEPTH.
        d = RAMP_PLANK_DEPTH
        for zz, nz in ((z, 1.0), (z1, -1.0)):
            if nz < 0.0 and zz <= -hl + 1e-6:
                continue  # a última tábua encosta no fundo
            pts = [(-iw, y(zz) - d, zz), (iw, y(zz) - d, zz), (iw, y(zz), zz), (-iw, y(zz), zz)]
            ramp_quad(deck, pts, (0.0, 0.0, nz), [(off, p[0] / wt) if p[1] < y(zz) else
                                                   (off + d / wt, p[0] / wt) for p in pts])
        z = z1 - RAMP_GAP
        i += 1
    return [frame, deck]


# ---------------------------------------------------------------------------
# Prédio de tijolos (Revisão final, item 3): SPEC, "prédios do ambiente com
# texturas de tijolos". Medidas iguais às de include/scenery.h
# (BUILDING_WIDTH, BRICK_BUILDING_MIN/MAX_FLOORS): alterar lá e aqui juntos.
# ---------------------------------------------------------------------------
BRICK_HALF = 3.0          # m: planta 6 x 6 (BUILDING_WIDTH), escala 1 no jogo
BRICK_BASE = 0.6          # m: embasamento antes do 1º andar
BRICK_FLOOR = 3.0         # m por andar
BRICK_WIN_W = 1.2         # m: largura da janela
BRICK_WIN_H = 1.6         # m: altura da janela
BRICK_WIN_SILL = 0.9      # m do piso do andar ao peitoril
BRICK_WIN_U = (0.9, 3.9)  # m: borda esquerda das 2 janelas de cada face (vista de fora)
BRICK_REVEAL = 0.15       # m: profundidade do vão da janela (vidro recuado)
BRICK_TILE = 2.0          # m por repetição de red_brick_diff_1k.jpg (como no túnel)
BRICK_CORNICE_OVER = 0.15 # m: quanto a cornija avança além da parede
BRICK_CORNICE_H = 0.4     # m
BRICK_PLINTH_OVER = 0.05  # m: embasamento de concreto (avança além da parede)
BRICK_SILL_OVER = 0.08    # m: peitoril (avança além da parede e da janela)


def model_brick_building(floors):
    """Prédio de tijolos de 'floors' andares, planta 6 x 6 m centrada em
    x = z = 0, base em y = 0, fachada principal em +Z (como os modelos da
    Kenney; o jogo gira +Z para a pista). Três objetos:
      <nome>_walls   (red_brick, UV em metros / BRICK_TILE): as 4 paredes com
                     2 janelas por andar em cada face, e os vãos das janelas;
      <nome>_windows (atlas: vidro azul) vidraças recuadas BRICK_REVEAL;
      <nome>_trim    (atlas: concreto) embasamento, peitoris e a cornija, cujo
                     topo é o telhado plano.
    Altura da parede: BRICK_BASE + floors * BRICK_FLOOR."""
    name = "brick_building_%d" % floors
    walls, wins, trim = Mesh(name + "_walls"), Mesh(name + "_windows"), Mesh(name + "_trim")
    hw = BRICK_HALF
    top = BRICK_BASE + floors * BRICK_FLOOR
    W = 2.0 * hw
    # Faces vistas de fora: (normal, origem = canto inferior esquerdo, eixo u).
    faces = [
        ((0.0, 0.0, 1.0), (-hw, 0.0, hw), (1.0, 0.0, 0.0)),
        ((0.0, 0.0, -1.0), (hw, 0.0, -hw), (-1.0, 0.0, 0.0)),
        ((1.0, 0.0, 0.0), (hw, 0.0, hw), (0.0, 0.0, -1.0)),
        ((-1.0, 0.0, 0.0), (-hw, 0.0, -hw), (0.0, 0.0, 1.0)),
    ]
    up = (0.0, 1.0, 0.0)
    rows = [(BRICK_BASE + f * BRICK_FLOOR + BRICK_WIN_SILL,
             BRICK_BASE + f * BRICK_FLOOR + BRICK_WIN_SILL + BRICK_WIN_H) for f in range(floors)]
    cols = [(u, u + BRICK_WIN_W) for u in BRICK_WIN_U]
    us = sorted({0.0, W} | {c for col in cols for c in col})
    vs = sorted({0.0, top} | {r for row in rows for r in row})

    def is_window(u0, u1, v0, v1):
        return any(abs(u0 - c[0]) < 1e-6 and abs(u1 - c[1]) < 1e-6 for c in cols) and \
               any(abs(v0 - r[0]) < 1e-6 and abs(v1 - r[1]) < 1e-6 for r in rows)

    def brick_quad(m, pts, n):
        """Quadrilátero de tijolo com UV planar em metros no plano da face
        (normal n): (x, z) nas horizontais, (x, y) com normal em z, (z, y) com
        normal em x. Nada estica, nem nos vãos estreitos."""
        if abs(n[1]) > 0.5:
            uv = lambda p: (p[0] / BRICK_TILE, p[2] / BRICK_TILE)
        elif abs(n[2]) > 0.5:
            uv = lambda p: (p[0] / BRICK_TILE, p[1] / BRICK_TILE)
        else:
            uv = lambda p: (p[2] / BRICK_TILE, p[1] / BRICK_TILE)
        ids = [m.vertex(p, n, uv(p)) for p in pts]
        m.quad(*ids)

    for n, o, eu in faces:
        inward = v_scale(n, -BRICK_REVEAL)

        def at(u, v, depth=(0.0, 0.0, 0.0)):
            return v_add(v_add(o, v_add(v_scale(eu, u), v_scale(up, v))), depth)

        # Parede: a grade de células entre as bordas das janelas, menos as janelas.
        for i in range(len(us) - 1):
            for j in range(len(vs) - 1):
                u0, u1, v0, v1 = us[i], us[i + 1], vs[j], vs[j + 1]
                if is_window(u0, u1, v0, v1):
                    continue
                brick_quad(walls, [at(u0, v0), at(u1, v0), at(u1, v1), at(u0, v1)], n)
        for (u0, u1) in cols:
            for (v0, v1) in rows:
                # Vão (4 faces de tijolo voltadas para o centro da janela).
                brick_quad(walls, [at(u0, v0), at(u0, v1), at(u0, v1, inward), at(u0, v0, inward)], eu)
                brick_quad(walls, [at(u1, v0), at(u1, v1), at(u1, v1, inward), at(u1, v0, inward)],
                           v_scale(eu, -1.0))
                brick_quad(walls, [at(u0, v0), at(u1, v0), at(u1, v0, inward), at(u0, v0, inward)], up)
                brick_quad(walls, [at(u0, v1), at(u1, v1), at(u1, v1, inward), at(u0, v1, inward)],
                           (0.0, -1.0, 0.0))
                # Vidraça recuada.
                glass = swatch_uv("light_blue")
                ids = [wins.vertex(at(u, v, inward), n, glass) for (u, v) in ((u0, v0), (u1, v0), (u1, v1), (u0, v1))]
                wins.quad(*ids)
                # Peitoril: caixa de concreto sob a janela, saliente; o topo
                # fica 2 cm acima da base do vão (sem faces coplanares).
                out = v_scale(n, BRICK_SILL_OVER)
                p0 = at(u0 - BRICK_SILL_OVER, v0 - 0.08, v_scale(n, -0.02))
                p1 = at(u1 + BRICK_SILL_OVER, v0 + 0.02, out)
                trim.append(box("sill", tuple(min(p0[k], p1[k]) for k in range(3)),
                                tuple(max(p0[k], p1[k]) for k in range(3)), color="concrete"))
    # Embasamento e cornija (o topo da cornija é o telhado plano).
    po, co = hw + BRICK_PLINTH_OVER, hw + BRICK_CORNICE_OVER
    plinth = box("plinth", (-po, 0.0, -po), (po, BRICK_BASE * 0.5, po), color="concrete")
    plinth.tris = plinth.tris[:10]  # sem a face de baixo
    trim.append(plinth)
    cornice = box("cornice", (-co, top, -co), (co, top + BRICK_CORNICE_H, co), color="concrete")
    trim.append(cornice)
    return [walls, wins, trim]


# ---------------------------------------------------------------------------
# Escrita
# ---------------------------------------------------------------------------

def write_obj(path, meshes, description, texture="atlas.png (mesmo diretorio)"):
    lines = ["# %s" % description,
             "# Gerado por tools/gen_models.py (Rail Rush, INF01047). Licenca: CC0.",
             "# Textura: %s." % texture]
    vbase = 0
    total = 0
    for m in meshes:
        lines.append("o %s" % m.name)
        for p in m.pos:
            lines.append("v %.5f %.5f %.5f" % p)
        for uv in m.uv:
            lines.append("vt %.5f %.5f" % uv)
        for n in m.nrm:
            lines.append("vn %.5f %.5f %.5f" % n)
        for (a, b, c) in m.tris:
            ia, ib, ic = a + 1 + vbase, b + 1 + vbase, c + 1 + vbase
            lines.append("f %d/%d/%d %d/%d/%d %d/%d/%d" % (ia, ia, ia, ib, ib, ib, ic, ic, ic))
        vbase += len(m.pos)
        total += len(m.tris)
    with open(path, "w", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    return total


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    write_png(os.path.join(OUT_DIR, "atlas.png"), ATLAS, ATLAS, build_atlas())
    models = [
        ("barrier_low.obj", [model_barrier_low()], "Barreira baixa (2.0 x 0.9 x 0.3 m)"),
        ("barrier_high.obj", [model_barrier_high()], "Barreira alta (2.0 x 3.0 x 0.3 m, vao de 1.1 m)"),
        ("sign_board.obj", [model_sign()], "Placa de bloqueio (2.0 x 3.0 x 0.3 m)"),
        ("coin.obj", [model_coin()], "Moeda (0.8 m de diametro)"),
        ("powerup_magnet.obj", [model_magnet()], "Power-up: ima"),
        ("powerup_sneaker.obj", [model_sneaker()], "Power-up: tenis"),
        ("powerup_multiplier.obj", [model_multiplier()], "Power-up: multiplicador x2"),
        ("piggy_bank.obj", [model_piggy_bank()], "Power-up: cofrinho"),
        ("flying_pig.obj", model_flying_pig(), "Porco voador (corpo + 2 asas)"),
        ("tunnel_section.obj", [model_tunnel_section()], "Tunel: modulo de 4 m (abobada)",
         "data/castle_wall_slates_diff_1k.jpg (UV em metros / 2)"),
        ("tunnel_portal.obj", [model_tunnel_portal()], "Tunel: portal de entrada/saida",
         "data/red_brick_diff_1k.jpg (UV em metros / 2)"),
        ("brick_building.obj",
         model_brick_building(2) + model_brick_building(3) + model_brick_building(4),
         "Predios de tijolo de 2, 3 e 4 andares (planta 6 x 6 m)",
         "*_walls: data/red_brick_diff_1k.jpg (UV em metros / 2); *_windows e *_trim: atlas.png"),
        ("ramp.obj", model_ramp(), "Rampa para subir no trem (2.2 x 3.2 x 8 m)",
         "ramp_frame: data/metal_plate_diff_1k.jpg (UV em metros / 2); "
         "ramp_deck: data/rough_wood_diff_1k.jpg (UV em metros / 1.5)"),
    ]
    for entry in models:
        filename, meshes, desc = entry[:3]
        texture = entry[3] if len(entry) > 3 else "atlas.png (mesmo diretorio)"
        tris = write_obj(os.path.join(OUT_DIR, filename), meshes, desc, texture)
        mn = tuple(min(m.bbox()[0][i] for m in meshes) for i in range(3))
        mx = tuple(max(m.bbox()[1][i] for m in meshes) for i in range(3))
        print("%-26s %5d triangulos  bbox (%.3f, %.3f, %.3f) .. (%.3f, %.3f, %.3f)" % (
            filename, tris, mn[0], mn[1], mn[2], mx[0], mx[1], mx[2]))


if __name__ == "__main__":
    main()
