"""macOS alias-app entry point; sources remain in this checkout."""
import os
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
# py2app starts inside Contents/Resources. Match the shell launcher's project
# directory so --state build/... cannot silently create a second ledger there.
os.chdir(Path(__file__).resolve().parent.parent)
from token_dashboard.__main__ import main
main()
