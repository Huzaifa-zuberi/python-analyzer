import subprocess
import os
import sys
import json
import tempfile

try:
    from flask import Flask, request, jsonify, send_from_directory
except ImportError:
    print("Flask is required. Install with: pip install flask")
    sys.exit(1)

app = Flask(__name__, static_folder=os.path.join(os.path.dirname(__file__), '..', 'frontend'), static_url_path='')

CXX_PATH = os.path.join(os.path.dirname(__file__), '..', 'bin', 'python_analyzer')
SRC_PATH = os.path.join(os.path.dirname(__file__), '..', 'src', 'python_analyzer.cpp')

@app.route('/')
def index():
    return send_from_directory(app.static_folder, 'index.html')

@app.route('/analyze', methods=['POST'])
def analyze():
    data = request.get_json()
    code = data.get('code', '')

    # Try to use the compiled C++ analyzer
    if os.path.isfile(CXX_PATH):
        try:
            with tempfile.NamedTemporaryFile(mode='w', suffix='.py', delete=False, encoding='utf-8') as f:
                f.write(code)
                tmpfile = f.name

            result = subprocess.run(
                [CXX_PATH, tmpfile],
                capture_output=True, text=True, timeout=10
            )
            os.unlink(tmpfile)

            return jsonify({
                'source': 'cpp',
                'stdout': result.stdout,
                'stderr': result.stderr,
                'returncode': result.returncode
            })
        except Exception as e:
            return jsonify({'source': 'cpp', 'error': str(e)}), 500
    else:
        return jsonify({
            'source': 'web',
            'message': 'C++ backend not compiled. Use the browser-based frontend (JavaScript) for analysis.',
            'build_instructions': 'Run: make  (requires g++)'
        }), 200

@app.route('/build-status')
def build_status():
    return jsonify({
        'cpp_compiled': os.path.isfile(CXX_PATH),
        'cpp_source': os.path.isfile(SRC_PATH)
    })

if __name__ == '__main__':
    print("=" * 60)
    print("  Python Analyzer - Backend Server")
    print("=" * 60)
    print(f"  C++ binary: {'EXISTS' if os.path.isfile(CXX_PATH) else 'NOT FOUND'}")
    print(f"  Frontend:   {app.static_folder}")
    print(f"  URL:        http://localhost:5000")
    print("=" * 60)
    app.run(host='0.0.0.0', port=5000, debug=True)
