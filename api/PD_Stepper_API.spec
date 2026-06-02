# -*- mode: python ; coding: utf-8 -*-
# PyInstaller spec for PD-Stepper API
# Build: pyinstaller PD_Stepper_API.spec --clean --noconfirm

from PyInstaller.utils.hooks import collect_all

bleak_datas, bleak_bins, bleak_hidden = collect_all('bleak')

try:
    winrt_datas, winrt_bins, winrt_hidden = collect_all('winrt')
except Exception:
    winrt_datas, winrt_bins, winrt_hidden = [], [], []

a = Analysis(
    ['main.py'],
    pathex=[],
    binaries=bleak_bins + winrt_bins,
    datas=[
        ('static', 'static'),
        *bleak_datas,
        *winrt_datas,
    ],
    hiddenimports=[
        # uvicorn internals
        'uvicorn.logging',
        'uvicorn.loops',
        'uvicorn.loops.auto',
        'uvicorn.loops.asyncio',
        'uvicorn.protocols',
        'uvicorn.protocols.http',
        'uvicorn.protocols.http.auto',
        'uvicorn.protocols.http.h11_impl',
        'uvicorn.protocols.websockets',
        'uvicorn.protocols.websockets.auto',
        'uvicorn.protocols.websockets.websockets_impl',
        'uvicorn.lifespan',
        'uvicorn.lifespan.on',
        # bleak Windows BLE backend
        'bleak.backends.winrt',
        'bleak.backends.winrt.client',
        'bleak.backends.winrt.scanner',
        'bleak.backends.winrt.service',
        'bleak.backends.winrt.characteristic',
        'bleak.backends.winrt.descriptor',
        # project modules
        'ble_manager',
        'websocket_manager',
        'models',
        *bleak_hidden,
        *winrt_hidden,
    ],
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=[],
    noarchive=False,
)

pyz = PYZ(a.pure)

exe = EXE(
    pyz,
    a.scripts,
    a.binaries,
    a.datas,
    [],
    name='PD_Stepper_API',
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    upx_exclude=[],
    runtime_tmpdir=None,
    console=True,   # set False to hide console window in production
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
)
