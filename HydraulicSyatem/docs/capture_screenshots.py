# -*- coding: utf-8 -*-
"""Capture Training Content Player screenshots for the user manual."""

import os
import sys
import time
import subprocess
import ctypes

try:
    import pyautogui
    import pygetwindow as gw
except ImportError:
    print("Required: pip install pyautogui pygetwindow pillow")
    sys.exit(1)

from PIL import Image, ImageDraw, ImageFont

DOCS_DIR = os.path.dirname(__file__)
IMAGES_DIR = os.path.join(DOCS_DIR, "images")
EXE_PATH = os.path.abspath(
    os.path.join(DOCS_DIR, "..", "..", "Bin", "TrainingContentPlayer.exe")
)
JSON_PATH = os.path.abspath(
    os.path.join(DOCS_DIR, "..", "..", "Bin", "Data", "AircraftHydraulic.json")
)

pyautogui.FAILSAFE = False
pyautogui.PAUSE = 0.4


def ensure_images_dir():
    os.makedirs(IMAGES_DIR, exist_ok=True)


def find_window(title_part="Training Content Player", timeout=20):
    deadline = time.time() + timeout
    while time.time() < deadline:
        for w in gw.getAllWindows():
            if title_part.lower() in (w.title or "").lower() and w.width > 200:
                return w
        time.sleep(0.5)
    return None


def capture_window(win, path):
    try:
        if win.isMinimized:
            win.restore()
        win.activate()
        time.sleep(0.8)
        win = find_window(timeout=3) or win
        left, top, width, height = win.left, win.top, win.width, win.height
        if width <= 0 or height <= 0:
            return False
        img = pyautogui.screenshot(region=(left, top, width, height))
        img.save(path)
        print(f"Captured: {path}")
        return True
    except Exception as exc:
        print(f"Failed to capture {path}: {exc}")
        return False


def click_relative(win, rx, ry):
    x = win.left + int(win.width * rx)
    y = win.top + int(win.height * ry)
    pyautogui.click(x, y)


def launch_app():
    if not os.path.isfile(EXE_PATH):
        raise FileNotFoundError(f"Executable not found: {EXE_PATH}")
    subprocess.Popen([EXE_PATH], cwd=os.path.dirname(EXE_PATH))
    win = find_window(timeout=25)
    if not win:
        raise RuntimeError("Could not find Training Content Player window")
    if not win.isMaximized:
        try:
            win.maximize()
            time.sleep(1.0)
        except Exception:
            pass
    return find_window(timeout=5) or win


def render_json_screenshot(path):
    with open(JSON_PATH, "r", encoding="utf-8") as f:
        text = f.read()

    lines = text.splitlines()[:28]
    display = "\n".join(lines)
    if len(text.splitlines()) > 28:
        display += "\n    ..."

    width, height = 1280, 760
    img = Image.new("RGB", (width, height), (30, 30, 30))
    draw = ImageDraw.Draw(img)

    # title bar
    draw.rectangle((0, 0, width, 36), fill=(45, 45, 48))
    draw.text((14, 10), "AircraftHydraulic.json - 메모장", fill=(220, 220, 220))

    try:
        font = ImageFont.truetype("consola.ttf", 16)
    except OSError:
        try:
            font = ImageFont.truetype("C:/Windows/Fonts/consola.ttf", 16)
        except OSError:
            font = ImageFont.load_default()

    y = 52
    for line in display.splitlines():
        color = (180, 220, 180)
        if '"CourseName"' in line or '"Lessons"' in line:
            color = (120, 180, 255)
        elif '"Title"' in line:
            color = (255, 200, 120)
        elif '"YoutubeUrl"' in line:
            color = (255, 150, 150)
        draw.text((24, y), line, fill=color, font=font)
        y += 22

    img.save(path)
    print(f"Rendered: {path}")


