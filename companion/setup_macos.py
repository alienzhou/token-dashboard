"""Package a local macOS launcher with an explicit Bluetooth usage description."""
from setuptools import setup
setup(
    name='TokenDashboard',
    app=['TokenDashboard.py'],
    options={'py2app': {
        'argv_emulation': False,
        'plist': {
            'CFBundleIdentifier': 'local.ai-passport.token-dashboard',
            'CFBundleName': 'Token Dashboard',
            'CFBundleDisplayName': 'Token Dashboard',
            'CFBundleShortVersionString': '1.0.0',
            'NSBluetoothAlwaysUsageDescription': '通过蓝牙把本机 AI 工具用量统计同步到你的 AI Passport。',
            'NSBluetoothPeripheralUsageDescription': '同步用量统计到 AI Passport。',
            'LSUIElement': True,
        },
    }},
    setup_requires=['py2app==0.28.8'],
)
