#!/usr/bin/env python3
"""Run the complete firmware layout + real LVGL + fAPP keyboard regression.

Pass an existing checkout of LVGL 8.3.11. No network access or MCU is required.
The renderer is extracted from the sketch on every run, so tests cannot silently
use a hand-written copy of the layout loader. Hardware/display IO and network
are replaced by host fixtures; UI, keyboard, event bridge and VM are real.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--lvgl-source', required=True, type=Path)
    parser.add_argument('--build-dir', required=True, type=Path)
    args = parser.parse_args()
    tests = Path(__file__).resolve().parent
    firmware = tests.parents[2] / 'fOS4.0'
    lvgl = args.lvgl_source.resolve()
    build = args.build_dir.resolve()
    if not (lvgl / 'src/extra/widgets/keyboard/lv_keyboard.c').is_file():
        parser.error('--lvgl-source must point to LVGL 8.3.11 sources')
    if build == tests or tests in build.parents:
        parser.error('Use a build directory outside the test source directory')
    build.mkdir(parents=True, exist_ok=True)
    source = (firmware / 'fOS4.0.ino').read_text()
    begin = source.index('static String decodeEscapedText(const String& text)\n{')
    end = source.index('static bool renderAppTextContent(', begin)
    (build / 'renderer.inc').write_text(source[begin:end])
    fixtures = tests / 'keyboard_lvgl'
    objects = build / 'obj'
    objects.mkdir(exist_ok=True)
    includes = ['-DLV_CONF_INCLUDE_SIMPLE', '-I'+str(fixtures), '-I'+str(lvgl)]

    def compile_source(path):
        obj = objects / (str(path.relative_to(lvgl)).replace('/', '_') + '.o')
        # Recompile when this test's LVGL configuration or source changes.
        if not obj.exists() or obj.stat().st_mtime < max(path.stat().st_mtime, (fixtures/'lv_conf.h').stat().st_mtime):
            subprocess.run(['gcc', '-O1', '-std=c99', *includes, '-c', str(path), '-o', str(obj)], check=True)
        return obj

    with ThreadPoolExecutor(max_workers=4) as executor:
        compiled = list(executor.map(compile_source, sorted((lvgl/'src').rglob('*.c'))))
    library = build / 'liblvgl.a'
    subprocess.run(['ar', 'rcs', str(library), *map(str, compiled)], check=True)
    # Stage a forward-binding diagnostic app without modifying the supplied apps.
    apps = build / 'apps'
    apps.mkdir(exist_ok=True)
    for app in (firmware/'example app/apps').iterdir():
        if app.is_dir() and not (apps/app.name).exists():
            (apps/app.name).symlink_to(app.resolve(), target_is_directory=True)
    diagnostic = apps/'fscript_keyboard_test'
    diagnostic.mkdir(exist_ok=True)
    (diagnostic/'layout.ui').write_text('type=keyboard;id=kb;target=ta;x=3;y=276;w=793;h=197\ntype=label;id=lbl;x=20;y=90;w=500;h=50;text=Ready\ntype=textarea;id=ta;x=13;y=69;w=774;h=203;text=\n')
    (diagnostic/'main.fscript').write_text('on kb.ready\n  lbl.text = "Confirmed: " + ta.text\nend\non kb.cancel\n  kb.hidden = true\nend\n')
    (diagnostic/'app.json').write_text('{"id":"test.keyboard","name":"Keyboard","version":"1.0.0","permissions":["ui"]}')
    subprocess.run([str(tests.parent/'build/fosc'),'build',str(diagnostic/'main.fscript'),'--ui',str(diagnostic/'layout.ui'),'-o',str(diagnostic/'main.fapp')],check=True)
    sources = ['ui_AppContent.c' , 'src/fapp/LvglFAppUiApi.cpp', 'src/fapp/FAppKeyboardController.cpp',
               'src/fapp/FAppUiRegistry.cpp', 'src/fapp/FAppLoader.cpp', 'src/fapp/FAppRuntime.cpp',
               'src/fapp/FAppEventBridge.cpp', 'src/fapp/FAppManifest.cpp']
    executable = build / 'keyboard-integration-tests'
    subprocess.run(['g++', '-std=c++17', '-O1', *includes, '-I'+str(build), '-I'+str(firmware),
                    '-DFOS_KEYBOARD_TEST_APPS_DIR="'+str(apps)+'"',
                    '-DFOS_KEYBOARD_TEST_OUTPUT_DIR="'+str(build)+'"',
                    str(fixtures/'KeyboardIntegrationTests.cpp'),
                    *[str(firmware/path) for path in sources], str(library), '-lm', '-o', str(executable)], check=True)
    subprocess.run([str(executable)], check=True)


if __name__ == '__main__':
    main()
