# Python Analyzer v3.0

[![CI](https://github.com/Huzaifa-zuberi/python-analyzer/actions/workflows/ci.yml/badge.svg)](https://github.com/Huzaifa-zuberi/python-analyzer/actions/workflows/ci.yml)
[![Python](https://img.shields.io/badge/Python-3.8%2B-blue?logo=python)](https://python.org)
[![License](https://img.shields.io/badge/License-MIT-success)](LICENSE)
![Last Commit](https://img.shields.io/github/last-commit/Huzaifa-zuberi/python-analyzer)
![Stars](https://img.shields.io/github/stars/Huzaifa-zuberi/python-analyzer?style=social)

**Repo:** [Huzaifa-zuberi/python-analyzer](https://github.com/Huzaifa-zuberi/python-analyzer)

## Screenshots

*(Add a screenshot of the frontend here — open `frontend/index.html` in your browser)*

A Python code analyzer with enhanced error detection, symbol table generation, and token analysis. Features both a **C++ command-line tool** and a **browser-based frontend** (JavaScript).

## Project Structure

```
analyzer/
├── src/
│   └── python_analyzer.cpp      # C++ source (CLI analyzer)
├── frontend/
│   └── index.html               # Web frontend (standalone, no server needed)
├── backend/
│   └── server.py                # Flask server (optional, serves frontend)
├── scripts/
│   └── compile.ps1              # Windows build script
├── Makefile                     # Build config (g++ / Linux/macOS)
├── requirements.txt             # Python dependencies
├── .gitignore
└── README.md
```

## Quick Start (Web)

Open **`frontend/index.html`** directly in any browser. No server required — the analyzer runs entirely in JavaScript.

- Type or paste Python code in the editor
- Click **Analyze** (or press Ctrl+Enter)
- Switch between **Token Analysis**, **Symbol Tables**, and **Diagnostics** tabs
- Click **Load Example** to try the built-in test code

## Run via Backend Server

```bash
pip install -r requirements.txt
python backend/server.py
```

Then open `http://localhost:5000`.

## Compile C++ CLI Tool

Requires **g++** (MinGW on Windows, or system g++ on Linux/macOS).

```bash
# On Linux/macOS
make
make run

# On Windows (PowerShell)
.\scripts\compile.ps1
.\scripts\compile.ps1 -Run
```

## Features

| Feature | Description |
|---|---|
| Tokenizer | Lexical analysis: keywords, identifiers, numbers, strings, operators, symbols |
| Indentation Check | Validates 4-space rule, detects inconsistent indentation |
| Syntax Check | Detects missing colons, unmatched parentheses/brackets/braces |
| Variable Validation | Detects undefined variables, tracks scopes |
| Symbol Tables | Scope-aware tables with types, values, usage counts |
| Type Inference | Infers types from literal values (int, float, str, bool, list, dict, etc.) |
| Control Flow | Detects break/continue outside loops |
| Function Checks | Validates def syntax, parameter lists |
| Import Checks | Validates import and from-import statements |
| Unused Variables | Warns about declared but unused variables |
| f-strings | Tracks variables used inside f-string expressions |

## Example Code (with deliberate errors)

```python
x = 10
name = "Alice"
items = [1, 2, 3]

def greet(person, age=0):
    message = "Hello"
    return message

for i in range(5)
    total = i * 2

print(undefined_var)
break
```

Click **Load Example** in the web UI to try this.
