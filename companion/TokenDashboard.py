"""macOS alias-app entry point; sources remain in this checkout."""
import os
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
# py2app starts inside Contents/Resources. Match the shell launcher's project
# directory so --state build/... cannot silently create a second ledger there.
os.chdir(Path(__file__).resolve().parent.parent)
from token_dashboard.__main__ import main
# The terminal wrapper needs this instance's PID to forward Ctrl+C after
# LaunchServices starts the app independently of the terminal's process group.
pid_file = os.environ.get('TOKEN_DASHBOARD_LAUNCH_PID')
if pid_file:
    Path(pid_file).write_text(str(os.getpid()))
try:
    main()
finally:
    if pid_file:
        Path(pid_file).unlink(missing_ok=True)