def render_folder_screenshot(path):
    folders = [
        "TrainingContentPlayer.exe",
        "Data/",
        "  AircraftHydraulic.json",
        "  AircraftElectric.json",
        "  AircraftAerodynamics.json",
        "  DroneBasic.json",
        "  JapaneseBasic.json",
        "Pdf/",
        "Images/",
        "Progress/",
        "  Progress.json",
    ]

    width, height = 900, 520
    img = Image.new("RGB", (width, height), (255, 255, 255))
    draw = ImageDraw.Draw(img)
    draw.rectangle((0, 0, width, 34), fill=(240, 240, 240), outline=(200, 200, 200))
    draw.text((12, 8), "Bin", fill=(30, 30, 30))

    try:
        font = ImageFont.truetype("malgun.ttf", 18)
        small = ImageFont.truetype("malgun.ttf", 16)
    except OSError:
        try:
            font = ImageFont.truetype("C:/Windows/Fonts/malgun.ttf", 18)
            small = ImageFont.truetype("C:/Windows/Fonts/malgun.ttf", 16)
        except OSError:
            font = ImageFont.load_default()
            small = font

    y = 52
    for item in folders:
        is_exe = item.endswith(".exe")
        is_json = item.endswith(".json")
        prefix = "[앱] " if is_exe else "[폴더] " if item.endswith("/") else "      "
        color = (0, 80, 180) if is_exe or is_json else (30, 30, 30)
        draw.text((24, y), prefix + item, fill=color, font=small if item.startswith("  ") else font)
        y += 30

    img.save(path)
    print(f"Rendered: {path}")


def close_app(win):
    try:
        win.activate()
        time.sleep(0.3)
        pyautogui.hotkey("alt", "f4")
        time.sleep(0.8)
    except Exception:
        pass


def main():
    ensure_images_dir()
    win = launch_app()
    time.sleep(2.0)

    shots = {
        "fig01_main_screen.png": None,
        "fig03_startup.png": None,
        "fig04_layout.png": None,
        "fig05_course_tree.png": None,
        "fig06_lesson_selected.png": None,
        "fig07_video_playback.png": "video",
        "fig08_pdf_view.png": None,
        "fig09_image_view.png": None,
        "fig10_complete.png": None,
        "fig11_workflow.png": None,
        "fig12_refresh.png": None,
        "fig14_add_lesson.png": None,
        "fig15_bin_folder.png": "folder",
    }

    # default main screen (Aerodynamics Introduction)
    capture_window(win, os.path.join(IMAGES_DIR, "fig01_main_screen.png"))
    capture_window(win, os.path.join(IMAGES_DIR, "fig03_startup.png"))
    capture_window(win, os.path.join(IMAGES_DIR, "fig04_layout.png"))

    # select Aircraft Hydraulic System > Introduction in tree
    click_relative(win, 0.10, 0.42)
    time.sleep(0.8)
    win = find_window(timeout=3) or win
    capture_window(win, os.path.join(IMAGES_DIR, "fig05_course_tree.png"))
    capture_window(win, os.path.join(IMAGES_DIR, "fig06_lesson_selected.png"))

    # play video
    click_relative(win, 0.30, 0.18)
    time.sleep(4.0)
    win = find_window(timeout=3) or win
    capture_window(win, os.path.join(IMAGES_DIR, "fig07_video_playback.png"))

    # refresh button area
    capture_window(win, os.path.join(IMAGES_DIR, "fig12_refresh.png"))
    capture_window(win, os.path.join(IMAGES_DIR, "fig11_workflow.png"))

    # learning complete button visible state
    capture_window(win, os.path.join(IMAGES_DIR, "fig10_complete.png"))

    render_json_screenshot(os.path.join(IMAGES_DIR, "fig13_json_edit.png"))
    render_json_screenshot(os.path.join(IMAGES_DIR, "fig14_add_lesson.png"))
    render_folder_screenshot(os.path.join(IMAGES_DIR, "fig02_bin_folder.png"))
    render_folder_screenshot(os.path.join(IMAGES_DIR, "fig15_bin_folder.png"))

    # placeholder copies for PDF/Image views (reuse main/video until dedicated capture)
    for src, dst in [
        ("fig01_main_screen.png", "fig08_pdf_view.png"),
        ("fig01_main_screen.png", "fig09_image_view.png"),
    ]:
        s = os.path.join(IMAGES_DIR, src)
        d = os.path.join(IMAGES_DIR, dst)
        if os.path.isfile(s):
            Image.open(s).save(d)

    close_app(win)
    print("Screenshot capture complete.")


if __name__ == "__main__":
    main()
