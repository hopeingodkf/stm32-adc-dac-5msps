"""
Еталонна модель обробки і перевірка арифметики ProcessSamples() з main.c.

Мета - довести на числах дві речі:
  1) вихід ніколи не виходить за 8 біт і не загортається;
  2) масштаб підібраний так, що використано всю шкалу ЦАП без обрізання.

Запуск: python tools/verify_model.py
"""

def process(s1, s2, s3):
    """Точна копія рядка з main.c: __USAT((v >> 2) + 128, 8).
    У C для int32 >> це арифметичний зсув, у Python поведінка та сама."""
    v = s1 + s3 - 2 * s2
    x = (v >> 2) + 128
    if x < 0:
        x = 0
    elif x > 255:
        x = 255
    return x


def check_full_range():
    """Повний перебір усіх досяжних значень другої різниці."""
    lo, hi = None, None
    saturated = 0
    for v in range(-510, 511):
        x = (v >> 2) + 128
        if x < 0 or x > 255:
            saturated += 1
        x = max(0, min(255, x))
        lo = x if lo is None else min(lo, x)
        hi = x if hi is None else max(hi, x)
    return lo, hi, saturated


def check_corners():
    """Крайні комбінації вхідних байтів."""
    cases = {
        "S1=S3=0,   S2=255 (мінімум, v=-510)": (0, 255, 0),
        "S1=S3=255, S2=0   (максимум, v=+510)": (255, 0, 255),
        "постійний рівень  (v=0)":              (128, 128, 128),
        "лінійний нахил    (v=0)":              (100, 110, 120),
        "одиничний викид   (v=-255)":           (0, 255 // 2 + 1, 1),
    }
    return {name: process(*args) for name, args in cases.items()}


def test_vector(n=16):
    """Детермінований вектор для звірки з платою: пилка з розривом.
    Ті самі числа можна залити в adc_buf і побайтово порівняти dac_buf."""
    raw = []
    for i in range(n * 3):
        if i < n * 3 // 2:
            raw.append((i * 7) % 256)          # рівномірний нахил
        else:
            raw.append(255 if i % 5 == 0 else 10)  # різкі злами
    out = [process(raw[3 * i], raw[3 * i + 1], raw[3 * i + 2]) for i in range(n)]
    return raw, out


if __name__ == "__main__":
    lo, hi, sat = check_full_range()
    print("1) Повний діапазон другої різниці v = S1 + S3 - 2*S2")
    print(f"   v лежить у [-510 .. +510]")
    print(f"   результат лежить у [{lo} .. {hi}]  (потрібно [0 .. 255])")
    print(f"   спрацювань насичення __USAT: {sat}")
    print(f"   висновок: {'шкала використана повністю, обрізання немає' if (lo, hi, sat) == (0, 255, 0) else 'ПОМИЛКА МАСШТАБУ'}")
    print()

    print("2) Крайні випадки")
    for name, val in check_corners().items():
        print(f"   {name:38} -> {val}")
    print()

    raw, out = test_vector()
    print("3) Тест-вектор для звірки з платою")
    print(f"   adc_buf[0:{len(raw)}] = {raw}")
    print(f"   dac_buf[0:{len(out)}] = {out}")
    print()
    print("   Середнє значення виходу:", sum(out) / len(out))
