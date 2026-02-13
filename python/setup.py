#!/usr/bin/env python3
"""
Setup script for plumbrc Python package.
"""

import os
from pathlib import Path
from setuptools import setup

readme_file = Path(__file__).parent / "README.md"
long_description = ""
if readme_file.exists():
    long_description = readme_file.read_text()

setup(
    name="plumbrc",
    version="1.0.1",
    long_description=long_description,
    long_description_content_type="text/markdown",
)
