#!/usr/bin/env python3
"""
Setup script for plumbrc Python package.

This script handles building the C library and packaging it with the Python wrapper.
"""

import os
import sys
import subprocess
from pathlib import Path
from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext


class BuildPlumbrC(build_ext):
    """Custom build command to compile PlumbrC C library."""
    
    def run(self):
        """Build the C library using make."""
        # Get paths
        project_root = Path(__file__).parent.parent
        build_dir = project_root / "build"
        lib_dir = build_dir / "lib"
        
        # Build shared library
        print("Building PlumbrC shared library...")
        try:
            subprocess.check_call(
                ["make", "shared"],
                cwd=str(project_root)
            )
        except subprocess.CalledProcessError as e:
            print(f"Error building C library: {e}", file=sys.stderr)
            print("Make sure you have gcc and libpcre2-dev installed.", file=sys.stderr)
            sys.exit(1)
        
        # Copy library to package directory
        package_lib_dir = Path("plumbrc/lib")
        package_lib_dir.mkdir(exist_ok=True)
        
        lib_file = lib_dir / "libplumbr.so"
        if lib_file.exists():
            import shutil
            shutil.copy(str(lib_file), str(package_lib_dir / "libplumbr.so"))
            print(f"Copied {lib_file} to {package_lib_dir}")
        else:
            print(f"Warning: {lib_file} not found", file=sys.stderr)
        
        # Continue with standard build
        super().run()


# Read long description from README
readme_file = Path(__file__).parent / "README.md"
long_description = ""
if readme_file.exists():
    long_description = readme_file.read_text()


setup(
    name="plumbrc",
    version="1.0.0",
    cmdclass={
        "build_ext": BuildPlumbrC,
    },
    # Dummy extension to trigger build_ext
    ext_modules=[Extension("plumbrc._dummy", sources=[])],
    long_description=long_description,
    long_description_content_type="text/markdown",
)
