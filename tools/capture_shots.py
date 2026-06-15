import pathlib
from playwright.sync_api import sync_playwright

OUT = pathlib.Path("D:/Projects/fantune-skin/screenshots")
OUT.mkdir(parents=True, exist_ok=True)

URL = pathlib.Path("D:/Projects/fantune-skin/index.html").resolve().as_uri()

with sync_playwright() as p:
    browser = p.chromium.launch(headless=True)
    ctx = browser.new_context(viewport={"width": 1440, "height": 900}, device_scale_factor=2)
    page = ctx.new_page()

    # Full chassis — default state
    page.goto(URL, wait_until="networkidle")
    page.wait_for_timeout(3000)
    chassis = page.query_selector("#plugin-chassis")
    chassis.screenshot(path=str(OUT / "plugin_hero.png"))
    page.screenshot(path=str(OUT / "page_full.png"), full_page=True)
    print("plugin_hero.png  — default")

    # Grille: high width, high speed
    page.goto(URL, wait_until="networkidle")
    page.wait_for_timeout(1500)
    for param, val in [("speed", 0.75), ("width", 0.85), ("pitch", 0.5), ("drive", 0.35)]:
        k = page.query_selector(f'[data-param="{param}"]')
        if k:
            bx = k.bounding_box()
            cx, cy = bx["x"] + bx["width"]/2, bx["y"] + bx["height"]/2
            cur = float(k.get_attribute("data-value") or "0.5")
            page.mouse.move(cx, cy)
            page.mouse.down()
            page.mouse.move(cx, cy - (val - cur) * 140, steps=25)
            page.mouse.up()
            page.wait_for_timeout(200)
    page.wait_for_timeout(1000)
    chassis2 = page.query_selector("#plugin-chassis")
    chassis2.screenshot(path=str(OUT / "plugin_grille.png"))
    page.screenshot(path=str(OUT / "page_grille.png"), full_page=True)
    print("plugin_grille.png — high chop depth")

    # Behind: medium speed, lower width, higher distance mode
    page.goto(URL, wait_until="networkidle")
    page.wait_for_timeout(1500)
    for param, val in [("speed", 0.55), ("width", 0.50), ("pitch", 0.25), ("drive", 0.15), ("mix", 0.80)]:
        k = page.query_selector(f'[data-param="{param}"]')
        if k:
            bx = k.bounding_box()
            cx, cy = bx["x"] + bx["width"]/2, bx["y"] + bx["height"]/2
            cur = float(k.get_attribute("data-value") or "0.5")
            page.mouse.move(cx, cy)
            page.mouse.down()
            page.mouse.move(cx, cy - (val - cur) * 140, steps=25)
            page.mouse.up()
            page.wait_for_timeout(200)
    page.wait_for_timeout(1000)
    chassis3 = page.query_selector("#plugin-chassis")
    chassis3.screenshot(path=str(OUT / "plugin_behind.png"))
    page.screenshot(path=str(OUT / "page_behind.png"), full_page=True)
    print("plugin_behind.png — soft chop")

    browser.close()

print(f"\nScreenshots in: {OUT}")
