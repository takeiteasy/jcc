from setuptools import setup, Extension
import glob
import os

# Collect C sources from the project `src/` directory plus the Python wrapper.
src_dir = os.path.join(os.path.dirname(__file__), "..", "src")
src_files = glob.glob(os.path.join(src_dir, "*.c"))
# Make paths relative to this directory for setuptools
src_files = [os.path.normpath(os.path.relpath(p, start=os.path.dirname(__file__))) for p in src_files]
src_files.append("jcc.c")

ext = Extension(
    "PyJCC",
    include_dirs=["../src"],
    sources=src_files,
)

setup(
    name="PyJCC",
    version="0.0.1",
    author="George Watson",
    author_email="gigolo@hotmail.co.uk",
    description="Python bindings for JCC",
    url="https://github.com/takeiteasy/jcc",
    ext_modules=[ext],
    classifiers=[
        "Programming Language :: Python :: 3"
    ]
)