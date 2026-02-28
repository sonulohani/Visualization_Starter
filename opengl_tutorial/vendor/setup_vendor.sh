#!/usr/bin/env bash
# =============================================================================
# Vendor dependency setup script
#
# Downloads GLAD (OpenGL 3.3 Core loader) and stb_image into the vendor/
# directory so the project can be built without system-wide installation of
# these header-only / single-file libraries.
#
# Usage:  cd vendor && bash setup_vendor.sh
# =============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# ---- stb_image ----
echo "Downloading stb_image.h ..."
mkdir -p stb
curl -sL "https://raw.githubusercontent.com/nothings/stb/master/stb_image.h" \
     -o stb/stb_image.h
echo "  -> stb/stb_image.h"

# ---- GLAD ----
# GLAD must be generated for your specific OpenGL version. The easiest way:
#   1. Visit https://glad.dav1d.de/
#   2. Language: C/C++, Specification: OpenGL, Profile: Core, API gl: 3.3
#   3. Click Generate, download the zip
#   4. Extract so you have:
#        vendor/glad/include/glad/glad.h
#        vendor/glad/include/KHR/khrplatform.h
#        vendor/glad/src/glad.c
#
# Alternatively, install the glad Python package and generate from CLI:
if command -v glad &>/dev/null || python3 -m glad --help &>/dev/null 2>&1; then
    echo "Generating GLAD with the glad CLI tool ..."
    mkdir -p glad
    python3 -m glad --profile core --api gl=3.3 --generator c --out-path glad \
        2>/dev/null \
    || glad --profile core --api gl=3.3 --generator c --out-path glad \
        2>/dev/null \
    || {
        echo "  [!] glad CLI generation failed. Please generate manually."
        echo "      Visit https://glad.dav1d.de/ — see instructions above."
    }
    echo "  -> glad/include/glad/glad.h"
    echo "  -> glad/src/glad.c"
else
    echo "[!] GLAD CLI not found. Install with:  pip install glad"
    echo "    Or generate manually at https://glad.dav1d.de/"
    echo "    Then place files in vendor/glad/ as described in CMakeLists.txt."
fi

echo ""
echo "Done. You can now build the project:"
echo "  cd .. && mkdir -p build && cd build && cmake .. && make -j\$(nproc)"
