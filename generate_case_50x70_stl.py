import math
from pathlib import Path


OUT = Path(__file__).resolve().parent


def box(name, x, y, z, sx, sy, sz):
    v = [
        (x, y, z),
        (x + sx, y, z),
        (x + sx, y + sy, z),
        (x, y + sy, z),
        (x, y, z + sz),
        (x + sx, y, z + sz),
        (x + sx, y + sy, z + sz),
        (x, y + sy, z + sz),
    ]
    faces = [
        (0, 3, 2, 1),
        (4, 5, 6, 7),
        (0, 1, 5, 4),
        (1, 2, 6, 5),
        (2, 3, 7, 6),
        (3, 0, 4, 7),
    ]
    tris = []
    for a, b, c, d in faces:
        tris.append((name, v[a], v[b], v[c]))
        tris.append((name, v[a], v[c], v[d]))
    return tris


def cyl(name, cx, cy, z, radius, height, segments=32):
    tris = []
    top = z + height
    center_bottom = (cx, cy, z)
    center_top = (cx, cy, top)
    for i in range(segments):
        a0 = 2 * math.pi * i / segments
        a1 = 2 * math.pi * (i + 1) / segments
        p0 = (cx + radius * math.cos(a0), cy + radius * math.sin(a0), z)
        p1 = (cx + radius * math.cos(a1), cy + radius * math.sin(a1), z)
        p2 = (cx + radius * math.cos(a1), cy + radius * math.sin(a1), top)
        p3 = (cx + radius * math.cos(a0), cy + radius * math.sin(a0), top)
        tris.append((name, p0, p1, p2))
        tris.append((name, p0, p2, p3))
        tris.append((name, center_bottom, p1, p0))
        tris.append((name, center_top, p3, p2))
    return tris


def normal(a, b, c):
    ux, uy, uz = b[0] - a[0], b[1] - a[1], b[2] - a[2]
    vx, vy, vz = c[0] - a[0], c[1] - a[1], c[2] - a[2]
    nx = uy * vz - uz * vy
    ny = uz * vx - ux * vz
    nz = ux * vy - uy * vx
    length = math.sqrt(nx * nx + ny * ny + nz * nz) or 1
    return nx / length, ny / length, nz / length


def write_stl(path, solid_name, tris):
    with path.open("w", encoding="ascii", newline="\n") as f:
        f.write(f"solid {solid_name}\n")
        for _, a, b, c in tris:
            nx, ny, nz = normal(a, b, c)
            f.write(f"  facet normal {nx:.6f} {ny:.6f} {nz:.6f}\n")
            f.write("    outer loop\n")
            for p in (a, b, c):
                f.write(f"      vertex {p[0]:.3f} {p[1]:.3f} {p[2]:.3f}\n")
            f.write("    endloop\n")
            f.write("  endfacet\n")
        f.write(f"endsolid {solid_name}\n")


