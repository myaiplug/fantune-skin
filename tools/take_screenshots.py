#!/usr/bin/env python3
"""Take production screenshots of the FanTune plugin GUI via Playwright."""

import asyncio
import pathlib
from playwright.async_api import async_playwright

OUTPUT_DIR = pathlib.Path(__file__).resolve().parent.parent / "screenshots"

SCENES = [
    {
        "name": "plugin_hero",
        "knobs": {},  # default values
        "description": "FanTune plugin at default settings",
    },
    {
        "name": "plugin_grille",
        "knobs": {
            "speed": 0.45,
            "pitch": 0.35,
            "width": 0.85,
            "tone": 0.60,
            "drive": 0.30,
            "spread": 0.50,
            "input": 0.75,
            "mix": 1.0,
            "output": 0.75,
        },
        "description": "Grille position — deep chop mode",
    },
    {
        "name": "plugin_behind",
        "knobs": {
            "speed": 0.60,
            "pitch": 0.20,
            "width": 0.55,
            "tone": 0.45,
            "drive": 0.15,
            "spread": 0.35,
            "input": 0.70,
            "mix": 0.85,
            "output": 0.70,
        },
        "description": "Behind position — softer chop, nostalgic",
    },
]


async def main():
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

    async with async_playwright() as p:
        browser = await p.chromium.launch(headless=True)
        context = await browser.new_context(
            viewport={"width": 1280, "height": 800},
            device_scale_factor=2,  # retina quality
        )
        page = await context.new_page()

        await page.goto("http://127.0.0.1:8080/", wait_until="networkidle")
        await asyncio.sleep(3)

        # Full page screenshot first (shows the chassis in context)
        await page.screenshot(
            path=str(OUTPUT_DIR / "page_full.png"),
            full_page=True,
        )
        print(f"  page_full.png — default state")

        for scene in SCENES:
            # Reset by reloading
            if scene["knobs"]:
                await page.goto("http://127.0.0.1:8080/", wait_until="networkidle")
                await asyncio.sleep(1.5)

                for param, value in scene["knobs"].items():
                    knob = await page.query_selector(f'[data-param="{param}"]')
                    if knob:
                        box = await knob.bounding_box()
                        if box:
                            # Map value 0-1 to angle rotation on knob
                            # Knob arc is 0.75pi to 2.25pi, so 270° sweep
                            # Full range from bottom of knob rotation
                            # The knob code reads: canvas center, pointer dot at angle
                            # startAngle=0.75pi, endAngle=2.25pi, valAngle = start + value * (end-start)
                            # For XY: valAngle = startAngle + value * (endAngle - startAngle)
                            # Drag up to increase value: dy = -(value - startVal) * 140
                            # startVal is the current value
                            current_val = float(knob.get_attribute("data-value") or "0.5")
                            dy = -(value - current_val) * 140
                            cx = box["x"] + box["width"] / 2
                            cy = box["y"] + box["height"] / 2
                            await page.mouse.move(cx, cy)
                            await page.mouse.down()
                            await page.mouse.move(cx, cy + dy, steps=30)
                            await page.mouse.up()
                            await asyncio.sleep(0.2)

                await asyncio.sleep(1.5)

            # Screenshot the plugin chassis only
            chassis = await page.query_selector("#plugin-chassis")
            if chassis:
                await chassis.screenshot(
                    path=str(OUTPUT_DIR / f"{scene['name']}.png"),
                )
                print(f"  {scene['name']}.png — {scene['description']}")
            else:
                print(f"  ERROR: #plugin-chassis not found for {scene['name']}")

            # Also take a detailed close-up of the fan guard area
            fan_zone = await page.query_selector("#fan-guard-zone")
            if fan_zone:
                await fan_zone.screenshot(
                    path=str(OUTPUT_DIR / f"{scene['name']}_fan.png"),
                )
                print(f"  {scene['name']}_fan.png — fan guard detail")

        await browser.close()

    print(f"\nAll screenshots saved to: {OUTPUT_DIR}/")


if __name__ == "__main__":
    asyncio.run(main())
