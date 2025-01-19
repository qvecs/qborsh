from setuptools import Extension, setup

with open("README.md", encoding="utf-8") as f:
    long_desc = f.read()

setup(
    name="qborsh",
    version="1.0.1",
    author="Quick Vectors",
    author_email="felipe@qvecs.com",
    description="Borsh serialization for Python written in C.",
    long_description=long_desc,
    long_description_content_type="text/markdown",
    url="https://github.com/qvecs/qborsh",
    project_urls={
        "Bug Tracker": "https://github.com/qvecs/qborsh/issues",
        "Source Code": "https://github.com/qvecs/qborsh",
    },
    python_requires=">=3.10",
    include_package_data=True,
    packages=["qborsh"],
    classifiers=[
        "Programming Language :: Python :: 3",
        "Programming Language :: C",
        "Programming Language :: Python :: Implementation :: CPython",
        "License :: OSI Approved :: MIT License",
        "Operating System :: OS Independent",
        "Topic :: Utilities",
    ],
    install_requires=[
        "qbase58==1.0.4",
    ],
    ext_modules=[
        Extension(
            name="qborsh.csrc.py_borsh",
            sources=[
                "qborsh/csrc/py_borsh.c",
                "qborsh/csrc/borsh.c",
            ],
            include_dirs=["qborsh/csrc"],
            extra_compile_args=[
                "-std=gnu17",
                "-Ofast",
                "-flto",
                "-fomit-frame-pointer",
                "-funroll-loops",
                "-ffast-math",
                "-fstrict-aliasing",
            ],
        )
    ],
    extras_require={"dev": ["pytest"]},
)