def build_base():
    # Caja para PCB 50x70 mm, PMS/CO2 externos, TFT en tapa.
    # Dimensiones en mm.
    outer_x = 95
    outer_y = 125
    height = 36
    wall = 2.2
    bottom = 2.4
    tris = []

    # Base abierta por arriba. La pared izquierda se divide para dejar ventana USB.
    tris += box("bottom", 0, 0, 0, outer_x, outer_y, bottom)
    # Pared derecha con respiraderos reales: se divide para dejar ranuras.
    tris += box("right_wall_lower", outer_x - wall, 0, 0, wall, outer_y, 10)
    tris += box("right_wall_upper", outer_x - wall, 0, 26, wall, outer_y, height - 26)
    for y, sy in ((0, 78), (83, 3), (91, 3), (99, 3), (107, 3), (115, outer_y - 115)):
        tris += box("right_wall_between_vents", outer_x - wall, y, 10, wall, sy, 16)
    tris += box("front_wall", 0, 0, 0, outer_x, wall, height)
    tris += box("back_wall", 0, outer_y - wall, 0, outer_x, wall, height)
    tris += box("left_wall_low", 0, 0, 0, wall, 42, height)
    tris += box("left_wall_high", 0, 58, 0, wall, outer_y - 58, height)

    # Labio superior para que encaje la tapa.
    lip_h = 3.0
    lip_z = height - lip_h
    tris += box("inner_lip_right", outer_x - wall - 2.0, wall + 1.0, lip_z, 2.0, outer_y - 2 * wall - 2.0, lip_h)
    tris += box("inner_lip_front", wall + 1.0, wall + 1.0, lip_z, outer_x - 2 * wall - 2.0, 2.0, lip_h)
    tris += box("inner_lip_back", wall + 1.0, outer_y - wall - 3.0, lip_z, outer_x - 2 * wall - 2.0, 2.0, lip_h)
    tris += box("inner_lip_left_low", wall + 1.0, wall + 1.0, lip_z, 2.0, 35, lip_h)
    tris += box("inner_lip_left_high", wall + 1.0, 65, lip_z, 2.0, outer_y - 68, lip_h)

    # Separadores PCB 50x70. La placa se monta vertical en el plano XY.
    standoff_z = bottom
    for x, y in ((16, 18), (66, 18), (16, 88), (66, 88)):
        tris += cyl("pcb_standoff", x, y, standoff_z, 3.2, 5.0)
        tris += cyl("pcb_pilot", x, y, standoff_z + 5.0, 1.25, 1.0)

    # Guias/soportes para orientar el ESP32 con USB al borde izquierdo.
    tris += box("esp32_stop_a", 8, 42, bottom, 5, 22, 4)
    tris += box("esp32_stop_b", 72, 42, bottom, 5, 22, 4)

    # Orejetas exteriores para tornillos de tapa.
    for x, y in ((8, 8), (87, 8), (8, 117), (87, 117)):
        tris += cyl("lid_post", x, y, bottom, 3.6, height - bottom)
        tris += cyl("lid_pilot", x, y, height - 4, 1.1, 4)

    return tris


def build_lid():
    outer_x = 95
    outer_y = 125
    plate = 3.0
    tris = []

    # Tapa en marco: deja ventana para TFT aprox. 63x48 mm.
    win_x = 16
    win_y = 33
    win_w = 63
    win_h = 48
    tris += box("lid_front_band", 0, 0, 0, outer_x, win_y, plate)
    tris += box("lid_back_band", 0, win_y + win_h, 0, outer_x, outer_y - win_y - win_h, plate)
    tris += box("lid_left_band", 0, win_y, 0, win_x, win_h, plate)
    tris += box("lid_right_band", win_x + win_w, win_y, 0, outer_x - win_x - win_w, win_h, plate)

    # Reborde inferior de encaje.
    tris += box("lid_lip_front", 4, 4, -3, outer_x - 8, 2, 3)
    tris += box("lid_lip_back", 4, outer_y - 6, -3, outer_x - 8, 2, 3)
    tris += box("lid_lip_left", 4, 6, -3, 2, outer_y - 12, 3)
    tris += box("lid_lip_right", outer_x - 6, 6, -3, 2, outer_y - 12, 3)

    # Cuatro postes para atornillar o pegar el modulo TFT a la tapa.
    for x, y in ((13, 28), (82, 28), (13, 86), (82, 86)):
        tris += cyl("tft_mount", x, y, -6, 3.2, 6)
        tris += cyl("tft_pilot", x, y, 0, 1.1, 1.2)

    # Agujeros/zonas de tornillos de cierre representadas como pilotos.
    for x, y in ((8, 8), (87, 8), (8, 117), (87, 117)):
        tris += cyl("case_screw_boss", x, y, -2.5, 2.5, 2.5)
        tris += cyl("case_screw_pilot", x, y, 0, 1.2, 1.0)

    return tris


def main():
    write_stl(OUT / "case_50x70_usb_borde_base.stl", "case_50x70_usb_borde_base", build_base())
    write_stl(OUT / "case_50x70_usb_borde_lid.stl", "case_50x70_usb_borde_lid", build_lid())


if __name__ == "__main__":
    main()
