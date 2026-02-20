#!/usr/bin/env python3
"""
Setup script for plumbrc Python package.

Builds libplumbr.so from C source and bundles it in the wheel.
"""

import os
import subprocess
from pathlib import Path
from setuptools import setup
from setuptools.command.build_py import build_py


class BuildWithC(build_py):
    """Custom build that compiles libplumbr.so before packaging."""

    def run(self):
        # Find the C source root (parent of python/)
        pkg_dir = Path(__file__).parent
        c_root = pkg_dir.parent
        lib_dest = pkg_dir / "plumbrc" / "lib"

        # Only build if Makefile exists (source distribution)
        makefile = c_root / "Makefile"
        if makefile.exists():
            lib_dest.mkdir(parents=True, exist_ok=True)
            subprocess.check_call(["make", "shared"], cwd=str(c_root))
            so_file = c_root / "build" / "lib" / "libplumbr.so"
            if so_file.exists():
                import shutil
                shutil.copy2(str(so_file), str(lib_dest / "libplumbr.so"))

        super().run()


readme_file = Path(__file__).parent / "README.md"
long_description = ""
if readme_file.exists():
    long_description = readme_file.read_text()

setup(
    name="plumbrc",
    version="1.0.2",
    long_description=long_description,
    long_description_content_type="text/markdown",
    cmdclass={"build_py": BuildWithC},
)
